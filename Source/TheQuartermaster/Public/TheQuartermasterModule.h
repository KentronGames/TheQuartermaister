// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

THEQUARTERMASTER_API DECLARE_LOG_CATEGORY_EXTERN(LogTheQuartermaster, Log, All);

class FTheQuartermasterModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
