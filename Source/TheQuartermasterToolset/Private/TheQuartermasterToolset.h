// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "ToolsetRegistry/ToolsetDefinition.h"

#include "TheQuartermasterToolset.generated.h"

/**
 * One placement rule as the project's taxonomy declares it.
 */
USTRUCT(BlueprintType)
struct FTheStructureRule
{
    GENERATED_BODY()

    /** The context this rule answers for, e.g. "library", "map", "shared". */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Context;

    /** The folder template below the content root, e.g. "Assets/{geography}/{category}/{family}". */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString FolderTemplate;

    /** The facts a caller must supply for this rule to place anything. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    TArray<FString> RequiredFacts;

    /** False for contexts whose assets carry no name prefix, e.g. maps. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    bool bRequiresNamePrefix = true;
};

/**
 * How a project is laid out, as data rather than prose.
 */
USTRUCT(BlueprintType)
struct FTheStructureDescription
{
    GENERATED_BODY()

    /** The mount point every rule is relative to, e.g. "/Game". */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString ContentRoot;

    /** Every context the taxonomy declares. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    TArray<FString> Contexts;

    /** Asset-name prefix to resource kind, e.g. "SM" to "mesh". */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    TMap<FString, FString> PrefixKinds;

    /** Resource kind to the directory it lives in, e.g. "mesh" to "Meshes". */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    TMap<FString, FString> KindDirectories;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    TArray<FTheStructureRule> Rules;
};

/**
 * Where an asset belongs, or what is still missing to decide.
 */
USTRUCT(BlueprintType)
struct FThePlacementAnswer
{
    GENERATED_BODY()

    /** "placed", "needs-facts", or "rejected". */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Outcome;

    /** The full package path when placed, e.g. "/Game/Characters/Hero/BP_Hero". */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString PackagePath;

    /** The folder below the content root when placed. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Folder;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString NamePrefix;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Kind;

    /** On "needs-facts", the complete list of what is still required. Supply all of them and ask again. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    TArray<FString> MissingFacts;

    /** On "rejected", why. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Reason;
};

/**
 * What an existing path means in the project's taxonomy.
 */
USTRUCT(BlueprintType)
struct FThePathAccount
{
    GENERATED_BODY()

    /** "matched", "unmatched", or "ambiguous". */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Outcome;

    /** The context whose rule the path satisfies. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Context;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString FolderTemplate;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Folder;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString AssetName;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString NamePrefix;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Kind;

    /** The template's placeholders as this path fills them, e.g. "family" to "Tower01". */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    TMap<FString, FString> Facts;

    /**
     * On "ambiguous", every context that fits equally. On "unmatched", the contexts whose rule
     * shares this path's first folder segment - a diagnosis, never a suggested answer.
     */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    TArray<FString> CandidateContexts;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Reason;
};

/**
 * One structural violation found in the dependency graph.
 */
USTRUCT(BlueprintType)
struct FTheLintFindingReport
{
    GENERATED_BODY()

    /** The rule broken, e.g. "shared-depends-on-family". */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Rule;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Package;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Detail;

    /** "error" or "warning", as the project's config declares for that rule. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Severity;
};

/**
 * The structural state of a content library.
 *
 * The counts stay complete even when the findings list is cut: a truncated list that also truncated
 * the totals would read as a cleaner project than it is.
 */
USTRUCT(BlueprintType)
struct FTheLintReport
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 PackagesChecked = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 Errors = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 Warnings = 0;

    /** The findings themselves, truncated when the caller asked for a limit. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    TArray<FTheLintFindingReport> Findings;

    /** How many findings the limit left out. Zero means the list above is complete. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 Omitted = 0;
};

/**
 * One top-level content folder, sized and weighed as a unit of work.
 */
USTRUCT(BlueprintType)
struct FTheRootReport
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Root;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 Packages = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int64 Bytes = 0;

    /** Packages outside this root that point into it: what moving it would have to rewrite. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 InboundReferencers = 0;

    /** Packages outside this root that it needs: what extracting it would drag along. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 OutboundDependencies = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 ExternalActors = 0;

    /** No inbound edges at all: movable or deletable without rewriting anything. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    bool bIsIsland = false;
};

/**
 * The work left in a project, partitioned by top-level folder.
 */
USTRUCT(BlueprintType)
struct FTheRootSurveyReport
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    TArray<FTheRootReport> Roots;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 TotalPackages = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 TotalExternalActors = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int64 TotalBytes = 0;
};

/**
 * A top-level folder the holders of a candidate set live under.
 */
USTRUCT(BlueprintType)
struct FTheHoldingRoot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Root;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 Holders = 0;
};

/**
 * What a candidate set can safely become. Sizes and partitions work; authorizes nothing.
 */
USTRUCT(BlueprintType)
struct FTheClosureReport
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 Candidates = 0;

    /** Referencer-closed: nothing outside the set points into it. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 Safe = 0;

    /** Candidates something outside still holds alive. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 Held = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 FixpointRounds = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 ExternalActorsExcluded = 0;

    /** Where the holders live, most frequent first. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    TArray<FTheHoldingRoot> HoldingRoots;

    /** The file the full package lists were written to, empty when the caller asked for none. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString ReportPath;
};

/**
 * A path named in product text that resolves to nothing.
 */
USTRUCT(BlueprintType)
struct FTheUnresolvedPath
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Path;

    /** The product files that name it. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    TArray<FString> Files;
};

/**
 * References the Asset Registry structurally cannot see, found by text.
 */
USTRUCT(BlueprintType)
struct FTheTextScanReport
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 FilesScanned = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 TokensSeen = 0;

    /** Packages held alive by a mention in product text rather than by a dependency edge. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 ReferencedPackages = 0;

    /** Tokens naming neither an existing package nor a folder that still holds packages. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    TArray<FTheUnresolvedPath> UnresolvedPaths;

    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    int32 Omitted = 0;
};

/**
 * The outcome of a quarantine move.
 */
USTRUCT(BlueprintType)
struct FTheQuarantineReport
{
    GENERATED_BODY()

    /** What was moved, restored, deleted or checked, in the operation's own words. */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    FString Report;

    /**
     * True when the move completed but this editor process still held one of the packages, so its
     * view of the world is now stale. Not a failure; a reason to restart before trusting it.
     */
    UPROPERTY(BlueprintReadWrite, Category = "Quartermaster")
    bool bNeedsRestart = false;
};

/**
 * Takes over a neglected Unreal project: says how the project is meant to be laid out, where a given
 * asset belongs, what an existing path already means, which packages are safe to delete, and what
 * structural rules the content breaks. Every answer comes from the project's own taxonomy file, so
 * the same questions get the same answers here, in a commandlet, and in a person's head.
 *
 * These tools read and report. The one exception is quarantine, which moves files on disk and is
 * reversible by construction; call it only on explicit instruction.
 */
UCLASS(BlueprintType, Hidden)
class UTheQuartermasterToolset : public UToolsetDefinition
{
    GENERATED_BODY()

public:
    /**
     * Returns how this project is laid out: its content root, the contexts an asset can belong to,
     * the name-prefix table, and the folder rule for each context. Ask this first - the rules name
     * the exact facts PlaceAsset needs, so there is nothing to guess afterwards.
     */
    UFUNCTION(meta = (AICallable), Category = "Quartermaster")
    static FTheStructureDescription DescribeStructure();

    /**
     * Returns where an asset belongs under this project's rules.
     * @param AssetName The asset's name including its prefix, e.g. "SM_Tower01".
     * @param Context One of the contexts DescribeStructure returns, e.g. "library".
     * @param Facts Ownership facts as "name=value" entries, e.g. "geography=City". The rule for the
     *   context names which ones it needs.
     * @return "placed" with the package path, "needs-facts" with the complete list of what is still
     *   missing, or "rejected" with the reason. It never guesses a folder.
     */
    UFUNCTION(meta = (AICallable), Category = "Quartermaster")
    static FThePlacementAnswer PlaceAsset(const FString& AssetName, const FString& Context, const TArray<FString>& Facts);

    /**
     * Returns what an existing package path means: which rule it satisfies and what its folder
     * segments stand for. This is the inverse of PlaceAsset and the way to audit content that is
     * already in place.
     * @param PackagePath A full package path, e.g. "/Game/Assets/City/Buildings/Tower01/SM_Tower01".
     * @return "matched" with the context and the recovered facts, "ambiguous" when the taxonomy
     *   lets two rules fit equally, or "unmatched" when no rule accounts for the path. An
     *   unmatched path is a finding about the content, not an error.
     */
    UFUNCTION(meta = (AICallable), Category = "Quartermaster")
    static FThePathAccount ExplainPath(const FString& PackagePath);

    /**
     * Checks the content library against the structural rules only a dependency graph can see: a
     * shared resource reaching into one family's private assets, a reference into an assembly's
     * internals, a forbidden folder prefix.
     * @param MaxFindings How many findings to return; 0 returns all of them. The counts are always
     *   complete regardless.
     */
    UFUNCTION(meta = (AICallable), Category = "Quartermaster")
    static FTheLintReport LintStructure(int32 MaxFindings = 100);

    /**
     * Surveys every top-level folder under the content root and reports how big each one is and how
     * entangled it is with the rest. Use it to size migration work and pick where to cut a batch.
     * @param ExcludeRoots Top-level folder names to leave out, e.g. "Maps".
     */
    UFUNCTION(meta = (AICallable), Category = "Quartermaster")
    static FTheRootSurveyReport SurveyRoots(const TArray<FString>& ExcludeRoots);

    /**
     * Returns the greatest subset of the given folders that nothing outside them still points at -
     * the part that could be deleted without breaking a reference.
     * @param Roots Package paths to consider, e.g. "/Game/SomePack". Recursive.
     * @param bIncludeTextReferences Treat a package named in an ini or a C++ literal as held from
     *   outside. Leave this on: such a mention is a real use that carries no dependency edge.
     * @param ReportFilePath Where to write the full package lists. Optional; the returned counts
     *   stand on their own.
     * @return Counts and where the holders live. This authorizes nothing - the registry reflects the
     *   last save, so any lane that deletes re-derives its own closure immediately before applying.
     */
    UFUNCTION(meta = (AICallable), Category = "Quartermaster")
    static FTheClosureReport SolveClosure(const TArray<FString>& Roots, bool bIncludeTextReferences = true, const FString& ReportFilePath = TEXT(""));

    /**
     * Scans the project's product text for package paths the Asset Registry cannot see: paths in
     * ini files, C++ literals and data tables. Reports the ones that resolve to nothing.
     * @param MaxUnresolved How many unresolved paths to return; 0 returns all of them.
     */
    UFUNCTION(meta = (AICallable), Category = "Quartermaster")
    static FTheTextScanReport ScanTextReferences(int32 MaxUnresolved = 100);

    /**
     * Parks a folder aside: unloads its packages, moves the files out of the content tree, and
     * registers one redirect so references baked into the pack still resolve. Nothing else in the
     * project is rewritten, which is what makes this reversible.
     * This MUTATES files on disk. Call it only after explicit direction from the user.
     * @param SourceFolder The folder to park, e.g. "/Game/SomePack".
     */
    UFUNCTION(meta = (AICallable), Category = "Quartermaster")
    static FTheQuarantineReport QuarantineFolder(const FString& SourceFolder);

    /**
     * Moves a quarantined folder back to where it came from, read from the marker left at
     * quarantine time.
     * This MUTATES files on disk. Call it only after explicit direction from the user.
     * @param Folder The parked folder, e.g. "/Game/SomePack".
     */
    UFUNCTION(meta = (AICallable), Category = "Quartermaster")
    static FTheQuarantineReport RestoreQuarantinedFolder(const FString& Folder);

    /**
     * Deletes a quarantined folder for good.
     * This DESTROYS files. Call it only after explicit direction from the user, and only once the
     * project has been verified to work without the folder.
     * Refuses while anything outside the quarantine still references the folder, naming what does.
     * @param Folder The parked folder, e.g. "/Game/SomePack".
     * @param bForce Delete even then, accepting the dangling references it leaves behind.
     */
    UFUNCTION(meta = (AICallable), Category = "Quartermaster")
    static FTheQuarantineReport DeleteQuarantinedFolder(const FString& Folder, bool bForce = false);

    /**
     * Checks that a folder really is parked: gone from the content tree, present under the
     * quarantine root, with its origin marker intact.
     * @param SourceFolder The folder's original path, e.g. "/Game/SomePack".
     */
    UFUNCTION(meta = (AICallable), Category = "Quartermaster")
    static FTheQuarantineReport VerifyQuarantined(const FString& SourceFolder);

    /**
     * Checks that a folder really came back: present at its original path, gone from quarantine.
     * @param SourceFolder The folder's original path, e.g. "/Game/SomePack".
     */
    UFUNCTION(meta = (AICallable), Category = "Quartermaster")
    static FTheQuarantineReport VerifyRestored(const FString& SourceFolder);
};
