// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class FThePlacementResolver;

/**
 * The resolver asked at the moment of a WRITE rather than after it.
 *
 * A lint over the finished tree finds a misplaced asset only once it exists, and by then the fix is a
 * relocation - the operation that drags references, redirectors and a level's fixup along with it. A tool
 * that is about to create or move a package asks here first and refuses instead, so the misplacement never
 * reaches the disk. Every tool that writes packages calls the same two functions, which is what keeps «the
 * taxonomy explains this path» one answer across the whole tool surface.
 *
 * Both functions load the project's taxonomy on every call. A cached copy would go on answering with the
 * config the editor started with after someone edited it, and the cost of a fresh load is one small JSON
 * file per tool call.
 */
class THEQUARTERMASTER_API FThePlacementGate
{
public:
    /**
     * Empty when every package path is accounted for; otherwise the refusal to hand back to the caller,
     * naming each path no rule explains. Paths under the config's top-level exceptions are not judged, and a
     * package inside a family's kind folder (Meshes/, Materials/, ...) is judged by the family root, the way
     * the tree lint judges it. A taxonomy that does not load refuses everything: a gate that cannot read its
     * rules and waves writes through is the silent pass it exists to prevent.
     */
    static FString Refusal(const TArray<FString>& PackagePaths);

    /**
     * The name an asset of this class carries here - the class's prefix by kind, found by walking the class's
     * parents in the config's class_prefixes. The name unchanged when no class in the chain has a row, or when
     * the taxonomy does not load; in that case the copy then meets Refusal, which names the problem.
     */
    static FString NameFor(const class UClass* AssetClass, const FString& Name);

private:
    static bool IsExplainedAtFamilyRoot(const FThePlacementResolver& Resolver, const FString& Package);
};
