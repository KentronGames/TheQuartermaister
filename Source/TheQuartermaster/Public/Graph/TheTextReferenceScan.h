// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The references the Asset Registry structurally cannot see.
 *
 * A dependency edge exists only for an FObjectPtr or FSoftObjectPath serialised into a package.
 * A path assembled from strings at runtime, declared in an ini, stored as a DataTable row or
 * written as a C++ literal carries no edge at all - and is exactly what a rename or a delete
 * breaks silently. So these are found by text and settled by resolution, not by graph walk.
 *
 * Two outputs, and the second is the one that changes decisions: unresolved paths are defects to
 * report, while resolved ones are *holders*. A package named in a config file is in use no matter
 * what the registry says, so the closure has to treat that mention as an external referencer.
 */
struct FTheTextScanResult
{
    /** Package path -> the product files that name it. Resolved; these hold their target alive. */
    TMap<FString, TArray<FString>> ReferencedPackages;

    /** Tokens naming neither an existing package nor a folder that still holds packages. */
    TMap<FString, TArray<FString>> UnresolvedPaths;

    int32 FilesScanned = 0;
    int32 TokensSeen = 0;
};

class THEQUARTERMASTER_API FTheTextReferenceScan
{
public:
    /** Scans product text under the project directory. Requires a populated Asset Registry. */
    static bool Scan(FTheTextScanResult& OutResult, FString& OutError);
};
