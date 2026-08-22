// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Asset/TheFolderQuarantine.h"
#include "TheQuartermasterSettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace TheQuartermasterTests
{
const TCHAR* TestRoot = TEXT("/Game/__TheQuartermasterTests");

FString DiskPathOf(const FString& GameFolder)
{
    FString Relative = GameFolder;
    Relative.RemoveFromStart(TEXT("/Game/"));
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / Relative);
}

/** A real saved package, because quarantine moves files on disk - an in-memory object proves nothing. */
bool MakeSavedAsset(const FString& PackagePath)
{
    UPackage* Package = CreatePackage(*PackagePath);
    if(!Package)
    {
        return false;
    }
    // A concrete asset class on purpose: UDataAsset is abstract, and the engine nulls an abstract
    // object out on save, which produces a package with nothing in it.
    UDataTable* Asset = NewObject<UDataTable>(Package, UDataTable::StaticClass(), *FPackageName::GetShortName(PackagePath), RF_Public | RF_Standalone);
    if(!Asset)
    {
        return false;
    }
    // A DataTable without a row struct logs an error on save, and the automation framework counts
    // any logged error as a test failure - the fixture would fail the test it is meant to set up.
    Asset->RowStruct = FTableRowBase::StaticStruct();
    FAssetRegistryModule::AssetCreated(Asset);
    Package->MarkPackageDirty();

    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    const FString Filename = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
    return UPackage::SavePackage(Package, Asset, *Filename, Args);
}

void RemoveFolderFromDisk(const FString& GameFolder)
{
    IFileManager::Get().DeleteDirectory(*DiskPathOf(GameFolder), /*RequireExists*/ false, /*Tree*/ true);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheQuartermasterQuarantineRoundTripTest, "TheQuartermaster.Quarantine.MoveVerifyRestore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheQuartermasterQuarantineRoundTripTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterTests;

    const FString Pack = FString(TestRoot) / TEXT("RoundTripPack");
    const FString Parked = FTheFolderQuarantine::QuarantineRoot() / TEXT("RoundTripPack");
    const FString AssetPath = Pack / TEXT("DA_RoundTrip");

    RemoveFolderFromDisk(Parked);
    RemoveFolderFromDisk(Pack);

    if(!TestTrue(TEXT("fixture asset saved"), MakeSavedAsset(AssetPath)))
    {
        return false;
    }

    FString Report;
    FString Error;
    bool bNeedsRestart = false;

    TestFalse(TEXT("VerifyQuarantined refuses before the move"), FTheFolderQuarantine::VerifyQuarantined(Pack, Report, Error));

    if(!TestTrue(FString::Printf(TEXT("MoveToQuarantine succeeded: %s"), *Error), FTheFolderQuarantine::MoveToQuarantine(Pack, Report, bNeedsRestart, Error)))
    {
        return false;
    }
    TestTrue(FString::Printf(TEXT("VerifyQuarantined passes: %s"), *Error), FTheFolderQuarantine::VerifyQuarantined(Pack, Report, Error));
    TestFalse(TEXT("the source folder is gone from disk"), IFileManager::Get().DirectoryExists(*DiskPathOf(Pack)));
    TestTrue(TEXT("the origin marker records where the pack came from"), IFileManager::Get().FileExists(*(DiskPathOf(Parked) / TEXT(".origin"))));

    if(!TestTrue(FString::Printf(TEXT("RestoreFromQuarantine succeeded: %s"), *Error), FTheFolderQuarantine::RestoreFromQuarantine(Parked, Report, bNeedsRestart, Error)))
    {
        return false;
    }
    TestTrue(FString::Printf(TEXT("VerifyRestoredFromQuarantine passes: %s"), *Error), FTheFolderQuarantine::VerifyRestoredFromQuarantine(Pack, Report, Error));
    TestTrue(TEXT("the asset is back at its original package path"), FPackageName::DoesPackageExist(AssetPath));

    // A second restore has nothing to read: refusing is the only safe answer, and moving something
    // else would be the dangerous one.
    TestFalse(TEXT("a second restore refuses instead of moving something"), FTheFolderQuarantine::RestoreFromQuarantine(Parked, Report, bNeedsRestart, Error));

    RemoveFolderFromDisk(Parked);
    RemoveFolderFromDisk(Pack);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheQuartermasterQuarantineDeleteTest, "TheQuartermaster.Quarantine.DeleteRemovesTheFolder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheQuartermasterQuarantineDeleteTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterTests;

    const FString Pack = FString(TestRoot) / TEXT("DeletePack");
    const FString Parked = FTheFolderQuarantine::QuarantineRoot() / TEXT("DeletePack");

    RemoveFolderFromDisk(Parked);
    RemoveFolderFromDisk(Pack);

    if(!TestTrue(TEXT("fixture asset saved"), MakeSavedAsset(Pack / TEXT("DA_Delete"))))
    {
        return false;
    }

    FString Report;
    FString Error;
    bool bNeedsRestart = false;

    if(!TestTrue(FString::Printf(TEXT("MoveToQuarantine succeeded: %s"), *Error), FTheFolderQuarantine::MoveToQuarantine(Pack, Report, bNeedsRestart, Error)))
    {
        return false;
    }
    TestFalse(TEXT("VerifyQuarantineDeleted refuses while the folder is still there"), FTheFolderQuarantine::VerifyQuarantineDeleted(Parked, Report, Error));

    TestTrue(FString::Printf(TEXT("DeleteFromQuarantine succeeded: %s"), *Error), FTheFolderQuarantine::DeleteFromQuarantine(Parked, Report, Error));
    TestTrue(FString::Printf(TEXT("VerifyQuarantineDeleted passes: %s"), *Error), FTheFolderQuarantine::VerifyQuarantineDeleted(Parked, Report, Error));

    RemoveFolderFromDisk(Pack);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheQuartermasterQuarantineGuardTest, "TheQuartermaster.Quarantine.RefusesOutsideTheRoot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheQuartermasterQuarantineGuardTest::RunTest(const FString& Parameters)
{
    FString Report;
    FString Error;
    bool bNeedsRestart = false;

    const FString Root = FTheFolderQuarantine::QuarantineRoot();

    // Deleting the root itself would take every parked pack with it, so it is refused by name
    // rather than by referencer analysis - there is nothing to analyse at that point.
    TestFalse(TEXT("deleting the quarantine root itself is refused"), FTheFolderQuarantine::DeleteFromQuarantine(Root, Report, Error));
    TestTrue(TEXT("the refusal names the root"), Error.Contains(Root));

    TestFalse(TEXT("deleting a path outside the root is refused"), FTheFolderQuarantine::DeleteFromQuarantine(TEXT("/Game/SomethingElse"), Report, Error));
    TestFalse(TEXT("restoring a path outside the root is refused"), FTheFolderQuarantine::RestoreFromQuarantine(TEXT("/Game/SomethingElse"), Report, bNeedsRestart, Error));
    TestFalse(TEXT("quarantining a non-/Game path is refused"), FTheFolderQuarantine::MoveToQuarantine(TEXT("/Engine/Whatever"), Report, bNeedsRestart, Error));
    TestFalse(TEXT("quarantining something already in quarantine is refused"), FTheFolderQuarantine::MoveToQuarantine(Root / TEXT("Anything"), Report, bNeedsRestart, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheQuartermasterSettingsRootTest, "TheQuartermaster.Settings.FallsBackOnUnusableRoot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheQuartermasterSettingsRootTest::RunTest(const FString& Parameters)
{
    UTheQuartermasterSettings* Settings = GetMutableDefault<UTheQuartermasterSettings>();
    if(!TestNotNull(TEXT("settings CDO"), Settings))
    {
        return false;
    }
    const FString Original = Settings->QuarantineRoot;

    Settings->QuarantineRoot = TEXT("/Game/Parked/");
    TestEqual(TEXT("a trailing slash is stripped"), UTheQuartermasterSettings::ResolvedQuarantineRoot(), FString(TEXT("/Game/Parked")));

    // A bad setting must not strand folders already parked under the default root, so the fallback
    // is the default rather than a failure.
    Settings->QuarantineRoot = TEXT("/Game");
    TestEqual(TEXT("/Game itself falls back to the default"), UTheQuartermasterSettings::ResolvedQuarantineRoot(), FString(UTheQuartermasterSettings::DefaultQuarantineRoot()));

    Settings->QuarantineRoot = TEXT("D:/NotAPackagePath");
    TestEqual(TEXT("a non-/Game path falls back to the default"), UTheQuartermasterSettings::ResolvedQuarantineRoot(), FString(UTheQuartermasterSettings::DefaultQuarantineRoot()));

    Settings->QuarantineRoot = FString();
    TestEqual(TEXT("an empty root falls back to the default"), UTheQuartermasterSettings::ResolvedQuarantineRoot(), FString(UTheQuartermasterSettings::DefaultQuarantineRoot()));

    Settings->QuarantineRoot = Original;
    return true;
}

#endif
