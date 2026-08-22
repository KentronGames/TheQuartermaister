// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * What a set of candidate packages can safely become, given who still points at it.
 *
 * Deletability is a property of the candidate SET, not of any single package: drop one candidate
 * and it turns into an external referencer of whatever it still uses. So the answer is the greatest
 * referencer-closed subset, reached by shrinking to a fixpoint rather than by filtering once.
 *
 * This never authorizes a mutation. The registry reflects on-disk state at last save, and any
 * project worth cleaning takes concurrent content from somewhere, so a stored result is stale the
 * moment it is written. It sizes and partitions work; the lane that mutates re-derives its own
 * closure immediately before applying.
 */
struct FTheClosureRequest
{
    /** Package paths, e.g. /Game/SomePack. Recursive. */
    TArray<FString> Roots;

    /**
     * Treat a package named in product text as held from outside. On by default: a path in an ini
     * or a C++ literal is a real use that carries no dependency edge, so a closure that ignores it
     * reports packages as free when deleting them would break the project.
     */
    bool bIncludeTextReferences = true;
};

struct FTheClosureResult
{
    TArray<FName> Candidates;

    /** Referencer-closed: nothing outside the set points into it. */
    TArray<FName> Safe;

    /** Candidates held alive from outside, with the referencers that hold them. */
    TArray<FName> Held;
    TMap<FName, TArray<FName>> HeldBy;

    /** Which top-level roots the holders live under, most frequent first. */
    TArray<TPair<FString, int32>> HoldingRoots;

    int32 FixpointRounds = 0;

    /** One File Per Actor packages, counted and excluded: their owner is trivially their map. */
    int32 ExternalActorsExcluded = 0;

    /** Candidates held by product text rather than by a dependency edge, with the files naming them. */
    TMap<FName, TArray<FString>> TextHeldBy;
};

class THEQUARTERMASTER_API FTheDependencyClosure
{
public:
    static bool Solve(const FTheClosureRequest& Request, FTheClosureResult& OutResult, FString& OutError);

    static bool WriteReport(const FTheClosureResult& Result, const FString& FilePath, FString& OutError);

    /** True for One File Per Actor and external-object packages. */
    static bool IsExternalActorPackage(const FString& PackageName);

    /** `/Game/Foo/Bar/Baz` -> `/Game/Foo`. Empty when the name has no top-level folder. */
    static FString TopLevelRootOf(const FString& PackageName);
};
