// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheQuartermasterModule.h"

#include "Asset/TheFolderQuarantine.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogTheQuartermaster);

static TAutoConsoleVariable<bool> CVarTheQuartermasterLog(TEXT("the.quartermaster.log"),
    false,
    TEXT("Log the numbers of every TheQuartermaster operation: quarantine moves, restores and deletes, redirects, referencer scans, placement and lint decisions. Off by default; 1 raises LogTheQuartermaster to Verbose."),
    FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Var) { LogTheQuartermaster.SetVerbosity(Var->GetBool() ? ELogVerbosity::Verbose : ELogVerbosity::Log); }));

#define LOCTEXT_NAMESPACE "FTheQuartermasterModule"

void FTheQuartermasterModule::StartupModule()
{
    // Core Redirects do not survive a restart, but the .origin markers on disk do. Re-registering
    // them here is what keeps a quarantined pack's baked-in references resolving across sessions;
    // skip it and every reference into a parked folder breaks on the next editor launch.
    FTheFolderQuarantine::RehydrateRedirects();
}

void FTheQuartermasterModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTheQuartermasterModule, TheQuartermaster)
