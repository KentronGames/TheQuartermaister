// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheQuartermasterModule.h"

#include "Asset/TheFolderQuarantine.h"

DEFINE_LOG_CATEGORY(LogTheQuartermaster);

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
