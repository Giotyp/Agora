
#include "config.hpp"
#include <boost/range/algorithm/count.hpp>

Config::Config(std::string jsonfile)
{
    std::string conf;
    Utils::loadTDDConfig(jsonfile, conf);
    const auto tddConf = json::parse(conf);
    hub_file = tddConf.value("hubs", "");
    Utils::loadDevices(hub_file, hub_ids);
    serial_file = tddConf.value("irises", "");
    ref_ant = tddConf.value("ref_ant", 0);
    nCells = tddConf.value("cells", 1);
    channel = tddConf.value("channel", "A");
    nChannels = std::min(channel.size(), (size_t)2); 
    isUE = tddConf.value("UE", false);
    freq = tddConf.value("frequency", 3.6e9);
    txgainA = tddConf.value("txgainA", 20);
    rxgainA = tddConf.value("rxgainA", 20);
    txgainB = tddConf.value("txgainB", 20);
    rxgainB = tddConf.value("rxgainB", 20);
    calTxGainA = tddConf.value("calTxGainA", 10);
    calTxGainB = tddConf.value("calTxGainB", 10);
    rate = tddConf.value("rate", 5e6);
    transpose_block_size = tddConf.value("transpose_block_size", 16);
    nco = tddConf.value("nco_frequency", 0.75 * rate);
    bwFilter = rate + 2 * nco;
    radioRfFreq = freq - nco;
    auto symbolSize = tddConf.value("symbol_size", 1);
    prefix = tddConf.value("prefix", 0);
    dl_prefix = tddConf.value("dl_prefix", 0);
    postfix = tddConf.value("postfix", 0);
    beacon_ant = tddConf.value("beacon_antenna", 0);
    beacon_len = tddConf.value("beacon_len", 256);
    beamsweep = tddConf.value("beamsweep", false);
    sampleCalEn = tddConf.value("sample_calibrate", false);
    imbalanceCalEn = tddConf.value("imbalance_calibrate", false);
    modulation = tddConf.value("modulation", "16QAM");
    printf("modulation: %s\n", modulation.c_str());
    exec_mode = tddConf.value("exec_mode", "hw");
    TX_PREFIX_LEN = tddConf.value("tx_prefix_len", 0);
    CP_LEN = tddConf.value("cp_len", 0);
    OFDM_PREFIX_LEN = tddConf.value("ofdm_prefix_len", 0 + CP_LEN);
    BS_ANT_NUM = tddConf.value("antenna_num", 8);
    OFDM_CA_NUM = tddConf.value("ofdm_ca_num", 2048);
    OFDM_DATA_NUM = tddConf.value("ofdm_data_num", 1200);
    OFDM_DATA_START = tddConf.value("ofdm_data_start", (OFDM_CA_NUM - OFDM_DATA_NUM) / 2);
    packet_header_offset = tddConf.value("packet_header_offset", 64);

    /* Millipede configurations */
    core_offset = tddConf.value("core_offset", 18);
    worker_thread_num = tddConf.value("worker_thread_num", 25);
    socket_thread_num = tddConf.value("socket_thread_num", 4);
    fft_thread_num = tddConf.value("fft_thread_num", 4);
    demul_thread_num = tddConf.value("demul_thread_num", 11);
    zf_thread_num = worker_thread_num - fft_thread_num - demul_thread_num;

    demul_block_size = tddConf.value("demul_block_size", 48);
    zf_block_size = tddConf.value("zf_block_size", 1);

    rx_addr = tddConf.value("rx_addr", "127.0.0.1");
    tx_addr = tddConf.value("tx_addr", "127.0.0.1");
    tx_port = tddConf.value("tx_port", 7991);
    rx_port = tddConf.value("rx_port", 7891);

    json jframes = tddConf.value("frames", json::array());
    framePeriod = jframes.size();
    for (size_t f = 0; f < framePeriod; f++) {
        frames.push_back(jframes.at(0).get<std::string>());
    }
    Utils::loadDevices(hub_file, hub_ids);
    Utils::loadDevices(serial_file, radio_ids);
    if (radio_ids.size() != 0)
    {
        nRadios = radio_ids.size();
        nAntennas = nChannels * nRadios;
        if (ref_ant >= nAntennas)
            ref_ant = 0;
        if (BS_ANT_NUM != nAntennas)
           BS_ANT_NUM = nAntennas; 
    }
    symbolsPerFrame = frames.at(0).size();
    nUEs = std::count(frames.at(0).begin(), frames.at(0).end(), 'P');
    pilotSymbols = Utils::loadSymbols(frames, 'P');
    ULSymbols = Utils::loadSymbols(frames, 'U');
    DLSymbols = Utils::loadSymbols(frames, 'D');
    pilotSymsPerFrame = pilotSymbols[0].size();
    ulSymsPerFrame = ULSymbols[0].size();
    dlSymsPerFrame = DLSymbols[0].size();
    if (isUE and nRadios != pilotSymsPerFrame) {
        std::cerr << "Number of Pilot Symbols don't match number of Clients!" << std::endl;
        exit(0);
    }

    freq_orthogonal_pilot = tddConf.value("freq_orthogonal_pilot", false);

    mod_type = modulation == "64QAM" ? CommsLib::QAM64 : (modulation == "16QAM" ? CommsLib::QAM16 : CommsLib::QPSK);
    mod_order = (size_t)pow(2, mod_type);
    /* LDPC configurations */
    LDPC_config.Bg = tddConf.value("base_graph", 1);
    LDPC_config.earlyTermination = tddConf.value("earlyTermination", 1);
    LDPC_config.decoderIter = tddConf.value("decoderIter", 5);
    LDPC_config.Zc = tddConf.value("Zc", 32);
    LDPC_config.nRows = (LDPC_config.Bg == 1) ? 46 : 42;
    LDPC_config.cbEncLen = LDPC_config.nRows * LDPC_config.Zc;
    LDPC_config.cbLen = (LDPC_config.Bg == 1) ? LDPC_config.Zc * 22 : LDPC_config.Zc * 10;
    LDPC_config.cbCodewLen = (LDPC_config.Bg == 1) ? LDPC_config.Zc * 66 : LDPC_config.Zc * 50;
    LDPC_config.nblocksInSymbol = 1; //OFDM_DATA_NUM * mod_type / LDPC_config.cbCodewLen;

    printf("Encoder: Zc: %d, code block per symbol: %d, code block len: %d, encoded block len: %d, decoder iterations: %d\n",
        LDPC_config.Zc, LDPC_config.nblocksInSymbol, LDPC_config.cbLen, LDPC_config.cbCodewLen, LDPC_config.decoderIter);

    UE_NUM = nUEs;
    OFDM_SYM_LEN = OFDM_CA_NUM + CP_LEN;
    OFDM_FRAME_LEN = OFDM_CA_NUM + OFDM_PREFIX_LEN;
    sampsPerSymbol = symbolSize * OFDM_SYM_LEN + prefix + postfix; 
    pilot_symbol_num_perframe = pilotSymsPerFrame;
    symbol_num_perframe = symbolsPerFrame;
    data_symbol_num_perframe = symbolsPerFrame - pilot_symbol_num_perframe;
    ul_data_symbol_num_perframe = ulSymsPerFrame;
    dl_data_symbol_num_perframe = dlSymsPerFrame;
    downlink_mode = dl_data_symbol_num_perframe > 0;
    dl_data_symbol_start = downlink_mode ? DLSymbols[0][0] - pilot_symbol_num_perframe : 0;
    dl_data_symbol_end = downlink_mode ? DLSymbols[0].back() : 0;
    packet_length = packet_header_offset + sizeof(short) * sampsPerSymbol * 2;
    //packet_length = packet_header_offset + sizeof(short) * OFDM_FRAME_LEN * 2;
    data_symbol_num_perframe = symbol_num_perframe - pilotSymsPerFrame; 
    std::cout << "Config file loaded!" << std::endl;
    std::cout << "BS_ANT_NUM " << BS_ANT_NUM << std::endl;
    std::cout << "UE_NUM " << nUEs << std::endl;
    std::cout << "pilot sym num " << pilotSymsPerFrame << std::endl;
    std::cout << "UL sym num " << ulSymsPerFrame << std::endl;
    std::cout << "DL sym num " << dlSymsPerFrame << std::endl;

#ifdef USE_ARGOS
    std::vector<std::vector<double>> gold_ifft = CommsLib::getSequence(128, CommsLib::GOLD_IFFT);
    std::vector<std::complex<int16_t>> coeffs_ci16 = Utils::double_to_int16(gold_ifft);
    coeffs = Utils::cint16_to_uint32(coeffs_ci16, true, "QI");
    beacon_ci16.resize(256);
    for (size_t i = 0; i < 128; i++) {
        beacon_ci16[i] = std::complex<int16_t>((int16_t)(gold_ifft[0][i] * 32768), (int16_t)(gold_ifft[1][i] * 32768));
        beacon_ci16[i + 128] = beacon_ci16[i];
    }

    std::vector<std::complex<int16_t>> pre0(prefix, 0);
    std::vector<std::complex<int16_t>> post0(sampsPerSymbol - 256 - prefix, 0);
    beacon_ci16.insert(beacon_ci16.begin(), pre0.begin(), pre0.end());
    beacon_ci16.insert(beacon_ci16.end(), post0.begin(), post0.end());
    beacon = Utils::cint16_to_uint32(beacon_ci16, false, "QI");
#endif

    pilots_ = (float*)aligned_alloc(64, OFDM_CA_NUM * sizeof(float));
    float *pilots_2048 = (float*)aligned_alloc(64, 2048 * sizeof(float));
    size_t r = 0;
#ifdef GENERATE_PILOT
    for (size_t i = 0; i < OFDM_CA_NUM; i++) {
        if (i < OFDM_DATA_START || i >= OFDM_DATA_START + OFDM_DATA_NUM)
            pilots_[i] = 0;
        else
            pilots_[i] = 1 - 2 * (rand() % 2);
    }
#else
    // read pilots from file
    std::string cur_directory = TOSTRING(PROJECT_DIRECTORY);
    std::string filename = cur_directory + "/data/pilot_f_2048.bin";
    FILE* fp = fopen(filename.c_str(), "rb");
    if (fp == NULL) {
        printf("open file %s faild.\n", filename.c_str());
        std::cerr << "Error: " << strerror(errno) << std::endl;
    }
    r = fread(pilots_, sizeof(float), OFDM_CA_NUM, fp);
    if (r < OFDM_CA_NUM)
        printf("bad read from file %s \n", filename.c_str());
    fclose(fp);
    for (size_t i = 0; i < OFDM_CA_NUM; i++) {
        if (i < OFDM_DATA_START || i >= OFDM_DATA_START + OFDM_DATA_NUM)
            pilots_[i] = 0;
        else
            pilots_[i] = pilots_2048[424+i-OFDM_DATA_START];
    }
#endif

    pilotsF.resize(OFDM_CA_NUM);
    for (size_t i = 0; i < OFDM_CA_NUM; i++)
        pilotsF[i] = pilots_[i];
    pilot_cf32 = CommsLib::IFFT(pilotsF, OFDM_CA_NUM);
    pilot_cf32.insert(pilot_cf32.begin(), pilot_cf32.end() - CP_LEN, pilot_cf32.end()); // add CP

    std::vector<std::complex<int16_t>> pre_ci16(prefix, 0);
    std::vector<std::complex<int16_t>> post_ci16(postfix, 0);
    for (size_t i = 0; i < OFDM_CA_NUM + CP_LEN; i++)
        pilot_ci16.push_back(std::complex<int16_t>((int16_t)(pilot_cf32[i].real() * 32768), (int16_t)(pilot_cf32[i].imag() * 32768)));
    pilot_ci16.insert(pilot_ci16.begin(), pre_ci16.begin(), pre_ci16.end());
    pilot_ci16.insert(pilot_ci16.end(), post_ci16.begin(), post_ci16.end());
    size_t seq_len = pilot_cf32.size(); 
    for (size_t i = 0; i < seq_len; i++) {
        std::complex<float> cf = pilot_cf32[i];
        //pilot_cs16.push_back(std::complex<int16_t>((int16_t)(cf.real() * 32768), (int16_t)(cf.imag() * 32768)));
        pilot_cd64.push_back(std::complex<double>(cf.real(), cf.imag()));
    }

    if (exec_mode == "hw") {
        pilot = Utils::cfloat32_to_uint32(pilot_cf32, false, "QI");
        std::vector<uint32_t> pre(prefix, 0);
        std::vector<uint32_t> post(postfix, 0);
        pilot.insert(pilot.begin(), pre.begin(), pre.end());
        pilot.insert(pilot.end(), post.begin(), post.end());
        if (pilot.size() != sampsPerSymbol) {
            std::cout << "generated pilot symbol size does not match configured symbol size!" << std::endl;
            exit(1);
        }
    }


    dl_IQ_data.malloc(data_symbol_num_perframe, OFDM_CA_NUM * UE_NUM, 64);
    dl_IQ_modul.malloc(data_symbol_num_perframe, OFDM_CA_NUM * UE_NUM, 64); // used for debug
    dl_IQ_symbol.malloc(data_symbol_num_perframe, sampsPerSymbol, 64); // used for debug
    ul_IQ_data.malloc(ul_data_symbol_num_perframe * UE_NUM, OFDM_DATA_NUM, 64);
    ul_IQ_modul.malloc(ul_data_symbol_num_perframe * UE_NUM, OFDM_CA_NUM, 64);

#ifdef GENERATE_DATA
    for (size_t i = 0; i < data_symbol_num_perframe; i++) {
        for (size_t j = 0; j < OFDM_CA_NUM * UE_NUM; j++)
            dl_IQ_data[i][j] = rand() % mod_order;

        std::vector<std::complex<float>> modul_data = CommsLib::modulate(std::vector<int8_t>(dl_IQ_data[i], (dl_IQ_data[i] + OFDM_CA_NUM * UE_NUM)), mod_type);
        for (size_t j = 0; j < OFDM_CA_NUM * UE_NUM; j++) {
            if ((j % OFDM_CA_NUM) < OFDM_DATA_START || (j % OFDM_CA_NUM) >= OFDM_DATA_START + OFDM_DATA_NUM) {
                dl_IQ_modul[i][j].re = 0;
                dl_IQ_modul[i][j].im = 0;
            } else {
                dl_IQ_modul[i][j].re = modul_data[j].real();
                dl_IQ_modul[i][j].im = modul_data[j].imag();
            }
        }

        // if (i % UE_NUM == 0) {
        // int c = i / UE_NUM;
        int c = i;
        std::vector<std::complex<float>> ifft_dl_data = CommsLib::IFFT(modul_data, OFDM_CA_NUM);
        ifft_dl_data.insert(ifft_dl_data.begin(), ifft_dl_data.end() - CP_LEN, ifft_dl_data.end());
        for (size_t j = 0; j < sampsPerSymbol; j++) {
            if (j < prefix || j >= prefix + CP_LEN + OFDM_CA_NUM) {
                dl_IQ_symbol[c][j] = 0;
            } else {
                dl_IQ_symbol[c][j] = { (int16_t)(ifft_dl_data[j - prefix].real() * 32768), (int16_t)(ifft_dl_data[j - prefix].imag() * 32768) };
            }
        }
        // }
    }

    for (size_t i = 0; i < ul_data_symbol_num_perframe * UE_NUM; i++) {
        for (size_t j = 0; j < OFDM_DATA_NUM; j++)
            ul_IQ_data[i][j] = rand() % mod_order;
        std::vector<std::complex<float>> modul_data = CommsLib::modulate(std::vector<int8_t>(ul_IQ_data[i], ul_IQ_data[i] + OFDM_DATA_NUM * UE_NUM), mod_type);
        for (size_t j = 0; j < OFDM_CA_NUM; j++) {
            if (j < OFDM_DATA_START || j >= OFDM_DATA_START + OFDM_DATA_NUM)
                continue;
            size_t k = j - OFDM_DATA_START;
            ul_IQ_modul[i][j].re = modul_data[k].real();
            ul_IQ_modul[i][j].im = modul_data[k].imag();
        }
    }
#else
    std::string cur_directory1 = TOSTRING(PROJECT_DIRECTORY);
    std::string filename1 = cur_directory1 + "/data/orig_data_2048_ant" + std::to_string(BS_ANT_NUM) + ".bin";
    FILE* fd = fopen(filename1.c_str(), "rb");
    if (fd == NULL) {
        printf("open file %s faild.\n", filename1.c_str());
        std::cerr << "Error: " << strerror(errno) << std::endl;
    }
    for (size_t i = 0; i < data_symbol_num_perframe; i++) {
        for (size_t j = 0; j < UE_NUM; j++) {
            r = fread(dl_IQ_data[i] + j * OFDM_CA_NUM, sizeof(int8_t), OFDM_CA_NUM, fd);
            if (r < OFDM_CA_NUM)
                printf("bad read from file %s (batch %zu) \n", filename1.c_str(), i);
            // for (size_t k = 0; k < OFDM_CA_NUM; k++) {
            //     dl_IQ_data[i][j * OFDM_CA_NUM + k] = dl_IQ_data[i][j * OFDM_CA_NUM + k] % (uint8_t) mod_order;
            // }
        }
    }
    fclose(fd);

#ifdef USE_ARGOS
    // read uplink
    std::string filename2 = cur_directory1 + "/data/tx_ul_data_" + std::to_string(BS_ANT_NUM) + "x" + std::to_string(nUEs) + ".bin";
    fp = fopen(filename2.c_str(), "rb");
    if (fp == NULL) {
        std::cerr << "Openning File " << filename2 << " fails. Error: " << strerror(errno) << std::endl;
    }
    size_t total_sc = OFDM_DATA_NUM * UE_NUM * ul_data_symbol_num_perframe; // coding is not considered yet
    L2_data = new mac_dtype[total_sc];
    r = fread(L2_data, sizeof(mac_dtype), total_sc, fp);
    if (r < total_sc)
        printf("bad read from file %s \n", filename2.c_str());
    fclose(fp);
    for (size_t i = 0; i < total_sc; i++) {
        size_t sid = i / (data_sc_len * nUEs);
        size_t cid = i % (data_sc_len * nUEs) + OFDM_DATA_START;
        ul_IQ_modul[sid][cid] = L2_data[i];
    }
#endif
#endif

    running = true;
    printf("finished config\n");
}

Config::~Config()
{
    free_buffer_1d(&pilots_);
    dl_IQ_data.free();
    dl_IQ_modul.free();
    dl_IQ_symbol.free();
    ul_IQ_data.free();
    ul_IQ_modul.free();
}

int Config::getDownlinkPilotId(size_t frame_id, size_t symbol_id)
{
    std::vector<size_t>::iterator it;
    size_t fid = frame_id % framePeriod;
    it = find(DLSymbols[fid].begin(), DLSymbols[fid].end(), symbol_id);
    if (it != DLSymbols[fid].end()) {
        int id = it - DLSymbols[fid].begin();
        if (id < DL_PILOT_SYMS) {
#ifdef DEBUG3
            printf("getDownlinkPilotId(%d, %d) = %d\n", frame_id, symbol_id, id);
#endif
            return id;
        }
    }
    return -1;
}

int Config::getDlSFIndex(size_t frame_id, size_t symbol_id)
{
    std::vector<size_t>::iterator it;
    size_t fid = frame_id % framePeriod;
    it = find(DLSymbols[fid].begin(), DLSymbols[fid].end(), symbol_id);
    if (it != DLSymbols[fid].end())
        return it - DLSymbols[fid].begin();
    else
        return -1;
}

int Config::getPilotSFIndex(size_t frame_id, size_t symbol_id)
{
    std::vector<size_t>::iterator it;
    size_t fid = frame_id % framePeriod;
    it = find(pilotSymbols[fid].begin(), pilotSymbols[fid].end(), symbol_id);
    if (it != pilotSymbols[fid].end()) {
#ifdef DEBUG3
        printf("getPilotSFIndex(%d, %d) = %d\n", frame_id, symbol_id, it - pilotSymbols[fid].begin());
#endif
        return it - pilotSymbols[fid].begin();
    } else
        return -1;
}

int Config::getUlSFIndex(size_t frame_id, size_t symbol_id)
{
    std::vector<size_t>::iterator it;
    size_t fid = frame_id % framePeriod;
    it = find(ULSymbols[fid].begin(), ULSymbols[fid].end(), symbol_id);
    if (it != ULSymbols[fid].end()) {
#ifdef DEBUG3
        printf("getUlSFIndexId(%d, %d) = %d\n", frame_id, symbol_id, it - ULSymbols[fid].begin());
#endif
        return it - ULSymbols[fid].begin();
    } else
        return -1;
}

bool Config::isPilot(size_t frame_id, size_t symbol_id)
{
    size_t fid = frame_id % framePeriod;
    char s = frames[fid].at(symbol_id);
    if (symbol_id > symbolsPerFrame) {
        printf("\x1B[31mERROR: Received out of range symbol %d at frame %d\x1B[0m\n", symbol_id, frame_id);
        return false;
    }
#ifdef DEBUG3
    printf("isPilot(%d, %d) = %c\n", frame_id, symbol_id, s);
#endif
    if (isUE) {
        std::vector<size_t>::iterator it;
        it = find(DLSymbols[fid].begin(), DLSymbols[fid].end(), symbol_id);
        int ind = DL_PILOT_SYMS;
        if (it != DLSymbols[fid].end())
            ind = it - DLSymbols[fid].begin();
        return (ind < DL_PILOT_SYMS);
        //return cfg->frames[fid].at(symbol_id) == 'P' ? true : false;
    } else
        return s == 'P';
        //return (symbol_id < UE_NUM);
}

bool Config::isCalDlPilot(size_t frame_id, size_t symbol_id)
{
    size_t fid = frame_id % framePeriod;
    char s = frames[fid].at(symbol_id);
    if (isUE) {
        return false;
    } else
        return (s == 'C');
}

bool Config::isCalUlPilot(size_t frame_id, size_t symbol_id)
{
    size_t fid = frame_id % framePeriod;
    char s = frames[fid].at(symbol_id);
    if (isUE) {
        return false;
    } else
        return (s == 'L');
}

bool Config::isUplink(size_t frame_id, size_t symbol_id)
{
    size_t fid = frame_id % framePeriod;
    char s = frames[fid].at(symbol_id);
    if (symbol_id > symbolsPerFrame) {
        printf("\x1B[31mERROR: Received out of range symbol %d at frame %d\x1B[0m\n", symbol_id, frame_id);
        return false;
    }
#ifdef DEBUG3
    printf("isUplink(%d, %d) = %c\n", frame_id, symbol_id, s);
#endif
    return s == 'U';
    //return (symbol_id < symbol_num_perframe) && (symbol_id >= UE_NUM);
}

bool Config::isDownlink(size_t frame_id, size_t symbol_id)
{
    size_t fid = frame_id % framePeriod;
    char s = frames[fid].at(symbol_id);
#ifdef DEBUG3
    printf("isDownlink(%d, %d) = %c\n", frame_id, symbol_id, s);
#endif
    if (isUE)
        return s == 'D' && !this->isPilot(frame_id, symbol_id);
    else
        return s == 'D';
}

extern "C" {
__attribute__((visibility("default"))) Config* Config_new(char* filename)
{

    Config* cfg = new Config(filename);

    return cfg;
}
}
