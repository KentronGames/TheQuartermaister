// (c) 2026 Kentron Cowboys. All rights reserved.

#include "CoreMinimal.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

    #include "CQTest.h"
    #include "Placement/TheStructureLint.h"

    #include "HAL/FileManager.h"
    #include "Misc/FileHelper.h"
    #include "Misc/Paths.h"

namespace TheQuartermasterLintTests
{
/** A destination table that mixes depths: a building kit is one family behind Buildings/Kits, an environment pack owns its family under the category. */
const TCHAR* const LayeredConfig = TEXT(R"json({
    "content_root": "/Game",
    "lint": {
        "shared_folder": "Assets/Shared",
        "family_root": "Assets",
        "family_patterns": [
            "*/Buildings/Kits/*",
            "*/Buildings/Parts/*",
            "*/Buildings/*",
            "*/Props/*/*",
            "*/Environment/*"
        ],
        "assembly_patterns": ["*/Buildings/Kits/*"],
        "rule_severity": { "family-depends-on-family": "warning" }
    }
})json");

/** A flat library: the family is the single segment below Assets, and nothing is declared an assembly. */
const TCHAR* const FlatConfig = TEXT(R"json({
    "content_root": "/Game",
    "lint": {
        "shared_folder": "Assets/Shared",
        "family_root": "Assets",
        "family_patterns": ["*"]
    }
})json");

/** The lint reads its config only from a file, so the test's own JSON goes through one that does not outlive the call. */
bool LoadConfig(const TCHAR* Json, FTheLintConfig& OutConfig, FString& OutError)
{
    const auto File = FPaths::AutomationTransientDir() / TEXT("TheQuartermaster") / FString::Printf(TEXT("lint-%s.json"), *FGuid::NewGuid().ToString());
    if(!FFileHelper::SaveStringToFile(FString(Json), *File))
    {
        OutError = FString::Printf(TEXT("cannot write the fixture config %s"), *File);
        return false;
    }
    const auto bLoaded = FTheStructureLint::LoadConfig(File, OutConfig, OutError);
    IFileManager::Get().Delete(*File, /*RequireExists*/ false, /*EvenReadOnly*/ true);
    return bLoaded;
}
}

TEST_CLASS(StructureLint, "TheQuartermaster")
{
    // Guards: the first matching route names the family, whatever its depth; shared, outside and unrouted packages belong to none.
    TEST_METHOD(FamilyRoutesCarryTheirOwnDepth)
    {
        using namespace TheQuartermasterLintTests;

        FTheLintConfig Layered;
        FTheLintConfig Flat;
        FString Error;
        ASSERT_THAT(IsTrue(LoadConfig(LayeredConfig, Layered, Error), FString::Printf(TEXT("the layered config loads: %s"), *Error)));
        ASSERT_THAT(IsTrue(LoadConfig(FlatConfig, Flat, Error), FString::Printf(TEXT("the flat config loads: %s"), *Error)));

        struct FRow
        {
            const TCHAR* Label;
            const FTheLintConfig* Config;
            const TCHAR* Package;
            const TCHAR* Family;
        };
        const FRow Rows[] = {
            {TEXT("a building kit is one family behind Kits"), &Layered, TEXT("/Game/Assets/City/Buildings/Kits/ModularSchool01/Meshes/SM_Wall"), TEXT("City/Buildings/Kits/ModularSchool01")},
            {TEXT("a prop is a family two levels under the category"), &Layered, TEXT("/Game/Assets/City/Props/Signs/RoadSigns01/SM_Sign"), TEXT("City/Props/Signs/RoadSigns01")},
            {TEXT("an environment pack owns its family directly"), &Layered, TEXT("/Game/Assets/City/Environment/CityPark01/Textures/T_Mask"), TEXT("City/Environment/CityPark01")},
            {TEXT("the general Buildings route takes what the specific ones do not"), &Layered, TEXT("/Game/Assets/City/Buildings/OldWorkshop01/SM_Bench"), TEXT("City/Buildings/OldWorkshop01")},
            {TEXT("shared is family-less"), &Layered, TEXT("/Game/Assets/Shared/Textures/Utility/T_Noise"), TEXT("")},
            {TEXT("outside the family root is family-less"), &Layered, TEXT("/Game/Characters/Someone/BP_Someone"), TEXT("")},
            {TEXT("an unrouted shape is family-less"), &Layered, TEXT("/Game/Assets/City/Weather/Rain01/SM_Drop"), TEXT("")},
            {TEXT("a flat family"), &Flat, TEXT("/Game/Assets/Tower01/SM_Tower01"), TEXT("Tower01")},
            {TEXT("a flat family keeps its internal layout"), &Flat, TEXT("/Game/Assets/Tower01/Meshes/SM_Tower01"), TEXT("Tower01")},
            {TEXT("flat shared is family-less"), &Flat, TEXT("/Game/Assets/Shared/Textures/T_Noise"), TEXT("")},
        };
        for(const auto& Row : Rows)
        {
            (void)Assert.AreEqual(FString(Row.Family), FTheStructureLint::FamilyOf(Row.Package, *Row.Config), FString::Printf(TEXT("%s: %s"), Row.Label, Row.Package));
        }
    }

    // Guards: only a family on an assembly route composes others; its container, its insides and a flat library's families do not.
    TEST_METHOD(AssembliesComposeAndAreNotComposed)
    {
        using namespace TheQuartermasterLintTests;

        FTheLintConfig Layered;
        FTheLintConfig Flat;
        FString Error;
        ASSERT_THAT(IsTrue(LoadConfig(LayeredConfig, Layered, Error), FString::Printf(TEXT("the layered config loads: %s"), *Error)));
        ASSERT_THAT(IsTrue(LoadConfig(FlatConfig, Flat, Error), FString::Printf(TEXT("the flat config loads: %s"), *Error)));

        struct FRow
        {
            const TCHAR* Label;
            const FTheLintConfig* Config;
            const TCHAR* Family;
            bool bAssembly;
        };
        const FRow Rows[] = {
            {TEXT("a building kit is an assembly"), &Layered, TEXT("City/Buildings/Kits/PreBuildings01"), true},
            {TEXT("a part is not"), &Layered, TEXT("City/Buildings/Parts/Wall04"), false},
            {TEXT("an environment pack is not"), &Layered, TEXT("City/Environment/CityPark01"), false},
            {TEXT("the kits container itself is not"), &Layered, TEXT("City/Buildings/Kits"), false},
            {TEXT("a folder inside a kit is not"), &Layered, TEXT("City/Buildings/Kits/PreBuildings01/Meshes"), false},
            {TEXT("the empty family never is"), &Layered, TEXT(""), false},
            {TEXT("a config declaring no assemblies has none"), &Flat, TEXT("Tower01"), false},
        };
        for(const auto& Row : Rows)
        {
            (void)Assert.AreEqual(Row.bAssembly, FTheStructureLint::IsAssembly(Row.Family, *Row.Config), FString::Printf(TEXT("%s: '%s'"), Row.Label, Row.Family));
        }
    }

    // Guards: a rule's severity is what the config declares, and a rule it does not name is not downgraded.
    TEST_METHOD(SeverityIsConfiguration)
    {
        using namespace TheQuartermasterLintTests;

        FTheLintConfig Config;
        FString Error;
        ASSERT_THAT(IsTrue(LoadConfig(LayeredConfig, Config, Error), FString::Printf(TEXT("the layered config loads: %s"), *Error)));

        const auto CrossFamily = Config.RuleSeverity.Find(TEXT("family-depends-on-family"));
        ASSERT_THAT(IsNotNull(CrossFamily, TEXT("the cross-family severity is read from the config")));
        (void)Assert.IsTrue(*CrossFamily == ETheLintSeverity::Warning, TEXT("cross-family is the warning the config declares"));
        (void)Assert.IsNull(Config.RuleSeverity.Find(TEXT("shared-depends-on-family")), TEXT("a rule the config does not name is not downgraded"));
    }
};

#endif
