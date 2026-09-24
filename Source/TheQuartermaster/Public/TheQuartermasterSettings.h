// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TheQuartermasterSettings.generated.h"

/**
 * Every project-specific path this plugin knows lives here. Code that hardcodes a /Game/ literal
 * is a defect: the plugin is meant to arrive in a client project whose taxonomy it has never seen.
 */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "#The Quartermaster"))
class THEQUARTERMASTER_API UTheQuartermasterSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UTheQuartermasterSettings();

    static const TCHAR* DefaultQuarantineRoot() { return TEXT("/Game/_Quarantine"); }

    /**
     * Where quarantined folders are parked. Must be a /Game/ path and must not be /Game itself.
     * Add it to the project's .gitignore: quarantine is a transient state, never a committed one.
     */
    UPROPERTY(EditAnywhere, config, Category = "Quarantine")
    FString QuarantineRoot;

    /**
     * The configured root, trailing slash stripped. An unusable value falls back to the default
     * rather than failing the operation, because a bad setting must not strand folders that are
     * already parked under the default root.
     */
    static FString ResolvedQuarantineRoot();

    /**
     * The file declaring how this project is laid out: content root, name prefixes, contexts and
     * placement rules. This setting is what makes the structure askable — the project points the
     * plugin at its taxonomy once, and every caller afterwards gets the same answer from the same
     * file instead of reading a style document.
     *
     * A relative path is resolved against the project directory. Ships empty: there is no sensible
     * default, and a guessed taxonomy answers confidently with someone else's structure.
     */
    UPROPERTY(EditAnywhere, config, Category = "Placement", meta = (RelativeToGameDir))
    FString PlacementConfigPath;

    /** The configured taxonomy file as a full path, or empty when the project has named none. */
    static FString ResolvedPlacementConfigPath();
};
