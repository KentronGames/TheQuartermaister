// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "TheQuartermasterCommandlet.generated.h"

/**
 * The headless face of the toolkit. Everything it does is scriptable and leaves a machine-readable
 * marker in the log, because the operations it drives are the ones an automated lane has to prove
 * afterwards, not the ones a human watches happen.
 *
 *   -run=TheQuartermaster -Quarantine=/Game/Pack
 *   -run=TheQuartermaster -Restore=/Game/_Quarantine/Pack
 *   -run=TheQuartermaster -DeleteQuarantined=/Game/_Quarantine/Pack
 *   -run=TheQuartermaster -VerifyQuarantined=/Game/Pack
 *   -run=TheQuartermaster -VerifyRestored=/Game/Pack
 */
UCLASS()
class THEQUARTERMASTER_API UTheQuartermasterCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
