// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Sizes the work left in a project and says where it can be cut.
 *
 * A batch should be cut along the axis of minimal cut and nameable failure: dense inside, few
 * edges outward, and a failure signature a human can recognise by name. Inbound edges are the cost
 * that matters - every package outside a root that points into it is a reference some later move
 * has to rewrite - so a root with few of them is a cheap batch, and one with thousands deserves
 * its own checkpoint.
 *
 * This does not decide anything. It tells the person deciding how big each piece is and what it
 * would drag along.
 */
struct FTheRootProfile
{
    FString Root;
    int32 Packages = 0;
    int64 Bytes = 0;

    /** Packages outside this root that reference into it - the rewrite surface of moving it. */
    int32 InboundReferencers = 0;

    /** Packages outside this root that it depends on - what it would drag along if extracted. */
    int32 OutboundDependencies = 0;

    /** External-actor packages under this root, counted separately and never batched as assets. */
    int32 ExternalActors = 0;

    /** No inbound edges at all: an island, movable or deletable without rewriting anything. */
    bool bIsIsland = false;
};

struct FTheRootPartitionResult
{
    TArray<FTheRootProfile> Roots;
    int32 TotalPackages = 0;
    int32 TotalExternalActors = 0;
    int64 TotalBytes = 0;
};

class THEQUARTERMASTER_API FTheRootPartition
{
public:
    /** Surveys every top-level folder under /Game, minus the ones named in Exclude. */
    static bool Survey(const TArray<FString>& Exclude, FTheRootPartitionResult& OutResult, FString& OutError);

    static bool WriteReport(const FTheRootPartitionResult& Result, const FString& FilePath, FString& OutError);
};
