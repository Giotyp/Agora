/**
 * @file mac_thread.cc
 * @brief Implementation file for the MacThreadBaseStation class.
 */
#include "mac_thread_basestation.h"

#include "data_generator.h"
#include "logger.h"
#include "utils_ldpc.h"

static constexpr size_t kUdpRxBufferPadding = 2048u;

MacThreadBaseStation::MacThreadBaseStation(
    Config* cfg, size_t core_offset,
    PtrCube<kFrameWnd, kMaxSymbols, kMaxUEs, int8_t>& decoded_buffer,
    Table<int8_t>* dl_bits_buffer, Table<int8_t>* dl_bits_buffer_status,
    moodycamel::ConcurrentQueue<EventData>* rx_queue,
    moodycamel::ConcurrentQueue<EventData>* tx_queue, MacScheduler* mac_sched,
    PhyStats* in_phy_stats, const std::string& log_filename)
    : cfg_(cfg),
      freq_ghz_(GetTime::MeasureRdtscFreq()),
      tsc_delta_((cfg_->GetFrameDurationSec() * 1e9) / freq_ghz_),
      core_offset_(core_offset),
      decoded_buffer_(decoded_buffer),
      rx_queue_(rx_queue),
      tx_queue_(tx_queue),
      mac_sched_(mac_sched),
      phy_stats_(in_phy_stats) {
  valid_mac_packets_.fill(0);
  error_mac_packets_.fill(0);
  // Set up MAC log file
  if (log_filename.empty() == false) {
    log_filename_ = log_filename;  // Use a non-default log filename
  } else {
    log_filename_ = kDefaultLogFilename;
  }
  log_file_ = std::fopen(log_filename_.c_str(), "w");
  RtAssert(log_file_ != nullptr, "Failed to open MAC log file");

  AGORA_LOG_INFO(
      "MacThreadBaseStation: Frame duration %.2f ms, tsc_delta %zu\n",
      cfg_->GetFrameDurationSec() * 1000, tsc_delta_);

  // Set up buffers
  client_.dl_bits_buffer_id_.fill(0);
  client_.dl_bits_buffer_ = dl_bits_buffer;
  client_.dl_bits_buffer_status_ = dl_bits_buffer_status;

  server_.n_filled_in_frame_.fill(0);
  for (size_t ue_ant = 0; ue_ant < cfg_->UeAntTotal(); ue_ant++) {
    server_.data_size_.emplace_back(
        std::vector<size_t>(cfg->Frame().NumUlDataSyms()));
  }

  // The frame data will hold the data coming from the Phy (Received)
  for (auto& v : server_.frame_data_) {
    v.resize(cfg_->MacDataBytesNumPerframe(Direction::kUplink));
  }

  const size_t udp_pkt_len =
      cfg_->MacDataBytesNumPerframe(Direction::kDownlink);
  udp_pkt_buf_.resize(udp_pkt_len + kUdpRxBufferPadding);

  if (kEnableMac == true) {
    // TODO: See if it makes more sense to split up the UE's by port here for
    // client mode.
    size_t udp_server_port = cfg_->BsMacRxPort();
    AGORA_LOG_INFO(
        "MacThreadBaseStation: setting up udp server for mac data at port "
        "%zu\n",
        udp_server_port);
    udp_comm_ =
        std::make_unique<UDPComm>(cfg_->BsServerAddr(), udp_server_port,
                                  udp_pkt_len * kMaxUEs * kMaxPktsPerUE, 0);
  } else {
    num_dl_mac_bytes_ = cfg->MacBytesNumPerframe(Direction::kDownlink);
    if (num_dl_mac_bytes_ > 0) {
      // Downlink LDPC input bits
      dl_mac_bytes_.Calloc(cfg_->UeAntNum(), num_dl_mac_bytes_,
                           Agora_memory::Alignment_t::kAlign64);
      const std::string dl_data_file =
          kExperimentFilepath + kDlLdpcDataPrefix +
          std::to_string(cfg_->OfdmCaNum()) + "_ue" +
          std::to_string(cfg_->UeAntTotal()) + ".bin";
      AGORA_LOG_FRAME("Config: Reading downlink data bits from %s\n",
                      dl_data_file.c_str());

      size_t seek_offset =
          num_dl_mac_bytes_ * cfg_->UeAntOffset() * sizeof(int8_t);
      for (size_t j = 0; j < cfg_->UeAntNum(); j++) {
        Utils::ReadBinaryFile(dl_data_file, sizeof(int8_t), num_dl_mac_bytes_,
                              seek_offset, dl_mac_bytes_[j]);
        seek_offset += num_dl_mac_bytes_ * sizeof(int8_t);
      }
    }
    num_ul_mac_bytes_ = cfg->MacBytesNumPerframe(Direction::kUplink);
    if (num_ul_mac_bytes_ > 0) {
      // Downlink LDPC input bits
      ul_mac_bytes_.Calloc(cfg_->UeAntNum(), num_ul_mac_bytes_,
                           Agora_memory::Alignment_t::kAlign64);
      const std::string ul_data_file =
          kExperimentFilepath + kUlLdpcDataPrefix +
          std::to_string(cfg_->OfdmCaNum()) + "_ue" +
          std::to_string(cfg_->UeAntTotal()) + ".bin";
      AGORA_LOG_FRAME("Config: Reading downlink data bits from %s\n",
                      ul_data_file.c_str());

      size_t seek_offset =
          num_ul_mac_bytes_ * cfg_->UeAntOffset() * sizeof(int8_t);
      for (size_t j = 0; j < cfg_->UeAntNum(); j++) {
        Utils::ReadBinaryFile(ul_data_file, sizeof(int8_t), num_ul_mac_bytes_,
                              seek_offset, ul_mac_bytes_[j]);
        seek_offset += num_ul_mac_bytes_ * sizeof(int8_t);
      }
    }
    next_radio_id_ = 0;
  }
  crc_obj_ = std::make_unique<DoCRC>();
}

MacThreadBaseStation::~MacThreadBaseStation() {
  if (kEnableMac == false) {
    dl_mac_bytes_.Free();
    ul_mac_bytes_.Free();
  }
  std::fclose(log_file_);
  AGORA_LOG_INFO("MacThreadBaseStation: MAC thread destroyed\n");
}

void MacThreadBaseStation::ProcessRxFromPhy() {
  EventData event;
  if (rx_queue_->try_dequeue(event) == false) {
    return;
  }

  if (event.event_type_ == EventType::kPacketToMac) {
    AGORA_LOG_TRACE("MacThreadBaseStation: MAC thread event kPacketToMac\n");
    ProcessCodeblocksFromPhy(event);
  } else if (event.event_type_ == EventType::kPacketFromMac) {
    AGORA_LOG_TRACE("MacThreadBaseStation: MAC thread event kPacketFromMac\n");
    ProcessUdpPacketsFromApps(event);
  } else if (event.event_type_ == EventType::kSNRReport) {
    AGORA_LOG_TRACE("MacThreadBaseStation: MAC thread event kSNRReport\n");
    ProcessSnrReportFromPhy(event);
  }
}

void MacThreadBaseStation::ProcessSnrReportFromPhy(EventData event) {
  const size_t ue_id = gen_tag_t(event.tags_[0]).ue_id_;
  if (server_.snr_[ue_id].size() == kSNRWindowSize) {
    server_.snr_[ue_id].pop();
  }

  float snr;
  std::memcpy(&snr, &event.tags_[1], sizeof(float));
  server_.snr_[ue_id].push(snr);
}

void MacThreadBaseStation::SendRanConfigUpdate(EventData /*event*/) {
  RanConfig rc;
  rc.n_antennas_ = 0;  // TODO [arjun]: What's the correct value here?
  rc.mcs_index_ = cfg_->McsIndex(Direction::kUplink);
  rc.frame_id_ = scheduler_next_frame_id_;
  // TODO: change n_antennas to a desired value
  // cfg_->BsAntNum() is added to fix compiler warning
  rc.n_antennas_ = cfg_->BsAntNum();

  EventData msg(EventType::kRANUpdate);
  msg.num_tags_ = 3;
  msg.tags_[0] = rc.n_antennas_;
  msg.tags_[1] = rc.mcs_index_;
  msg.tags_[2] = rc.frame_id_;
  RtAssert(tx_queue_->enqueue(msg),
           "MAC thread: failed to send RAN update to Agora");

  scheduler_next_frame_id_++;
}

void MacThreadBaseStation::ProcessCodeblocksFromPhy(EventData event) {
  assert(event.event_type_ == EventType::kPacketToMac);

  const size_t frame_id = gen_tag_t(event.tags_[0]).frame_id_;
  const size_t symbol_id = gen_tag_t(event.tags_[0]).symbol_id_;
  const size_t ue_id = gen_tag_t(event.tags_[0]).ue_id_;
  const size_t symbol_array_index = cfg_->Frame().GetULSymbolIdx(symbol_id);
  const size_t num_pilot_symbols = cfg_->Frame().ClientUlPilotSymbols();
  if (symbol_array_index >= num_pilot_symbols) {
    const size_t data_symbol_idx_ul = symbol_array_index - num_pilot_symbols;
    const size_t frame_slot = frame_id % kFrameWnd;
    const int8_t* src_data =
        decoded_buffer_[frame_slot][data_symbol_idx_ul][ue_id];

    if (kEnableMac == false) {
      if (kPrintPhyStats == true) {
        const size_t symbol_offset =
            cfg_->GetTotalDataSymbolIdxUl(frame_id, data_symbol_idx_ul);
        const size_t mac_packet_len = cfg_->MacPacketLength(Direction::kUplink);
        phy_stats_->UpdateDecodedBits(ue_id, symbol_offset, frame_slot,
                                      mac_packet_len * 8);
        phy_stats_->IncrementDecodedBlocks(ue_id, symbol_offset, frame_slot);
        size_t block_error(0);
        for (size_t i = 0; i < mac_packet_len; i++) {
          int8_t rx_byte = src_data[i];
          int8_t tx_byte =
              ul_mac_bytes_[ue_id][data_symbol_idx_ul * mac_packet_len + i];
          phy_stats_->UpdateBitErrors(ue_id, symbol_offset, frame_slot, tx_byte,
                                      rx_byte);
          if (rx_byte != tx_byte) {
            block_error++;
          }
        }
        phy_stats_->UpdateBlockErrors(ue_id, symbol_offset, frame_slot,
                                      block_error);
      }
    } else {
      // The decoded symbol knows nothing about the padding / storage of the data
      const auto* pkt = reinterpret_cast<const MacPacketPacked*>(src_data);
      // Destination only contains "payload"
      const size_t mac_data_bytes_per_frame =
          cfg_->MacDataBytesNumPerframe(Direction::kUplink);
      const size_t data_symbol_index_start =
          cfg_->Frame().GetULSymbol(num_pilot_symbols);
      const size_t data_symbol_index_end = cfg_->Frame().GetULSymbolLast();
      const size_t num_mac_packets_per_frame =
          cfg_->MacPacketsPerframe(Direction::kUplink);
      const size_t mac_payload_max_length =
          cfg_->MacPayloadMaxLength(Direction::kUplink);
      const size_t dest_packet_size = mac_payload_max_length;

      // TODO: enable ARQ and ensure reliable data goes to app
      const size_t frame_data_offset =
          (symbol_array_index - num_pilot_symbols) * dest_packet_size;

      // Who's junk is better? No reason to copy currupted data
      server_.n_filled_in_frame_.at(ue_id) += dest_packet_size;

      std::stringstream ss;  // Debug formatting
      ss << "MacThreadBasestation: Received frame " << pkt->Frame() << ":"
         << frame_id << " symbol " << pkt->Symbol() << ":" << symbol_id
         << " user " << pkt->Ue() << ":" << ue_id << " length "
         << pkt->PayloadLength() << ":" << dest_packet_size << " crc "
         << pkt->Crc() << " copied to offset " << frame_data_offset
         << std::endl;

      if (kLogMacPackets) {
        ss << "Header Info:" << std::endl
           << "FRAME_ID: " << pkt->Frame() << std::endl
           << "SYMBOL_ID: " << pkt->Symbol() << std::endl
           << "UE_ID: " << pkt->Ue() << std::endl
           << "DATLEN: " << pkt->PayloadLength() << std::endl
           << "PAYLOAD:" << std::endl;
        for (size_t i = 0; i < dest_packet_size; i++) {
          ss << std::to_string(pkt->Data()[i]) << " ";
        }
        ss << std::endl;
      }

      bool data_valid = false;
      // Data validity check
      if ((static_cast<size_t>(pkt->PayloadLength()) <= dest_packet_size) &&
          ((pkt->Symbol() >= data_symbol_index_start) &&
           (pkt->Symbol() <= data_symbol_index_end)) &&
          (pkt->Ue() <= cfg_->UeAntNum())) {
        auto crc = static_cast<uint16_t>(
            crc_obj_->CalculateCrc24(pkt->Data(), pkt->PayloadLength()) &
            0xFFFF);

        data_valid = (crc == pkt->Crc());
      }

      if (data_valid) {
        if (pkt->Ue() < kMaxUEs) {
          valid_mac_packets_.at(pkt->Ue())++;
        } else {
          throw std::runtime_error("Ue ID out of range " + pkt->Ue());
        }
        AGORA_LOG_FRAME("%s", ss.str().c_str());
        /// Spot to be optimized #1
        std::memcpy(&server_.frame_data_.at(ue_id).at(frame_data_offset),
                    pkt->Data(), pkt->PayloadLength());

        server_.data_size_.at(ue_id).at(
            symbol_array_index - num_pilot_symbols) = pkt->PayloadLength();

      } else {
        if (pkt->Ue() < kMaxUEs) {
          error_mac_packets_.at(pkt->Ue())++;
        }
        ss << "  *****Failed Data integrity check - invalid parameters"
           << std::endl;

        AGORA_LOG_ERROR("%s", ss.str().c_str());
        // Set the default to 0 valid data bytes
        server_.data_size_.at(ue_id).at(symbol_array_index -
                                        num_pilot_symbols) = 0;
      }
      std::fprintf(log_file_, "%s", ss.str().c_str());
      ss.str("");

      // When the frame is full, send it to the application
      if (server_.n_filled_in_frame_.at(ue_id) == mac_data_bytes_per_frame) {
        server_.n_filled_in_frame_.at(ue_id) = 0;
        /// Spot to be optimized #2 -- left shift data over to remove padding
        bool shifted = false;
        size_t src_offset = 0;
        size_t dest_offset = 0;
        for (size_t packet = 0; packet < num_mac_packets_per_frame; packet++) {
          const size_t rx_packet_size = server_.data_size_.at(ue_id).at(packet);
          if ((rx_packet_size < mac_payload_max_length) || (shifted == true)) {
            shifted = true;
            if (rx_packet_size > 0) {
              std::memmove(&server_.frame_data_.at(ue_id).at(dest_offset),
                           &server_.frame_data_.at(ue_id).at(src_offset),
                           rx_packet_size);
            }
          }
          dest_offset += rx_packet_size;
          src_offset += mac_payload_max_length;
        }

        if (dest_offset > 0) {
          udp_comm_->Send(kMacRemoteHostname, cfg_->BsMacTxPort() + ue_id,
                          &server_.frame_data_.at(ue_id).at(0), dest_offset);
        }

        ss << "MacThreadBasestation: Sent data for frame " << frame_id
           << ", ue " << ue_id << ", size " << dest_offset << ":"
           << mac_data_bytes_per_frame << std::endl;

        if (kLogMacPackets) {
          std::fprintf(stdout, "%s", ss.str().c_str());
        }

        for (size_t i = 0u; i < dest_offset; i++) {
          ss << static_cast<uint8_t>(server_.frame_data_.at(ue_id).at(i))
             << " ";
        }
        std::fprintf(log_file_, "%s", ss.str().c_str());
        ss.str("");
      }
    }
  }
  RtAssert(
      tx_queue_->enqueue(EventData(EventType::kPacketToMac, event.tags_[0])),
      "Socket message enqueue failed\n");
}

void MacThreadBaseStation::SendControlInformation() {
  // send RAN control information UE
  RBIndicator ri;
  ri.ue_id_ = next_radio_id_;
  ri.mcs_index_ = cfg_->McsIndex(Direction::kUplink);
  udp_comm_->Send(cfg_->UeServerAddr(), kMacBaseClientPort + ri.ue_id_,
                  reinterpret_cast<std::byte*>(&ri), sizeof(RBIndicator));

  // update RAN config within Agora
  SendRanConfigUpdate(EventData(EventType::kRANUpdate));
}

void MacThreadBaseStation::ProcessUdpPacketsFromApps(EventData event) {
  const size_t max_data_bytes_per_frame =
      cfg_->MacDataBytesNumPerframe(Direction::kDownlink);
  const size_t num_mac_packets_per_frame =
      cfg_->MacPacketsPerframe(Direction::kDownlink);
  if (0 == max_data_bytes_per_frame) {
    return;
  }

  char* payload;
  if (kEnableMac) {
    // Processes the packets of an entire frame (remove variable later)
    const size_t packets_required = num_mac_packets_per_frame;

    size_t packets_received = 0;
    size_t current_packet_bytes = 0;
    size_t current_packet_start_index = 0;
    size_t total_bytes_received = 0;

    const size_t max_recv_attempts = (packets_required * 10u);
    size_t rx_attempts;
    for (rx_attempts = 0u; rx_attempts < max_recv_attempts; rx_attempts++) {
      const ssize_t ret =
          udp_comm_->Recv(&udp_pkt_buf_.at(total_bytes_received),
                          (udp_pkt_buf_.size() - total_bytes_received));
      if (ret == 0) {
        AGORA_LOG_TRACE(
            "MacThreadBaseStation: No data received with %zu pending\n",
            total_bytes_received);
        if (total_bytes_received == 0) {
          return;  // No data received
        } else {
          AGORA_LOG_INFO(
              "MacThreadBaseStation: No data received but there was data in "
              "buffer pending %zu : try %zu out of %zu\n",
              total_bytes_received, rx_attempts, max_recv_attempts);
        }
      } else if (ret < 0) {
        // There was an error in receiving
        AGORA_LOG_ERROR("MacThreadBaseStation: Error in reception %zu\n", ret);
        cfg_->Running(false);
        return;
      } else { /* Got some data */
        total_bytes_received += ret;
        current_packet_bytes += ret;

        // std::printf(
        //    "Received %zu bytes packet number %zu packet size %zu total %zu\n",
        //    ret, packets_received, total_bytes_received, current_packet_bytes);

        // While we have packets remaining and a header to process
        const size_t header_size = sizeof(MacPacketHeaderPacked);
        while ((packets_received < packets_required) &&
               (current_packet_bytes >= header_size)) {
          // See if we have enough data and process the MacPacket header
          const auto* rx_mac_packet_header =
              reinterpret_cast<const MacPacketPacked*>(
                  &udp_pkt_buf_.at(current_packet_start_index));

          const size_t current_packet_size =
              header_size + rx_mac_packet_header->PayloadLength();

          // std::printf("Packet number %zu @ %zu packet size %d:%zu total %zu\n",
          //            packets_received, current_packet_start_index,
          //            rx_mac_packet_header->datalen_, current_packet_size,
          //            current_packet_bytes);

          if (current_packet_bytes >= current_packet_size) {
            current_packet_bytes = current_packet_bytes - current_packet_size;
            current_packet_start_index =
                current_packet_start_index + current_packet_size;
            packets_received++;
          } else {
            // Don't have the entire packet, keep trying
            break;
          }
        }
        AGORA_LOG_FRAME(
            "MacThreadBaseStation: Received %zu : %zu bytes in packet %zu : "
            "%zu\n",
            ret, total_bytes_received, packets_received, packets_required);
      }

      // Check for completion
      if (packets_received == packets_required) {
        break;
      }
    }  // end rx attempts

    if (packets_received != packets_required) {
      AGORA_LOG_ERROR(
          "MacThreadBaseStation: Received %zu : %zu packets with %zu total "
          "bytes "
          "in %zu attempts\n",
          packets_received, packets_required, total_bytes_received,
          rx_attempts);
    } else {
      AGORA_LOG_FRAME("MacThreadClient: Received Mac Frame Data\n");
    }
    RtAssert(packets_received == packets_required,
             "MacThreadBaseStation: ProcessUdpPacketsFromApps incorrect data "
             "received!");
    payload = reinterpret_cast<char*>(&udp_pkt_buf_[0]);
  } else {
    const size_t ue_id = gen_tag_t(event.tags_[0]).ue_id_;
    payload = reinterpret_cast<char*>(dl_mac_bytes_[ue_id]);
  }
  // Currently this is a packet list of mac packets
  ProcessUdpPacketsFromAppsBs(event, payload);
}

void MacThreadBaseStation::ProcessUdpPacketsFromAppsBs(EventData event,
                                                       const char* payload) {
  const size_t frame_id = gen_tag_t(event.tags_[0]).frame_id_;
  size_t ue_id = gen_tag_t(event.tags_[0]).ue_id_;
  const size_t mac_packet_length = cfg_->MacPacketLength(Direction::kDownlink);
  const size_t num_mac_packets_per_frame =
      cfg_->MacPacketsPerframe(Direction::kDownlink);
  const size_t num_pilot_symbols = cfg_->Frame().ClientDlPilotSymbols();
  // Data integrity check
  size_t pkt_offset = 0;
  size_t symbol_id = 0;
  for (size_t packet = 0u; packet < num_mac_packets_per_frame; packet++) {
    const auto* pkt =
        reinterpret_cast<const MacPacketPacked*>(&payload[pkt_offset]);

    if (kEnableMac) {
      if (frame_id != pkt->Frame()) {
        AGORA_LOG_ERROR(
            "Received pkt %zu data with unexpected frame id %zu, expected "
            "%d\n",
            packet, frame_id, pkt->Frame());
      }
      if (packet == 0) {
        ue_id = pkt->Ue();
      } else {
        if ((symbol_id + 1) != pkt->Symbol()) {
          AGORA_LOG_ERROR("Received out of order symbol id %d, expected %zu\n",
                          pkt->Symbol(), symbol_id + 1);
        }
      }
      symbol_id = pkt->Symbol();
    } else {
      if (cfg_->Frame().GetDLSymbol(packet + num_pilot_symbols) !=
          pkt->Symbol()) {
        AGORA_LOG_ERROR("Received out of order symbol id %d, expected %zu\n",
                        pkt->Symbol(),
                        cfg_->Frame().GetDLSymbol(packet + num_pilot_symbols));
      }
    }
    if (ue_id != pkt->Ue()) {
      AGORA_LOG_ERROR(
          "Received pkt %zu data with unexpected UE id %zu, expected %d\n",
          packet, ue_id, pkt->Ue());
    }
    pkt_offset += mac_packet_length;
  }

  /*if (next_radio_id_ != ue_id) {
    AGORA_LOG_ERROR("Error - radio id %zu, expected %zu\n", ue_id,
                    next_radio_id_);
  }*/
  // End data integrity check

  next_radio_id_ = ue_id;

#if defined(ENABLE_RB_IND)
  RBIndicator ri;
  ri.ue_id_ = ue_id;  //next_radio_id_;
  ri.mcs_index_ = kDefaultMcsIndex;
#endif

  if (kLogMacPackets) {
    std::stringstream ss;
    std::fprintf(
        log_file_,
        "MacThreadBasestation: Received data from app for frame %zu, ue "
        "%zu size %zu\n",
        frame_id, ue_id /*next_radio_id_*/, pkt_offset);

    for (size_t i = 0; i < pkt_offset; i++) {
      ss << std::to_string((uint8_t)(payload[i])) << " ";
    }
    std::fprintf(log_file_, "%s\n", ss.str().c_str());
  }
  // We've received bits for the uplink.
  //size_t& radio_buf_id = client_.dl_bits_buffer_id_[ue_id][next_radio_id_];
  size_t radio_buf_id = frame_id % kFrameWnd;

  //if ((*client_.dl_bits_buffer_status_)[next_radio_id_][radio_buf_id] == 1) {
  if ((*client_.dl_bits_buffer_status_)[ue_id][radio_buf_id] == 1) {
    std::fprintf(
        stderr,
        "MacThreadBasestation: UDP RX buffer full, buffer ID: %zu. Dropping "
        "rx frame data\n",
        radio_buf_id);
    return;
  }

  size_t src_pkt_offset = 0;
  // Copy from the packet rx buffer into ul_bits memory (unpacked)
  for (size_t pkt_id = 0; pkt_id < num_mac_packets_per_frame; pkt_id++) {
    // could use pkt_id vs src_packet->symbol_id_ but might reorder packets
    const size_t dest_pkt_offset =
        ((radio_buf_id * num_mac_packets_per_frame) + pkt_id) *
        mac_packet_length;

    auto* pkt = reinterpret_cast<MacPacketPacked*>(
        //&(*client_.dl_bits_buffer_)[next_radio_id_][dest_pkt_offset]);
        &(*client_.dl_bits_buffer_)[ue_id][dest_pkt_offset]);

    const char* src_data = &payload[src_pkt_offset];
    /*kEnableMac ? &payload[src_pkt_offset]
                   : reinterpret_cast<const char*>(
                         dl_mac_bytes_[next_radio_id_] + src_pkt_offset);*/
    const auto* src_packet = reinterpret_cast<const MacPacketPacked*>(src_data);
    /*const size_t symbol_idx =
        cfg_->Frame().GetDLSymbolIdx(src_packet->Symbol());
    RtAssert((symbol_idx == pkt_id + num_pilot_symbols) &&
                 (src_packet->Ue() == next_radio_id_),
             "Invalid MAC packet symbol or radio ID!\n");*/

    pkt->Set(kEnableMac ? frame_id : 0, src_packet->Symbol(), src_packet->Ue(),
             src_packet->PayloadLength());

#if ENABLE_RB_IND
    pkt->rb_indicator_ = ri;
#endif

    pkt->LoadData(src_packet->Data());
    src_pkt_offset += mac_packet_length;
    // Insert CRC
    pkt->Crc(
        (uint16_t)(crc_obj_->CalculateCrc24(pkt->Data(), pkt->PayloadLength()) &
                   0xFFFF));

    if (kLogMacPackets) {
      std::stringstream ss;

      ss << "MacThreadBasestation: created packet frame " << frame_id
         << ", pkt " << pkt_id << ", size "
         << cfg_->MacPayloadMaxLength(Direction::kDownlink) << " radio buff id "
         << radio_buf_id << ", loc " << (size_t)pkt << " dest offset "
         << dest_pkt_offset << std::endl;

      ss << "Header Info:" << std::endl
         << "FRAME_ID: " << pkt->Frame() << std::endl
         << "SYMBOL_ID: " << pkt->Symbol() << std::endl
         << "UE_ID: " << pkt->Ue() << std::endl
         << "DATLEN: " << pkt->PayloadLength() << std::endl
         << "PAYLOAD:" << std::endl;
      for (size_t i = 0; i < pkt->PayloadLength(); i++) {
        ss << std::to_string(pkt->Data()[i]) << " ";
      }
      ss << std::endl;
      std::fprintf(stdout, "%s", ss.str().c_str());
      std::fprintf(log_file_, "%s", ss.str().c_str());
      ss.str("");
    }
  }  // end all packets

  //(*client_.dl_bits_buffer_status_)[next_radio_id_][radio_buf_id] = 1;
  (*client_.dl_bits_buffer_status_)[ue_id][radio_buf_id] = 1;
  EventData msg(EventType::kPacketFromMac,
                rx_mac_tag_t(ue_id, radio_buf_id, frame_id).tag_);
  //rx_mac_tag_t(next_radio_id_, radio_buf_id, frame_id).tag_);
  AGORA_LOG_TRACE("MacThreadBasestation: Tx mac information to %zu %zu\n",
                  ue_id, radio_buf_id);
  //next_radio_id_, radio_buf_id);
  RtAssert(tx_queue_->enqueue(msg),
           "MacThreadBasestation: Failed to enqueue downlink packet");

  //radio_buf_id = (radio_buf_id + 1) % kFrameWnd;
  // Might be unnecessary now.
  /*next_radio_id_ = (next_radio_id_ + 1) % cfg_->UeAntNum();
  if (next_radio_id_ == 0) {
    next_tx_frame_id_++;
  }*/
}

void MacThreadBaseStation::RunEventLoop() {
  AGORA_LOG_INFO(
      "MacThreadBasestation: Running MAC thread event loop, logging to file "
      "%s\n",
      log_filename_.c_str());
  PinToCoreWithOffset(ThreadType::kWorkerMacTXRX, core_offset_,
                      0 /* thread ID */);

  size_t last_frame_tx_tsc = 0;

  while (cfg_->Running() == true) {
    ProcessRxFromPhy();

    if constexpr (kEnableMac) {
      if ((GetTime::Rdtsc() - last_frame_tx_tsc) > tsc_delta_) {
        SendControlInformation();
        last_frame_tx_tsc = GetTime::Rdtsc();
      }
    }

    // No need to process incomming packets if we are finished
    /*if (next_tx_frame_id_ != cfg_->FramesToTest()) {
      ProcessUdpPacketsFromApps();
    }*/
  }
}

void MacThreadBaseStation::PrintUplinkMacErrors() {
  std::string tx_type = "Uplink";

  for (size_t ue_id = 0; ue_id < cfg_->UeAntNum(); ue_id++) {
    const size_t total_mac_packets =
        error_mac_packets_.at(ue_id) + valid_mac_packets_.at(ue_id);
    AGORA_LOG_INFO("UE %zu: %s mac packet errors %zu/%zu (%f)\n", ue_id,
                   tx_type.c_str(), error_mac_packets_.at(ue_id),
                   total_mac_packets,
                   static_cast<float>(error_mac_packets_.at(ue_id)) /
                       static_cast<float>(total_mac_packets));
  }
}
