// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Parks a pack folder aside without the engine's rename path: unload the folder's packages, move
 * the files on disk, and register one prefix Core Redirect so the pack's baked-in /Game/<Pack>/
 * references still resolve. That is what makes quarantine cheap enough to be reversible — nothing
 * in the project is rewritten, so restoring is the same move backwards.
 */
class THEQUARTERMASTER_API FTheFolderQuarantine
{
public:
    /** Configured root, normalised. Not a constant: the destination is a per-project decision. */
    static FString QuarantineRoot();

    static bool MoveToQuarantine(const FString& SourceFolder, FString& OutReport, bool& OutNeedsRestart, FString& OutError);

    static bool DeleteFromQuarantine(const FString& Folder, FString& OutReport, FString& OutError);

    static bool RestoreFromQuarantine(const FString& Folder, FString& OutReport, bool& OutNeedsRestart, FString& OutError);

    static bool DropQuarantineBookkeepingAfterPackagesLeft(const FString& Folder, FString& OutReport);

    static bool HoldsNothingButTheOriginMarker(const FString& Folder);

    static bool VerifyQuarantined(const FString& SourceFolder, FString& OutReport, FString& OutError);

    static bool VerifyRestoredFromQuarantine(const FString& SourceFolder, FString& OutReport, FString& OutError);

    static bool VerifyQuarantineDeleted(const FString& Folder, FString& OutReport, FString& OutError);

    /** Re-registers redirects from the .origin markers on disk. Redirects do not survive a restart. */
    static void RehydrateRedirects();
};
