// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Placement/TheStructureLint.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

namespace TheQuartermasterLintTests
{
FString ConfigPath(const FString& Leaf)
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("TheQuartermaster"));
    return Plugin.IsValid() ? Plugin->GetBaseDir() / TEXT("Config") / Leaf : FString();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheLintMixedDepthTest, "TheQuartermaster.Lint.FamilyRoutesCarryTheirOwnDepth", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheLintMixedDepthTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterLintTests;

    FTheLintConfig Config;
    FString Error;
    if(!TestTrue(FString::Printf(TEXT("OWH config loads: %s"), *Error),
        FTheStructureLint::LoadConfig(ConfigPath(TEXT("placement.owh.json")), Config, Error)))
    {
        return false;
    }

    TestEqual(TEXT("building kit family"),
        FTheStructureLint::FamilyOf(TEXT("/Game/Assets/City/Buildings/Kits/ModularSchool01/Meshes/SM_Wall"), Config),
        FString(TEXT("City/Buildings/Kits/ModularSchool01")));
    TestEqual(TEXT("prop family"),
        FTheStructureLint::FamilyOf(TEXT("/Game/Assets/City/Props/Signs/RoadSigns01/SM_Sign"), Config),
        FString(TEXT("City/Props/Signs/RoadSigns01")));
    TestEqual(TEXT("vehicle family"),
        FTheStructureLint::FamilyOf(TEXT("/Game/Assets/City/Vehicles/Background/Sedan01/SM_Sedan"), Config),
        FString(TEXT("City/Vehicles/Background/Sedan01")));

    TestEqual(TEXT("environment family"),
        FTheStructureLint::FamilyOf(TEXT("/Game/Assets/City/Environment/CityPark01/Textures/T_Mask"), Config),
        FString(TEXT("City/Environment/CityPark01")));
    TestEqual(TEXT("foliage family"),
        FTheStructureLint::FamilyOf(TEXT("/Game/Assets/City/Foliage/Trees01/SM_Tree"), Config),
        FString(TEXT("City/Foliage/Trees01")));

    const FString Left = FTheStructureLint::FamilyOf(TEXT("/Game/Assets/City/Props/Signs/RoadSigns01/SM_A"), Config);
    const FString Right = FTheStructureLint::FamilyOf(TEXT("/Game/Assets/City/Props/Signs/ShopSigns01/SM_B"), Config);
    TestNotEqual(TEXT("sibling packs are distinct families"), Left, Right);

    TestEqual(TEXT("shared is family-less"),
        FTheStructureLint::FamilyOf(TEXT("/Game/Assets/Shared/Textures/Utility/T_Noise"), Config), FString());
    TestEqual(TEXT("outside the family root"),
        FTheStructureLint::FamilyOf(TEXT("/Game/Characters/Jimmy/BP_Jimmy"), Config), FString());

    TestEqual(TEXT("unrouted shape"),
        FTheStructureLint::FamilyOf(TEXT("/Game/Assets/City/Weather/Rain01/SM_Drop"), Config), FString());

    TestEqual(TEXT("specific route wins over general"),
        FTheStructureLint::FamilyOf(TEXT("/Game/Assets/City/Buildings/OldWorkshop01/SM_Bench"), Config),
        FString(TEXT("City/Buildings/OldWorkshop01")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheLintFlatTaxonomyTest, "TheQuartermaster.Lint.OneSchemaCarriesAFlatTaxonomy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheLintFlatTaxonomyTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterLintTests;

    FTheLintConfig Config;
    FString Error;
    if(!TestTrue(FString::Printf(TEXT("TheGame config loads: %s"), *Error),
        FTheStructureLint::LoadConfig(ConfigPath(TEXT("placement.forms.json")), Config, Error)))
    {
        return false;
    }

    TestEqual(TEXT("flat family"),
        FTheStructureLint::FamilyOf(TEXT("/Game/Assets/Tower01/SM_Tower01"), Config), FString(TEXT("Tower01")));
    TestEqual(TEXT("flat family keeps its internal layout"),
        FTheStructureLint::FamilyOf(TEXT("/Game/Assets/Tower01/Meshes/SM_Tower01"), Config), FString(TEXT("Tower01")));
    TestEqual(TEXT("flat shared is family-less"),
        FTheStructureLint::FamilyOf(TEXT("/Game/Assets/Shared/Textures/T_Noise"), Config), FString());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheLintAssemblyRoleTest, "TheQuartermaster.Lint.AssembliesComposeAndAreNotComposed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheLintAssemblyRoleTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterLintTests;

    FTheLintConfig Config;
    FString Error;
    if(!TestTrue(FString::Printf(TEXT("OWH config loads: %s"), *Error),
        FTheStructureLint::LoadConfig(ConfigPath(TEXT("placement.owh.json")), Config, Error)))
    {
        return false;
    }

    TestTrue(TEXT("a building kit is an assembly"),
        FTheStructureLint::IsAssembly(TEXT("City/Buildings/Kits/PreBuildings01"), Config));
    TestFalse(TEXT("a part is not an assembly"),
        FTheStructureLint::IsAssembly(TEXT("City/Buildings/Parts/Wall04"), Config));
    TestFalse(TEXT("an environment pack is not an assembly"),
        FTheStructureLint::IsAssembly(TEXT("City/Environment/CityPark01"), Config));

    TestFalse(TEXT("the kits container itself is not an assembly"),
        FTheStructureLint::IsAssembly(TEXT("City/Buildings/Kits"), Config));
    TestFalse(TEXT("a folder inside a kit is not an assembly"),
        FTheStructureLint::IsAssembly(TEXT("City/Buildings/Kits/PreBuildings01/Meshes"), Config));
    TestFalse(TEXT("empty family is never an assembly"),
        FTheStructureLint::IsAssembly(FString(), Config));

    FTheLintConfig Flat;
    if(TestTrue(TEXT("TheGame config loads"),
        FTheStructureLint::LoadConfig(ConfigPath(TEXT("placement.forms.json")), Flat, Error)))
    {
        TestFalse(TEXT("no assemblies declared"), FTheStructureLint::IsAssembly(TEXT("Tower01"), Flat));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheLintSeverityConfigTest, "TheQuartermaster.Lint.SeverityIsConfiguration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheLintSeverityConfigTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterLintTests;

    FTheLintConfig Config;
    FString Error;
    if(!TestTrue(FString::Printf(TEXT("OWH config loads: %s"), *Error),
        FTheStructureLint::LoadConfig(ConfigPath(TEXT("placement.owh.json")), Config, Error)))
    {
        return false;
    }

    const ETheLintSeverity* CrossFamily = Config.RuleSeverity.Find(TEXT("family-depends-on-family"));
    if(TestNotNull(TEXT("cross-family severity is declared"), CrossFamily))
    {
        TestTrue(TEXT("cross-family is a warning"), *CrossFamily == ETheLintSeverity::Warning);
    }
    TestNull(TEXT("shared-depends-on-family is not downgraded"), Config.RuleSeverity.Find(TEXT("shared-depends-on-family")));

    FTheLintResult Result;
    Result.Findings.Add({TEXT("family-depends-on-family"), TEXT("/Game/Assets/A/B/C/SM_X"), TEXT(""), ETheLintSeverity::Warning});
    Result.Findings.Add({TEXT("shared-depends-on-family"), TEXT("/Game/Assets/Shared/M_Y"), TEXT(""), ETheLintSeverity::Error});
    TestEqual(TEXT("error count"), Result.CountOf(ETheLintSeverity::Error), 1);
    TestEqual(TEXT("warning count"), Result.CountOf(ETheLintSeverity::Warning), 1);

    return true;
}

#endif
