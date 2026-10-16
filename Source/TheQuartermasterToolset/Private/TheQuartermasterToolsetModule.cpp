// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheQuartermasterToolset.h"

#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

DEFINE_LOG_CATEGORY(LogTheQuartermasterToolset);

static TAutoConsoleVariable<bool> CVarTheQuartermasterToolsetLog(TEXT("the.quartermastertoolset.log"),
    false,
    TEXT("Log the numbers of every TheQuartermaster toolset tool call: arguments, counts and results. Off by default; 1 raises LogTheQuartermasterToolset to Verbose."),
    FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Var) { LogTheQuartermasterToolset.SetVerbosity(Var->GetBool() ? ELogVerbosity::Verbose : ELogVerbosity::Log); }));

/**
 * The whole module: register the toolset class, unregister it on shutdown.
 *
 * Everything the tools do lives in TheQuartermaster and is reachable from the commandlet too. That
 * is the point of the split - this module can be absent, and the operations are unchanged.
 */
class FTheQuartermasterToolsetModule : public IModuleInterface
{
public:
    virtual void StartupModule() override { UToolsetRegistry::RegisterToolsetClass(UTheQuartermasterToolset::StaticClass()); }

    virtual void ShutdownModule() override { UToolsetRegistry::UnregisterToolsetClass(UTheQuartermasterToolset::StaticClass()); }
};

IMPLEMENT_MODULE(FTheQuartermasterToolsetModule, TheQuartermasterToolset)
