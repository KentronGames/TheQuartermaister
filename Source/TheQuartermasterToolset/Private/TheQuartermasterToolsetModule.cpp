// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheQuartermasterToolset.h"

#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

/**
 * The whole module: register the toolset class, unregister it on shutdown.
 *
 * Everything the tools do lives in TheQuartermaster and is reachable from the commandlet too. That
 * is the point of the split - this module can be absent, and the operations are unchanged.
 */
class FTheQuartermasterToolsetModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        UToolsetRegistry::RegisterToolsetClass(UTheQuartermasterToolset::StaticClass());
    }

    virtual void ShutdownModule() override
    {
        UToolsetRegistry::UnregisterToolsetClass(UTheQuartermasterToolset::StaticClass());
    }
};

IMPLEMENT_MODULE(FTheQuartermasterToolsetModule, TheQuartermasterToolset)
