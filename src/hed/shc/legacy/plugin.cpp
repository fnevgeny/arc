#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/loader/Plugin.h>

#include "LegacySecHandler.h"
#include "LegacyPDP.h"

Arc::PluginDescriptor PLUGINS_TABLE_NAME[] = {
    { "arclegacy.handler", "HED:SHC", 0,
                  &ArcSHCLegacy::LegacySecHandler::get_sechandler},
    { "arclegacy.pdp", "HED:PDP", 0,
                  &ArcSHCLegacy::LegacyPDP::get_pdp}
};

