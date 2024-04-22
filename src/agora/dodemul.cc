/**
 * @file dodemul.cc
 * @brief Implmentation file for the DoDemul class.
 */
#include "dodemul.h"

#include "comms-lib.h"
#include "concurrent_queue_wrapper.h"
#include "modulation.h"

static constexpr bool kUseSIMDGather = true;
static constexpr bool kPrintBeamWeights = false;
static constexpr bool kPrintInputData = false;
static constexpr bool kPrintEquOutput = false;
static constexpr bool kPrintDemulInput = false;
static constexpr bool kPrintDemulOutput = false;

DoDemul::DoDemul(
    Config* config, int tid, Table<complex_float>& data_buffer,
    PtrGrid<kFrameWnd, kMaxDataSCs, complex_float>& ul_beam_matrices,
    Table<complex_float>& ue_spec_pilot_buffer,
    Table<complex_float>& equal_buffer,
    PtrCube<kFrameWnd, kMaxSymbols, kMaxUEs, int8_t>& demod_buffers,
    MacScheduler* mac_sched, PhyStats* in_phy_stats, Stats* stats_manager)
    : Doer(config, tid),
      data_buffer_(data_buffer),
      ul_beam_matrices_(ul_beam_matrices),
      ue_spec_pilot_buffer_(ue_spec_pilot_buffer),
      equal_buffer_(equal_buffer),
      demod_buffers_(demod_buffers),
      mac_sched_(mac_sched),
      phy_stats_(in_phy_stats) {
  duration_stat_ = stats_manager->GetDurationStat(DoerType::kDemul, tid);

  data_gather_buffer_ =
      static_cast<complex_float*>(Agora_memory::PaddedAlignedAlloc(
          Agora_memory::Alignment_t::kAlign64,
          kSCsPerCacheline * kMaxAntennas * sizeof(complex_float)));
  equaled_buffer_temp_ =
      static_cast<complex_float*>(Agora_memory::PaddedAlignedAlloc(
          Agora_memory::Alignment_t::kAlign64,
          cfg_->DemulBlockSize() * kMaxUEs * sizeof(complex_float)));
  equaled_buffer_temp_transposed_ =
      static_cast<complex_float*>(Agora_memory::PaddedAlignedAlloc(
          Agora_memory::Alignment_t::kAlign64,
          cfg_->DemulBlockSize() * kMaxUEs * sizeof(complex_float)));

  // phase offset calibration data
  arma::cx_float* ue_pilot_ptr =
      reinterpret_cast<arma::cx_float*>(cfg_->UeSpecificPilot()[0]);
  arma::cx_fmat mat_pilot_data(ue_pilot_ptr, cfg_->OfdmDataNum(),
                               cfg_->UeAntNum(), false);
  ue_pilot_data_ = mat_pilot_data.st();
}

DoDemul::~DoDemul() {
  std::free(data_gather_buffer_);
  std::free(equaled_buffer_temp_);
  std::free(equaled_buffer_temp_transposed_);
}

EventData DoDemul::Launch(size_t tag) {
  const size_t frame_id = gen_tag_t(tag).frame_id_;
  const size_t symbol_id = gen_tag_t(tag).symbol_id_;
  const size_t base_sc_id = gen_tag_t(tag).sc_id_;

  const size_t symbol_idx_ul = this->cfg_->Frame().GetULSymbolIdx(symbol_id);
  const size_t total_symbol_idx_ul =
      cfg_->GetTotalSymbolIdxUl(frame_id, symbol_idx_ul);
  const complex_float* data_buf = data_buffer_[total_symbol_idx_ul];

  const size_t frame_slot = frame_id % kFrameWnd;
  const size_t data_symbol_idx_ul =
      symbol_idx_ul - this->cfg_->Frame().ClientUlPilotSymbols();
  const size_t total_data_symbol_idx_ul =
      cfg_->GetTotalDataSymbolIdxUl(frame_id, symbol_idx_ul);

  auto ue_list = mac_sched_->ScheduledUeList(frame_id, base_sc_id);
  size_t n_users = ue_list.n_elem;
  size_t start_tsc = GetTime::WorkerRdtsc();

  if (kDebugPrintInTask == true) {
    std::printf(
        "In doDemul tid %d: frame: %zu, symbol idx: %zu, symbol idx ul: %zu, "
        "subcarrier: %zu, databuffer idx %zu \n",
        tid_, frame_id, symbol_id, symbol_idx_ul, base_sc_id,
        total_symbol_idx_ul);
  }

  size_t max_sc_ite =
      std::min(cfg_->DemulBlockSize(), cfg_->OfdmDataNum() - base_sc_id);
  assert(max_sc_ite % kSCsPerCacheline == 0);
  // Iterate through cache lines
  for (size_t i = 0; i < max_sc_ite; i += kSCsPerCacheline) {
    size_t start_tsc0 = GetTime::WorkerRdtsc();

    // Step 1: Populate data_gather_buffer as a row-major matrix with
    // kSCsPerCacheline rows and BsAntNum() columns

    // Since kSCsPerCacheline divides demul_block_size and
    // kTransposeBlockSize, all subcarriers (base_sc_id + i) lie in the
    // same partial transpose block.
    const size_t partial_transpose_block_base =
        ((base_sc_id + i) / kTransposeBlockSize) *
        (kTransposeBlockSize * cfg_->BsAntNum());

#ifdef __AVX512F__
    static constexpr size_t kAntNumPerSimd = 8;
#else
    static constexpr size_t kAntNumPerSimd = 4;
#endif

    size_t ant_start = 0;
    if (kUseSIMDGather && kUsePartialTrans &&
        (cfg_->BsAntNum() % kAntNumPerSimd) == 0) {
      // Gather data for all antennas and 8 subcarriers in the same cache
      // line, 1 subcarrier and 4 (AVX2) or 8 (AVX512) ants per iteration
      size_t cur_sc_offset =
          partial_transpose_block_base + (base_sc_id + i) % kTransposeBlockSize;
      const float* src =
          reinterpret_cast<const float*>(&data_buf[cur_sc_offset]);
      float* dst = reinterpret_cast<float*>(data_gather_buffer_);
#ifdef __AVX512F__
      __m512i index = _mm512_setr_epi32(
          0, 1, kTransposeBlockSize * 2, kTransposeBlockSize * 2 + 1,
          kTransposeBlockSize * 4, kTransposeBlockSize * 4 + 1,
          kTransposeBlockSize * 6, kTransposeBlockSize * 6 + 1,
          kTransposeBlockSize * 8, kTransposeBlockSize * 8 + 1,
          kTransposeBlockSize * 10, kTransposeBlockSize * 10 + 1,
          kTransposeBlockSize * 12, kTransposeBlockSize * 12 + 1,
          kTransposeBlockSize * 14, kTransposeBlockSize * 14 + 1);
      for (size_t ant_i = 0; ant_i < cfg_->BsAntNum();
           ant_i += kAntNumPerSimd) {
        for (size_t j = 0; j < kSCsPerCacheline; j++) {
          __m512 data_rx = kTransposeBlockSize == 1
                               ? _mm512_load_ps(&src[j * cfg_->BsAntNum() * 2])
                               : _mm512_i32gather_ps(index, &src[j * 2], 4);

          assert((reinterpret_cast<intptr_t>(&dst[j * cfg_->BsAntNum() * 2]) %
                  (kAntNumPerSimd * sizeof(float) * 2)) == 0);
          assert((reinterpret_cast<intptr_t>(&src[j * cfg_->BsAntNum() * 2]) %
                  (kAntNumPerSimd * sizeof(float) * 2)) == 0);
          _mm512_store_ps(&dst[j * cfg_->BsAntNum() * 2], data_rx);
        }
        src += kAntNumPerSimd * kTransposeBlockSize * 2;
        dst += kAntNumPerSimd * 2;
      }
#else
      __m256i index = _mm256_setr_epi32(
          0, 1, kTransposeBlockSize * 2, kTransposeBlockSize * 2 + 1,
          kTransposeBlockSize * 4, kTransposeBlockSize * 4 + 1,
          kTransposeBlockSize * 6, kTransposeBlockSize * 6 + 1);
      for (size_t ant_i = 0; ant_i < cfg_->BsAntNum();
           ant_i += kAntNumPerSimd) {
        for (size_t j = 0; j < kSCsPerCacheline; j++) {
          assert((reinterpret_cast<intptr_t>(&dst[j * cfg_->BsAntNum() * 2]) %
                  (kAntNumPerSimd * sizeof(float) * 2)) == 0);
          __m256 data_rx = _mm256_i32gather_ps(&src[j * 2], index, 4);
          _mm256_store_ps(&dst[j * cfg_->BsAntNum() * 2], data_rx);
        }
        src += kAntNumPerSimd * kTransposeBlockSize * 2;
        dst += kAntNumPerSimd * 2;
      }
#endif
      // Set the remaining number of antennas for non-SIMD gather
      ant_start = cfg_->BsAntNum() - (cfg_->BsAntNum() % kAntNumPerSimd);
    }
    if (ant_start < cfg_->BsAntNum()) {
      complex_float* dst = data_gather_buffer_ + ant_start;
      for (size_t j = 0; j < kSCsPerCacheline; j++) {
        for (size_t ant_i = ant_start; ant_i < cfg_->BsAntNum(); ant_i++) {
          *dst++ =
              kUsePartialTrans
                  ? data_buf[partial_transpose_block_base +
                             (ant_i * kTransposeBlockSize) +
                             ((base_sc_id + i + j) % kTransposeBlockSize)]
                  : data_buf[ant_i * cfg_->OfdmDataNum() + base_sc_id + i + j];
        }
      }
    }
    duration_stat_->task_duration_[1] += GetTime::WorkerRdtsc() - start_tsc0;

    // Step 2: For each subcarrier, perform equalization by multiplying the
    // subcarrier's data from each antenna with the subcarrier's precoder
    for (size_t j = 0; j < kSCsPerCacheline; j++) {
      const size_t cur_sc_id = base_sc_id + i + j;

      arma::cx_float* equal_ptr = nullptr;
      if (kExportConstellation) {
        equal_ptr = (arma::cx_float*)(&equal_buffer_[total_symbol_idx_ul]
                                                    [cur_sc_id * n_users]);
      } else {
        equal_ptr =
            (arma::cx_float*)(&equaled_buffer_temp_[(cur_sc_id - base_sc_id) *
                                                    n_users]);
      }
      arma::cx_fmat mat_equaled(equal_ptr, n_users, 1, false);

      arma::cx_float* data_ptr = reinterpret_cast<arma::cx_float*>(
          &data_gather_buffer_[j * cfg_->BsAntNum()]);

      arma::cx_float* ul_beam_ptr = reinterpret_cast<arma::cx_float*>(
          ul_beam_matrices_[frame_slot][cfg_->GetBeamScId(cur_sc_id)]);

      size_t start_tsc2 = GetTime::WorkerRdtsc();
#if defined(USE_MKL_CBLAS)
      MKL_Complex8 alpha = {1, 0};
      MKL_Complex8 beta = {0, 0};

      cblas_cgemm(CblasColMajor, CblasNoTrans, CblasNoTrans, n_users, 1,
                  cfg_->BsAntNum(), &alpha, (MKL_Complex8*)ul_beam_ptr, n_users,
                  (MKL_Complex8*)data_ptr, cfg_->BsAntNum(), &beta,
                  (MKL_Complex8*)equal_ptr, n_users);
#else
      arma::cx_fmat mat_data(data_ptr, cfg_->BsAntNum(), 1, false);

      arma::cx_fmat mat_ul_beam(ul_beam_ptr, n_users, cfg_->BsAntNum(), false);
      mat_equaled = mat_ul_beam * mat_data;
#endif

      if (kPrintInputData) {
        if (cur_sc_id == 0) {
          arma::cx_fmat mat_ul_beam(ul_beam_ptr, n_users, cfg_->BsAntNum(),
                                    false);
          std::printf(
              "UL Beam Weights (in demul): frame %zu, cur sc id %zu, rows "
              "%lld, cols %lld\n",
              frame_id, cur_sc_id, mat_ul_beam.n_rows, mat_ul_beam.n_cols);
          for (arma::uword row = 0; row < mat_ul_beam.n_rows; row++) {
            for (arma::uword col = 0; col < mat_ul_beam.n_cols; col++) {
              std::printf("(%.3f +1j* %.3f) ", mat_ul_beam(row, col).real(),
                          mat_ul_beam(row, col).imag());
            }
            std::printf("\n");
          }
          std::printf("\n");
        }
      }

      if (kPrintInputData) {
        if (cur_sc_id == 0) {
          arma::cx_fmat mat_data(data_ptr, cfg_->BsAntNum(), 1, false);
          std::printf(
              "UL Data: frame %zu, cur sc id %zu, rows %lld, cols %lld\n",
              frame_id, cur_sc_id, mat_data.n_rows, mat_data.n_cols);
          for (arma::uword row = 0; row < mat_data.n_rows; row++) {
            for (arma::uword col = 0; col < mat_data.n_cols; col++) {
              std::printf("(%.3f+1j*%.3f) ", mat_data(row, col).real(),
                          mat_data(row, col).imag());
            }
            std::printf("\n");
          }
          std::printf("\n");
        }
      }

      if (kPrintEquOutput) {
        if (cur_sc_id == 0) {
          arma::cx_fmat mat_equal(equal_ptr, n_users, 1, false);
          std::printf(
              "Equalized output: frame %zu, cur sc id %zu, rows %lld, cols "
              "%lld\n",
              frame_id, cur_sc_id, mat_equal.n_rows, mat_equal.n_cols);
          for (arma::uword row = 0; row < mat_equal.n_rows; row++) {
            for (arma::uword col = 0; col < mat_equal.n_cols; col++) {
              std::printf("(%.3f+1j*%.3f) ", mat_equal(row, col).real(),
                          mat_equal(row, col).imag());
            }
            std::printf("\n");
          }
          std::printf("\n");
        }
      }

      if (symbol_idx_ul <
          cfg_->Frame().ClientUlPilotSymbols()) {  // Calc new phase shift
        if (symbol_idx_ul == 0 && cur_sc_id == 0) {
          // Reset previous frame
          arma::cx_float* phase_shift_ptr = reinterpret_cast<arma::cx_float*>(
              ue_spec_pilot_buffer_[(frame_id - 1) % kFrameWnd]);
          arma::cx_fmat mat_phase_shift(phase_shift_ptr, n_users,
                                        cfg_->Frame().ClientUlPilotSymbols(),
                                        false);
          mat_phase_shift.fill(0);
        }
        arma::cx_float* phase_shift_ptr = reinterpret_cast<arma::cx_float*>(
            &ue_spec_pilot_buffer_[frame_id % kFrameWnd]
                                  [symbol_idx_ul * n_users]);
        arma::cx_fmat mat_phase_shift(phase_shift_ptr, n_users, 1, false);

        arma::cx_fvec cur_sc_pilot_data = ue_pilot_data_.col(cur_sc_id);
        arma::cx_fmat shift_sc =
            arma::sign(mat_equaled % conj(cur_sc_pilot_data(ue_list)));
        mat_phase_shift += shift_sc;
      }
      // apply previously calc'ed phase shift to data
      else if (cfg_->Frame().ClientUlPilotSymbols() > 0) {
        arma::cx_float* pilot_corr_ptr = reinterpret_cast<arma::cx_float*>(
            ue_spec_pilot_buffer_[frame_id % kFrameWnd]);
        arma::cx_fmat pilot_corr_mat(pilot_corr_ptr, n_users,
                                     cfg_->Frame().ClientUlPilotSymbols(),
                                     false);
        arma::fmat theta_mat = arg(pilot_corr_mat);
        arma::fmat theta_inc = arma::zeros<arma::fmat>(n_users, 1);
        for (size_t s = 1; s < cfg_->Frame().ClientUlPilotSymbols(); s++) {
          arma::fmat theta_diff = theta_mat.col(s) - theta_mat.col(s - 1);
          theta_inc += theta_diff;
        }
        theta_inc /= (float)std::max(
            1, static_cast<int>(cfg_->Frame().ClientUlPilotSymbols() - 1));
        arma::fmat cur_theta = theta_mat.col(0) + (symbol_idx_ul * theta_inc);
        arma::cx_fmat mat_phase_correct =
            arma::zeros<arma::cx_fmat>(size(cur_theta));
        mat_phase_correct.set_real(cos(-cur_theta));
        mat_phase_correct.set_imag(sin(-cur_theta));
        mat_equaled %= mat_phase_correct;

        // Measure EVM from ground truth
        if (symbol_idx_ul >= cfg_->Frame().ClientUlPilotSymbols()) {
          if constexpr (kEnableMac == false) {
            phy_stats_->UpdateEvm(frame_id, data_symbol_idx_ul, cur_sc_id,
                                  mat_equaled.col(0), ue_list);
          }
        }
      }
      size_t start_tsc3 = GetTime::WorkerRdtsc();
      duration_stat_->task_duration_[2] += start_tsc3 - start_tsc2;
      duration_stat_->task_count_++;
    }
  }

  size_t start_tsc3 = GetTime::WorkerRdtsc();
  if (symbol_idx_ul >= cfg_->Frame().ClientUlPilotSymbols()) {
    __m256i index2 =
        _mm256_setr_epi32(0, 1, n_users * 2, n_users * 2 + 1, n_users * 4,
                          n_users * 4 + 1, n_users * 6, n_users * 6 + 1);
    auto* equal_t_ptr =
        reinterpret_cast<float*>(equaled_buffer_temp_transposed_);
    for (size_t ss_id = 0; ss_id < n_users; ss_id++) {
      float* equal_ptr = nullptr;
      if (kExportConstellation) {
        equal_ptr = reinterpret_cast<float*>(
            &equal_buffer_[total_symbol_idx_ul][base_sc_id * n_users + ss_id]);
      } else {
        equal_ptr = reinterpret_cast<float*>(equaled_buffer_temp_ + ss_id);
      }
      size_t k_num_double_in_sim_d256 =
          sizeof(__m256) / sizeof(double);  // == 4
      for (size_t j = 0; j < max_sc_ite / k_num_double_in_sim_d256; j++) {
        __m256 equal_t_temp = _mm256_i32gather_ps(equal_ptr, index2, 4);
        _mm256_store_ps(equal_t_ptr, equal_t_temp);
        equal_t_ptr += 8;
        equal_ptr += n_users * k_num_double_in_sim_d256 * 2;
      }
      equal_t_ptr = (float*)(equaled_buffer_temp_transposed_);
      int8_t* demod_ptr =
          demod_buffers_[frame_slot][data_symbol_idx_ul][ss_id] +
          (cfg_->ModOrderBits(Direction::kUplink) * base_sc_id);
      Demodulate(equal_t_ptr, demod_ptr, max_sc_ite,
                 cfg_->ModOrderBits(Direction::kUplink), kUplinkHardDemod);
      // if hard demod is enabled calculate BER with modulated bits
      if (((kPrintPhyStats || kEnableCsvLog) && kUplinkHardDemod)) {
        size_t ue_id =
            mac_sched_->ScheduledUeIndex(frame_id, base_sc_id, ss_id);
        phy_stats_->UpdateDecodedBits(
            ue_id, total_data_symbol_idx_ul, frame_slot,
            max_sc_ite * cfg_->ModOrderBits(Direction::kUplink));
        // Each block here is max_sc_ite
        phy_stats_->IncrementDecodedBlocks(ue_id, total_data_symbol_idx_ul,
                                           frame_slot);
        size_t block_error(0);
        int8_t* tx_bytes =
            cfg_->GetModBitsBuf(cfg_->UlModBits(), Direction::kUplink, 0,
                                data_symbol_idx_ul, ue_id, base_sc_id);
        for (size_t i = 0; i < max_sc_ite; i++) {
          uint8_t rx_byte = static_cast<uint8_t>(demod_ptr[i]);
          uint8_t tx_byte = static_cast<uint8_t>(tx_bytes[i]);
          phy_stats_->UpdateBitErrors(ue_id, total_data_symbol_idx_ul,
                                      frame_slot, tx_byte, rx_byte);
          if (rx_byte != tx_byte) {
            block_error++;
          }
        }
        phy_stats_->UpdateBlockErrors(ue_id, total_data_symbol_idx_ul,
                                      frame_slot, block_error);
      }

      if (kPrintDemulInput) {
        if (base_sc_id == 0) {
          std::printf(
              "Demul input: frame: %zu, symbol: %zu, ss id: %zu, "
              "base sc id: %zu, max sc ite: %zu\n",
              frame_id, symbol_idx_ul, ss_id, base_sc_id, max_sc_ite);
          for (size_t k = 0; k < max_sc_ite; k++) {
            std::printf("%f ", equal_ptr[k]);
          }
          std::printf("\n");
        }
      }

      if (kPrintDemulOutput) {
        if (base_sc_id == 0) {
          std::printf(
              "Demuled data: frame: %zu, symbol: %zu, ss id: %zu, base sc id: "
              "%zu, "
              "num sc: %zu\n",
              frame_id, symbol_idx_ul, ss_id, base_sc_id,
              max_sc_ite * cfg_->ModOrderBits(Direction::kUplink));
          for (size_t k = 0;
               k < max_sc_ite * cfg_->ModOrderBits(Direction::kUplink); k++) {
            std::printf("%i ", demod_ptr[k]);
          }
          std::printf("\n");
        }
      }
    }
  }
  duration_stat_->task_duration_[3] += GetTime::WorkerRdtsc() - start_tsc3;
  duration_stat_->task_duration_[0] += GetTime::WorkerRdtsc() - start_tsc;
  return {EventType::kDemul, tag};
}
