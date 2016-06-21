#ifndef GRID_MANAGER_RUN_PARALLEL_H
#define GRID_MANAGER_RUN_PARALLEL_H

#include <arc/Run.h>

#include "../jobs/JobsList.h"
#include "RunPlugin.h"

namespace ARex {

/// Run child process in parallel with stderr redirected to job.jobid.errors
class RunParallel {
 private:
  RunParallel(const char* procid, const char* errlog,
              RunPlugin* cred, RunPlugin::substitute_t subst, void* subst_arg)
    :procid_(procid?procid:""), errlog_(errlog?errlog:""),
     cred_(cred), subst_(subst), subst_arg_(subst_arg) { };
  ~RunParallel(void) { };
  std::string procid_;
  std::string errlog_;
  RunPlugin* cred_;
  RunPlugin::substitute_t subst_;
  void* subst_arg_;
  static void initializer(void* arg);
  // TODO: no static variables
  static void (*kicker_func_)(void*);
  static void* kicker_arg_;
  operator bool(void) { return true; };
  bool operator!(void) { return false; };
  static bool run(const GMConfig& config, const Arc::User& user,
                  const char* procid, const char* errlog,
                  const std::string& args, Arc::Run**,
                  const char* job_proxy, bool su = true,
                  RunPlugin* cred = NULL, RunPlugin::substitute_t subst = NULL, void* subst_arg = NULL);
 public:
  static bool run(const GMConfig& config, const GMJob& job,
                  const std::string& args, Arc::Run**,
                  bool su = true);
  static void kicker(void (*kicker_func)(void*),void* kicker_arg) {
    kicker_arg_=kicker_arg;
    kicker_func_=kicker_func;
  };
};

} // namespace ARex

#endif
