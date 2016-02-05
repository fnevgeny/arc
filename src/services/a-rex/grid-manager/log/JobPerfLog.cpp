#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <fstream>

#include <arc/DateTime.h>

#include "JobPerfLog.h"

namespace ARex {

void JobPerfLog::SetEnabled(bool enabled) {
  if(enabled != log_enabled) {
    log_enabled = enabled;
    start_recorded = false;
  };
}

void JobPerfLog::LogStart() {
  start_recorded = false;
  if(!log_enabled) return;
  if((clock_gettime(CLOCK_MONOTONIC, &start_time) == 0) || (clock_gettime(CLOCK_REALTIME, &start_time) == 0)) {
    start_recorded = true;
  };
}

void JobPerfLog::LogEnd(const std::string& name, const std::string& id) {
  if(start_recorded) {
    timespec end_time;
    if((clock_gettime(CLOCK_MONOTONIC, &end_time) == 0) || (clock_gettime(CLOCK_REALTIME, &end_time) == 0)) {
      Log(name, id, start_time, end_time);
    };
    start_recorded = false;
  };
}

void JobPerfLog::Log(const std::string& name, const std::string& id, const timespec& start, const timespec& end) {
  if(!log_enabled) return;
  if(log_path.empty()) return;
  std::ofstream logout(log_path.c_str(), std::ofstream::app);
  if(logout.is_open()) {
    uint64_t delta = ((uint64_t)(end.tv_sec-start.tv_sec))*1000000000 + end.tv_nsec - start.tv_nsec;
    logout << Arc::Time().str(Arc::UTCTime) << "\t" << name << "\t" << "id" << "\t" << delta << " nS" << std::endl;
  };
}

} // namespace ARex

