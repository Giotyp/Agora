/**
 * @file dodecode.cc
 * @brief Implmentation file for the DoDecode class. Currently, just supports
 * basestation
 */

#include "dodecode_acc.h"

#include "concurrent_queue_wrapper.h"

#include "rte_bbdev.h"
#include "rte_bbdev_op.h"
#include "rte_bus_vdev.h"


static constexpr bool kPrintLLRData = false;
static constexpr bool kPrintDecodedData = false;

static constexpr size_t kVarNodesSize = 1024 * 1024 * sizeof(int16_t);

struct thread_params {
	uint8_t dev_id;
	uint16_t queue_id;
	uint32_t lcore_id;
	uint64_t start_time;
	double ops_per_sec;
	double mbps;
	uint8_t iter_count;
	double iter_average;
	double bler;
	uint16_t nb_dequeued;
	int16_t processing_status;
	uint16_t burst_sz;
	struct test_op_params *op_params;
	struct rte_bbdev_dec_op *dec_ops[MAX_BURST];
	struct rte_bbdev_enc_op *enc_ops[MAX_BURST];
};

static int init_op_data_objs_from_table(
    struct rte_bbdev_op_data *bufs,
    int8_t* demod_data,
    struct rte_mempool *mbuf_pool, 
    const uint16_t n, 
    uint16_t min_alignment,
    size_t seg_length // Added seg_length as a parameter
    // rte_mbuf *m_head
) {
    int ret;
    unsigned int i, j;

    for (i = 0; i < n; ++i) {
        char *data;
        // std::cout << rte_mempool_avail_count(mbuf_pool) << std::endl;
        // if (rte_mempool_avail_count(mbuf_pool) == 0) {
        //     printf("No more mbufs available in the pool!\n");
        //     return -1;
        // }
        struct rte_mbuf *m_head = rte_pktmbuf_alloc(mbuf_pool);
        // if (m_head == nullptr) {
        //     std::cerr << "Error: Unable to create mbuf pool: " << rte_strerror(rte_errno) << std::endl;
        //     return -1;
        // }

        bufs[i].data = m_head;
        bufs[i].offset = 0;
        bufs[i].length = 0;

        data = rte_pktmbuf_append(m_head, seg_length);
        
        // Copy data from demod_data to the mbuf
        rte_memcpy(data, demod_data + (i * seg_length), seg_length);
        bufs[i].length += seg_length;

        // Continue the same as before
    }

    return 0;
}

static int
init_op_output_objs_from_buffer(struct rte_bbdev_op_data *bufs,
		uint8_t* decoded_buffer_ptr,
		struct rte_mempool *mbuf_pool, const uint16_t n, 
    uint16_t min_alignment, size_t seg_length)
{
	unsigned int i;

	for (i = 0; i < n; ++i) {
		// if (rte_mempool_avail_count(mbuf_pool) == 0) {
		// 	printf("No more mbufs available in the pool!\n");
		// 	return -1;
		// } 
		struct rte_mbuf *m_head = rte_pktmbuf_alloc(mbuf_pool);
		// if (m_head == nullptr) {
		// 	std::cerr << "Error: Unable to create mbuf pool: " << rte_strerror(rte_errno) << std::endl;
		// 	return -1;  // Exit the program with an error code
		// }

		bufs[i].data = m_head;
		bufs[i].offset = 0;
		bufs[i].length = 0;

		// Prepare the mbuf to receive the output data
		char* data = rte_pktmbuf_append(m_head, seg_length);
		assert(data == RTE_PTR_ALIGN(data, min_alignment));

		// Assuming you will copy data from decoded_buffer_ptr to data
		rte_memcpy(data, decoded_buffer_ptr + i * seg_length, seg_length);

		bufs[i].length += seg_length;
	}

	return 0;
}



DoDecode_ACC::DoDecode_ACC(
    Config* in_config, int in_tid,
    PtrCube<kFrameWnd, kMaxSymbols, kMaxUEs, int8_t>& demod_buffers,
    PtrCube<kFrameWnd, kMaxSymbols, kMaxUEs, int8_t>& decoded_buffers,
    PhyStats* in_phy_stats, Stats* in_stats_manager)
    : Doer(in_config, in_tid),
      demod_buffers_(demod_buffers),
      decoded_buffers_(decoded_buffers),
      phy_stats_(in_phy_stats),
      scrambler_(std::make_unique<AgoraScrambler::Scrambler>()) {
  duration_stat_ = in_stats_manager->GetDurationStat(DoerType::kDecode, in_tid);
  resp_var_nodes_ = static_cast<int16_t*>(Agora_memory::PaddedAlignedAlloc(
      Agora_memory::Alignment_t::kAlign64, kVarNodesSize));
    // std::cout<<"decode constructor"<<std::endl;
    std::string core_list = std::to_string(34);
    //  + "," + std::to_string(35) + "," + std::to_string(36) + "," + std::to_string(37);
    const char* rte_argv[] = {"txrx",        "-l",           core_list.c_str(),
                              "--log-level", "lib.eal:info", nullptr};
    int rte_argc = static_cast<int>(sizeof(rte_argv) / sizeof(rte_argv[0])) - 1;

    // Initialize DPDK environment
    // std::cout<<"getting ready to init dpdk" << std::endl;
    int ret = rte_eal_init(rte_argc, const_cast<char**>(rte_argv));
    RtAssert(
        ret >= 0,
        "Failed to initialize DPDK.  Are you running with root permissions?");

    int nb_bbdevs = rte_bbdev_count();
    // std::cout<<"num bbdevs: " << nb_bbdevs << std::endl;
    if (nb_bbdevs == 0)
      rte_exit(EXIT_FAILURE, "No bbdevs detected!\n");
    int ret_acc;
    struct rte_bbdev_info info;
    rte_bbdev_info_get(dev_id, &info);
    const struct rte_bbdev_info *dev_info = &info;
    const struct rte_bbdev_op_cap *op_cap = dev_info->drv.capabilities;
    // for (unsigned int i = 0; op_cap->type != RTE_BBDEV_OP_NONE; ++i, ++op_cap) {
    //   std::cout<<"capabilities is: " << op_cap->type << std::endl;
    // }

    rte_bbdev_intr_enable(dev_id);
    rte_bbdev_info_get(dev_id, &info);

    bbdev_op_pool= rte_bbdev_op_pool_create("bbdev_op_pool_dec", RTE_BBDEV_OP_LDPC_DEC, NB_MBUF, 128, rte_socket_id());
    ret = rte_bbdev_setup_queues(dev_id, 4, info.socket_id);

    struct rte_bbdev_queue_conf qconf;
    qconf.socket = info.socket_id;
    qconf.queue_size = info.drv.queue_size_lim;
    qconf.op_type = RTE_BBDEV_OP_LDPC_DEC;
    qconf.priority = 0;

    for (int q_id = 0; q_id < 4; q_id++) {
    /* Configure all queues belonging to this bbdev device */
      ret = rte_bbdev_queue_configure(dev_id, q_id, &qconf);
      if (ret < 0)
      rte_exit(EXIT_FAILURE,
      "ERROR(%d): BBDEV %u queue %u not configured properly\n",
      ret, dev_id, q_id);
    }

    ret = rte_bbdev_start(dev_id);

    ops_mp = rte_bbdev_op_pool_create("RTE_BBDEV_OP_LDPC_DEC_poo", RTE_BBDEV_OP_LDPC_DEC,
      num_ops, OPS_CACHE_SIZE, rte_socket_id());

    mbuf_pool = rte_pktmbuf_pool_create("bbdev_mbuf_pool", NB_MBUF, 256, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mbuf_pool == NULL)
      rte_exit(EXIT_FAILURE, "Unable to create\n");

    char pool_name[RTE_MEMPOOL_NAMESIZE];

    in_mbuf_pool = rte_pktmbuf_pool_create("in_pool_0", 16383, 0, 0, 22744, 0);
    out_mbuf_pool = rte_pktmbuf_pool_create("hard_out_pool_0", 16383, 0, 0, 22744, 0);

    if (in_mbuf_pool == nullptr or out_mbuf_pool == nullptr) {
      std::cerr << "Error: Unable to create mbuf pool: " << rte_strerror(rte_errno) << std::endl;
    }

    if (ops_mp == nullptr) {
      std::cerr << "Error: Failed to create memory pool for bbdev operations." << std::endl;
    } else {
      std::cout << "Memory pool for bbdev operations created successfully." << std::endl;
    }

    int rte_alloc_ref = rte_bbdev_dec_op_alloc_bulk(ops_mp, ref_dec_op, burst_sz);
    // std::cout<<"rte_alloc_ref is: " << rte_alloc_ref << std::endl;
  // std::cout<<"rte_alloc is: " << rte_alloc << std::endl;
    // std::cout<< "op alloc bulk is: " << (rte_bbdev_dec_op_alloc_bulk(bbdev_op_pool, ref_dec_op, burst_sz)) << std::endl;

    ret = rte_pktmbuf_alloc_bulk(mbuf_pool, input_pkts_burst, MAX_PKT_BURST);
    ret = rte_pktmbuf_alloc_bulk(mbuf_pool, output_pkts_burst, MAX_PKT_BURST);
    const struct rte_bbdev_op_cap *cap = info.drv.capabilities;
    const struct rte_bbdev_op_cap *capabilities = NULL;
    rte_bbdev_info_get(dev_id, &info);
    for (unsigned int i = 0; cap->type != RTE_BBDEV_OP_NONE; ++i, ++cap) {
      std::cout<<"cap is: " << cap->type  << std::endl;
      if (cap->type == RTE_BBDEV_OP_LDPC_DEC) {
        capabilities = cap;
        std::cout<<"capability is being set to: " << capabilities->type << std::endl;
        break;
      }
    }

    inputs = (struct rte_bbdev_op_data **)malloc(sizeof(struct rte_bbdev_op_data *));
    hard_outputs = (struct rte_bbdev_op_data **)malloc(sizeof(struct rte_bbdev_op_data *));

    int ret_socket_in = allocate_buffers_on_socket(inputs, 4 * sizeof(struct rte_bbdev_op_data), 0);
    int ret_socket_hard_out = allocate_buffers_on_socket(hard_outputs, 4 * sizeof(struct rte_bbdev_op_data), 0);

    ldpc_llr_decimals = capabilities->cap.ldpc_dec.llr_decimals;
    ldpc_llr_size = capabilities->cap.ldpc_dec.llr_size;
    ldpc_cap_flags = capabilities->cap.ldpc_dec.capability_flags;

    min_alignment = info.drv.min_alignment;

    // in_m_head = rte_pktmbuf_alloc(in_mbuf_pool);
    // out_m_head = rte_pktmbuf_alloc(out_mbuf_pool);

    std::cout<<"rte_pktmbuf_alloc successful" << std::endl;
}

DoDecode_ACC::~DoDecode_ACC() { 
    std::free(resp_var_nodes_); 
    // cleanup_bbdev_device("8086:0d5c"); // TODO: hardcoded
}

int DoDecode_ACC::allocate_buffers_on_socket(struct rte_bbdev_op_data **buffers, const int len, const int socket)
{
	int i;
  std::cout<<"start to allocate to socket"<<std::endl;
	*buffers = static_cast<struct rte_bbdev_op_data*>(rte_zmalloc_socket(NULL, len, 0, socket));
  std::cout<<"no error"<<std::endl;
	if (*buffers == NULL) {
		printf("WARNING: Failed to allocate op_data on socket %d\n",
				socket);
		/* try to allocate memory on other detected sockets */
		for (i = 0; i < socket; i++) {
			*buffers = static_cast<struct rte_bbdev_op_data*>(rte_zmalloc_socket(NULL, len, 0, i));
			if (*buffers != NULL)
				break;
		}
	}


	return (*buffers == NULL) ? TEST_FAILED : TEST_SUCCESS;
}


EventData DoDecode_ACC::Launch(size_t tag) {
  const LDPCconfig& ldpc_config = cfg_->LdpcConfig(Direction::kUplink);
  const size_t frame_id = gen_tag_t(tag).frame_id_;
  const size_t symbol_id = gen_tag_t(tag).symbol_id_;
  const size_t symbol_idx_ul = cfg_->Frame().GetULSymbolIdx(symbol_id);
  const size_t data_symbol_idx_ul = cfg_->Frame().GetULSymbolIdx(symbol_id) -
                                  cfg_->Frame().ClientUlPilotSymbols();
  const size_t cb_id = gen_tag_t(tag).cb_id_;
  const size_t symbol_offset =
      cfg_->GetTotalDataSymbolIdxUl(frame_id, symbol_idx_ul);
  const size_t cur_cb_id = (cb_id % ldpc_config.NumBlocksInSymbol());
  const size_t ue_id = (cb_id / ldpc_config.NumBlocksInSymbol());
  const size_t frame_slot = (frame_id % kFrameWnd);
  const size_t num_bytes_per_cb = cfg_->NumBytesPerCb(Direction::kUplink);
  if (kDebugPrintInTask == true) {
    std::printf(
        "In doDecode thread %d: frame: %zu, symbol: %zu, code block: "
        "%zu, ue: %zu offset %zu\n",
        tid_, frame_id, symbol_id, cur_cb_id, ue_id, symbol_offset);
  }

  size_t start_tsc = GetTime::WorkerRdtsc();
  for (int i = 0; i < burst_sz; ++i) {
    // ref_dec_op[i]->ldpc_dec.op_flags += RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE;
    ref_dec_op[i]->ldpc_dec.basegraph = (uint8_t) ldpc_config.BaseGraph();
    ref_dec_op[i]->ldpc_dec.z_c = (uint16_t) ldpc_config.ExpansionFactor();
    ref_dec_op[i]->ldpc_dec.n_filler =  (uint16_t) 0;

    // ref_dec_op[i]->status = 0; // Default value
    // ref_dec_op[i]->mempool = ops_mp; 
    // ref_dec_op[i]->opaque_data = nullptr; // U
    ref_dec_op[i]->ldpc_dec.rv_index = (uint8_t) 0;
    ref_dec_op[i]->ldpc_dec.n_cb = (uint16_t) ldpc_config.NumCbCodewLen();
    ref_dec_op[i]->ldpc_dec.q_m = (uint8_t) 4;
    ref_dec_op[i]->ldpc_dec.code_block_mode = (uint8_t) 1;
    ref_dec_op[i]->ldpc_dec.cb_params.e =(uint32_t) 44;
    if (!check_bit(ref_dec_op[i]->ldpc_dec.op_flags, RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE)){
      ref_dec_op[i]->ldpc_dec.op_flags += RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE;
    }
    // if (check_bit(ref_dec_op[i]->ldpc_dec.op_flags, RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE)){
    //   ref_dec_op[i]->ldpc_dec.op_flags -= RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE;
    // }
    ref_dec_op[i]->ldpc_dec.iter_max = (uint8_t) ldpc_config.MaxDecoderIter();
    // ref_dec_op[i]->ldpc_dec.iter_count = (uint8_t) ref_dec_op[i]->ldpc_dec.iter_max;
    // std::cout<<"iter count is: " << unsigned(ref_dec_op[i]->ldpc_dec.iter_count) << std::endl;

    // ref_dec_op[i]->ldpc_dec.op_flags = RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE;
    ref_dec_op[i]->opaque_data = (void *)(uintptr_t)i;
}


  int8_t* llr_buffer_ptr = demod_buffers_[frame_slot][data_symbol_idx_ul][ue_id] +
                           (cfg_->ModOrderBits(Direction::kUplink) *
                            (ldpc_config.NumCbCodewLen() * cur_cb_id));

  uint8_t* decoded_buffer_ptr =
      (uint8_t*)decoded_buffers_[frame_slot][data_symbol_idx_ul][ue_id] +
      (cur_cb_id * Roundup<64>(num_bytes_per_cb));

  // size_t start_tsc1 = GetTime::WorkerRdtsc();
  // duration_stat_->task_duration_[1] += start_tsc1 - start_tsc;

  int ret_init_op = init_op_data_objs_from_table(*inputs, llr_buffer_ptr, in_mbuf_pool, 1, min_alignment, ldpc_config.NumCbCodewLen());
  // std::cout<<"ret_init_op is " << ret_init_op << std::endl;
  ret_init_op = init_op_output_objs_from_buffer(*hard_outputs, decoded_buffer_ptr, out_mbuf_pool, 1, min_alignment, ldpc_config.NumCbCodewLen());
  // std::cout<<"ret_init_op is " << ret_init_op << std::endl;


  ref_dec_op[0]->ldpc_dec.input = *inputs[0];
  ref_dec_op[0]->ldpc_dec.hard_output = *hard_outputs[0];
  std::cout<<"no error when putting to ldpc_config" << std::endl;

  size_t start_tsc1 = GetTime::WorkerRdtsc();
  duration_stat_->task_duration_[1] += start_tsc1 - start_tsc;
  // size_t start_tsc2 = GetTime::WorkerRdtsc();
  // duration_stat_->task_duration_[2] += start_tsc2 - start_tsc1;

  uint16_t enq = 0, deq = 0;
  bool first_time = true;
  uint64_t start_time = 0, last_time = 0;

  enq += rte_bbdev_enqueue_ldpc_dec_ops(0, 0, &ref_dec_op[0], 1);
  // std::cout<<"afater enqueue"<<std::endl;
  // std::cout<<"enq is: " << enq << std::endl;

  deq += rte_bbdev_dequeue_ldpc_dec_ops(0, 0, &ops_deq[0], enq-deq);
  // std::cout<<"afater dequeue"<<std::endl;

  int max_retries = 1000000;
  int retry_count = 0;

  while (deq == 0 && retry_count < max_retries) {
      // rte_delay_ms(10);  // Wait for 10 milliseconds
      deq += rte_bbdev_dequeue_ldpc_dec_ops(0, 0, &ops_deq[0], enq-deq);
      retry_count++;
  }

  if (cfg_->ScrambleEnabled()) {
    scrambler_->Descramble(decoded_buffer_ptr, num_bytes_per_cb);
  }
  
  size_t start_tsc2 = GetTime::WorkerRdtsc();
  duration_stat_->task_duration_[2] += start_tsc2 - start_tsc1;

  if (kPrintLLRData) {
    std::printf("LLR data, symbol_offset: %zu\n", symbol_offset);
    for (size_t i = 0; i < ldpc_config.NumCbCodewLen(); i++) {
      std::printf("%d ", *(llr_buffer_ptr + i));
    }
    std::printf("\n");
  }

  if (kPrintDecodedData) {
    std::printf("Decoded data\n");
    for (size_t i = 0; i < (ldpc_config.NumCbLen() >> 3); i++) {
      std::printf("%u ", *(decoded_buffer_ptr + i));
    }
    std::printf("\n");
  }

  // if ((kEnableMac == false) && (kPrintPhyStats == true) &&
  //     (symbol_idx_ul >= cfg_->Frame().ClientUlPilotSymbols())) {
  //   phy_stats_->UpdateDecodedBits(ue_id, symbol_offset, frame_slot,
  //                                 num_bytes_per_cb * 8);
  //   phy_stats_->IncrementDecodedBlocks(ue_id, symbol_offset, frame_slot);
  //   size_t block_error(0);
  //   for (size_t i = 0; i < num_bytes_per_cb; i++) {
  //     uint8_t rx_byte = decoded_buffer_ptr[i];
  //     auto tx_byte = static_cast<uint8_t>(
  //         cfg_->GetInfoBits(cfg_->UlBits(), Direction::kUplink, symbol_idx_ul,
  //                           ue_id, cur_cb_id)[i]);
  //     phy_stats_->UpdateBitErrors(ue_id, symbol_offset, frame_slot, tx_byte,
  //                                 rx_byte);
  //     if (rx_byte != tx_byte) {
  //       block_error++;
  //     }
  //   }
  //   phy_stats_->UpdateBlockErrors(ue_id, symbol_offset, frame_slot,
  //                                 block_error);
  // }

  size_t end = GetTime::WorkerRdtsc();
  size_t duration_3 = end - start_tsc2;
  size_t duration = end - start_tsc;

  duration_stat_->task_duration_[3] += duration_3;
  duration_stat_->task_duration_[0] += duration;
  // duration_stat_->task_duration_[0] += 0;

  duration_stat_->task_count_++;
  if (GetTime::CyclesToUs(duration, cfg_->FreqGhz()) > 500) {
    std::printf("Thread %d Decode takes %.2f\n", tid_,
                GetTime::CyclesToUs(duration, cfg_->FreqGhz()));
  }

  // if (in_mbuf_pool != NULL) {
  //   rte_mempool_free(in_mbuf_pool);
  //   in_mbuf_pool = NULL;
  // }

  // if (out_mbuf_pool != NULL) {
  //   rte_mempool_free(out_mbuf_pool);
  //   out_mbuf_pool = NULL;
  // }

  // rte_bbdev_stop(dev_id);

  // rte_pktmbuf_free(m);

  return EventData(EventType::kDecode, tag);
}
