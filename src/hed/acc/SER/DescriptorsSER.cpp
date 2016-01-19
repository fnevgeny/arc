// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/compute/EndpointQueryingStatus.h>
#include <arc/loader/Plugin.h>

#include "ServiceEndpointRetrieverPluginEMIR.h"

extern Arc::PluginDescriptor const ARC_PLUGINS_TABLE_NAME[] = {
  { "EMIR", "HED:ServiceEndpointRetrieverPlugin", "EMIR registry", 0, &Arc::ServiceEndpointRetrieverPluginEMIR::Instance },
  { NULL, NULL, NULL, 0, NULL }
};
