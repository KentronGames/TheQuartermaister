// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Placement/ThePlacementResolver.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

namespace TheQuartermasterPlacementTests
{
FString ConfigPath(const FString& Leaf)
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("TheQuartermaster"));
    return Plugin.IsValid() ? Plugin->GetBaseDir() / TEXT("Config") / Leaf : FString();
}

FThePlacementFacts Facts(const FString& Name, const FString& Context, const TMap<FString, FString>& Values = {})
{
    FThePlacementFacts Result;
    Result.Name = Name;
    Result.Context = Context;
    Result.Values = Values;
    return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePlacementBothTaxonomiesTest, "TheQuartermaster.Placement.OneSchemaExpressesBothTaxonomies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FThePlacementBothTaxonomiesTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterPlacementTests;

    FThePlacementResolver Owh;
    FString Error;
    if(!TestTrue(FString::Printf(TEXT("OWH config loads: %s"), *Error), Owh.LoadConfig(ConfigPath(TEXT("placement.owh.json")), Error)))
    {
        return false;
    }

    FThePlacementResolver Forms;
    if(!TestTrue(FString::Printf(TEXT("the forms fixture loads: %s"), *Error), Forms.LoadConfig(ConfigPath(TEXT("placement.forms.json")), Error)))
    {
        return false;
    }

    const FThePlacementResult OwhLibrary = Owh.Resolve(Facts(TEXT("SM_Tower01"), TEXT("library"),
        {{TEXT("geography"), TEXT("City")}, {TEXT("category"), TEXT("Buildings")}, {TEXT("family"), TEXT("Tower01")}}));
    TestEqual(TEXT("OWH library placement"), OwhLibrary.PackagePath, FString(TEXT("/Game/Assets/City/Buildings/Tower01/Meshes/SM_Tower01")));

    const FThePlacementResult Flat = Forms.Resolve(Facts(TEXT("SM_Tower01"), TEXT("library"),
        {{TEXT("category"), TEXT("Town")}, {TEXT("entity"), TEXT("Tower01")}}));
    TestEqual(TEXT("a section of one segment"), Flat.PackagePath, FString(TEXT("/Game/Assets/Town/Tower01/SM_Tower01")));

    const FThePlacementResult Deep = Forms.Resolve(Facts(TEXT("SM_Cliff01"), TEXT("library"),
        {{TEXT("category"), TEXT("Nature/Rocks")}, {TEXT("entity"), TEXT("Cliff01")}}));
    TestEqual(TEXT("a section of two segments"), Deep.PackagePath, FString(TEXT("/Game/Assets/Nature/Rocks/Cliff01/SM_Cliff01")));

    const FThePlacementResult OwhShared = Owh.Resolve(Facts(TEXT("T_Noise"), TEXT("shared"), {{TEXT("category"), TEXT("Utility")}}));
    TestEqual(TEXT("OWH shared placement"), OwhShared.PackagePath, FString(TEXT("/Game/Assets/Shared/Textures/Utility/T_Noise")));

    const FThePlacementResult BareShared = Forms.Resolve(Facts(TEXT("T_Noise"), TEXT("shared")));
    TestEqual(TEXT("an optional level disappears when unnamed"), BareShared.PackagePath, FString(TEXT("/Game/Assets/Shared/Textures/T_Noise")));

    const FThePlacementResult OwhChapter = Owh.Resolve(Facts(TEXT("QST_FindTheDog"), TEXT("chapter"), {{TEXT("chapter"), TEXT("Ch01_Yard")}}));
    TestEqual(TEXT("OWH has no chapter context"), OwhChapter.Outcome, EThePlacementOutcome::Rejected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePlacementFiveFormsTest, "TheQuartermaster.Placement.ExpressesEveryFormItClaims", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FThePlacementFiveFormsTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterPlacementTests;

    FThePlacementResolver Resolver;
    FString Error;
    if(!TestTrue(FString::Printf(TEXT("the forms fixture loads: %s"), *Error), Resolver.LoadConfig(ConfigPath(TEXT("placement.forms.json")), Error)))
    {
        return false;
    }

    const FThePlacementResult Rig = Resolver.Resolve(Facts(TEXT("SK_Jimmy"), TEXT("character"), {{TEXT("entity"), TEXT("Jimmy")}}));
    TestEqual(TEXT("the rig sits at the character root"), Rig.PackagePath, FString(TEXT("/Game/Characters/Jimmy/SK_Jimmy")));
    const FThePlacementResult Skin = Resolver.Resolve(Facts(TEXT("MI_Jimmy_Skin"), TEXT("character"), {{TEXT("entity"), TEXT("Jimmy")}}));
    TestEqual(TEXT("its material goes one level down"), Skin.PackagePath, FString(TEXT("/Game/Characters/Jimmy/Materials/MI_Jimmy_Skin")));

    const FThePlacementResult Water = Resolver.Resolve(Facts(TEXT("M_Water"), TEXT("surface"), {{TEXT("category"), TEXT("Water")}}));
    TestEqual(TEXT("a declared value places"), Water.PackagePath, FString(TEXT("/Game/Assets/Shared/Materials/Water/M_Water")));
    const FThePlacementResult Sky = Resolver.Resolve(Facts(TEXT("M_Sky"), TEXT("surface"), {{TEXT("category"), TEXT("Sky")}}));
    TestEqual(TEXT("a value outside the closed list is refused"), Sky.Outcome, EThePlacementOutcome::Rejected);
    TestTrue(TEXT("the refusal names the closed list"), Sky.Error.Contains(TEXT("Water")));

    const FThePlacementResult Dialogue = Resolver.Resolve(Facts(TEXT("DLG_Intro"), TEXT("chapter"), {{TEXT("chapter"), TEXT("Ch01_Yard")}}));
    TestEqual(TEXT("a dialogue sorts into its own subfolder"), Dialogue.PackagePath, FString(TEXT("/Game/Narrative/Chapters/Ch01_Yard/Dialogues/DLG_Intro")));
    const FThePlacementResult Art = Resolver.Resolve(Facts(TEXT("SM_Shrine"), TEXT("chapter"), {{TEXT("chapter"), TEXT("Ch01_Yard")}}));
    TestEqual(TEXT("anything else falls to the default subfolder"), Art.PackagePath, FString(TEXT("/Game/Narrative/Chapters/Ch01_Yard/Assets/SM_Shrine")));
    const FThePlacementResult Malformed = Resolver.Resolve(Facts(TEXT("DLG_Intro"), TEXT("chapter"), {{TEXT("chapter"), TEXT("Yard")}}));
    TestEqual(TEXT("a fact off its declared shape is refused"), Malformed.Outcome, EThePlacementOutcome::Rejected);

    const FThePlacementResult SharedAsSection = Resolver.Resolve(Facts(TEXT("SM_Part"), TEXT("library"),
        {{TEXT("category"), TEXT("Shared")}, {TEXT("entity"), TEXT("Parts")}}));
    TestEqual(TEXT("a refused value does not place"), SharedAsSection.Outcome, EThePlacementOutcome::Rejected);
    TestTrue(TEXT("the refusal carries its reason"), SharedAsSection.Error.Contains(TEXT("not a section")));
    const FThePlacementResult Any = Resolver.Resolve(Facts(TEXT("M_Generic"), TEXT("surface"), {{TEXT("category"), TEXT("Any")}}));
    TestEqual(TEXT("a value naming the absence of a level collapses it"), Any.PackagePath, FString(TEXT("/Game/Assets/Shared/Materials/M_Generic")));
    const FThePlacementResult PrefixedMap = Resolver.Resolve(Facts(TEXT("L_Demo"), TEXT("map")));
    TestEqual(TEXT("a refused name does not place"), PrefixedMap.Outcome, EThePlacementOutcome::Rejected);
    const FThePlacementResult BareMap = Resolver.Resolve(Facts(TEXT("Demo"), TEXT("map")));
    TestEqual(TEXT("the same context takes the name without it"), BareMap.PackagePath, FString(TEXT("/Game/Maps/Demo")));

    const FThePlacementResult PersonaAsCharacter = Resolver.Resolve(Facts(TEXT("PER_Jimmy"), TEXT("character"), {{TEXT("entity"), TEXT("Jimmy")}}));
    TestEqual(TEXT("a redirected prefix does not place in the context offered"), PersonaAsCharacter.Outcome, EThePlacementOutcome::Rejected);
    TestTrue(TEXT("the refusal names where it belongs"), PersonaAsCharacter.Error.Contains(TEXT("persona")));
    const FThePlacementResult Persona = Resolver.Resolve(Facts(TEXT("PER_Jimmy"), TEXT("persona")));
    TestEqual(TEXT("and it places in the context it was sent to"), Persona.PackagePath, FString(TEXT("/Game/Narrative/Personas/PER_Jimmy")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePlacementPlaceExplainRoundTripTest, "TheQuartermaster.Placement.ExplainsWhatItPlaces", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FThePlacementPlaceExplainRoundTripTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterPlacementTests;

    FThePlacementResolver Resolver;
    FString Error;
    if(!TestTrue(FString::Printf(TEXT("the forms fixture loads: %s"), *Error), Resolver.LoadConfig(ConfigPath(TEXT("placement.forms.json")), Error)))
    {
        return false;
    }

    const TArray<TPair<FString, FThePlacementFacts>> Cases = {
        {TEXT("the rig at a character root"), Facts(TEXT("SK_Jimmy"), TEXT("character"), {{TEXT("entity"), TEXT("Jimmy")}})},
        {TEXT("art under its kind subfolder"), Facts(TEXT("MI_Jimmy_Skin"), TEXT("character"), {{TEXT("entity"), TEXT("Jimmy")}})},
        {TEXT("a section of one segment"), Facts(TEXT("SM_Tower01"), TEXT("library"), {{TEXT("category"), TEXT("Town")}, {TEXT("entity"), TEXT("Tower01")}})},
        {TEXT("a section of two segments"), Facts(TEXT("SM_Cliff01"), TEXT("library"), {{TEXT("category"), TEXT("Nature/Rocks")}, {TEXT("entity"), TEXT("Cliff01")}})},
        {TEXT("a collapsed level"), Facts(TEXT("M_Generic"), TEXT("surface"), {{TEXT("category"), TEXT("Any")}})},
        {TEXT("an absent optional level"), Facts(TEXT("T_Noise"), TEXT("shared"))},
        {TEXT("a sub resolved by prefix"), Facts(TEXT("DLG_Intro"), TEXT("chapter"), {{TEXT("chapter"), TEXT("Ch01_Yard")}})},
        {TEXT("a redirect destination"), Facts(TEXT("PER_Jimmy"), TEXT("persona"))},
        {TEXT("an unprefixed context"), Facts(TEXT("Demo"), TEXT("map"))},
    };

    for(const TPair<FString, FThePlacementFacts>& Case : Cases)
    {
        const FThePlacementResult Placed = Resolver.Resolve(Case.Value);
        if(!TestEqual(FString::Printf(TEXT("%s places"), *Case.Key), Placed.Outcome, EThePlacementOutcome::Placed))
        {
            continue;
        }
        const FThePathExplanation Back = Resolver.Explain(Placed.PackagePath);
        TestEqual(FString::Printf(TEXT("%s is explained by the context that placed it"), *Case.Key), Back.Context, Case.Value.Context);
    }

    const FThePathExplanation Stray = Resolver.Explain(TEXT("/Game/Characters/Jimmy/Rigs/SK_Jimmy"));
    TestEqual(TEXT("a level no rule describes is unmatched"), Stray.Outcome, ETheExplanationOutcome::Unmatched);
    const FThePathExplanation RefusedName = Resolver.Explain(TEXT("/Game/Maps/L_Demo"));
    TestEqual(TEXT("a name the context refuses is unmatched"), RefusedName.Outcome, ETheExplanationOutcome::Unmatched);
    const FThePathExplanation Redirected = Resolver.Explain(TEXT("/Game/Characters/Jimmy/PER_Jimmy"));
    TestEqual(TEXT("a redirected prefix is unmatched where it does not belong"), Redirected.Outcome, ETheExplanationOutcome::Unmatched);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePlacementRefusalTest, "TheQuartermaster.Placement.RefusesRatherThanGuesses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FThePlacementRefusalTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterPlacementTests;

    FThePlacementResolver Resolver;
    FString Error;
    if(!TestTrue(TEXT("config loads"), Resolver.LoadConfig(ConfigPath(TEXT("placement.owh.json")), Error)))
    {
        return false;
    }

    const FThePlacementResult Unknown = Resolver.Resolve(Facts(TEXT("XYZ_Thing"), TEXT("ui-widget")));
    TestEqual(TEXT("unknown prefix is rejected"), Unknown.Outcome, EThePlacementOutcome::Rejected);
    TestTrue(TEXT("the refusal names the offending prefix"), Unknown.Error.Contains(TEXT("XYZ")));

    const FThePlacementResult BadContext = Resolver.Resolve(Facts(TEXT("SM_Thing"), TEXT("not-a-context")));
    TestEqual(TEXT("unknown context is rejected"), BadContext.Outcome, EThePlacementOutcome::Rejected);

    const FThePlacementResult Incomplete = Resolver.Resolve(Facts(TEXT("SM_Tower01"), TEXT("library"),
        {{TEXT("geography"), TEXT("City")}}));
    TestEqual(TEXT("incomplete facts do not place"), Incomplete.Outcome, EThePlacementOutcome::NeedsFacts);
    TestEqual(TEXT("both missing facts are named at once"), Incomplete.MissingFacts.Num(), 2);
    TestTrue(TEXT("category is named"), Incomplete.MissingFacts.Contains(TEXT("category")));
    TestTrue(TEXT("family is named"), Incomplete.MissingFacts.Contains(TEXT("family")));

    const FThePlacementResult Map = Resolver.Resolve(Facts(TEXT("L_Demo"), TEXT("map")));
    TestEqual(TEXT("an unprefixed context places without a prefix"), Map.Outcome, EThePlacementOutcome::Placed);
    TestEqual(TEXT("map placement"), Map.PackagePath, FString(TEXT("/Game/Maps/L_Demo")));

    const FThePlacementResult NoConfig = FThePlacementResolver().Resolve(Facts(TEXT("SM_Thing"), TEXT("library")));
    TestEqual(TEXT("a resolver with no config refuses"), NoConfig.Outcome, EThePlacementOutcome::Rejected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePlacementDescribesItselfTest, "TheQuartermaster.Placement.DescribesItsOwnStructure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FThePlacementDescribesItselfTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterPlacementTests;

    FThePlacementResolver Resolver;
    FString Error;
    if(!TestTrue(TEXT("config loads"), Resolver.LoadConfig(ConfigPath(TEXT("placement.owh.json")), Error)))
    {
        return false;
    }

    const FThePlacementStructure Structure = Resolver.Describe();
    TestEqual(TEXT("content root"), Structure.ContentRoot, FString(TEXT("/Game")));
    for(const FThePlacementRuleInfo& Rule : Structure.Rules)
    {
        TestTrue(FString::Printf(TEXT("context '%s' is in the context list"), *Rule.Context), Structure.Contexts.Contains(Rule.Context));
    }
    TestTrue(TEXT("the prefix table is carried"), Structure.PrefixKinds.Contains(TEXT("SM")));
    TestTrue(TEXT("the kind directories are carried"), Structure.KindDirectories.Contains(TEXT("mesh")));

    const FThePlacementRuleInfo* Library = Structure.Rules.FindByPredicate(
        [](const FThePlacementRuleInfo& Rule) { return Rule.Context == TEXT("library"); });
    if(!TestNotNull(TEXT("the library rule is described"), Library))
    {
        return false;
    }
    TestEqual(TEXT("its template is the config's, unexpanded"), Library->FolderTemplate, FString(TEXT("Assets/{geography}/{category}/{family}/{kind_dir}")));
    TestEqual(TEXT("its required facts are named"), Library->Requires.Num(), 3);
    TestTrue(TEXT("a library asset carries a prefix"), Library->bRequiresPrefix);

    const FThePlacementRuleInfo* Map = Structure.Rules.FindByPredicate(
        [](const FThePlacementRuleInfo& Rule) { return Rule.Context == TEXT("map"); });
    if(!TestNotNull(TEXT("the map rule is described"), Map))
    {
        return false;
    }
    TestFalse(TEXT("a map carries none"), Map->bRequiresPrefix);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePlacementRoundTripTest, "TheQuartermaster.Placement.ReadsAPathBackToItsRule", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FThePlacementRoundTripTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterPlacementTests;

    FThePlacementResolver Owh;
    FString Error;
    if(!TestTrue(TEXT("OWH config loads"), Owh.LoadConfig(ConfigPath(TEXT("placement.owh.json")), Error)))
    {
        return false;
    }

    const FThePlacementResult Placed = Owh.Resolve(Facts(TEXT("SM_Tower01"), TEXT("library"),
        {{TEXT("geography"), TEXT("City")}, {TEXT("category"), TEXT("Buildings")}, {TEXT("family"), TEXT("Tower01")}}));
    const FThePathExplanation Read = Owh.Explain(Placed.PackagePath);
    TestEqual(TEXT("the path it produced is explained"), Read.Outcome, ETheExplanationOutcome::Matched);
    TestEqual(TEXT("back to the same context"), Read.Context, FString(TEXT("library")));
    TestEqual(TEXT("with the geography recovered"), Read.Facts.FindRef(TEXT("geography")), FString(TEXT("City")));
    TestEqual(TEXT("with the category recovered"), Read.Facts.FindRef(TEXT("category")), FString(TEXT("Buildings")));
    TestEqual(TEXT("with the family recovered"), Read.Facts.FindRef(TEXT("family")), FString(TEXT("Tower01")));
    TestEqual(TEXT("and the prefix read off the name"), Read.Prefix, FString(TEXT("SM")));
    TestEqual(TEXT("and its kind"), Read.Kind, FString(TEXT("mesh")));

    const FThePathExplanation Shared = Owh.Explain(TEXT("/Game/Assets/Shared/Textures/Utility/T_Noise"));
    TestEqual(TEXT("the more specific rule wins over the free-form one"), Shared.Context, FString(TEXT("shared")));
    TestEqual(TEXT("its category is recovered"), Shared.Facts.FindRef(TEXT("category")), FString(TEXT("Utility")));

    const FThePathExplanation Map = Owh.Explain(TEXT("/Game/Maps/L_Demo"));
    TestEqual(TEXT("a map is explained"), Map.Outcome, ETheExplanationOutcome::Matched);
    TestEqual(TEXT("as a map"), Map.Context, FString(TEXT("map")));

    FThePlacementResolver TheGame;
    if(!TestTrue(TEXT("the forms fixture loads"), TheGame.LoadConfig(ConfigPath(TEXT("placement.forms.json")), Error)))
    {
        return false;
    }
    const FThePathExplanation Persona = TheGame.Explain(TEXT("/Game/Narrative/Personas/PER_Jimmy"));
    TestEqual(TEXT("the fixture's persona is explained"), Persona.Context, FString(TEXT("persona")));
    TestEqual(TEXT("a fully literal rule captures no facts"), Persona.Facts.Num(), 0);

    const FThePathExplanation Foreign = TheGame.Explain(Placed.PackagePath);
    TestEqual(TEXT("another project's path is not fitted to this taxonomy"), Foreign.Outcome, ETheExplanationOutcome::Unmatched);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePlacementExplanationRefusalTest, "TheQuartermaster.Placement.RefusesAPathItCannotAccountFor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FThePlacementExplanationRefusalTest::RunTest(const FString& Parameters)
{
    using namespace TheQuartermasterPlacementTests;

    FThePlacementResolver Resolver;
    FString Error;
    if(!TestTrue(TEXT("config loads"), Resolver.LoadConfig(ConfigPath(TEXT("placement.owh.json")), Error)))
    {
        return false;
    }

    const FThePathExplanation Legacy = Resolver.Explain(TEXT("/Game/MenuSystemPro/Text/ST_Menu"));
    TestEqual(TEXT("an unmigrated path matches nothing"), Legacy.Outcome, ETheExplanationOutcome::Unmatched);
    TestTrue(TEXT("and the refusal names the path"), Legacy.Error.Contains(TEXT("MenuSystemPro")));

    const FThePathExplanation TooDeep = Resolver.Explain(TEXT("/Game/Assets/City/Buildings/Kits/Set01/Meshes/Parts/SM_Wall01"));
    TestEqual(TEXT("a deeper route than any rule declares is refused"), TooDeep.Outcome, ETheExplanationOutcome::Unmatched);
    TestTrue(TEXT("the rules sharing its first segment are named as near misses"), TooDeep.CandidateContexts.Contains(TEXT("library")));

    const FThePathExplanation Outside = Resolver.Explain(TEXT("/Engine/Maps/L_Thing"));
    TestEqual(TEXT("a path outside the content root is refused"), Outside.Outcome, ETheExplanationOutcome::Unmatched);

    const FThePathExplanation NoFolder = Resolver.Explain(TEXT("/Game/SM_Loose"));
    TestEqual(TEXT("a package with no folder is refused"), NoFolder.Outcome, ETheExplanationOutcome::Unmatched);

    FThePlacementResolver Ambiguous;
    const FString TwoWays = TEXT(R"json({
        "content_root": "/Game",
        "prefixes": { "SM": "mesh" },
        "rules": [
            { "context": "by-owner",  "folder": "Assets/{owner}" },
            { "context": "by-family", "folder": "Assets/{family}" }
        ]
    })json");
    if(!TestTrue(TEXT("the ambiguous config loads"), Ambiguous.LoadConfigFromString(TwoWays, Error)))
    {
        return false;
    }
    const FThePathExplanation Tie = Ambiguous.Explain(TEXT("/Game/Assets/Tower01/SM_Tower01"));
    TestEqual(TEXT("a tie is reported as ambiguous"), Tie.Outcome, ETheExplanationOutcome::Ambiguous);
    TestEqual(TEXT("and both contexts are named"), Tie.CandidateContexts.Num(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePlacementOutsideTaxonomyTest, "TheQuartermaster.Placement.TopLevelExceptionsAreOutsideTheTaxonomy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FThePlacementOutsideTaxonomyTest::RunTest(const FString& Parameters)
{
    FThePlacementResolver Resolver;
    FString Error;
    const FString Json = TEXT("{\"content_root\":\"/Game\",\"top_level\":{\"roots\":[\"Assets\"],\"exceptions\":[\"_Tests\",\"_Quarantine\"]},")
        TEXT("\"prefixes\":{\"SM\":\"mesh\"},\"rules\":[{\"context\":\"library\",\"folder\":\"Assets/{category}/{entity}\",\"requires\":[\"category\",\"entity\"]}]}");
    if(!TestTrue(FString::Printf(TEXT("the config loads: %s"), *Error), Resolver.LoadConfigFromString(Json, Error)))
    {
        return false;
    }

    TestTrue(TEXT("a package under an exception folder is outside the taxonomy"), Resolver.IsOutsideTaxonomy(TEXT("/Game/_Tests/TheMCP/BP_Probe")));
    TestTrue(TEXT("the folder name is compared the way the content browser compares it"), Resolver.IsOutsideTaxonomy(TEXT("/Game/_tests/themcp/BP_Probe.BP_Probe")));
    TestFalse(TEXT("a package under a described root is not"), Resolver.IsOutsideTaxonomy(TEXT("/Game/Assets/Town/Barrel/SM_Barrel")));
    TestFalse(TEXT("a folder that only starts like an exception is not"), Resolver.IsOutsideTaxonomy(TEXT("/Game/_TestsExtra/BP_Probe")));
    TestFalse(TEXT("a path outside the content root is not"), Resolver.IsOutsideTaxonomy(TEXT("/Engine/_Tests/BP_Probe")));
    TestEqual(TEXT("Describe carries the exceptions"), Resolver.Describe().TopLevelExceptions.Num(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePlacementClassPrefixTest, "TheQuartermaster.Placement.ClassNamesItsPrefixByKind", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FThePlacementClassPrefixTest::RunTest(const FString& Parameters)
{
    FThePlacementResolver Resolver;
    FString Error;
    const FString Json = TEXT("{\"content_root\":\"/Game\",")
        TEXT("\"prefixes\":{\"SM\":\"mesh\",\"SK\":\"mesh\",\"AS\":\"anim\",\"MM\":\"anim\",\"MF\":\"material\",\"PHYS\":\"physics\",\"S\":\"audio\"},")
        TEXT("\"class_prefixes\":{\"StaticMesh\":\"SM\",\"SkeletalMesh\":\"SK\",\"AnimSequence\":\"AS\",\"PhysicsAsset\":\"PHYS\"},")
        TEXT("\"rules\":[{\"context\":\"library\",\"folder\":\"Assets/{category}/{entity}\",\"requires\":[\"category\",\"entity\"]}]}");
    if(!TestTrue(FString::Printf(TEXT("the config loads: %s"), *Error), Resolver.LoadConfigFromString(Json, Error)))
    {
        return false;
    }

    TestEqual(TEXT("a class the config names answers its prefix"), Resolver.PrefixForClass(TEXT("StaticMesh")), FString(TEXT("SM")));
    TestEqual(TEXT("a class it does not name answers nothing"), Resolver.PrefixForClass(TEXT("World")), FString());

    TestEqual(TEXT("a vendor name with no declared prefix gets the class's"), Resolver.NameWithPrefix(TEXT("Cliff_01"), TEXT("SM")), FString(TEXT("SM_Cliff_01")));
    TestEqual(TEXT("our own prefix stays"), Resolver.NameWithPrefix(TEXT("SM_Cliff01"), TEXT("SM")), FString(TEXT("SM_Cliff01")));
    TestEqual(TEXT("a pack's MM_ sequence keeps its prefix - same kind"), Resolver.NameWithPrefix(TEXT("MM_Run_Fwd"), TEXT("AS")), FString(TEXT("MM_Run_Fwd")));
    TestEqual(TEXT("a pack's MF_ physics asset is renamed - another kind"), Resolver.NameWithPrefix(TEXT("MF_Body"), TEXT("PHYS")), FString(TEXT("PHYS_Body")));
    TestEqual(TEXT("a static-mesh prefix on a skeletal mesh stays - both are meshes"), Resolver.NameWithPrefix(TEXT("SM_Hero"), TEXT("SK")), FString(TEXT("SM_Hero")));
    TestEqual(TEXT("an audio prefix on a mesh is replaced"), Resolver.NameWithPrefix(TEXT("S_Rock"), TEXT("SM")), FString(TEXT("SM_Rock")));
    TestEqual(TEXT("no prefix for the class leaves the name alone"), Resolver.NameWithPrefix(TEXT("Rock"), FString()), FString(TEXT("Rock")));
    TestEqual(TEXT("an undeclared prefix leaves the name alone rather than inventing one"), Resolver.NameWithPrefix(TEXT("Rock"), TEXT("XX")), FString(TEXT("Rock")));
    TestEqualSensitive(TEXT("a pack's lower-case sm_ is our SM_ spelled its way - same kind, our spelling"), Resolver.NameWithPrefix(TEXT("sm_Rock_01_01"), TEXT("SM")), FString(TEXT("SM_Rock_01_01")));
    TestEqualSensitive(TEXT("a lower-case mm_ keeps its kind and takes the config's case"), Resolver.NameWithPrefix(TEXT("mm_Run_Fwd"), TEXT("AS")), FString(TEXT("MM_Run_Fwd")));
    TestEqualSensitive(TEXT("a lower-case prefix of another kind is replaced like any other"), Resolver.NameWithPrefix(TEXT("s_Rock"), TEXT("SM")), FString(TEXT("SM_Rock")));
    TestNotEqual(TEXT("Explain does not read a lower-case sm_ as the SM_ the naming table declares"), static_cast<int32>(Resolver.Explain(TEXT("/Game/Assets/Nature/Rocks/sm_Rock_01_01")).Outcome), static_cast<int32>(ETheExplanationOutcome::Matched));
    TestEqual(TEXT("while our own spelling of the same path is explained"), static_cast<int32>(Resolver.Explain(TEXT("/Game/Assets/Nature/Rocks/SM_Rock_01_01")).Outcome), static_cast<int32>(ETheExplanationOutcome::Matched));
    return true;
}
#endif
