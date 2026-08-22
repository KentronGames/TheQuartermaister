// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Where an asset belongs, as a function rather than a judgement.
 *
 * Placement is deterministic: the folder follows from (prefix, context, ownership facts). Prose in
 * a style document has to be read and weighed every time; this decides the same question the same
 * way twice, and refuses when it cannot.
 *
 * The taxonomy itself is DATA. A resolver that switches on context names in code is how a tool ends
 * up welded to the project it was written for - the working prototype in TheGame is exactly that.
 * Here every root, prefix, context and rule comes from a config file, so a new client's structure
 * is a new file rather than a new build.
 *
 * Three outcomes, and the middle one matters most: placed, or a closed list of the facts still
 * needed to decide, or a rejection. Never a guess.
 */
enum class EThePlacementOutcome : uint8
{
    Placed,
    NeedsFacts,
    Rejected
};

struct FThePlacementFacts
{
    /** Asset name, e.g. SM_RoadTile. Carries the prefix. */
    FString Name;

    /** One of the contexts the config declares. */
    FString Context;

    /** Ownership facts a rule may interpolate: entity, category, owner, map, ... */
    TMap<FString, FString> Values;
};

struct FThePlacementResult
{
    EThePlacementOutcome Outcome = EThePlacementOutcome::Rejected;

    /** Folder below the content root, e.g. Characters/Hero. */
    FString Folder;

    /** Full package path, e.g. /Game/Characters/Hero/BP_Hero. */
    FString PackagePath;

    FString Prefix;
    FString Kind;

    /** Facts the matched rule needs and the caller did not supply. Closed question, never advice. */
    TArray<FString> MissingFacts;

    FString Error;
};

/**
 * What reading an existing path yields, and it is deliberately not a placement outcome.
 *
 * Placement can answer "not yet, give me these facts"; reading cannot - a path either satisfies a
 * rule or it does not. Ambiguous is its own answer rather than a tie broken by order, because a
 * taxonomy in which one path satisfies two rules equally is a defect in the taxonomy, and picking
 * a winner would hide it.
 */
enum class ETheExplanationOutcome : uint8
{
    Matched,
    Unmatched,
    Ambiguous
};

struct FThePathExplanation
{
    ETheExplanationOutcome Outcome = ETheExplanationOutcome::Unmatched;

    /** The context whose rule the path satisfies. */
    FString Context;

    /** That rule's folder template, e.g. Assets/{geography}/{category}/{family}. */
    FString FolderTemplate;

    /** The folder as it actually is, below the content root. */
    FString Folder;

    FString Name;
    FString Prefix;
    FString Kind;

    /** The template's placeholders filled in from the path: geography, category, family, ... */
    TMap<FString, FString> Facts;

    /**
     * On Ambiguous, every context that matched equally well. On Unmatched, the contexts whose
     * template shares this path's first folder segment - a diagnosis of the refusal, never a
     * substitute for it.
     */
    TArray<FString> CandidateContexts;

    FString Error;
};

/** One rule as the config declares it, for a caller that needs the structure rather than a verdict. */
struct FThePlacementRuleInfo
{
    FString Context;
    FString FolderTemplate;
    TArray<FString> Requires;

    /** False for the contexts the config lists as unprefixed, e.g. maps. */
    bool bRequiresPrefix = true;
};

/**
 * The whole taxonomy as data. This is what makes the config the source of truth rather than a
 * document: a caller with no access to the file can still be told how the project is laid out.
 */
struct FThePlacementStructure
{
    FString ContentRoot;
    TArray<FString> Contexts;

    /** Name prefix -> resource kind, e.g. SM -> mesh. */
    TMap<FString, FString> PrefixKinds;

    /** Resource kind -> directory name, e.g. mesh -> Meshes. */
    TMap<FString, FString> KindDirectories;

    TArray<FThePlacementRuleInfo> Rules;
};

class THEQUARTERMASTER_API FThePlacementResolver
{
public:
    bool LoadConfig(const FString& FilePath, FString& OutError);

    /** Parses config from a JSON string. Used by the tests and by callers holding config in memory. */
    bool LoadConfigFromString(const FString& Json, FString& OutError);

    /**
     * Loads the taxonomy the project settings name. Both entry points go through here, so neither
     * one can be pointed at a different file than the other by accident.
     */
    static bool LoadProjectConfig(FThePlacementResolver& OutResolver, FString& OutError);

    FThePlacementResult Resolve(const FThePlacementFacts& Facts) const;

    /**
     * The inverse of Resolve: given a package path, which rule does it satisfy and what are its
     * facts. Refuses rather than fitting a path to the nearest rule - a path that matches nothing
     * is a finding, and rounding it to a plausible rule would erase exactly that finding.
     *
     * Only whole-segment placeholders are read back ({family}, not Ch{chapter}); a rule whose
     * template mixes literal text and a placeholder inside one segment is reported as inexplicable
     * instead of being matched approximately.
     */
    FThePathExplanation Explain(const FString& PackagePath) const;

    /** The loaded taxonomy, for a caller that asks what the structure IS. */
    FThePlacementStructure Describe() const;

    const TArray<FString>& GetContexts() const { return Contexts; }
    const FString& GetContentRoot() const { return ContentRoot; }

private:
    struct FRule
    {
        FString Context;
        FString Folder;
        TArray<FString> Requires;
    };

    /** Longest known prefix wins, so PHYS_ beats a shorter false match. Empty when none applies. */
    FString ParsePrefix(const FString& Name) const;

    /**
     * Whether one rule accounts for these folder segments, filling OutFacts when it does.
     * OutLiteralSegments is how specific the match is - the count of segments the template pinned
     * down as literal text - and is what separates a genuine tie from a more specific winner.
     */
    bool RuleAccountsForFolder(const FRule& Rule, const TArray<FString>& FolderSegments, const FString& Kind,
        TMap<FString, FString>& OutFacts, int32& OutLiteralSegments) const;

    FString ContentRoot = TEXT("/Game");
    TMap<FString, FString> PrefixKinds;
    TMap<FString, FString> KindDirectories;
    TArray<FString> Contexts;
    TArray<FRule> Rules;
    TArray<FString> UnprefixedContexts;
};
