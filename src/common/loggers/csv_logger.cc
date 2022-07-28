/**
 * @file csv_logger.cc
 * @brief Implementation file for the CsvLogger class which records runtime
 * physical-layer performance into csv files. Enabled or disabled by cmake.
 */

#include "csv_logger.h"

#include "logger.h"
#include "utils.h"

namespace CsvLog {

CsvLogger::CsvLogger(size_t log_id, const std::string& radio_name) {
#if defined(ENABLE_CSV_LOG)
  if (log_id >= kAllLogs) {
    AGORA_LOG_ERROR("Invalid log id %zu in CsvLogger\n", log_id);
  } else {
    std::string filename =
        "log/log-" + kCsvName.at(log_id) + "-" + radio_name + ".csv";
    std::remove(filename.c_str());
    logger_ = spdlog::create_async_nb<spdlog::sinks::basic_file_sink_mt>(
        kCsvName.at(log_id), filename);
    logger_->set_level(spdlog::level::info);
    logger_->set_pattern("%v");
  }
#else
  unused(log_id);
  unused(radio_name);
#endif  //ENABLE_CSV_LOG
}

}  //namespace CsvLog