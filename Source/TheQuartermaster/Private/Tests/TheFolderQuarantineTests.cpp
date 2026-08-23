// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/TheQuartermasterTestFixtures.h"

#include "Asset/TheFolderQuarantine.h"
#include "TheQuartermasterSettings.h"

#include "HAL/FileManager.h"
#include "Misc/PackageName.h"

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

    TestFalse(TEXT("a second restore has nothing to read, so it refuses instead of moving something else"), FTheFolderQuarantine::RestoreFromQuarantine(Parked, Report, bNeedsRestart, Error));

    RemoveFolderFromDisk(Parked);
    ReleaseTestRoot();
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

    ReleaseTestRoot();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheQuartermasterQuarantineAbsenceTest, "TheQuartermaster.Quarantine.RefusesToActOnWhatIsNotThere", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheQuartermasterQuarantineAbsenceTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterTests;

    const FString Missing = FTheFolderQuarantine::QuarantineRoot() / TEXT("__NoSuchPackEverExisted");
    RemoveFolderFromDisk(Missing);

    FString Report;
    FString Error;

    TestFalse(TEXT("DeleteFromQuarantine refuses a folder that is not on disk, instead of reading a mistyped path as a completed removal"), FTheFolderQuarantine::DeleteFromQuarantine(Missing, Report, Error));
    TestTrue(TEXT("the refusal says the folder is not there"), Error.Contains(TEXT("does not exist")));

    TestFalse(TEXT("PlanDeleteFromQuarantine refuses it too"), FTheFolderQuarantine::PlanDeleteFromQuarantine(Missing, Report, Error));
    TestFalse(TEXT("PlanMoveToQuarantine refuses a source that is not on disk"), FTheFolderQuarantine::PlanMoveToQuarantine(FString(TestRoot) / TEXT("__NoSuchSource"), Report, Error));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheQuartermasterQuarantinePlanTest, "TheQuartermaster.Quarantine.PlanPromisesWhatTheMoveDelivers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheQuartermasterQuarantinePlanTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterTests;

    const FString Pack = FString(TestRoot) / TEXT("PlanPack");
    const FString Parked = FTheFolderQuarantine::QuarantineRoot() / TEXT("PlanPack");

    RemoveFolderFromDisk(Parked);
    RemoveFolderFromDisk(Pack);

    if(!TestTrue(TEXT("fixture asset saved"), MakeSavedAsset(Pack / TEXT("DA_Plan"))))
    {
        return false;
    }

    FString Plan;
    FString Report;
    FString Error;
    bool bNeedsRestart = false;

    if(!TestTrue(FString::Printf(TEXT("PlanMoveToQuarantine answered: %s"), *Error), FTheFolderQuarantine::PlanMoveToQuarantine(Pack, Plan, Error)))
    {
        ReleaseTestRoot();
        return false;
    }
    TestTrue(TEXT("the plan names the destination"), Plan.Contains(Parked));
    TestTrue(TEXT("the plan wrote nothing: the pack is still where it was"), IFileManager::Get().DirectoryExists(*DiskPathOf(Pack)));

    TestTrue(FString::Printf(TEXT("the move the plan promised lands: %s"), *Error), FTheFolderQuarantine::MoveToQuarantine(Pack, Report, bNeedsRestart, Error));
    TestTrue(TEXT("a plan for the delete reads the parked folder"), FTheFolderQuarantine::PlanDeleteFromQuarantine(Parked, Plan, Error));
    TestTrue(TEXT("the delete plan wrote nothing either"), IFileManager::Get().DirectoryExists(*DiskPathOf(Parked)));

    TestTrue(FString::Printf(TEXT("DeleteFromQuarantine succeeded: %s"), *Error), FTheFolderQuarantine::DeleteFromQuarantine(Parked, Report, Error));

    ReleaseTestRoot();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheQuartermasterQuarantineGuardTest, "TheQuartermaster.Quarantine.RefusesOutsideTheRoot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheQuartermasterQuarantineGuardTest::RunTest(const FString& Parameters)
{
    FString Report;
    FString Error;
    bool bNeedsRestart = false;

    const FString Root = FTheFolderQuarantine::QuarantineRoot();

    TestFalse(TEXT("deleting the quarantine root itself is refused by name: it would take every parked pack with it, and there is nothing to analyse at that point"), FTheFolderQuarantine::DeleteFromQuarantine(Root, Report, Error));
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

    Settings->QuarantineRoot = TEXT("/Game");
    TestEqual(TEXT("/Game itself falls back to the default rather than failing - a bad setting must not strand folders already parked under the default root"), UTheQuartermasterSettings::ResolvedQuarantineRoot(), FString(UTheQuartermasterSettings::DefaultQuarantineRoot()));

    Settings->QuarantineRoot = TEXT("D:/NotAPackagePath");
    TestEqual(TEXT("a non-/Game path falls back to the default"), UTheQuartermasterSettings::ResolvedQuarantineRoot(), FString(UTheQuartermasterSettings::DefaultQuarantineRoot()));

    Settings->QuarantineRoot = FString();
    TestEqual(TEXT("an empty root falls back to the default"), UTheQuartermasterSettings::ResolvedQuarantineRoot(), FString(UTheQuartermasterSettings::DefaultQuarantineRoot()));

    Settings->QuarantineRoot = Original;
    return true;
}

#endif
