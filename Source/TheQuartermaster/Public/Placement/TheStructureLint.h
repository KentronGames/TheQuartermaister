// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Structural rules that only a dependency graph can check.
 *
 * A naming or folder-shape rule can be enforced by reading paths, and most linters stop there. The
 * rules that actually decay a library are directional: a shared resource quietly starts depending
 * on one object's private family, or two families grow a reference between them. After that the
 * "shared" folder is no longer shared and the families are no longer extractable, and no amount of
 * path inspection notices.
 *
 * Severity is configuration, not a property of the code. A rule the project has decided to live
 * with must still be reported, and a rule it enforces must still stop a commit; hard-coding either
 * one produces a lint that is edited instead of obeyed.
 */
enum class ETheLintSeverity : uint8
{
    Warning,
    Error,
};

struct FTheLintFinding
{
    FString Rule;
    FString Package;
    FString Detail;
    ETheLintSeverity Severity = ETheLintSeverity::Error;
};

struct FTheLintResult
{
    TArray<FTheLintFinding> Findings;
    int32 PackagesChecked = 0;

    int32 CountOf(ETheLintSeverity Severity) const
    {
        int32 Count = 0;
        for(const FTheLintFinding& Finding : Findings)
        {
            Count += Finding.Severity == Severity ? 1 : 0;
        }
        return Count;
    }
};

struct FTheLintConfig
{
    /** Folder holding resources no single object owns, e.g. Assets/Shared. */
    FString SharedFolder = TEXT("Assets/Shared");

    /** Root under which families live, e.g. Assets. */
    FString FamilyRoot = TEXT("Assets");

    /**
     * Which path segments below FamilyRoot name one family, as slash-separated segment patterns
     * where a lone asterisk matches any single segment. A flat library is one asterisk
     * (Assets/Tower01); a table that mixes depths lists a three-segment route for environment packs
     * next to a four-segment one for building kits. The first matching pattern wins, so list the
     * specific route before the general one.
     *
     * A single depth cannot express a destination table that mixes them: with one number, every
     * package on a deeper route has a category container named as its family, and every reference
     * inside that container looks intra-family. Patterns are what the placement rules already are.
     */
    TArray<FString> FamilyPatterns;

    /**
     * Families whose whole purpose is composing other families, in the same pattern form as
     * FamilyPatterns: a building kit is assembled from wall, door and roof parts, so its outgoing
     * family references are the artifact working as intended, not decay.
     *
     * The rule this replaces is symmetric, and that is what made it wrong here. Direction is what
     * matters: an assembly may reach into families, but nothing may reach into an assembly - a
     * reference into a kit's internals means the kit can no longer be changed as one thing.
     */
    TArray<FString> AssemblyPatterns;

    /** Folder name prefixes that are never valid, e.g. a leading underscore. */
    TArray<FString> ForbiddenFolderPrefixes;

    /** Per-rule severity; a rule absent from the map is an error. */
    TMap<FString, ETheLintSeverity> RuleSeverity;

    /**
     * Cross-family dependencies the project has reviewed and accepted, as `<from> -> <to>` family
     * pairs. An acknowledged pair is not reported at all — the register is the record, so an
     * entry has to be added deliberately rather than accumulating as noise nobody reads.
     */
    TSet<FString> AcknowledgedCrossFamily;

    FString ContentRoot = TEXT("/Game");
};

class THEQUARTERMASTER_API FTheStructureLint
{
public:
    static bool LoadConfig(const FString& FilePath, FTheLintConfig& OutConfig, FString& OutError);

    static bool Run(const FTheLintConfig& Config, FTheLintResult& OutResult, FString& OutError);

    /**
     * The family owning a package, or empty when the package is outside the family root, inside the
     * shared folder, or matches no route. Public because it is the whole judgement the lint makes
     * and the only part testable without a populated registry.
     */
    static FString FamilyOf(const FString& PackageName, const FTheLintConfig& Config);

    /** Whether a family, as returned by FamilyOf, is declared a composition of other families. */
    static bool IsAssembly(const FString& Family, const FTheLintConfig& Config);
};
