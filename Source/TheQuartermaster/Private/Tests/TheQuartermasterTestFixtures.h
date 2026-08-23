// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Asset/TheFolderQuarantine.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

/**
 * Ground for the plugin's own tests. It lives in a header because the traps below are the kind a
 * fixture has to state and implementation code may not: what is written here is the difference
 * between a test that sets itself up and one that fails during setup for reasons of its own.
 */
namespace TheQuartermasterTests
{
/** The scratch root every test builds under. ReleaseTestRoot is what keeps it from outliving a run. */
inline const TCHAR* TestRoot = TEXT("/Game/__TheQuartermasterTests");

inline FString DiskPathOf(const FString& GameFolder)
{
    FString Relative = GameFolder;
    Relative.RemoveFromStart(TEXT("/Game/"));
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / Relative);
}

/**
 * A real saved package, because quarantine moves files on disk and an in-memory object proves
 * nothing about that.
 *
 * Two traps decide the asset class. UDataAsset is abstract and the engine nulls an abstract object
 * out on save, which leaves a package with nothing in it; and a UDataTable without a row struct
 * logs an error on save, which the automation framework counts as a failure of the very test the
 * fixture is setting up. Hence a concrete class with its row struct filled in.
 */
inline bool MakeSavedAsset(const FString& PackagePath)
{
    UPackage* Package = CreatePackage(*PackagePath);
    if(!Package)
    {
        return false;
    }
    UDataTable* Asset = NewObject<UDataTable>(Package, UDataTable::StaticClass(), *FPackageName::GetShortName(PackagePath), RF_Public | RF_Standalone);
    if(!Asset)
    {
        return false;
    }
    Asset->RowStruct = FTableRowBase::StaticStruct();
    FAssetRegistryModule::AssetCreated(Asset);
    Package->MarkPackageDirty();

    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    const FString Filename = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
    return UPackage::SavePackage(Package, Asset, *Filename, Args);
}

inline void RemoveFolderFromDisk(const FString& GameFolder)
{
    IFileManager::Get().DeleteDirectory(*DiskPathOf(GameFolder), /*RequireExists*/ false, /*Tree*/ true);
}

/**
 * Puts the scratch root away with the plugin's own operations rather than with a file delete.
 *
 * A test root left standing under /Game is not cosmetic: a host project's structure gate reads the
 * top level as a closed set and refuses every commit until the folder is removed by hand. Going
 * through quarantine also makes the teardown one more run of the operation on real files. The
 * plain removal at the end is what keeps the guarantee absolute - quarantine refuses an occupied
 * destination, and no refusal may leave the root behind.
 */
inline void ReleaseTestRoot()
{
    if(!IFileManager::Get().DirectoryExists(*DiskPathOf(TestRoot)))
    {
        return;
    }

    FString Report;
    FString Error;
    bool bNeedsRestart = false;
    if(FTheFolderQuarantine::MoveToQuarantine(TestRoot, Report, bNeedsRestart, Error))
    {
        const FString Parked = FTheFolderQuarantine::QuarantineRoot() / FPaths::GetCleanFilename(FString(TestRoot));
        if(FTheFolderQuarantine::DeleteFromQuarantine(Parked, Report, Error))
        {
            return;
        }
        RemoveFolderFromDisk(Parked);
    }
    RemoveFolderFromDisk(TestRoot);
}
}

#endif
