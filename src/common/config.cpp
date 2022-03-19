#include "config.hpp"
#include "utils_ldpc.hpp"
#include <boost/range/algorithm/count.hpp>

Config::Config(std::string jsonfile)
{
    std::string conf;
    Utils::loadTDDConfig(jsonfile, conf);
    const auto tddConf = json::parse(conf);

    /* antenna configurations */
    std::string hub_file = tddConf.value("hubs", "");
    std::string serial_file = tddConf.value("irises", "");
    ref_ant = tddConf.value("ref_ant", 0);
    nCells = tddConf.value("cells", 1);
    channel = tddConf.value("channel", "A");
    nChannels = std::min(channel.size(), (size_t)2);
    BS_ANT_NUM = tddConf.value("antenna_num", 8);
    isUE = tddConf.value("UE", false);
    UE_NUM = tddConf.value("ue_num", 8);
    UE_ANT_NUM = UE_NUM;
    if (hub_file.size() > 0)
        Utils::loadDevices(hub_file, hub_ids);
    if (serial_file.size() > 0)
        Utils::loadDevices(serial_file, radio_ids);
    if (radio_ids.size() != 0) {
        nRadios = radio_ids.size();
        nAntennas = nChannels * nRadios;
        if (isUE) {
            UE_ANT_NUM = nAntennas;
            UE_NUM = nRadios;
        } else {
            if (ref_ant >= nAntennas)
                ref_ant = 0;
            if (BS_ANT_NUM != nAntennas)
                BS_ANT_NUM = nAntennas;
        }
    } else
        nRadios = tddConf.value("radio_num", isUE ? UE_ANT_NUM : BS_ANT_NUM);

    if (kUseArgos) {
        rt_assert(nRadios != 0, "Error: No radios exist in Argos mode");
    }

    /* radio configurations */
    freq = tddConf.value("frequency", 3.6e9);
    txgainA = tddConf.value("txgainA", 20);
    rxgainA = tddConf.value("rxgainA", 20);
    txgainB = tddConf.value("txgainB", 20);
    rxgainB = tddConf.value("rxgainB", 20);
    calTxGainA = tddConf.value("calTxGainA", 10);
    calTxGainB = tddConf.value("calTxGainB", 10);
    rate = tddConf.value("rate", 5e6);
    nco = tddConf.value("nco_frequency", 0.75 * rate);
    bwFilter = rate + 2 * nco;
    radioRfFreq = freq - nco;
    beacon_ant = tddConf.value("beacon_antenna", 0);
    beamsweep = tddConf.value("beamsweep", false);
    sampleCalEn = tddConf.value("sample_calibrate", false);
    imbalanceCalEn = tddConf.value("imbalance_calibrate", false);
    modulation = tddConf.value("modulation", "16QAM");

    bs_server_addr = tddConf.value("bs_server_addr", "127.0.0.1");
    bs_rru_addr = tddConf.value("bs_rru_addr", "127.0.0.1");
    ue_server_addr = tddConf.value("ue_server_addr", "127.0.0.1");
    mac_remote_addr = tddConf.value("mac_remote_addr", "127.0.0.1");
    bs_server_port = tddConf.value("bs_server_port", 8000);
    bs_rru_port = tddConf.value("bs_rru_port", 9000);
    ue_rru_port = tddConf.value("ue_rru_port", 7000);
    ue_server_port = tddConf.value("ue_sever_port", 6000);

    mac_rx_port = tddConf.value("mac_rx_port", 5000);
    mac_tx_port = tddConf.value("mac_tx_port", 4000);
    init_mac_running = tddConf.value("init_mac_running", false);

    /* frame configurations */
    CP_LEN = tddConf.value("cp_len", 0);
    OFDM_CA_NUM = tddConf.value("ofdm_ca_num", 2048);
    OFDM_DATA_NUM = tddConf.value("ofdm_data_num", 1200);
    ofdm_tx_zero_prefix_ = tddConf.value("ofdm_tx_zero_prefix", 0);
    ofdm_tx_zero_postfix_ = tddConf.value("ofdm_tx_zero_postfix", 0);
    ofdm_rx_zero_prefix_bs_
        = tddConf.value("ofdm_rx_zero_prefix_bs", 0) + CP_LEN;
    ofdm_rx_zero_prefix_client_
        = tddConf.value("ofdm_rx_zero_prefix_client", 0);
    rt_assert(OFDM_DATA_NUM % kSCsPerCacheline == 0,
        "OFDM_DATA_NUM must be a multiple of subcarriers per cacheline");
    rt_assert(OFDM_DATA_NUM % kTransposeBlockSize == 0,
        "Transpose block size must divide number of OFDM data subcarriers");
    OFDM_PILOT_SPACING = tddConf.value("ofdm_pilot_spacing", 16);
    OFDM_DATA_START
        = tddConf.value("ofdm_data_start", (OFDM_CA_NUM - OFDM_DATA_NUM) / 2);
    OFDM_DATA_STOP = OFDM_DATA_START + OFDM_DATA_NUM;
    downlink_mode = tddConf.value("downlink_mode", false);
    freq_orthogonal_pilot = tddConf.value("freq_orthogonal_pilot", false);
    correct_phase_shift = tddConf.value("correct_phase_shift", false);
    DL_PILOT_SYMS = tddConf.value("client_dl_pilot_syms", 0);
    UL_PILOT_SYMS = tddConf.value("client_ul_pilot_syms", 0);
    cl_tx_advance = tddConf.value("tx_advance", 100);
    hw_framer = tddConf.value("hw_framer", true);
    if (tddConf.find("frames") == tddConf.end()) {
        symbol_num_perframe = tddConf.value("symbol_num_perframe", 70);
        pilot_symbol_num_perframe = tddConf.value(
            "pilot_num", freq_orthogonal_pilot ? 1 : UE_ANT_NUM);
        ul_data_symbol_num_perframe = tddConf.value("ul_symbol_num_perframe",
            downlink_mode
                ? 0
                : symbol_num_perframe - pilot_symbol_num_perframe - 1);
        dl_data_symbol_num_perframe
            = tddConf.value("dl_symbol_num_perframe", downlink_mode ? 10 : 0);
        dl_data_symbol_start = tddConf.value("dl_data_symbol_start", 10);
        std::string sched("B");
        for (size_t s = 0; s < pilot_symbol_num_perframe; s++)
            sched += "P";
        // Below it is assumed either dl or ul to be active at one time
        if (downlink_mode) {
            size_t dl_symbol_start
                = 1 + pilot_symbol_num_perframe + dl_data_symbol_start;
            size_t dl_symbol_end
                = dl_symbol_start + dl_data_symbol_num_perframe;
            for (size_t s = 1 + pilot_symbol_num_perframe; s < dl_symbol_start;
                 s++)
                sched += "G";
            for (size_t s = dl_symbol_start; s < dl_symbol_end; s++)
                sched += "D";
            for (size_t s = dl_symbol_end; s < symbol_num_perframe; s++)
                sched += "G";
        } else {
            size_t ul_data_symbol_end
                = 1 + pilot_symbol_num_perframe + ul_data_symbol_num_perframe;
            for (size_t s = 1 + pilot_symbol_num_perframe;
                 s < ul_data_symbol_end; s++)
                sched += "U";
            for (size_t s = ul_data_symbol_end; s < symbol_num_perframe; s++)
                sched += "G";
        }
        frames.push_back(sched);
        printf("Config: Frame schedule %s (%zu symbols)\n", sched.c_str(),
            sched.size());
    } else {
        json jframes = tddConf.value("frames", json::array());
        for (size_t f = 0; f < jframes.size(); f++) {
            frames.push_back(jframes.at(f).get<std::string>());
        }
    }

    beaconSymbols = Utils::loadSymbols(frames, 'B');
    pilotSymbols = Utils::loadSymbols(frames, 'P');
    ULSymbols = Utils::loadSymbols(frames, 'U');
    DLSymbols = Utils::loadSymbols(frames, 'D');
    ULCalSymbols = Utils::loadSymbols(frames, 'L');
    DLCalSymbols = Utils::loadSymbols(frames, 'C');
    recipCalEn = (ULCalSymbols[0].size() == 1 and DLCalSymbols[0].size() == 1);

    symbol_num_perframe = frames.at(0).size();
    beacon_symbol_num_perframe = beaconSymbols[0].size();
    pilot_symbol_num_perframe = pilotSymbols[0].size();
    data_symbol_num_perframe = symbol_num_perframe - pilot_symbol_num_perframe
        - beacon_symbol_num_perframe;
    ul_data_symbol_num_perframe = ULSymbols[0].size();
    dl_data_symbol_num_perframe = DLSymbols[0].size();
    downlink_mode = dl_data_symbol_num_perframe > 0;
    dl_data_symbol_start
        = dl_data_symbol_num_perframe > 0 ? DLSymbols[0].front() : 0;
    dl_data_symbol_end
        = dl_data_symbol_num_perframe > 0 ? DLSymbols[0].back() + 1 : 0;

    if (isUE and !freq_orthogonal_pilot
        and UE_ANT_NUM != pilot_symbol_num_perframe) {
        rt_assert(false, "Number of pilot symbols doesn't match number of UEs");
    }
    if (!isUE and !freq_orthogonal_pilot
        and tddConf.find("ue_num") == tddConf.end()) {
        UE_NUM = pilot_symbol_num_perframe;
        UE_ANT_NUM = UE_NUM;
    }
    ue_ant_offset = tddConf.value("ue_ant_offset", 0);
    total_ue_ant_num = tddConf.value("total_ue_ant_num", UE_ANT_NUM);

    /* Agora configurations */
    frames_to_test = tddConf.value("frames_to_test", 9600);
    core_offset = tddConf.value("core_offset", 0);
    worker_thread_num = tddConf.value("worker_thread_num", 25);
    rx_thread_num = tddConf.value("rx_thread_num", 4);
    tx_thread_num = tddConf.value("tx_thread_num", 4);

    demul_block_size = tddConf.value("demul_block_size", 48);
    rt_assert(demul_block_size % kSCsPerCacheline == 0,
        "Demodulation block size must be a multiple of subcarriers per "
        "cacheline");
    // rt_assert(demul_block_size % kTransposeBlockSize == 0,
    //     "Demodulation block size must be a multiple of transpose block size");
    rt_assert(demul_block_size > 0,
        "Demodulation block size must be greater than 0!");

    zf_block_size = freq_orthogonal_pilot ? UE_ANT_NUM
                                          : tddConf.value("zf_block_size", 1);
    rt_assert(zf_block_size % demul_block_size == 0,
        "ZF block size must be a multiple of demul block size!");

    bs_rru_addr_list = tddConf.value("bs_rru_addr_list", std::vector<std::string>());
    rt_assert(bs_rru_addr_list.size() > 0, "RRU address list is 0!");
    bs_rru_addr_idx = tddConf.value("bs_rru_addr_idx", 0);
    rt_assert(bs_rru_addr_idx >= 0 && bs_rru_addr_idx < bs_rru_addr_list.size(), 
        "The rru address index must be within the list size!");
    bs_rru_mac_list = tddConf.value("bs_rru_mac_list", std::vector<std::string>());
    rt_assert(bs_rru_mac_list.size() == bs_rru_addr_list.size(), "RRU addr and mac list should have same size!");

    // rt_assert(pilot_symbol_num_perframe + ul_data_symbol_num_perframe
    //         == symbol_num_perframe,
    //     "Masterless mode supports only pilot and uplink data syms for now");

    bs_server_addr_list
        = tddConf.value("bs_server_addr_list", std::vector<std::string>());
    rt_assert(bs_server_addr_list.size() > 0, "Address list is 0!");
    bs_server_addr_idx = tddConf.value("bs_server_addr_idx", 0);
    // rt_assert(OFDM_DATA_NUM % bs_server_addr_list.size() == 0,
    //     "OFDM_DATA_NUM % # servers should be 0!");
    if (kUseDPDK) {
        bs_server_mac_list
            = tddConf.value("bs_server_mac_list", std::vector<std::string>());
        rt_assert(bs_server_mac_list.size() > 0, "MAC list is 0!");
        rt_assert(bs_server_mac_list.size() == bs_server_addr_list.size(), "Two list not equal!");
        bs_rru_mac_addr
            = tddConf.value("bs_rru_mac_addr", "");
    }

    /* Distributed & normal mode options */
    std::vector<size_t> subcarrier_vec = tddConf.value(
        "subcarrier_block_list", std::vector<size_t>());
    rt_assert(subcarrier_vec.size() == bs_server_addr_list.size(),
        "Subcarrier block list must be the same length with server list!");
    // subcarrier_block_size = tddConf.value(
    //     "subcarrier_block_size", zf_block_size);
    subcarrier_block_size = subcarrier_vec[bs_server_addr_idx];
    // rt_assert(subcarrier_block_size % zf_block_size == 0,
    //     "Subcarrier block size should be a multiple of zf_block_size)!");
    // rt_assert(subcarrier_block_size % kSCsPerCacheline == 0,
    //     "Subcarrier block size should be a multiple of cacheline size)!");
    rt_assert(demul_block_size <= subcarrier_block_size,
        "Demodulation block size must no larger than subcarrier block size!");

    decode_thread_num_per_ue = tddConf.value("decode_thread_num_per_ue", 1);

    subcarrier_num_list = tddConf.value("subcarrier_num_list", std::vector<size_t>());
    rt_assert(bs_server_addr_list.size() == subcarrier_num_list.size(), "Subcarrier num list has a different size!");
    size_t sum_subcarriers = 0;
    for (const size_t subc : subcarrier_num_list) {
        subcarrier_num_start.push_back(sum_subcarriers);
        sum_subcarriers += subc;
    }
    rt_assert(sum_subcarriers == OFDM_DATA_NUM, "Subcarrier sum is different from OFDM DATA NUM!");
    sum_subcarriers = 0;
    for (size_t i = 0; i < bs_server_addr_idx; i ++) {
        sum_subcarriers += subcarrier_num_list[i];
    }

    subcarrier_start = sum_subcarriers;
    subcarrier_end = subcarrier_start + subcarrier_num_list[bs_server_addr_idx];
    ue_start = bs_server_addr_idx < UE_NUM % bs_server_addr_list.size()
        ? bs_server_addr_idx * (UE_NUM / bs_server_addr_list.size() + 1)
        : UE_NUM
            - (bs_server_addr_list.size() - bs_server_addr_idx)
                * (UE_NUM / bs_server_addr_list.size());
    ue_end = bs_server_addr_idx < UE_NUM % bs_server_addr_list.size()
        ? ue_start + UE_NUM / bs_server_addr_list.size() + 1
        : ue_start + UE_NUM / bs_server_addr_list.size();

    demul_events_per_symbol
        = 1 + (get_num_sc_to_process() - 1) / demul_block_size;
    zf_events_per_symbol = 1 + (get_num_sc_to_process() - 1) / zf_block_size;

    demod_tx_port = tddConf.value("demod_tx_port", 8100);
    demod_rx_port = tddConf.value("demod_rx_port", 8600);

    encode_tx_port = tddConf.value("encode_tx_port", 7100);
    encode_rx_port = tddConf.value("encode_rx_port", 7600);

    fft_block_size = tddConf.value("fft_block_size", 1);

    /* LDPC Coding configurations */
    LDPC_config.Bg = tddConf.value("base_graph", 1);
    LDPC_config.earlyTermination = tddConf.value("earlyTermination", 1);
    LDPC_config.decoderIter = tddConf.value("decoderIter", 5);
    LDPC_config.Zc = tddConf.value("Zc", 72);
    LDPC_config.nRows = tddConf.value("nRows", (LDPC_config.Bg == 1) ? 46 : 42);
    LDPC_config.cbLen = ldpc_num_input_bits(LDPC_config.Bg, LDPC_config.Zc);
    LDPC_config.cbCodewLen = ldpc_num_encoded_bits(
        LDPC_config.Bg, LDPC_config.Zc, LDPC_config.nRows);

    /* Modulation configurations */
    mod_order_bits = modulation == "64QAM"
        ? CommsLib::QAM64
        : (modulation == "16QAM" ? CommsLib::QAM16 : CommsLib::QPSK);
    update_mod_cfgs(mod_order_bits);

    printf("Config: LDPC: Zc: %d, %d information "
           "bits per encoding, %d bits per encoded code word, decoder "
           "iterations: %d, code rate %.3f (nRows = %zu)\n",
        LDPC_config.Zc, LDPC_config.cbLen,
        LDPC_config.cbCodewLen, LDPC_config.decoderIter,
        1.f * ldpc_num_input_cols(LDPC_config.Bg)
            / (ldpc_num_input_cols(LDPC_config.Bg) - 2 + LDPC_config.nRows),
        LDPC_config.nRows);

    pci_addr = tddConf.value("pci_addr", "37:00.0");
    
    fixed_control = tddConf.value("fixed_control", -1);
    user_level_list = tddConf.value("user_level_list", std::vector<size_t>(UE_NUM));
    num_load_levels = tddConf.value("num_load_levels", 10);
    sleep_mode = tddConf.value("sleep_mode", true);

    if (downlink_mode) {
        std::vector<size_t> tmp_vec = tddConf.value("coding_thread_num", std::vector<size_t>());
        if (tmp_vec.size() <= bs_server_addr_idx) {
            encode_thread_num = ue_end - ue_start;
        } else {
            encode_thread_num = tmp_vec[bs_server_addr_idx];
        }
    } else {
        std::vector<size_t> tmp_vec = tddConf.value("coding_thread_num", std::vector<size_t>());
        if (tmp_vec.size() <= bs_server_addr_idx) {
            decode_thread_num = ue_end - ue_start;
        } else {
            decode_thread_num = tmp_vec[bs_server_addr_idx];
        }
    }

    use_time_domain_iq = tddConf.value("use_time_domain_iq", false);
    if (use_time_domain_iq) {
        std::vector<size_t> tmp_vec = tddConf.value("fft_thread_num", std::vector<size_t>());
        rt_assert(tmp_vec.size() == bs_server_addr_list.size());
        fft_thread_num = tmp_vec[bs_server_addr_idx];
        fft_tx_thread_num = tddConf.value("fft_tx_thread_num", 0);
        ant_start = bs_server_addr_idx < BS_ANT_NUM % bs_server_addr_list.size()
            ? bs_server_addr_idx * (BS_ANT_NUM / bs_server_addr_list.size() + 1)
            : BS_ANT_NUM - (bs_server_addr_list.size() - bs_server_addr_idx)
            * (BS_ANT_NUM / bs_server_addr_list.size());
        ant_end = bs_server_addr_idx < BS_ANT_NUM % bs_server_addr_list.size()
            ? ant_start + BS_ANT_NUM / bs_server_addr_list.size() + 1
            : ant_start + BS_ANT_NUM / bs_server_addr_list.size();
        fft_tx_port = tddConf.value("fft_tx_port", 9100);
        fft_rx_port = tddConf.value("fft_rx_port", 9600);
    }

    slot_us = tddConf.value("slot_us", 1000);
    use_hyperthreading = tddConf.value("use_hyperthreading", false);
    phy_core_num = tddConf.value("phy_core_num", 10);

    use_central_scheduler = tddConf.value("use_central_scheduler", false);
    use_general_worker = tddConf.value("use_general_worker", false);
    use_bigstation_mode = tddConf.value("use_bigstation_mode", false);

    if (use_bigstation_mode) {
        rt_assert(!downlink_mode, "Bigstation mode is not supported for downlink");
        num_fft_workers = tddConf.value("num_fft_workers", std::vector<size_t>());
        num_zf_workers = tddConf.value("num_zf_workers", std::vector<size_t>());
        num_demul_workers = tddConf.value("num_demul_workers", std::vector<size_t>());
        num_decode_workers = tddConf.value("num_decode_workers", std::vector<size_t>());
        
        rt_assert(num_fft_workers.size() == bs_server_addr_list.size(),
            "num_fft_workers size is not equal to bs_server_addr_list size");
        size_t next_bar = num_fft_workers[0];
        size_t cur_server_id = 0;
        size_t total_thread_num = 0;
        ant_server_mapping.resize(BS_ANT_NUM);
        for (size_t i = 0; i < num_fft_workers.size(); i++) {
            total_thread_num += num_fft_workers[i];
        }
        for (size_t i = 0; i < BS_ANT_NUM; i ++) {
            double tid = i * 1.0 * total_thread_num / BS_ANT_NUM;
            if (tid < next_bar) {
                ant_server_mapping[i] = cur_server_id;
            } else {
                cur_server_id ++;
                next_bar += num_fft_workers[cur_server_id];
                ant_server_mapping[i] = cur_server_id;
            }
        }
        ant_start = -1;
        ant_end = 0;
        for (size_t i = 0; i < BS_ANT_NUM; i ++) {
            if (ant_server_mapping[i] == bs_server_addr_idx) {
                ant_start = std::min(ant_start, i);
                ant_end = std::max(ant_end, i);
            }
        }
        ant_end ++;
        fft_thread_offset = 0;
        for (size_t i = 0; i < bs_server_addr_idx; i ++) {
            fft_thread_offset += num_fft_workers[i];
        }
        total_fft_workers = 0;
        for (size_t i = 0; i < num_fft_workers.size(); i ++) {
            total_fft_workers += num_fft_workers[i];
        }

        rt_assert(num_zf_workers.size() == bs_server_addr_list.size(),
            "num_zf_workers size is not equal to bs_server_addr_list size");
        next_bar = num_zf_workers[0];
        cur_server_id = 0;
        total_thread_num = 0;
        zf_server_mapping.resize(OFDM_DATA_NUM);
        for (size_t i = 0; i < num_zf_workers.size(); i++) {
            total_thread_num += num_zf_workers[i];
        }
        for (size_t i = 0; i < OFDM_DATA_NUM; i ++) {
            double tid = i * 1.0 * total_thread_num / OFDM_DATA_NUM;
            if (tid < next_bar) {
                zf_server_mapping[i] = cur_server_id;
            } else {
                cur_server_id ++;
                next_bar += num_zf_workers[cur_server_id];
                zf_server_mapping[i] = cur_server_id;
            }
        }
        zf_start = -1;
        zf_end = 0;
        for (size_t i = 0; i < OFDM_DATA_NUM; i ++) {
            if (zf_server_mapping[i] == bs_server_addr_idx) {
                zf_start = std::min(zf_start, i);
                zf_end = std::max(zf_end, i);
            }
        }
        zf_end ++;
        zf_thread_offset = 0;
        for (size_t i = 0; i < bs_server_addr_idx; i ++) {
            zf_thread_offset += num_zf_workers[i];
        }
        total_zf_workers = 0;
        for (size_t i = 0; i < num_zf_workers.size(); i ++) {
            total_zf_workers += num_zf_workers[i];
        }

        rt_assert(num_demul_workers.size() == bs_server_addr_list.size(),
            "num_demul_workers size is not equal to bs_server_addr_list size");
        next_bar = num_demul_workers[0];
        cur_server_id = 0;
        total_thread_num = 0;
        demul_server_mapping.resize(OFDM_DATA_NUM);
        for (size_t i = 0; i < num_demul_workers.size(); i++) {
            total_thread_num += num_demul_workers[i];
        }
        for (size_t i = 0; i < OFDM_DATA_NUM; i ++) {
            double tid = i * 1.0 * total_thread_num / OFDM_DATA_NUM;
            if (tid < next_bar) {
                demul_server_mapping[i] = cur_server_id;
            } else {
                cur_server_id ++;
                next_bar += num_demul_workers[cur_server_id];
                demul_server_mapping[i] = cur_server_id;
            }
        }
        demul_start = -1;
        demul_end = 0;
        for (size_t i = 0; i < OFDM_DATA_NUM; i ++) {
            if (demul_server_mapping[i] == bs_server_addr_idx) {
                demul_start = std::min(demul_start, i);
                demul_end = std::max(demul_end, i);
            }
        }
        demul_end ++;
        demul_thread_offset = 0;
        for (size_t i = 0; i < bs_server_addr_idx; i ++) {
            demul_thread_offset += num_demul_workers[i];
        }
        total_demul_workers = 0;
        for (size_t i = 0; i < num_demul_workers.size(); i ++) {
            total_demul_workers += num_demul_workers[i];
        }

        rt_assert(num_decode_workers.size() == bs_server_addr_list.size(),
            "num_decode_workers size is not equal to bs_server_addr_list size");
        next_bar = num_decode_workers[0];
        cur_server_id = 0;
        total_thread_num = 0;
        ue_server_mapping.resize(UE_NUM);
        for (size_t i = 0; i < num_decode_workers.size(); i++) {
            total_thread_num += num_decode_workers[i];
        }
        for (size_t i = 0; i < UE_NUM; i ++) {
            double tid = i * 1.0 * total_thread_num / UE_NUM;
            if (tid < next_bar) {
                ue_server_mapping[i] = cur_server_id;
            } else {
                cur_server_id ++;
                next_bar += num_decode_workers[cur_server_id];
                ue_server_mapping[i] = cur_server_id;
            }
        }
        ue_start = -1;
        ue_end = 0;
        for (size_t i = 0; i < UE_NUM; i ++) {
            if (ue_server_mapping[i] == bs_server_addr_idx) {
                ue_start = std::min(ue_start, i);
                ue_end = std::max(ue_end, i);
            }
        }
        ue_end ++;
        decode_thread_offset = 0;
        for (size_t i = 0; i < bs_server_addr_idx; i ++) {
            decode_thread_offset += num_decode_workers[i];
        }
        total_decode_workers = 0;
        for (size_t i = 0; i < num_decode_workers.size(); i ++) {
            total_decode_workers += num_decode_workers[i];
        }

        time_iq_sc_step_size = tddConf.value("time_iq_sc_step_size", 200);
        post_zf_step_size = tddConf.value("post_zf_step_size", 1200);
    }

    sampsPerSymbol
        = ofdm_tx_zero_prefix_ + OFDM_CA_NUM + CP_LEN + ofdm_tx_zero_postfix_;
    packet_length
        = Packet::kOffsetOfData + (2 * sizeof(short) * sampsPerSymbol);
    rt_assert(
        packet_length < 9000, "Packet size must be smaller than jumbo frame");

    num_bytes_per_cb = LDPC_config.cbLen / 8; // TODO: Use bits_to_bytes()?
    data_bytes_num_persymbol = num_bytes_per_cb;
    mac_packet_length = data_bytes_num_persymbol;
    mac_payload_length = mac_packet_length - MacPacket::kOffsetOfData;
    mac_packets_perframe = ul_data_symbol_num_perframe - UL_PILOT_SYMS;
    mac_data_bytes_num_perframe = mac_payload_length * mac_packets_perframe;
    mac_bytes_num_perframe = mac_packet_length * mac_packets_perframe;

    running = true;
    error = false;
    printf("Config: %zu BS antennas, %zu UE antennas, %zu pilot symbols per "
           "frame,\n\t"
           "%zu uplink data symbols per frame, %zu downlink data "
           "symbols per frame,\n\t"
           "%zu OFDM subcarriers (%zu data subcarriers), modulation %s,\n\t"
           "%zu MAC data bytes per frame, %zu MAC bytes per frame\n",
        BS_ANT_NUM, UE_ANT_NUM, pilot_symbol_num_perframe,
        ul_data_symbol_num_perframe, dl_data_symbol_num_perframe, OFDM_CA_NUM,
        OFDM_DATA_NUM, modulation.c_str(), mac_data_bytes_num_perframe,
        mac_bytes_num_perframe);
}

void Config::genData()
{
    if (kUseArgos) {
        std::vector<std::vector<double>> gold_ifft
            = CommsLib::getSequence(128, CommsLib::GOLD_IFFT);
        std::vector<std::complex<int16_t>> gold_ifft_ci16
            = Utils::double_to_cint16(gold_ifft);
        for (size_t i = 0; i < 128; i++) {
            gold_cf32.push_back(
                std::complex<float>(gold_ifft[0][i], gold_ifft[1][i]));
        }

        std::vector<std::vector<double>> sts_seq
            = CommsLib::getSequence(0, CommsLib::STS_SEQ);
        std::vector<std::complex<int16_t>> sts_seq_ci16
            = Utils::double_to_cint16(sts_seq);

        // Populate STS (stsReps repetitions)
        int stsReps = 15;
        for (int i = 0; i < stsReps; i++) {
            beacon_ci16.insert(
                beacon_ci16.end(), sts_seq_ci16.begin(), sts_seq_ci16.end());
        }

        // Populate gold sequence (two reps, 128 each)
        int goldReps = 2;
        for (int i = 0; i < goldReps; i++) {
            beacon_ci16.insert(beacon_ci16.end(), gold_ifft_ci16.begin(),
                gold_ifft_ci16.end());
        }

        beacon_len = beacon_ci16.size();

        if (sampsPerSymbol
            < beacon_len + ofdm_tx_zero_prefix_ + ofdm_tx_zero_postfix_) {
            std::string msg = "Minimum supported symbol_size is ";
            msg += std::to_string(beacon_len);
            throw std::invalid_argument(msg);
        }

        beacon = Utils::cint16_to_uint32(beacon_ci16, false, "QI");
        coeffs = Utils::cint16_to_uint32(gold_ifft_ci16, true, "QI");
    }

    // Generate common pilots based on Zadoff-Chu sequence for channel estimation
    auto zc_seq_double
        = CommsLib::getSequence(OFDM_DATA_NUM, CommsLib::LTE_ZADOFF_CHU);
    auto zc_seq = Utils::double_to_cfloat(zc_seq_double);
    auto common_pilot
        = CommsLib::seqCyclicShift(zc_seq, M_PI / 4); // Used in LTE SRS

    pilots_ = (complex_float*)aligned_alloc(
        64, OFDM_DATA_NUM * sizeof(complex_float));
    pilots_sgn_ = (complex_float*)aligned_alloc(
        64, OFDM_DATA_NUM * sizeof(complex_float)); // used in CSI estimation
    for (size_t i = 0; i < OFDM_DATA_NUM; i++) {
        pilots_[i] = { common_pilot[i].real(), common_pilot[i].imag() };
        auto pilot_sgn
            = common_pilot[i] / (float)std::pow(std::abs(common_pilot[i]), 2);
        pilots_sgn_[i] = { pilot_sgn.real(), pilot_sgn.imag() };
    }
    complex_float* pilot_ifft;
    alloc_buffer_1d(&pilot_ifft, OFDM_CA_NUM, 64, 1);
    for (size_t j = 0; j < OFDM_DATA_NUM; j++)
        pilot_ifft[j + OFDM_DATA_START] = pilots_[j];
    CommsLib::IFFT(pilot_ifft, OFDM_CA_NUM, false);

    // Generate UE-specific pilots based on Zadoff-Chu sequence for phase tracking
    ue_specific_pilot.malloc(UE_ANT_NUM, OFDM_DATA_NUM, 64);
    ue_specific_pilot_t.calloc(UE_ANT_NUM, sampsPerSymbol, 64);
    Table<complex_float> ue_pilot_ifft;
    ue_pilot_ifft.calloc(UE_ANT_NUM, OFDM_CA_NUM, 64);
    auto zc_ue_pilot_double
        = CommsLib::getSequence(OFDM_DATA_NUM, CommsLib::LTE_ZADOFF_CHU);
    auto zc_ue_pilot = Utils::double_to_cfloat(zc_ue_pilot_double);
    for (size_t i = 0; i < UE_ANT_NUM; i++) {
        auto zc_ue_pilot_i = CommsLib::seqCyclicShift(
            zc_ue_pilot, (i + ue_ant_offset) * (float)M_PI / 6); // LTE DMRS
        for (size_t j = 0; j < OFDM_DATA_NUM; j++) {
            ue_specific_pilot[i][j]
                = { zc_ue_pilot_i[j].real(), zc_ue_pilot_i[j].imag() };
            ue_pilot_ifft[i][j + OFDM_DATA_START] = ue_specific_pilot[i][j];
        }
        CommsLib::IFFT(ue_pilot_ifft[i], OFDM_CA_NUM, false);
    }

    // Get uplink and downlink raw bits either from file or random numbers
    size_t num_bytes_per_ue = num_bytes_per_cb;
    _unused(num_bytes_per_ue);
    size_t num_bytes_per_ue_pad
        = roundup<64>(num_bytes_per_cb);
    dl_bits.malloc(
        dl_data_symbol_num_perframe, num_bytes_per_ue_pad * UE_ANT_NUM, 64);
    dl_iq_f.calloc(dl_data_symbol_num_perframe, OFDM_CA_NUM * UE_ANT_NUM, 64);
    dl_iq_t.calloc(
        dl_data_symbol_num_perframe, sampsPerSymbol * UE_ANT_NUM, 64);

    ul_bits.malloc(
        ul_data_symbol_num_perframe, num_bytes_per_ue_pad * UE_ANT_NUM, 64);
    ul_iq_f.calloc(ul_data_symbol_num_perframe, OFDM_CA_NUM * UE_ANT_NUM, 64);
    ul_iq_t.calloc(
        ul_data_symbol_num_perframe, sampsPerSymbol * UE_ANT_NUM, 64);

/*
#ifdef GENERATE_DATA
    for (size_t ue_id = 0; ue_id < UE_ANT_NUM; ue_id++) {
        for (size_t j = 0; j < num_bytes_per_ue_pad; j++) {
            int cur_offset = j * UE_ANT_NUM + ue_id;
            for (size_t i = 0; i < ul_data_symbol_num_perframe; i++)
                ul_bits[i][cur_offset] = rand() % mod_order;
            for (size_t i = 0; i < dl_data_symbol_num_perframe; i++)
                dl_bits[i][cur_offset] = rand() % mod_order;
        }
    }
#else
    std::string cur_directory1 = TOSTRING(PROJECT_DIRECTORY);
    std::string filename1 = cur_directory1 + "/data/LDPC_orig_data_"
        + std::to_string(OFDM_CA_NUM) + "_ant"
        + std::to_string(total_ue_ant_num) + ".bin";
    std::cout << "Config: Reading raw data from " << filename1 << std::endl;
    FILE* fd = fopen(filename1.c_str(), "rb");
    if (fd == nullptr) {
        printf("Failed to open antenna file %s. Error %s.\n", filename1.c_str(),
            strerror(errno));
        exit(-1);
    }
    for (size_t i = 0; i < ul_data_symbol_num_perframe; i++) {
        if (fseek(fd, num_bytes_per_ue * ue_ant_offset, SEEK_SET) != 0)
            return;
        for (size_t j = 0; j < UE_ANT_NUM; j++) {
            size_t r = fread(ul_bits[i] + j * num_bytes_per_ue_pad,
                sizeof(int8_t), num_bytes_per_ue, fd);
            if (r < num_bytes_per_ue)
                printf("bad read from file %s (batch %zu) \n",
                    filename1.c_str(), i);
        }
        if (fseek(fd,
                num_bytes_per_ue
                    * (total_ue_ant_num - ue_ant_offset - UE_ANT_NUM),
                SEEK_SET)
            != 0)
            return;
    }
    for (size_t i = 0; i < dl_data_symbol_num_perframe; i++) {
        for (size_t j = 0; j < UE_ANT_NUM; j++) {
            size_t r = fread(dl_bits[i] + j * num_bytes_per_ue_pad,
                sizeof(int8_t), num_bytes_per_ue, fd);
            if (r < num_bytes_per_ue)
                printf("bad read from file %s (batch %zu) \n",
                    filename1.c_str(), i);
        }
    }
    fclose(fd);
#endif
*/
}

Config::~Config()
{
    free_buffer_1d(&pilots_);
    free_buffer_1d(&pilots_sgn_);
    mod_table.free();
}

int Config::getSymbolId(size_t symbol_id)
{
    return (symbol_id < pilot_symbol_num_perframe
            ? pilotSymbols[0][symbol_id]
            : ULSymbols[0][symbol_id - pilot_symbol_num_perframe]);
}

size_t Config::get_dl_symbol_idx(size_t frame_id, size_t symbol_id) const
{
    size_t fid = frame_id % frames.size();
    const auto it
        = find(DLSymbols[fid].begin(), DLSymbols[fid].end(), symbol_id);
    if (it != DLSymbols[fid].end())
        return it - DLSymbols[fid].begin();
    else
        return SIZE_MAX;
}

size_t Config::get_pilot_symbol_idx(size_t frame_id, size_t symbol_id) const
{
    size_t fid = frame_id % frames.size();
    const auto it
        = find(pilotSymbols[fid].begin(), pilotSymbols[fid].end(), symbol_id);
    if (it != pilotSymbols[fid].end()) {
#ifdef DEBUG3
        printf("get_pilot_symbol_idx(%zu, %zu) = %zu\n", frame_id, symbol_id,
            it - pilotSymbols[fid].begin());
#endif
        return it - pilotSymbols[fid].begin();
    } else
        return SIZE_MAX;
}

size_t Config::get_ul_symbol_idx(size_t frame_id, size_t symbol_id) const
{
    size_t fid = frame_id % frames.size();
    const auto it
        = find(ULSymbols[fid].begin(), ULSymbols[fid].end(), symbol_id);
    if (it != ULSymbols[fid].end()) {
#ifdef DEBUG3
        printf("get_ul_symbol_idx(%zu, %zu) = %zu\n", frame_id, symbol_id,
            it - ULSymbols[fid].begin());
#endif
        return it - ULSymbols[fid].begin();
    } else
        return SIZE_MAX;
}

bool Config::isPilot(size_t frame_id, size_t symbol_id)
{
    assert(symbol_id < symbol_num_perframe);
    size_t fid = frame_id % frames.size();
    char s = frames[fid].at(symbol_id);
#ifdef DEBUG3
    printf("isPilot(%zu, %zu) = %c\n", frame_id, symbol_id, s);
#endif
    if (isUE) {
        std::vector<size_t>::iterator it;
        it = find(DLSymbols[fid].begin(), DLSymbols[fid].end(), symbol_id);
        int ind = DL_PILOT_SYMS;
        if (it != DLSymbols[fid].end())
            ind = it - DLSymbols[fid].begin();
        return (ind < (int)DL_PILOT_SYMS);
        // return cfg->frames[fid].at(symbol_id) == 'P' ? true : false;
    } else
        return s == 'P';
    // return (symbol_id < UE_NUM);
}

bool Config::isCalDlPilot(size_t frame_id, size_t symbol_id)
{
    assert(symbol_id < symbol_num_perframe);
    if (isUE)
        return false;
    return frames[frame_id % frames.size()].at(symbol_id) == 'C';
}

bool Config::isCalUlPilot(size_t frame_id, size_t symbol_id)
{
    assert(symbol_id < symbol_num_perframe);
    if (isUE)
        return false;
    return frames[frame_id % frames.size()].at(symbol_id) == 'L';
}

bool Config::isUplink(size_t frame_id, size_t symbol_id)
{
    assert(symbol_id < symbol_num_perframe);
    char s = frames[frame_id % frames.size()].at(symbol_id);
#ifdef DEBUG3
    printf("isUplink(%zu, %zu) = %c\n", frame_id, symbol_id, s);
#endif
    return s == 'U';
}

bool Config::isDownlink(size_t frame_id, size_t symbol_id)
{
    assert(symbol_id < symbol_num_perframe);
    char s = frames[frame_id % frames.size()].at(symbol_id);
#ifdef DEBUG3
    printf("isDownlink(%zu, %zu) = %c\n", frame_id, symbol_id, s);
#endif
    if (isUE)
        return s == 'D' && !this->isPilot(frame_id, symbol_id);
    else
        return s == 'D';
}

SymbolType Config::get_symbol_type(size_t frame_id, size_t symbol_id)
{
    assert(!isUE); // Currently implemented for only the Agora server
    char s = frames[frame_id % frames.size()][symbol_id];
    switch (s) {
    case 'B':
        return SymbolType::kBeacon;
    case 'D':
        return SymbolType::kDL;
    case 'U':
        return SymbolType::kUL;
    case 'P':
        return SymbolType::kPilot;
    case 'C':
        return SymbolType::kCalDL;
    case 'L':
        return SymbolType::kCalUL;
    }
    rt_assert(false, std::string("Should not reach here") + std::to_string(s));
    return SymbolType::kUnknown;
}

extern "C" {
__attribute__((visibility("default"))) Config* Config_new(char* filename)
{

    auto* cfg = new Config(filename);
    cfg->genData();
    return cfg;
}
}
