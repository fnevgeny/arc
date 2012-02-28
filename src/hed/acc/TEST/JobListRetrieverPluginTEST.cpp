// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/client/TestACCControl.h>

#include "JobListRetrieverPluginTEST.h"

namespace Arc {

EndpointQueryingStatus JobListRetrieverPluginTEST::Query(const UserConfig&,
                                                          const ComputingInfoEndpoint&,
                                                          std::list<Job>& jobs,
                                                          const EndpointQueryOptions<Job>&) const {
  Glib::usleep(JobListRetrieverPluginTESTControl::delay*1000000);
  jobs = JobListRetrieverPluginTESTControl::jobs;
  return JobListRetrieverPluginTESTControl::status;
};


}
