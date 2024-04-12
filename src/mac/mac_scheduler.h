/**
 * @file mac_scheduler.h
 * @brief Declaration file for the simple MAC scheduler class
 */
#ifndef MAC_SCHEDULER_H_
#define MAC_SCHEDULER_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "armadillo"
#include "config.h"
#include "mac_utils.h"
#include "memory_manage.h"
#include "scheduler_model.h"

class MacScheduler {
 public:
  explicit MacScheduler(Config* const cfg);
  ~MacScheduler();

  bool IsUeScheduled(size_t frame_id, size_t sc_id, size_t ue_id);
  size_t ScheduledUeIndex(size_t frame_id, size_t sc_id, size_t sched_ue_id);
  arma::uvec ScheduledUeList(size_t frame_id, size_t sc_id);
  arma::uvec ScheduledUeMap(size_t frame_id, size_t sc_id);
  size_t UeScheduleIndex(size_t sched_d);
  size_t ScheduledUeUlMcs(size_t frame_id, size_t ue_id) const;
  size_t ScheduledUeDlMcs(size_t frame_id, size_t ue_id) const;

  //Used for Proportional Fairness Algorithm
  void UpdateCSI(size_t cur_sc_id, const arma::cx_fmat& csi_in);
  void UpdateSNR(std::vector<float> snr_per_ue);
  void UpdateScheduler(size_t frame_id);

  /*void UpdateUlMCS(const nlohmann::json& ul_mcs_params);
  void UpdateDlMCS(const nlohmann::json& dl_mcs_params);
  void UpdateCtrlMCS();*/
  inline MacUtils& Params() { return this->params_; }

 private:
  Table<size_t> ul_mcs_buffer_;
  Table<size_t> dl_mcs_buffer_;
  /*std::string ul_modulation_;  // Modulation order as a string, e.g., "16QAM"
  size_t
      ul_mod_order_bits_;  // Number of binary bits used for a modulation order
  std::string dl_modulation_;
  size_t dl_mod_order_bits_;
  size_t dl_bcast_mod_order_bits_;*/

  // Modulation lookup table for mapping binary bits to constellation points
  /*Table<complex_float> ul_mod_table_;
  Table<complex_float> dl_mod_table_;*/

  /*LDPCconfig ul_ldpc_config_;        // Uplink LDPC parameters
  LDPCconfig dl_ldpc_config_;        // Downlink LDPC parameters
  LDPCconfig dl_bcast_ldpc_config_;  // Downlink Broadcast LDPC parameters
				     */
  size_t ul_mcs_index_;
  size_t dl_mcs_index_;
  size_t dl_code_rate_;
  size_t ul_code_rate_;
  Config* const cfg_;

  std::vector<float> snr_per_ue_;
  arma::cx_fmat csi_;

  std::unique_ptr<SchedulerModel> scheduler_model_;

  MacUtils params_;
};

#endif  // MAC_SCHEDULER_H_
