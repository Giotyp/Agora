/**
 * @file stats.cc
 * @brief Implmentation file for the Stats class.
 */
#include "stats.h"

#include <typeinfo>

#include "gettime.h"
#include "logger.h"

static const std::string kProjectDir = TOSTRING(PROJECT_DIRECTORY);
static const std::string kStatsOutputFilePath =
    kProjectDir + "/files/experiment/";
static const std::string kStatsDataFilename =
    kStatsOutputFilePath + "timeresult.txt";
static const std::string kStatsDetailedDataFilename =
    kStatsOutputFilePath + "timeresult_detail.txt";
static const std::string kAgoraConfigFilename =
    kStatsOutputFilePath + "agora_config.txt";
static const std::string kMasterFilename =
    kStatsOutputFilePath + "timestamps_master.txt";
static const std::string kWorkerFilename =
    kStatsOutputFilePath + "timestamps_workers.txt";

Stats::Stats(const Config* const cfg)
    : config_(cfg),
      task_thread_num_(cfg->WorkerThreadNum()),
      fft_thread_num_(cfg->FftThreadNum()),
      beam_thread_num_(cfg->BeamThreadNum()),
      demul_thread_num_(cfg->DemulThreadNum()),
      decode_thread_num_(cfg->DecodeThreadNum()),
      freq_ghz_(cfg->FreqGhz()),
      creation_tsc_(GetTime::Rdtsc()) {
  frame_start_.Calloc(config_->SocketThreadNum(), kNumStatsFrames,
                      Agora_memory::Alignment_t::kAlign64);
}

Stats::~Stats() { frame_start_.Free(); }

void Stats::PopulateSummary(FrameSummary* frame_summary, size_t thread_id,
                            DoerType doer_type) {
  DurationStat* ds = GetDurationStat(doer_type, thread_id);
  DurationStat* ds_old = GetDurationStatOld(doer_type, thread_id);

  frame_summary->count_this_thread_ = ds->task_count_ - ds_old->task_count_;
  frame_summary->count_all_threads_ += frame_summary->count_this_thread_;

  for (size_t j = 0; j < break_down_num_; j++) {
    frame_summary->us_this_thread_.at(j) = GetTime::CyclesToUs(
        (ds->task_duration_.at(j) - ds_old->task_duration_.at(j)), freq_ghz_);
    frame_summary->us_avg_threads_.at(j) +=
        frame_summary->us_this_thread_.at(j);
  }
  *ds_old = *ds;
}

void Stats::ComputeAvgOverThreads(FrameSummary* frame_summary,
                                  size_t thread_num, size_t break_down_num_) {
  for (size_t j = 0; j < break_down_num_; j++) {
    frame_summary->us_avg_threads_.at(j) =
        frame_summary->us_avg_threads_.at(j) / thread_num;
  }
}

void Stats::PrintPerThreadPerTask(std::string const& doer_string,
                                  FrameSummary const& s) {
  if (s.count_this_thread_ > 0) {
    std::printf("%s: %zu tasks %.1f us (~", doer_string.c_str(),
                s.count_this_thread_,
                s.us_this_thread_.at(0u) / s.count_this_thread_);

    for (size_t i = 1u; i < s.us_this_thread_.size(); i++) {
      if (i != 1) {
        std::printf("+ ");
      }
      std::printf(" %.1f ", s.us_this_thread_.at(i) / s.count_this_thread_);
    }
    std::printf("us), ");
  }
}

std::string Stats::PrintPerFrame(std::string const& doer_string,
                                 FrameSummary const& frame_summary) {
  std::stringstream output;
  if (frame_summary.count_all_threads_ > 0) {
    output << doer_string.c_str() << " (" << frame_summary.count_all_threads_
           << " tasks): " << (frame_summary.us_avg_threads_.at(0u) / 1000.0f)
           << " ms (~";

    for (size_t i = 1u; i < frame_summary.us_avg_threads_.size(); i++) {
      if (i != 1) {
        output << "+ ";
      }
      output << frame_summary.us_avg_threads_.at(i) / 1000.0f << " ";
    }
    output << "ms), ";
  }
  return output.str();
}

void Stats::UpdateStats(size_t frame_id) {
  this->last_frame_id_ = frame_id;
  size_t frame_slot = (frame_id % kNumStatsFrames);

  if (kIsWorkerTimingEnabled) {
    std::vector<FrameSummary> work_summary(kAllDoerTypes.size());
    for (size_t i = 0u; i < task_thread_num_; i++) {
      for (size_t j = 0u; j < kAllDoerTypes.size(); j++) {
        PopulateSummary(&work_summary.at(j), i, kAllDoerTypes.at(j));
      }

      if (kDebugPrintStatsPerThread) {
        std::printf("In frame %zu, thread %zu, \t", frame_id, i);
        double sum_us_this_frame_this_thread = 0;
        for (size_t j = 0u; j < kAllDoerTypes.size(); j++) {
          PrintPerThreadPerTask(kDoerNames.at(kAllDoerTypes.at(j)),
                                work_summary.at(j));
          sum_us_this_frame_this_thread +=
              work_summary.at(j).us_this_thread_.at(0);
        }
        std::printf("sum: %.3f\n", sum_us_this_frame_this_thread);
      }
    }
    for (auto& summary : work_summary) {
      ComputeAvgOverThreads(&summary, task_thread_num_, break_down_num_);
    }

    double sum_us = 0.0f;
    for (size_t i = 0u; i < this->doer_us_.size(); i++) {
      double us_avg = work_summary.at(i).us_avg_threads_.at(0u);
      this->doer_us_.at(i).at(frame_slot) = us_avg;
      sum_us += us_avg;
    }

    for (size_t i = 0; i < this->break_down_num_; i++) {
      for (size_t doer = 0; doer < work_summary.size(); doer++) {
        this->doer_breakdown_us_.at(doer).at(i).at(frame_slot) =
            work_summary.at(doer).us_avg_threads_.at(i);
      }
    }

    if (kStatsPrintFrameSummary) {
      std::string print_summary =
          "Frame " + std::to_string(frame_id) + " Summary: ";

      for (size_t i = 0u; i < kAllDoerTypes.size(); i++) {
        print_summary += PrintPerFrame(kDoerNames.at(kAllDoerTypes.at(i)),
                                       work_summary.at(i));
      }
      print_summary += "Total: " + std::to_string(sum_us / 1000.0f) + " ms\n";
      AGORA_LOG_INFO("%s", print_summary.c_str());
    }
  }
}

double Stats::MeasureLastFrameLatency() {
  size_t frame_id = this->last_frame_id_;
  size_t ref_tsc = SIZE_MAX;

  for (size_t j = 0; j < config_->SocketThreadNum(); j++) {
    ref_tsc = std::min(ref_tsc, this->frame_start_[j][frame_id]);
  }
  double processing_started =
      MasterGetUsFromRef(TsType::kProcessingStarted, frame_id, ref_tsc);
  double decoding_done =
      MasterGetUsFromRef(TsType::kDecodeDone, frame_id, ref_tsc);

  return decoding_done - processing_started;
}

void Stats::SaveToFile() {
  AGORA_LOG_INFO("Stats: Saving master timestamps to %s\n",
                 kStatsDataFilename.c_str());
  FILE* fp_debug = std::fopen(kStatsDataFilename.c_str(), "w");
  RtAssert(fp_debug != nullptr,
           std::string("Open file failed ") + std::to_string(errno));

  size_t first_frame_idx = 0;
  size_t last_frame_idx = this->last_frame_id_;
  size_t total_stat_frames = this->last_frame_id_;
  if (total_stat_frames > kNumStatsFrames) {
    last_frame_idx = last_frame_idx % kNumStatsFrames;
    first_frame_idx = (last_frame_idx + 1) % kNumStatsFrames;
    total_stat_frames = kNumStatsFrames;
  }

  // For backwards compatibility, it is easier to make a new file format for
  // the combined case
  if ((config_->Frame().NumDLSyms() > 0) &&
      (config_->Frame().NumULSyms() > 0)) {
    std::fprintf(fp_debug,
                 "Pilot RX by socket threads (= reference time), "
                 "kFirstSymbolRX, kProcessingStarted, kPilotAllRX, kFFTPilotsDone, "
                 "kBeamDone, kRXDone, kDemulDone, kDecodeDone, kEncodeDone, "
                 "kPrecodeDone, kIFFTDone, kTXDone\n");

    for (size_t frame = 0; frame < total_stat_frames; frame++) {
      const size_t i = (first_frame_idx + frame) % kNumStatsFrames;
      size_t ref_tsc = SIZE_MAX;
      for (size_t j = 0; j < config_->SocketThreadNum(); j++) {
        ref_tsc = std::min(ref_tsc, this->frame_start_[j][i]);
      }
      std::fprintf(
          fp_debug,
          "%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
          GetTime::CyclesToUs(ref_tsc - this->creation_tsc_, this->freq_ghz_),
          MasterGetUsFromRef(TsType::kFirstSymbolRX, i, ref_tsc),
          MasterGetUsFromRef(TsType::kProcessingStarted, i, ref_tsc),
          MasterGetUsFromRef(TsType::kPilotAllRX, i, ref_tsc),
          MasterGetUsFromRef(TsType::kFFTPilotsDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kBeamDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kRXDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kDemulDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kDecodeDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kEncodeDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kPrecodeDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kIFFTDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kTXDone, i, ref_tsc));

    }
  } else if (config_->Frame().NumDLSyms() > 0) {
    std::fprintf(fp_debug,
                 "Pilot RX by socket threads (= reference time), "
                 "kFirstSymbolRX, kProcessingStarted, kPilotAllRX, kFFTPilotsDone, "
                 "kBeamDone, kPrecodeDone, kIFFTDone, kEncodeDone, kRXDone\n");
    for (size_t frame = 0; frame < total_stat_frames; frame++) {
      const size_t i = (first_frame_idx + frame) % kNumStatsFrames;
      size_t ref_tsc = SIZE_MAX;
      for (size_t j = 0; j < config_->SocketThreadNum(); j++) {
        ref_tsc = std::min(ref_tsc, this->frame_start_[j][i]);
      }
      std::fprintf(
          fp_debug, "%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
          GetTime::CyclesToUs(ref_tsc - this->creation_tsc_, this->freq_ghz_),
          MasterGetUsFromRef(TsType::kFirstSymbolRX, i, ref_tsc),
          MasterGetUsFromRef(TsType::kProcessingStarted, i, ref_tsc),
          MasterGetUsFromRef(TsType::kPilotAllRX, i, ref_tsc),
          MasterGetUsFromRef(TsType::kFFTPilotsDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kBeamDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kPrecodeDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kIFFTDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kEncodeDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kRXDone, i, ref_tsc));
    }
  } else if (config_->Frame().NumULSyms() > 0) {
    // Print the header
    std::fprintf(
        fp_debug,
        "Pilot RX by socket threads (= reference time), "
        "kFirstSymbolRX, kProcessingStarted, kPilotAllRX, kFFTPilotsDone, "
        "kBeamDone, kDemulDone, kDecodeDone, kRXDone\n");
    for (size_t frame = 0; frame < total_stat_frames; frame++) {
      const size_t i = (first_frame_idx + frame) % kNumStatsFrames;
      size_t ref_tsc = SIZE_MAX;
      for (size_t j = 0; j < config_->SocketThreadNum(); j++) {
        ref_tsc = std::min(ref_tsc, this->frame_start_[j][i]);
      }
      std::fprintf(
          fp_debug,
          "%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
          GetTime::CyclesToUs(ref_tsc - this->creation_tsc_, this->freq_ghz_),
          MasterGetUsFromRef(TsType::kFirstSymbolRX, i, ref_tsc),
          MasterGetUsFromRef(TsType::kProcessingStarted, i, ref_tsc),
          MasterGetUsFromRef(TsType::kPilotAllRX, i, ref_tsc),
          MasterGetUsFromRef(TsType::kFFTPilotsDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kBeamDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kDemulDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kDecodeDone, i, ref_tsc),
          MasterGetUsFromRef(TsType::kRXDone, i, ref_tsc));
    }
  } else {
    // Shouldn't happen
    RtAssert(false,
             std::string("No uplink or downlink symbols in the frame\n"));
  }
  std::fclose(fp_debug);

  if (config_->EnableProfiling() == true) {

    AGORA_LOG_INFO("Stats: Printing Agora configurations to %s\n",
                   kAgoraConfigFilename.c_str());

    FILE* fp_agora_config =
        std::fopen(kAgoraConfigFilename.c_str(), "w");

    RtAssert(fp_agora_config != nullptr,
             std::string("Open file failed ") + std::to_string(errno));

    // Print Agora configuration parameters
    std::fprintf(fp_agora_config,
                 "freq_ghz, rate, fft_size, ofdm_data_num, samps_per_symbol, "
                 "demul_block_size, bs_radio_num, ue_radio_num, worker_thread_num, "
                 "pilot_symbol_num_perframe, ul_symbol_num_perframe, "
                 "dl_symbol_num_perframe, total_symbol_num_perframe, max_frame, profiling_frame\n");

    std::fprintf(fp_agora_config,
                 "%f %.0f %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu\n",
                 config_->FreqGhz(), config_->Rate(), config_->OfdmCaNum(),
                 config_->OfdmDataNum(), config_->SampsPerSymbol(), config_->DemulBlockSize(),
                 config_->NumRadios(), config_->UeNum(), config_->WorkerThreadNum(),
                 config_->Frame().NumPilotSyms(), config_->Frame().NumULSyms(),
                 config_->Frame().NumDLSyms(), config_->Frame().NumTotalSyms(),
                 config_->FramesToTest(), config_->FrameToProfile());

    std::fclose(fp_agora_config);

    AGORA_LOG_INFO("Stats: Printing detailed results to %s\n",
                   kStatsDetailedDataFilename.c_str());

    FILE* fp_debug_detailed =
        std::fopen(kStatsDetailedDataFilename.c_str(), "w");

    RtAssert(fp_debug_detailed != nullptr,
             std::string("Open file failed ") + std::to_string(errno));
             
    // Print the header
    std::fprintf(fp_debug_detailed,
                 "fft_0, fft_1, fft_2, csi_0, csi_1, csi_2, "
                 "beam_0, beam_1, beam_2, demul_0, demul_1, demul_2, "
                 "decode_0, decode_1, decode_2, encode_0, encode_1, encode_2, "
                 "ifft_0, ifft_1, ifft_2, broadcast_0, broadcast_1, broadcast_2, "
                 "precode_0, precode_1, precode_2, rc_0, rc_1, rc_2\n");

    for (size_t frame = 0; frame < total_stat_frames; frame++) {
      const size_t i = (first_frame_idx + frame) % kNumStatsFrames;
      std::fprintf(
          fp_debug_detailed,
          "%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f "
          "%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kFFT))
              .at(0)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kFFT))
              .at(1)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kFFT))
              .at(2)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kCSI))
              .at(0)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kCSI))
              .at(1)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kCSI))
              .at(2)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kBeam))
              .at(0)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kBeam))
              .at(1)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kBeam))
              .at(2)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kDemul))
              .at(0)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kDemul))
              .at(1)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kDemul))
              .at(2)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kDecode))
              .at(0)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kDecode))
              .at(1)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kDecode))
              .at(2)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kEncode))
              .at(0)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kEncode))
              .at(1)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kEncode))
              .at(2)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kIFFT))
              .at(0)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kIFFT))
              .at(1)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kIFFT))
              .at(2)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kBroadcast))
              .at(0)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kBroadcast))
              .at(1)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kBroadcast))
              .at(2)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kPrecode))
              .at(0)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kPrecode))
              .at(1)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kPrecode))
              .at(2)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kRC))
              .at(0)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kRC))
              .at(1)
              .at(i),
          this->doer_breakdown_us_.at(static_cast<size_t>(DoerType::kRC))
              .at(2)
              .at(i));
    }
    std::fclose(fp_debug_detailed);
  }

  if (config_->EnableProfiling() == true) {

    FILE* fp_master = std::fopen(kMasterFilename.c_str(), "w");

    RtAssert(fp_master != nullptr,
             std::string("Open file failed ") + std::to_string(errno));

    std::printf("Master: Saving breakdown timestamps to %s\n", kMasterFilename.c_str());
  
    std::fprintf(
      fp_master, "Master frame %zu: dequeue %zu tasks, start: %zu\n",
      config_->FrameToProfile(), config_->dequeue_stats_id_,
      MasterGetTsc(TsType::kFirstSymbolRX, config_->FrameToProfile()));

    for (size_t i = 0; i < config_->Frame().NumTotalSyms(); i++) {
      for (size_t j = 0; j < config_->enqueue_stats_id_.at(i); j++) {
        std::fprintf(fp_master,
                     "Master frame %zu symbol %zu: enqueue task %s tsc "
                     "[%zu-%zu] = %.3f\n",
                     config_->FrameToProfile(), i,
                     eventTypeToString.at(static_cast<size_t>(config_->enqueue_stats_[i][j].event_type_)).c_str(),
                     config_->enqueue_stats_[i][j].tsc_end_,
                     config_->enqueue_stats_[i][j].tsc_start_,
                     GetTime::CyclesToUs(config_->enqueue_stats_[i][j].tsc_end_ -
                                         config_->enqueue_stats_[i][j].tsc_start_,
                                         config_->FreqGhz()));
      }
    }

    for (size_t i = 0; i < config_->dequeue_stats_id_; i++) {
      std::fprintf(fp_master,
                   "Master frame %zu: dequeue task %s tsc [%zu-%zu] = %.3f\n",
                   config_->FrameToProfile(),
                   eventTypeToString.at(static_cast<size_t>(config_->dequeue_stats_[i].event_type_)).c_str(),
                   config_->dequeue_stats_[i].tsc_end_, config_->dequeue_stats_[i].tsc_start_,
                   GetTime::CyclesToUs(config_->dequeue_stats_[i].tsc_end_ - 
                                       config_->dequeue_stats_[i].tsc_start_,
                                       config_->FreqGhz()));
    }
    std::fclose(fp_master);
  }

  if (config_->EnableProfiling() == true) {

    FILE* fp_worker = std::fopen(kWorkerFilename.c_str(), "w");

    RtAssert(fp_worker != nullptr,
             std::string("Open file failed ") + std::to_string(errno));

    std::printf("Saving breakdown timestamps of workers to %s\n",
                kWorkerFilename.c_str());

    for (size_t tid = 0; tid < task_thread_num_; tid++) {
      for (size_t i = config_->FrameToProfile(); i <= this->last_frame_id_; i += 2000) {
        std::fprintf(
          fp_worker,
          "Worker %zu frame %zu: %zu enqueue takes %.2f us, dequeue takes %.2f us"
          "(non-empty: %.2f)\n",
          tid, i, config_->worker_num_valid_enqueue_[tid][i],
          GetTime::CyclesToUs(config_->total_worker_enqueue_tsc_[tid][i],
                              config_->FreqGhz()),
          GetTime::CyclesToUs(config_->total_worker_dequeue_tsc_[tid][i],
                              config_->FreqGhz()),
          GetTime::CyclesToUs(config_->total_worker_valid_dequeue_tsc_[tid][i],
                              config_->FreqGhz()));
      }

      for (size_t i = 0; i < config_->Frame().NumTotalSyms(); i++) {
        for (size_t j = 0; j < config_->worker_enqueue_stats_id_[tid].at(i); j++) {
          std::fprintf(
            fp_worker,
            "Worker %zu frame %zu symbol %zu: enqueue task %s tsc [%zu-%zu] = %.3f\n",
            tid, config_->FrameToProfile(), i,
            eventTypeToString.at(static_cast<size_t>(config_->worker_enqueue_stats_[tid][i][j].event_type_)).c_str(),
            config_->worker_enqueue_stats_[tid][i][j].tsc_end_,
            config_->worker_enqueue_stats_[tid][i][j].tsc_start_,
            GetTime::CyclesToUs(config_->worker_enqueue_stats_[tid][i][j].tsc_end_ -
                                config_->worker_enqueue_stats_[tid][i][j].tsc_start_,
                                config_->FreqGhz()));
        }
      }
      for (size_t i = 0; i < config_->Frame().NumTotalSyms(); i++) {
        for (size_t j = 0; j < config_->worker_dequeue_stats_id_[tid].at(i); j++) {
          std::fprintf(
            fp_worker,
            "Worker %zu frame %zu symbol %zu: dequeue task %s tsc [%zu-%zu] = "
            "%.3f\n",
            tid, config_->FrameToProfile(), i,
            eventTypeToString.at(static_cast<size_t>(config_->worker_dequeue_stats_[tid][i][j].event_type_)).c_str(),
            config_->worker_dequeue_stats_[tid][i][j].tsc_end_,
            config_->worker_dequeue_stats_[tid][i][j].tsc_start_,
            GetTime::CyclesToUs(config_->worker_dequeue_stats_[tid][i][j].tsc_end_ -
                                config_->worker_dequeue_stats_[tid][i][j].tsc_start_,
                                config_->FreqGhz()));
        }
      }
    }
    std::fclose(fp_worker);
  }
}

size_t Stats::GetTotalTaskCount(DoerType doer_type, size_t thread_num) {
  size_t total_count = 0;
  for (size_t i = 0; i < thread_num; i++) {
    total_count = total_count + GetDurationStat(doer_type, i)->task_count_;
  }
  return total_count;
}

void Stats::PrintSummary() {
  AGORA_LOG_INFO("Stats: total processed frames %zu\n",
                 this->last_frame_id_ + 1);
  if (kIsWorkerTimingEnabled == false) {
    AGORA_LOG_INFO("Stats: Worker timing is disabled. Not printing summary\n");
  } else {
    std::vector<size_t> num_tasks;

    num_tasks.reserve(kAllDoerTypes.size());
    for (auto k_all_doer_type : kAllDoerTypes) {
      num_tasks.push_back(GetTotalTaskCount(k_all_doer_type, task_thread_num_));
    }

    double csi_frames =
        (static_cast<double>(
            num_tasks.at(static_cast<size_t>(DoerType::kCSI)))) /
        (this->config_->BsAntNum() * this->config_->Frame().NumPilotSyms());
    double beam_frames = (static_cast<double>(num_tasks.at(
                             static_cast<size_t>(DoerType::kBeam)))) /
                         this->config_->BeamEventsPerSymbol();

    if (config_->Frame().NumDLSyms() > 0) {
      double precode_frames =
          (static_cast<double>(
              num_tasks.at(static_cast<size_t>(DoerType::kPrecode)))) /
          (this->config_->OfdmDataNum() * this->config_->Frame().NumDLSyms());
      double ifft_frames =
          (static_cast<double>(
              num_tasks.at(static_cast<size_t>(DoerType::kIFFT)))) /
          (this->config_->BsAntNum() * this->config_->Frame().NumDLSyms());
      double encode_frames =
          (static_cast<double>(
              num_tasks.at(static_cast<size_t>(DoerType::kEncode)))) /
          (this->config_->LdpcConfig(Direction::kDownlink).NumBlocksInSymbol() *
           this->config_->UeAntNum() * this->config_->Frame().NumDLSyms());
      std::printf("Downlink totals (tasks, frames): ");
      std::printf("CSI (%zu, %.2f), ",
                  num_tasks.at(static_cast<size_t>(DoerType::kCSI)),
                  csi_frames);
      std::printf("Beamweights (%zu, %.2f), ",
                  num_tasks.at(static_cast<size_t>(DoerType::kBeam)),
                  beam_frames);
      std::printf("Encode (%zu, %.2f), ",
                  num_tasks.at(static_cast<size_t>(DoerType::kEncode)),
                  encode_frames);
      std::printf("Precode (%zu, %.2f), ",
                  num_tasks.at(static_cast<size_t>(DoerType::kPrecode)),
                  precode_frames);
      std::printf("IFFT (%zu, %.2f)",
                  num_tasks.at(static_cast<size_t>(DoerType::kIFFT)),
                  ifft_frames);
      std::printf("\n");
    }  // config_->frame().NumDLSyms() > 0

    if (config_->Frame().NumULSyms() > 0) {
      double fft_frames =
          (static_cast<double>(
              num_tasks.at(static_cast<size_t>(DoerType::kFFT)))) /
          (this->config_->BsAntNum() * this->config_->Frame().NumULSyms());
      double demul_frames =
          (static_cast<double>(
              num_tasks.at(static_cast<size_t>(DoerType::kDemul)))) /
          (this->config_->OfdmDataNum() * this->config_->Frame().NumULSyms());
      double decode_frames =
          (static_cast<double>(
              num_tasks.at(static_cast<size_t>(DoerType::kDecode)))) /
          (this->config_->LdpcConfig(Direction::kUplink).NumBlocksInSymbol() *
           this->config_->UeAntNum() * this->config_->Frame().NumULSyms());
      std::printf("Uplink totals (tasks, frames): ");
      std::printf("CSI (%zu, %.2f), ",
                  num_tasks.at(static_cast<size_t>(DoerType::kCSI)),
                  csi_frames);
      std::printf("Beamweights (%zu, %.2f), ",
                  num_tasks.at(static_cast<size_t>(DoerType::kBeam)),
                  beam_frames);
      std::printf("FFT (%zu, %.2f), ",
                  num_tasks.at(static_cast<size_t>(DoerType::kFFT)),
                  fft_frames);
      std::printf("Demul (%zu, %.2f), ",
                  num_tasks.at(static_cast<size_t>(DoerType::kDemul)),
                  demul_frames);
      std::printf("Decode (%zu, %.2f)",
                  num_tasks.at(static_cast<size_t>(DoerType::kDecode)),
                  decode_frames);
      std::printf("\n");
    }  // config_->frame().NumULSyms() > 0

    for (size_t i = 0; i < task_thread_num_; i++) {
      std::printf("Thread %zu performed (tasks, fraction of tasks): ", i);
      for (size_t j = 0u; j < kAllDoerTypes.size(); j++) {
        size_t duration_stat =
            GetDurationStat(kAllDoerTypes.at(j), i)->task_count_;
        if (duration_stat > 0) {
          double percent_stat =
              (static_cast<double>(duration_stat) * 100.0f) / num_tasks.at(j);
          std::printf("%s (%zu, %.2f%%), ",
                      kDoerNames.at(kAllDoerTypes.at(j)).c_str(), duration_stat,
                      percent_stat);
        }
      }
      std::printf("\n");
    }
  }  // kIsWorkerTimingEnabled == true
}

void Stats::PrintPerFrameDone(PrintType print_type, size_t frame_id) const {
  if (kDebugPrintPerFrameDone == true) {
    switch (print_type) {
      case (PrintType::kPacketRXPilots):
        AGORA_LOG_INFO("Main [frame %zu + %.2f ms]: Received all pilots\n",
                       frame_id,
                       MasterGetDeltaMs(TsType::kPilotAllRX,
                                        TsType::kFirstSymbolRX, frame_id));
        break;
      case (PrintType::kPacketRX):
        AGORA_LOG_INFO("Main [frame %zu + %.2f ms]: Received all packets\n",
                       frame_id,
                       MasterGetDeltaMs(TsType::kRXDone, TsType::kFirstSymbolRX,
                                        frame_id));
        break;
      case (PrintType::kFFTPilots):
        AGORA_LOG_INFO("Main [frame %zu + %.2f ms]: FFT-ed all pilots\n",
                       frame_id,
                       MasterGetDeltaMs(TsType::kFFTPilotsDone,
                                        TsType::kFirstSymbolRX, frame_id));
        break;
      case (PrintType::kFFTCal):
        AGORA_LOG_INFO(
            "Main [frame %zu + %.2f ms]: FFT-ed all calibration symbols\n",
            frame_id, MasterGetUsSince(TsType::kRCAllRX, frame_id) / 1000.0);
        break;
      case (PrintType::kBeam):
        AGORA_LOG_INFO(
            "Main [frame %zu + %.2f ms]: Completed %s beamweight calc\n",
            frame_id,
            MasterGetDeltaMs(TsType::kBeamDone, TsType::kFirstSymbolRX,
                             frame_id),
            config_->Beamforming().c_str());
        break;
      case (PrintType::kDemul):
        AGORA_LOG_INFO("Main [frame %zu + %.2f ms]: Completed demodulation\n",
                       frame_id,
                       MasterGetDeltaMs(TsType::kDemulDone,
                                        TsType::kFirstSymbolRX, frame_id));
        break;
      case (PrintType::kDecode):
        AGORA_LOG_INFO(
            "Main [frame %zu + %.2f ms]: Completed LDPC decoding (%zu UL "
            "symbols)\n",
            frame_id,
            MasterGetDeltaMs(TsType::kDecodeDone, TsType::kFirstSymbolRX,
                             frame_id),
            this->config_->Frame().NumULSyms());
        break;
      case (PrintType::kPacketFromMac):
        AGORA_LOG_INFO("Main [frame %zu + %.2f ms]: Completed MAC RX \n",
                       frame_id,
                       MasterGetMsSince(TsType::kFirstSymbolRX, frame_id));
        break;
      case (PrintType::kEncode):
        AGORA_LOG_INFO("Main [frame %zu + %.2f ms]: Completed LDPC encoding\n",
                       frame_id,
                       MasterGetDeltaMs(TsType::kEncodeDone,
                                        TsType::kFirstSymbolRX, frame_id));
        break;
      case (PrintType::kPrecode):
        AGORA_LOG_INFO("Main [frame %zu + %.2f ms]: Completed precoding\n",
                       frame_id,
                       MasterGetDeltaMs(TsType::kPrecodeDone,
                                        TsType::kFirstSymbolRX, frame_id));
        break;
      case (PrintType::kIFFT):
        AGORA_LOG_INFO("Main [frame %zu + %.2f ms]: Completed IFFT\n", frame_id,
                       MasterGetDeltaMs(TsType::kIFFTDone,
                                        TsType::kFirstSymbolRX, frame_id));
        break;
      case (PrintType::kPacketTXFirst):
        AGORA_LOG_INFO(
            "Main [frame %zu + %.2f ms]: Completed TX of first symbol\n",
            frame_id,
            MasterGetDeltaMs(TsType::kTXProcessedFirst, TsType::kFirstSymbolRX,
                             frame_id));
        break;
      case (PrintType::kPacketTX):
        AGORA_LOG_INFO(
            "Main [frame %zu + %.2f ms]: Completed TX (%zu DL Control & %zu DL "
            "Data symbols)\n",
            frame_id,
            MasterGetDeltaMs(TsType::kTXDone, TsType::kFirstSymbolRX, frame_id),
            this->config_->Frame().NumDlControlSyms(),
            this->config_->Frame().NumDLSyms());
        break;
      case (PrintType::kBroadcast):
        AGORA_LOG_INFO(
            "Main [frame %zu + %.2f ms]: Completed Generating (%zu DL Control "
            "symbols)\n",
            frame_id,
            MasterGetDeltaMs(TsType::kBroadcastDone, TsType::kFirstSymbolRX,
                             frame_id),
            this->config_->Frame().NumDlControlSyms());
        break;
      case (PrintType::kPacketToMac):
        AGORA_LOG_INFO("Main [frame %zu + %.2f ms]: Completed MAC TX \n",
                       frame_id,
                       MasterGetMsSince(TsType::kFirstSymbolRX, frame_id));
        break;
      default:
        AGORA_LOG_ERROR("Wrong task type in frame done print!");
    }
  }
}

void Stats::PrintPerSymbolDone(PrintType print_type, size_t frame_id,
                               size_t symbol_id, size_t sub_count) const {
  if (kDebugPrintPerSymbolDone == true) {
    switch (print_type) {
      case (PrintType::kFFTPilots):
        AGORA_LOG_INFO(
            "Main [frame %zu symbol %zu + %.3f ms]: FFT-ed pilot symbol, "
            "%zu symbols done\n",
            frame_id, symbol_id,
            MasterGetMsSince(TsType::kFirstSymbolRX, frame_id), sub_count);
        break;
      case (PrintType::kFFTData):
        AGORA_LOG_INFO(
            "Main [frame %zu symbol %zu + %.3f ms]: FFT-ed data symbol, "
            "%zu symbols done\n",  //precoder status: %d\n",
            frame_id, symbol_id,
            MasterGetMsSince(TsType::kFirstSymbolRX, frame_id), sub_count);
        break;
      case (PrintType::kDemul):
        AGORA_LOG_INFO(
            "Main [frame %zu symbol %zu + %.3f ms]: Completed "
            "demodulation, "
            "%zu symbols done\n",
            frame_id, symbol_id,
            MasterGetMsSince(TsType::kFirstSymbolRX, frame_id), sub_count);
        break;
      case (PrintType::kDecode):
        AGORA_LOG_INFO(
            "Main [frame %zu symbol %zu + %.3f ms]: Completed decoding, "
            "%zu symbols done\n",
            frame_id, symbol_id,
            MasterGetMsSince(TsType::kFirstSymbolRX, frame_id), sub_count);
        break;
      case (PrintType::kEncode):
        AGORA_LOG_INFO(
            "Main [frame %zu symbol %zu + %.3f ms]: Completed encoding, "
            "%zu symbols done\n",
            frame_id, symbol_id,
            MasterGetMsSince(TsType::kFirstSymbolRX, frame_id), sub_count);
        break;
      case (PrintType::kPrecode):
        AGORA_LOG_INFO(
            "Main [frame %zu symbol %zu + %.3f ms]: Completed precoding, "
            "%zu symbols done\n",
            frame_id, symbol_id,
            MasterGetMsSince(TsType::kFirstSymbolRX, frame_id), sub_count);
        break;
      case (PrintType::kIFFT):
        AGORA_LOG_INFO(
            "Main [frame %zu symbol %zu + %.3f ms]: Completed IFFT, "
            "%zu symbols done\n",
            frame_id, symbol_id,
            MasterGetMsSince(TsType::kFirstSymbolRX, frame_id), sub_count);
        break;
      case (PrintType::kPacketTX):
        AGORA_LOG_INFO(
            "Main [frame %zu symbol %zu + %.3f ms]: Completed TX, "
            "%zu symbols done\n",
            frame_id, symbol_id,
            MasterGetMsSince(TsType::kFirstSymbolRX, frame_id), sub_count);
        break;
      case (PrintType::kPacketToMac):
        AGORA_LOG_INFO(
            "Main [frame %zu symbol %zu + %.3f ms]: Completed MAC TX, "
            "%zu symbols done\n",
            frame_id, symbol_id,
            MasterGetMsSince(TsType::kFirstSymbolRX, frame_id), sub_count);
        break;
      default:
        AGORA_LOG_INFO("Wrong task type in symbol done print!");
    }
  }
}

void Stats::PrintPerTaskDone(PrintType print_type, size_t frame_id,
                             size_t symbol_id, size_t ant_or_sc_id,
                             size_t task_count) const {
  if (kDebugPrintPerTaskDone == true) {
    switch (print_type) {
      case (PrintType::kBeam):
        AGORA_LOG_INFO(
            "Main thread: Beamweights done frame: %zu, subcarrier %zu\n",
            frame_id, ant_or_sc_id);
        break;
      case (PrintType::kRC):
        AGORA_LOG_INFO("Main thread: RC done frame: %zu, subcarrier %zu\n",
                       frame_id, ant_or_sc_id);
        break;
      case (PrintType::kDemul):
        AGORA_LOG_INFO(
            "Main thread: Demodulation done frame: %zu, symbol: %zu, sc: "
            "%zu, num blocks done: %zu\n",
            frame_id, symbol_id, ant_or_sc_id, task_count);
        break;
      case (PrintType::kDecode):
        AGORA_LOG_INFO(
            "Main thread: Decoding done frame: %zu, symbol: %zu, sc: %zu, "
            "num blocks done: %zu\n",
            frame_id, symbol_id, ant_or_sc_id, task_count);
        break;
      case (PrintType::kPrecode):
        AGORA_LOG_INFO(
            "Main thread: Precoding done frame: %zu, symbol: %zu, "
            "subcarrier: %zu, total SCs: %zu\n",
            frame_id, symbol_id, ant_or_sc_id, task_count);
        break;
      case (PrintType::kIFFT):
        AGORA_LOG_INFO(
            "Main thread: IFFT done frame: %zu, symbol: %zu, antenna: %zu, "
            "total ants: %zu\n",
            frame_id, symbol_id, ant_or_sc_id, task_count);
        break;
      case (PrintType::kPacketTX):
        AGORA_LOG_INFO(
            "Main thread: TX done frame: %zu, symbol: %zu, antenna: %zu, "
            "total packets: %zu\n",
            frame_id, symbol_id, ant_or_sc_id, task_count);
        break;
      default:
        AGORA_LOG_INFO("Wrong task type in task done print!");
    }
  }
}
