// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Parks a pack folder aside without the engine's rename path: unload the folder's packages, move
 * the files on disk, and register one prefix Core Redirect so the pack's baked-in /Game/<Pack>/
 * references still resolve. That is what makes quarantine cheap enough to be reversible — nothing
 * in the project is rewritten, so restoring is the same move backwards.
 *
 * Moving the directory whole is tried first and is not relied on: it fails across volumes and on
 * partially-locked trees, so the fallback moves file by file and rolls back on the first failure.
 * A half-moved folder is the outcome worth extra code to avoid - neither path would then hold a
 * working pack.
 */
class THEQUARTERMASTER_API FTheFolderQuarantine
{
public:
    /** Configured root, normalised. Not a constant: the destination is a per-project decision. */
    static FString QuarantineRoot();

    /**
     * Parks the folder and records where it came from. The .origin marker is what lets the redirect
     * be rebuilt after a restart, so a move that cannot write it is rolled back rather than left:
     * a folder parked without a marker silently breaks every reference into it.
     */
    static bool MoveToQuarantine(const FString& SourceFolder, FString& OutReport, bool& OutNeedsRestart, FString& OutError);

    /**
     * What MoveToQuarantine would do, writing nothing. Every refusal the real call makes is made
     * here too, so a plan that returns is a move that will land.
     */
    static bool PlanMoveToQuarantine(const FString& SourceFolder, FString& OutReport, FString& OutError);

    /**
     * Removes a parked folder for good. Refuses a folder that is not on disk: a delete that reports
     * success for a path that never existed makes a mistyped path indistinguishable from a real
     * removal, and the caller then believes a pack is gone that is still sitting under its own name.
     *
     * Also refuses while anything OUTSIDE the quarantine still references the folder, naming what does.
     * Quarantined content is not supposed to be referenced, but a project that harvested one asset out
     * of a parked pack breaks that assumption silently: the delete succeeds and leaves the referencing
     * package pointing at nothing. bForce deletes anyway, for when the dangling reference is the
     * intended outcome.
     */
    static bool DeleteFromQuarantine(const FString& Folder, bool bForce, FString& OutReport, FString& OutError);

    /** What DeleteFromQuarantine would remove, writing nothing — including who still references it. */
    static bool PlanDeleteFromQuarantine(const FString& Folder, FString& OutReport, FString& OutError);

    /**
     * Packages outside the quarantine root that reference something under Folder, sorted. Empty means
     * the folder can be deleted without leaving a dangling import behind.
     */
    static TArray<FString> ExternalReferencersOf(const FString& Folder);

    static bool RestoreFromQuarantine(const FString& Folder, FString& OutReport, bool& OutNeedsRestart, FString& OutError);

    static bool DropQuarantineBookkeepingAfterPackagesLeft(const FString& Folder, FString& OutReport);

    static bool HoldsNothingButTheOriginMarker(const FString& Folder);

    static bool VerifyQuarantined(const FString& SourceFolder, FString& OutReport, FString& OutError);

    static bool VerifyRestoredFromQuarantine(const FString& SourceFolder, FString& OutReport, FString& OutError);

    /**
     * Absence proves a deletion only where a deletion could have happened, so this refuses outright
     * when the quarantine root itself is missing from disk - there a green answer would mean nothing
     * beyond the path never having been used.
     */
    static bool VerifyQuarantineDeleted(const FString& Folder, FString& OutReport, FString& OutError);

    /** Re-registers redirects from the .origin markers on disk. Redirects do not survive a restart. */
    static void RehydrateRedirects();
};
