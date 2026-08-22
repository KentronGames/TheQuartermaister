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

    // The point of the whole module: if one schema cannot carry two real projects' structures
    // without a special case in code, the schema is wrong and the tool is not portable.
    FThePlacementResolver Owh;
    FString Error;
    if(!TestTrue(FString::Printf(TEXT("OWH config loads: %s"), *Error), Owh.LoadConfig(ConfigPath(TEXT("placement.owh.json")), Error)))
    {
        return false;
    }

    FThePlacementResolver TheGame;
    if(!TestTrue(FString::Printf(TEXT("TheGame config loads: %s"), *Error), TheGame.LoadConfig(ConfigPath(TEXT("placement.thegame.json")), Error)))
    {
        return false;
    }

    // OWH places reusable art by geography, then category, then family.
    const FThePlacementResult OwhLibrary = Owh.Resolve(Facts(TEXT("SM_Tower01"), TEXT("library"),
        {{TEXT("geography"), TEXT("City")}, {TEXT("category"), TEXT("Buildings")}, {TEXT("family"), TEXT("Tower01")}}));
    TestEqual(TEXT("OWH library placement"), OwhLibrary.PackagePath, FString(TEXT("/Game/Assets/City/Buildings/Tower01/SM_Tower01")));

    // TheGame has no geography level at all - one family folder per object.
    const FThePlacementResult GameLibrary = TheGame.Resolve(Facts(TEXT("SM_Tower01"), TEXT("library"),
        {{TEXT("family"), TEXT("Tower01")}}));
    TestEqual(TEXT("TheGame library placement"), GameLibrary.PackagePath, FString(TEXT("/Game/Assets/Tower01/SM_Tower01")));

    // Shared resolves through the resource kind of the prefix, in both.
    const FThePlacementResult OwhShared = Owh.Resolve(Facts(TEXT("T_Noise"), TEXT("shared"), {{TEXT("category"), TEXT("Utility")}}));
    TestEqual(TEXT("OWH shared placement"), OwhShared.PackagePath, FString(TEXT("/Game/Assets/Shared/Textures/Utility/T_Noise")));

    const FThePlacementResult GameShared = TheGame.Resolve(Facts(TEXT("T_Noise"), TEXT("shared")));
    TestEqual(TEXT("TheGame shared placement"), GameShared.PackagePath, FString(TEXT("/Game/Assets/Shared/Textures/T_Noise")));

    // A singular Hero in one project, a named character folder in the other.
    const FThePlacementResult GameHero = TheGame.Resolve(Facts(TEXT("BP_Hero"), TEXT("hero")));
    TestEqual(TEXT("TheGame hero placement"), GameHero.PackagePath, FString(TEXT("/Game/Characters/Hero/BP_Hero")));

    const FThePlacementResult OwhCharacter = Owh.Resolve(Facts(TEXT("BP_Jimmy"), TEXT("character"), {{TEXT("entity"), TEXT("Jimmy")}}));
    TestEqual(TEXT("OWH character placement"), OwhCharacter.PackagePath, FString(TEXT("/Game/Characters/Jimmy/BP_Jimmy")));

    // A root that exists in one taxonomy and not the other is just a rule, not a code path.
    const FThePlacementResult Chapter = TheGame.Resolve(Facts(TEXT("QST_FindTheDog"), TEXT("chapter"), {{TEXT("chapter"), TEXT("Ch01")}}));
    TestEqual(TEXT("TheGame chapter placement"), Chapter.PackagePath, FString(TEXT("/Game/Chapters/Ch01/QST_FindTheDog")));

    const FThePlacementResult OwhChapter = Owh.Resolve(Facts(TEXT("QST_FindTheDog"), TEXT("chapter"), {{TEXT("chapter"), TEXT("Ch01")}}));
    TestEqual(TEXT("OWH has no chapter context"), OwhChapter.Outcome, EThePlacementOutcome::Rejected);
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

    // An unknown prefix is a naming-table violation. Placing it anyway would launder the violation
    // into the structure, which is the failure mode this whole tool exists to prevent.
    const FThePlacementResult Unknown = Resolver.Resolve(Facts(TEXT("XYZ_Thing"), TEXT("ui-widget")));
    TestEqual(TEXT("unknown prefix is rejected"), Unknown.Outcome, EThePlacementOutcome::Rejected);
    TestTrue(TEXT("the refusal names the offending prefix"), Unknown.Error.Contains(TEXT("XYZ")));

    const FThePlacementResult BadContext = Resolver.Resolve(Facts(TEXT("SM_Thing"), TEXT("not-a-context")));
    TestEqual(TEXT("unknown context is rejected"), BadContext.Outcome, EThePlacementOutcome::Rejected);

    // Missing facts are a closed question answered in one round trip, not advice to use judgement.
    const FThePlacementResult Incomplete = Resolver.Resolve(Facts(TEXT("SM_Tower01"), TEXT("library"),
        {{TEXT("geography"), TEXT("City")}}));
    TestEqual(TEXT("incomplete facts do not place"), Incomplete.Outcome, EThePlacementOutcome::NeedsFacts);
    TestEqual(TEXT("both missing facts are named at once"), Incomplete.MissingFacts.Num(), 2);
    TestTrue(TEXT("category is named"), Incomplete.MissingFacts.Contains(TEXT("category")));
    TestTrue(TEXT("family is named"), Incomplete.MissingFacts.Contains(TEXT("family")));

    // Maps carry no asset prefix; the file type is the discrimination.
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

    // The config is the source of truth about the project's layout only if something other than
    // C++ can read it back. This is that guarantee, not a getter test.
    FThePlacementResolver Resolver;
    FString Error;
    if(!TestTrue(TEXT("config loads"), Resolver.LoadConfig(ConfigPath(TEXT("placement.owh.json")), Error)))
    {
        return false;
    }

    const FThePlacementStructure Structure = Resolver.Describe();
    TestEqual(TEXT("content root"), Structure.ContentRoot, FString(TEXT("/Game")));
    TestEqual(TEXT("every rule's context is in the context list"), Structure.Contexts.Num(), Structure.Rules.Num());
    TestTrue(TEXT("the prefix table is carried"), Structure.PrefixKinds.Contains(TEXT("SM")));
    TestTrue(TEXT("the kind directories are carried"), Structure.KindDirectories.Contains(TEXT("mesh")));

    const FThePlacementRuleInfo* Library = Structure.Rules.FindByPredicate(
        [](const FThePlacementRuleInfo& Rule) { return Rule.Context == TEXT("library"); });
    if(!TestNotNull(TEXT("the library rule is described"), Library))
    {
        return false;
    }
    TestEqual(TEXT("its template is the config's, unexpanded"), Library->FolderTemplate, FString(TEXT("Assets/{geography}/{category}/{family}")));
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

    // Placement and explanation have to be one function read in two directions. If they can
    // disagree, the config stops being able to explain the content it produced.
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

    // A shared texture is the interesting case: two rules have the same segment count, and only the
    // one that pins Shared and the kind directory down accounts for the path.
    const FThePathExplanation Shared = Owh.Explain(TEXT("/Game/Assets/Shared/Textures/Utility/T_Noise"));
    TestEqual(TEXT("the more specific rule wins over the free-form one"), Shared.Context, FString(TEXT("shared")));
    TestEqual(TEXT("its category is recovered"), Shared.Facts.FindRef(TEXT("category")), FString(TEXT("Utility")));

    // A map has no prefix at all, so only the contexts declared unprefixed may claim it.
    const FThePathExplanation Map = Owh.Explain(TEXT("/Game/Maps/L_Demo"));
    TestEqual(TEXT("a map is explained"), Map.Outcome, ETheExplanationOutcome::Matched);
    TestEqual(TEXT("as a map"), Map.Context, FString(TEXT("map")));

    // The same reading against the other taxonomy, from the same code.
    FThePlacementResolver TheGame;
    if(!TestTrue(TEXT("TheGame config loads"), TheGame.LoadConfig(ConfigPath(TEXT("placement.thegame.json")), Error)))
    {
        return false;
    }
    const FThePathExplanation Hero = TheGame.Explain(TEXT("/Game/Characters/Hero/BP_Hero"));
    TestEqual(TEXT("TheGame's hero is explained"), Hero.Context, FString(TEXT("hero")));
    TestEqual(TEXT("a fully literal rule captures no facts"), Hero.Facts.Num(), 0);

    // A path one taxonomy produced is not explainable by the other, and says so instead of fitting.
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

    // Legacy content is exactly what this is pointed at, and a rule that "nearly" fits would turn
    // an unmigrated pack into a compliant one on paper.
    const FThePathExplanation Legacy = Resolver.Explain(TEXT("/Game/MenuSystemPro/Text/ST_Menu"));
    TestEqual(TEXT("an unmigrated path matches nothing"), Legacy.Outcome, ETheExplanationOutcome::Unmatched);
    TestTrue(TEXT("and the refusal names the path"), Legacy.Error.Contains(TEXT("MenuSystemPro")));

    // Right root, wrong depth: the near-miss is reported as diagnosis, and the verdict stays refusal.
    const FThePathExplanation TooDeep = Resolver.Explain(TEXT("/Game/Assets/City/Buildings/Kits/Set01/Meshes/SM_Wall01"));
    TestEqual(TEXT("a deeper route than any rule declares is refused"), TooDeep.Outcome, ETheExplanationOutcome::Unmatched);
    TestTrue(TEXT("the rules sharing its first segment are named as near misses"), TooDeep.CandidateContexts.Contains(TEXT("library")));

    const FThePathExplanation Outside = Resolver.Explain(TEXT("/Engine/Maps/L_Thing"));
    TestEqual(TEXT("a path outside the content root is refused"), Outside.Outcome, ETheExplanationOutcome::Unmatched);

    const FThePathExplanation NoFolder = Resolver.Explain(TEXT("/Game/SM_Loose"));
    TestEqual(TEXT("a package with no folder is refused"), NoFolder.Outcome, ETheExplanationOutcome::Unmatched);

    // A taxonomy in which one path satisfies two rules equally is a defect in the taxonomy. Say so
    // rather than letting rule order decide.
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

#endif
