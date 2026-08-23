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
     *
     * No default and no search: a guessed taxonomy is worse than none, because it answers confidently
     * with another project's structure and nothing in the answer says so.
     */
    static bool LoadProjectConfig(FThePlacementResolver& OutResolver, FString& OutError);

    /**
     * Where this asset belongs, or what is still missing to say.
     *
     * The order of the refusals is deliberate. A name the context refuses and a prefix a redirect sends
     * elsewhere are answered BEFORE the facts are weighed: the answer is "not here, there", and asking for
     * the facts of a placement that will not happen wastes a round trip on the caller. A name that LOOKS
     * prefixed but carries an unknown prefix is refused outright rather than placed, because guessing a
     * folder for it launders a naming-table violation into the structure.
     *
     * Missing facts are resolved as a SET before any substitution, so the caller gets the whole closed list
     * in one answer instead of discovering it one round trip at a time.
     *
     * The path is assembled segment by segment rather than substituted into the whole template, because a
     * segment can legitimately DISAPPEAR - an optional fact, a value whose whole meaning is that the level
     * does not exist, a `{sub}` that resolves to nothing. Whole-string replacement leaves an empty segment
     * behind as a double slash, and the path then reads as a folder nobody has.
     */
    FThePlacementResult Resolve(const FThePlacementFacts& Facts) const;

    /**
     * The inverse of Resolve: given a package path, which rule does it satisfy and what are its
     * facts. Refuses rather than fitting a path to the nearest rule - a path that matches nothing
     * is a finding, and rounding it to a plausible rule would erase exactly that finding.
     *
     * Only whole-segment placeholders are read back ({family}, not Ch{chapter}); a rule whose
     * template mixes literal text and a placeholder inside one segment is reported as inexplicable
     * instead of being matched approximately.
     *
     * A rule is skipped before it is even tried when Resolve would not have produced this package through
     * it: a context that demands a prefix cannot account for a name carrying none, a name the rule refuses
     * is not a name it explains, and a prefix a redirect sends elsewhere does not belong here either -
     * without those three, `Maps/L_RoadLoop` would read back as a legal map that Resolve refuses to place.
     *
     * Among the rules that DO match, more literal segments means the template pinned more of the path down,
     * so it is the more specific account of it. Equal specificity is a genuine ambiguity in the taxonomy and
     * is reported as one rather than settled by rule order.
     */
    FThePathExplanation Explain(const FString& PackagePath) const;

    /** The loaded taxonomy, for a caller that asks what the structure IS. */
    FThePlacementStructure Describe() const;

    const TArray<FString>& GetContexts() const { return Contexts; }
    const FString& GetContentRoot() const { return ContentRoot; }

private:
    /**
     * What a rule allows ONE fact's value to be. Every field is optional, and a rule that declares none
     * behaves exactly as it did before they existed - which is what keeps a client project's config
     * working when the schema grows.
     */
    struct FFactConstraint
    {
        TArray<FString> OneOf;
        TArray<FString> Excluded;
        FString Pattern;
        /** Values naming the ABSENCE of a level: the segment disappears instead of being written. */
        TMap<FString, FString> Collapse;
        /**
         * The MOST folder segments this fact may cover when a path is read back. One by default: a fact
         * that silently swallowed several segments re-attributes a whole subtree, and an unbounded one
         * fits any depth at all - a rule that can no longer refuse anything.
         */
        int32 Spans = 1;
        FString Why;
    };

    /**
     * A `{sub}` segment resolved from the asset's own name rather than from a supplied fact. An empty
     * Default is load-bearing: it is how a folder that holds its own root files AND kind subfolders is
     * expressed without a special case in code.
     */
    struct FSubSpec
    {
        bool bDeclared = false;
        bool bByPrefix = false;
        bool bUseKindDirectories = false;
        TMap<FString, FString> Map;
        FString Default;
        TArray<FString> RootPrefixes;
    };

    struct FNameRefusal
    {
        FString StartsWith;
        FString Why;
    };

    struct FRule
    {
        FString Context;
        FString Folder;
        TArray<FString> Requires;
        TArray<FString> OptionalFacts;
        TMap<FString, FFactConstraint> Facts;
        FSubSpec Sub;
        TArray<FNameRefusal> NameRefuses;
    };

    /** A name that is right for the project and wrong for the context it was offered in. */
    struct FRedirect
    {
        FString Context;
        FString Prefix;
        FString To;
        FString Why;
    };

    /** Longest known prefix wins, so PHYS_ beats a shorter false match. Empty when none applies. */
    FString ParsePrefix(const FString& Name) const;

    /**
     * Whether one rule accounts for these folder segments, filling OutFacts when it does.
     *
     * OutLiteralSegments is how specific the match is, and it is counted from the TEMPLATE rather than from
     * the match: a segment that legitimately disappeared did not make the rule any less specific, and a
     * route naming Buildings/Kits must still beat the generic library route it sits inside.
     */
    bool RuleAccountsForFolder(const FRule& Rule, const TArray<FString>& FolderSegments, const FString& Prefix, const FString& Kind, TMap<FString, FString>& OutFacts, int32& OutLiteralSegments) const;

    /**
     * The `{sub}` value for this name under this rule, and whether the rule declares one at all. An empty
     * value with bOutDeclared true means the level does not exist for this asset and its segment is skipped.
     */
    FString SubFor(const FRule& Rule, const FString& Prefix, const FString& Kind, bool& bOutDeclared) const;

    /** The first constraint this rule puts on the supplied facts that the values do not satisfy. */
    FString RefusalByFacts(const FRule& Rule, const TMap<FString, FString>& Values) const;

    /**
     * Walks a rule's template against a path's segments, backtracking so a fact declared `spans: many` can
     * cover several of them. A fact covers exactly ONE segment unless it says otherwise: a rule written
     * before mixed depth existed must keep reading the paths it always read.
     */
    bool MatchTemplateSegments(const FRule& Rule, const TArray<FString>& Template, int32 TemplateAt, const TArray<FString>& Folder, int32 FolderAt, const FString& Sub, const FString& Kind, TMap<FString, FString>& OutFacts) const;

    static bool ReadFacts(const TSharedPtr<class FJsonObject>& Object, FRule& Rule, FString& OutError);
    static bool ReadSub(const TSharedPtr<class FJsonObject>& Object, FRule& Rule, FString& OutError);
    static void ReadNameRefusals(const TSharedPtr<class FJsonObject>& Object, FRule& Rule);

    FString ContentRoot = TEXT("/Game");
    TMap<FString, FString> PrefixKinds;
    TMap<FString, FString> KindDirectories;
    TArray<FString> Contexts;
    TArray<FRule> Rules;
    TArray<FString> UnprefixedContexts;
    TArray<FRedirect> Redirects;
};
