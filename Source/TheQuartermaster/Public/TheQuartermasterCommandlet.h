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
 *   -run=TheQuartermaster -DeleteQuarantined=/Game/_Quarantine/Pack [-Force]
 *   -run=TheQuartermaster -VerifyQuarantined=/Game/Pack
 *   -run=TheQuartermaster -VerifyRestored=/Game/Pack
 *
 * What each verdict means, since the exit code is the whole answer a lane gets:
 *
 * - A quarantine or restore that needs a restart SUCCEEDS and warns. The move itself completed; a
 *   package the editor still held only means this process's view of the world is stale.
 * - A delete REFUSES while anything outside the quarantine still references the folder, naming the
 *   referencers. -Force deletes anyway, accepting the dangling imports that leaves.
 * - The lint fails closed on the rules the project enforces. A warning rule is still reported on every
 *   run and counted in the marker, so lowering a severity records a decision instead of hiding one —
 *   the config, not this commandlet, says which rules the project has chosen to live with. The lint
 *   block lives in the same taxonomy file as the placement rules and answers from the same setting;
 *   -Config= stays for a lane checking a file the project has not adopted.
 * - The closure run has no default -Out on purpose: a guessed location is how a planning artifact ends
 *   up somewhere a project clean deletes it, and the run is expensive enough to be worth naming.
 * - An unmatched path in an explain run is a finding about the CONTENT, not a failure of the tool, so
 *   it reports as a verdict rather than an error exit — a lane sweeping a library expects to collect
 *   these.
 */
UCLASS()
class THEQUARTERMASTER_API UTheQuartermasterCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
