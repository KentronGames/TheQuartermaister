// (c) 2026 Kentron Cowboys. All rights reserved.

#include "CoreMinimal.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

    #include "CQTest.h"
    #include "Tests/TheQuartermasterTestFixtures.h"

    #include "Asset/TheFolderQuarantine.h"
    #include "TheQuartermasterSettings.h"

    #include "HAL/FileManager.h"
    #include "Misc/PackageName.h"
    #include "Misc/Paths.h"
    #include "Misc/ScopeExit.h"

TEST_CLASS(FolderQuarantine, "TheQuartermaster")
{
    // Guards: a quarantined pack leaves Content with a record of its origin, and a restore puts it back exactly once.
    TEST_METHOD(MoveVerifyRestore)
    {
        using namespace TheQuartermasterTests;

        const auto Pack = FString(TestRoot) / TEXT("RoundTripPack");
        const auto Parked = FTheFolderQuarantine::QuarantineRoot() / TEXT("RoundTripPack");
        const auto AssetPath = Pack / TEXT("DA_RoundTrip");

        RemoveFolderFromDisk(Parked);
        RemoveFolderFromDisk(Pack);
        ON_SCOPE_EXIT
        {
            RemoveFolderFromDisk(Parked);
            ReleaseTestRoot();
        };

        ASSERT_THAT(IsTrue(MakeSavedAsset(AssetPath), TEXT("fixture asset saved")));

        FString Report;
        FString Error;
        auto bNeedsRestart = false;

        (void)Assert.IsFalse(FTheFolderQuarantine::VerifyQuarantined(Pack, Report, Error), TEXT("VerifyQuarantined refuses before the move"));

        ASSERT_THAT(IsTrue(FTheFolderQuarantine::MoveToQuarantine(Pack, Report, bNeedsRestart, Error), FString::Printf(TEXT("MoveToQuarantine succeeded: %s"), *Error)));
        (void)Assert.IsTrue(FTheFolderQuarantine::VerifyQuarantined(Pack, Report, Error), FString::Printf(TEXT("VerifyQuarantined passes: %s"), *Error));
        (void)Assert.IsFalse(IFileManager::Get().DirectoryExists(*DiskPathOf(Pack)), TEXT("the source folder is gone from disk"));
        (void)Assert.IsTrue(IFileManager::Get().FileExists(*(DiskPathOf(Parked) / TEXT(".origin"))), TEXT("the origin marker records where the pack came from"));

        ASSERT_THAT(IsTrue(FTheFolderQuarantine::RestoreFromQuarantine(Parked, Report, bNeedsRestart, Error), FString::Printf(TEXT("RestoreFromQuarantine succeeded: %s"), *Error)));
        (void)Assert.IsTrue(FTheFolderQuarantine::VerifyRestoredFromQuarantine(Pack, Report, Error), FString::Printf(TEXT("VerifyRestoredFromQuarantine passes: %s"), *Error));
        (void)Assert.IsTrue(FPackageName::DoesPackageExist(AssetPath), TEXT("the asset is back at its original package path"));

        (void)Assert.IsFalse(FTheFolderQuarantine::RestoreFromQuarantine(Parked, Report, bNeedsRestart, Error), TEXT("a second restore has nothing to read, so it refuses instead of moving something else"));
    }

    // Guards: a plan writes nothing and promises what the move and the delete then do; the delete is verified gone.
    TEST_METHOD(PlanPromisesWhatTheMoveAndDeleteDeliver)
    {
        using namespace TheQuartermasterTests;

        const auto Pack = FString(TestRoot) / TEXT("PlanPack");
        const auto Parked = FTheFolderQuarantine::QuarantineRoot() / TEXT("PlanPack");

        RemoveFolderFromDisk(Parked);
        RemoveFolderFromDisk(Pack);
        ON_SCOPE_EXIT
        {
            RemoveFolderFromDisk(Parked);
            ReleaseTestRoot();
        };

        ASSERT_THAT(IsTrue(MakeSavedAsset(Pack / TEXT("DA_Plan")), TEXT("fixture asset saved")));

        FString Plan;
        FString Report;
        FString Error;
        auto bNeedsRestart = false;

        ASSERT_THAT(IsTrue(FTheFolderQuarantine::PlanMoveToQuarantine(Pack, Plan, Error), FString::Printf(TEXT("PlanMoveToQuarantine answered: %s"), *Error)));
        (void)Assert.IsTrue(Plan.Contains(Parked), TEXT("the plan names the destination"));
        (void)Assert.IsTrue(IFileManager::Get().DirectoryExists(*DiskPathOf(Pack)), TEXT("the plan wrote nothing: the pack is still where it was"));

        ASSERT_THAT(IsTrue(FTheFolderQuarantine::MoveToQuarantine(Pack, Report, bNeedsRestart, Error), FString::Printf(TEXT("the move the plan promised lands: %s"), *Error)));
        (void)Assert.IsTrue(FTheFolderQuarantine::PlanDeleteFromQuarantine(Parked, Plan, Error), FString::Printf(TEXT("a plan for the delete reads the parked folder: %s"), *Error));
        (void)Assert.IsTrue(IFileManager::Get().DirectoryExists(*DiskPathOf(Parked)), TEXT("the delete plan wrote nothing either"));
        (void)Assert.IsFalse(FTheFolderQuarantine::VerifyQuarantineDeleted(Parked, Report, Error), TEXT("VerifyQuarantineDeleted refuses while the folder is still there"));

        ASSERT_THAT(IsTrue(FTheFolderQuarantine::DeleteFromQuarantine(Parked, /*bForce*/ false, Report, Error), FString::Printf(TEXT("DeleteFromQuarantine succeeded: %s"), *Error)));
        (void)Assert.IsTrue(FTheFolderQuarantine::VerifyQuarantineDeleted(Parked, Report, Error), FString::Printf(TEXT("VerifyQuarantineDeleted passes: %s"), *Error));
    }

    // Guards: a parked folder something outside still references is not deleted unless forced, and the refusal names the referencer.
    TEST_METHOD(DeleteRefusesWhileSomethingOutsideStillPointsAtIt)
    {
        using namespace TheQuartermasterTests;

        const auto Pack = FString(TestRoot) / TEXT("ReferencedPack");
        const auto Parked = FTheFolderQuarantine::QuarantineRoot() / TEXT("ReferencedPack");
        const auto UserFolder = FString(TestRoot) / TEXT("Harvested");
        const auto UserPackage = UserFolder / TEXT("MI_Harvested");

        RemoveFolderFromDisk(Parked);
        RemoveFolderFromDisk(Pack);
        RemoveFolderFromDisk(UserFolder);
        ON_SCOPE_EXIT
        {
            RemoveFolderFromDisk(Parked);
            RemoveFolderFromDisk(UserFolder);
            ReleaseTestRoot();
        };

        ASSERT_THAT(IsNotNull(MakeSavedMaterial(Pack / TEXT("M_Referenced")), TEXT("fixture material saved")));

        FString Report;
        FString Error;
        auto bNeedsRestart = false;
        ASSERT_THAT(IsTrue(FTheFolderQuarantine::MoveToQuarantine(Pack, Report, bNeedsRestart, Error), FString::Printf(TEXT("MoveToQuarantine succeeded: %s"), *Error)));

        auto* const ParkedMaterial = LoadObject<UMaterialInterface>(nullptr, *(Parked / TEXT("M_Referenced.M_Referenced")));
        ASSERT_THAT(IsNotNull(ParkedMaterial, TEXT("the parked material loads from its quarantine path")));
        ASSERT_THAT(IsTrue(MakeSavedReferencingAsset(UserPackage, ParkedMaterial), TEXT("a package outside the quarantine now references the parked one")));

        (void)Assert.IsTrue(FTheFolderQuarantine::ExternalReferencersOf(Parked).Contains(UserPackage), TEXT("the referencer is seen"));

        (void)Assert.IsFalse(FTheFolderQuarantine::DeleteFromQuarantine(Parked, /*bForce*/ false, Report, Error), TEXT("DeleteFromQuarantine refuses while something outside still points at the folder"));
        (void)Assert.IsTrue(Error.Contains(UserPackage), FString::Printf(TEXT("the refusal names the referencer (%s)"), *Error));

        (void)Assert.IsTrue(FTheFolderQuarantine::DeleteFromQuarantine(Parked, /*bForce*/ true, Report, Error), FString::Printf(TEXT("bForce deletes it anyway: %s"), *Error));
        (void)Assert.IsTrue(FTheFolderQuarantine::VerifyQuarantineDeleted(Parked, Report, Error), FString::Printf(TEXT("VerifyQuarantineDeleted passes: %s"), *Error));
    }

    // Guards: a mistyped path is refused as absent, never read as a completed removal or a movable source.
    TEST_METHOD(RefusesToActOnWhatIsNotThere)
    {
        using namespace TheQuartermasterTests;

        const auto Missing = FTheFolderQuarantine::QuarantineRoot() / TEXT("__NoSuchPackEverExisted");
        RemoveFolderFromDisk(Missing);

        FString Report;
        FString Error;

        (void)Assert.IsFalse(FTheFolderQuarantine::DeleteFromQuarantine(Missing, /*bForce*/ false, Report, Error), TEXT("DeleteFromQuarantine refuses a folder that is not on disk"));
        (void)Assert.IsTrue(Error.Contains(TEXT("does not exist")), FString::Printf(TEXT("the refusal says the folder is not there (%s)"), *Error));

        (void)Assert.IsFalse(FTheFolderQuarantine::PlanDeleteFromQuarantine(Missing, Report, Error), TEXT("PlanDeleteFromQuarantine refuses it too"));
        (void)Assert.IsFalse(FTheFolderQuarantine::PlanMoveToQuarantine(FString(TestRoot) / TEXT("__NoSuchSource"), Report, Error), TEXT("PlanMoveToQuarantine refuses a source that is not on disk"));
    }

    // Guards: delete and restore never reach outside the quarantine root - by name, by `..`, or by a doubled slash collapsing onto it.
    TEST_METHOD(RefusesOutsideTheRoot)
    {
        FString Report;
        FString Error;
        auto bNeedsRestart = false;

        const auto Root = FTheFolderQuarantine::QuarantineRoot();

        (void)Assert.IsFalse(FTheFolderQuarantine::DeleteFromQuarantine(Root, /*bForce*/ false, Report, Error), TEXT("deleting the quarantine root itself is refused: it would take every parked pack with it"));
        (void)Assert.IsTrue(Error.Contains(Root), FString::Printf(TEXT("the refusal names the root (%s)"), *Error));

        (void)Assert.IsFalse(FTheFolderQuarantine::DeleteFromQuarantine(TEXT("/Game/SomethingElse"), /*bForce*/ false, Report, Error), TEXT("deleting a path outside the root is refused"));

        // A prefix test alone lets these through: on disk `..` climbs out and `//` collapses onto the
        // root, so the first would delete a live Content folder and the second every parked pack.
        const FString Escapes[] = {
            Root / TEXT("../SomethingElse"),
            Root / TEXT("Pack/../../SomethingElse"),
            Root + TEXT("//"),
            Root + TEXT("//Pack"),
            Root / TEXT("./Pack"),
            Root / TEXT("Pack/.."),
            Root + TEXT("/..\\SomethingElse"),
        };
        for(const auto& Escape : Escapes)
        {
            Error.Reset();
            (void)Assert.IsFalse(FTheFolderQuarantine::DeleteFromQuarantine(Escape, /*bForce*/ false, Report, Error), FString::Printf(TEXT("deleting '%s' is refused"), *Escape));
            (void)Assert.IsTrue(Error.StartsWith(TEXT("refused")), FString::Printf(TEXT("the refusal for '%s' says why (%s)"), *Escape, *Error));
            (void)Assert.IsFalse(FTheFolderQuarantine::RestoreFromQuarantine(Escape, Report, bNeedsRestart, Error), FString::Printf(TEXT("restoring '%s' is refused"), *Escape));
        }
        (void)Assert.IsFalse(FTheFolderQuarantine::RestoreFromQuarantine(TEXT("/Game/SomethingElse"), Report, bNeedsRestart, Error), TEXT("restoring a path outside the root is refused"));
        (void)Assert.IsFalse(FTheFolderQuarantine::MoveToQuarantine(TEXT("/Engine/Whatever"), Report, bNeedsRestart, Error), TEXT("quarantining a non-/Game path is refused"));
        (void)Assert.IsFalse(FTheFolderQuarantine::MoveToQuarantine(Root / TEXT("Anything"), Report, bNeedsRestart, Error), TEXT("quarantining something already in quarantine is refused"));
    }

    // Guards: the bug where a source resolving onto Content, or holding the root, moved into itself and erased the project.
    TEST_METHOD(RefusesASourceThatContainsTheRoot)
    {
        FString Report;
        FString Error;
        const auto Root = FTheFolderQuarantine::QuarantineRoot();

        // Only the Plan is called: it runs the same guard as the move, and a regression must not be able
        // to reach the disk from a test.
        const FString Sources[] = {
            TEXT("/Game//"),
            TEXT("/Game/."),
            TEXT("/Game/./"),
            TEXT("/Game/Pack/.."),
            TEXT("/Game/Pack/../Other"),
            TEXT("/Game//Pack"),
            FPaths::GetPath(Root),
        };
        for(const auto& Source : Sources)
        {
            Error.Reset();
            (void)Assert.IsFalse(FTheFolderQuarantine::PlanMoveToQuarantine(Source, Report, Error), FString::Printf(TEXT("quarantining '%s' is refused"), *Source));
            // The path guard must be what refuses: the later "already exists in quarantine" check only fires
            // while the root exists on disk, and the erasure happened exactly when it did not.
            (void)Assert.IsTrue(Error.StartsWith(TEXT("refused")) || Error.StartsWith(TEXT("only /Game/")), FString::Printf(TEXT("'%s' is refused by the path guard, not by an incidental disk check: %s"), *Source, *Error));
        }

        const auto Neighbour = Root + TEXT("Old");
        Error.Reset();
        (void)FTheFolderQuarantine::PlanMoveToQuarantine(Neighbour, Report, Error);
        (void)Assert.IsFalse(Error.Contains(TEXT("already in quarantine")), FString::Printf(TEXT("'%s' is not read as inside the root (%s)"), *Neighbour, *Error));
    }

    // Guards: an unusable quarantine root setting falls back to the default rather than stranding packs parked under it.
    TEST_METHOD(UnusableRootFallsBackToTheDefault)
    {
        auto* const Settings = GetMutableDefault<UTheQuartermasterSettings>();
        ASSERT_THAT(IsNotNull(Settings, TEXT("the settings default object")));
        const auto Original = Settings->QuarantineRoot;
        ON_SCOPE_EXIT
        {
            Settings->QuarantineRoot = Original;
        };

        const FString Default = UTheQuartermasterSettings::DefaultQuarantineRoot();
        const TPair<const TCHAR*, FString> Rows[] = {
            {TEXT("/Game/Parked/"), FString(TEXT("/Game/Parked"))},
            {TEXT("/Game"), Default},
            {TEXT("D:/NotAPackagePath"), Default},
            {TEXT(""), Default},
        };
        for(const auto& Row : Rows)
        {
            Settings->QuarantineRoot = Row.Key;
            (void)Assert.AreEqual(Row.Value, UTheQuartermasterSettings::ResolvedQuarantineRoot(), FString::Printf(TEXT("the setting '%s' resolves to '%s'"), Row.Key, *Row.Value));
        }
    }
};

#endif
