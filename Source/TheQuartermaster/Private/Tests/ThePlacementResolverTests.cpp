// (c) 2026 Kentron Cowboys. All rights reserved.

#include "CoreMinimal.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

    #include "CQTest.h"
    #include "Placement/ThePlacementResolver.h"

    #include "Interfaces/IPluginManager.h"
    #include "Misc/Paths.h"

namespace TheQuartermasterPlacementTests
{
/** The schema's own fixture shipped beside the plugin: a taxonomy of no project, written to exercise every form the schema claims. */
FString FormsFixturePath()
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("TheQuartermaster"));
    return Plugin.IsValid() ? Plugin->GetBaseDir() / TEXT("Config") / TEXT("placement.forms.json") : FString();
}

/**
 * A layered library taxonomy, the test's own: reusable art placed by geography and category before the owning family,
 * each route in a typed and a flat shape, the typed one listed first so placement answers with it.
 */
const TCHAR* const LayeredConfig = TEXT(R"json({
    "content_root": "/Game",
    "prefixes": { "SM": "mesh", "T": "texture", "WBP": "widget" },
    "kind_directories": { "mesh": "Meshes", "texture": "Textures" },
    "unprefixed_contexts": ["map"],
    "rules": [
        { "context": "library",   "folder": "Assets/{geography}/{category}/{family}/{kind_dir}", "requires": ["geography", "category", "family"] },
        { "context": "library",   "folder": "Assets/{geography}/{category}/{family}",            "requires": ["geography", "category", "family"] },
        { "context": "shared",    "folder": "Assets/Shared/{kind_dir}/{category}",               "requires": ["category"] },
        { "context": "ui-widget", "folder": "UI/Widgets" },
        { "context": "map",       "folder": "Maps" }
    ]
})json");

FThePlacementFacts Facts(const FString& Name, const FString& Context, const TMap<FString, FString>& Values = {})
{
    FThePlacementFacts Result;
    Result.Name = Name;
    Result.Context = Context;
    Result.Values = Values;
    return Result;
}
}

TEST_CLASS(PlacementResolver, "TheQuartermaster")
{
    // Guards: every form the schema claims places where it says, and each refusal carries its reason.
    TEST_METHOD(ExpressesEveryFormItClaims)
    {
        using namespace TheQuartermasterPlacementTests;

        FThePlacementResolver Resolver;
        FString Error;
        ASSERT_THAT(IsTrue(Resolver.LoadConfig(FormsFixturePath(), Error), FString::Printf(TEXT("the forms fixture loads: %s"), *Error)));

        struct FRow
        {
            const TCHAR* Label;
            FThePlacementFacts Facts;
            const TCHAR* Path; // empty: the facts are refused
            const TCHAR* ErrorContains; // empty: the refusal's wording is not part of the rule
        };
        const FRow Rows[] = {
            {TEXT("the rig sits at the character root"), Facts(TEXT("SK_Jimmy"), TEXT("character"), {{TEXT("entity"), TEXT("Jimmy")}}), TEXT("/Game/Characters/Jimmy/SK_Jimmy"), TEXT("")},
            {TEXT("its material goes one level down"), Facts(TEXT("MI_Jimmy_Skin"), TEXT("character"), {{TEXT("entity"), TEXT("Jimmy")}}), TEXT("/Game/Characters/Jimmy/Materials/MI_Jimmy_Skin"), TEXT("")},
            {TEXT("a declared value places"), Facts(TEXT("M_Water"), TEXT("surface"), {{TEXT("category"), TEXT("Water")}}), TEXT("/Game/Assets/Shared/Materials/Water/M_Water"), TEXT("")},
            {TEXT("a value outside the closed list is refused, naming the list"), Facts(TEXT("M_Sky"), TEXT("surface"), {{TEXT("category"), TEXT("Sky")}}), TEXT(""), TEXT("Water")},
            {TEXT("a value naming the absence of a level collapses it"), Facts(TEXT("M_Generic"), TEXT("surface"), {{TEXT("category"), TEXT("Any")}}), TEXT("/Game/Assets/Shared/Materials/M_Generic"), TEXT("")},
            {TEXT("a dialogue sorts into its own subfolder"), Facts(TEXT("DLG_Intro"), TEXT("chapter"), {{TEXT("chapter"), TEXT("Ch01_Yard")}}), TEXT("/Game/Narrative/Chapters/Ch01_Yard/Dialogues/DLG_Intro"), TEXT("")},
            {TEXT("anything else falls to the default subfolder"), Facts(TEXT("SM_Shrine"), TEXT("chapter"), {{TEXT("chapter"), TEXT("Ch01_Yard")}}), TEXT("/Game/Narrative/Chapters/Ch01_Yard/Assets/SM_Shrine"), TEXT("")},
            {TEXT("a fact off its declared shape is refused"), Facts(TEXT("DLG_Intro"), TEXT("chapter"), {{TEXT("chapter"), TEXT("Yard")}}), TEXT(""), TEXT("")},
            {TEXT("a section of one segment"), Facts(TEXT("SM_Tower01"), TEXT("library"), {{TEXT("category"), TEXT("Town")}, {TEXT("entity"), TEXT("Tower01")}}), TEXT("/Game/Assets/Town/Tower01/SM_Tower01"), TEXT("")},
            {TEXT("a section of two segments"), Facts(TEXT("SM_Cliff01"), TEXT("library"), {{TEXT("category"), TEXT("Nature/Rocks")}, {TEXT("entity"), TEXT("Cliff01")}}), TEXT("/Game/Assets/Nature/Rocks/Cliff01/SM_Cliff01"), TEXT("")},
            {TEXT("a refused value does not place, and says why"), Facts(TEXT("SM_Part"), TEXT("library"), {{TEXT("category"), TEXT("Shared")}, {TEXT("entity"), TEXT("Parts")}}), TEXT(""), TEXT("not a section")},
            {TEXT("an optional level disappears when unnamed"), Facts(TEXT("T_Noise"), TEXT("shared")), TEXT("/Game/Assets/Shared/Textures/T_Noise"), TEXT("")},
            {TEXT("a refused name does not place"), Facts(TEXT("L_Demo"), TEXT("map")), TEXT(""), TEXT("")},
            {TEXT("the same context takes the name without it"), Facts(TEXT("Demo"), TEXT("map")), TEXT("/Game/Maps/Demo"), TEXT("")},
            {TEXT("a redirected prefix does not place in the context offered, and names where it belongs"), Facts(TEXT("PER_Jimmy"), TEXT("character"), {{TEXT("entity"), TEXT("Jimmy")}}), TEXT(""), TEXT("persona")},
            {TEXT("and it places in the context it was sent to"), Facts(TEXT("PER_Jimmy"), TEXT("persona")), TEXT("/Game/Narrative/Personas/PER_Jimmy"), TEXT("")},
        };
        for(const auto& Row : Rows)
        {
            const auto Result = Resolver.Resolve(Row.Facts);
            if(FCString::Strlen(Row.Path) == 0)
            {
                (void)Assert.AreEqual(EThePlacementOutcome::Rejected, Result.Outcome, FString::Printf(TEXT("%s: refused"), Row.Label));
                (void)Assert.IsTrue(Result.Error.Contains(Row.ErrorContains), FString::Printf(TEXT("%s: the refusal names '%s' (%s)"), Row.Label, Row.ErrorContains, *Result.Error));
                continue;
            }
            (void)Assert.AreEqual(FString(Row.Path), Result.PackagePath, FString::Printf(TEXT("%s (%s)"), Row.Label, *Result.Error));
        }
    }

    // Guards: Explain reads every path Resolve places back to the context that placed it, and nothing it would refuse.
    TEST_METHOD(ExplainsWhatItPlaces)
    {
        using namespace TheQuartermasterPlacementTests;

        FThePlacementResolver Resolver;
        FString Error;
        ASSERT_THAT(IsTrue(Resolver.LoadConfig(FormsFixturePath(), Error), FString::Printf(TEXT("the forms fixture loads: %s"), *Error)));

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
        for(const auto& Case : Cases)
        {
            const auto Placed = Resolver.Resolve(Case.Value);
            if(!Assert.AreEqual(EThePlacementOutcome::Placed, Placed.Outcome, FString::Printf(TEXT("%s places (%s)"), *Case.Key, *Placed.Error)))
            {
                continue;
            }
            (void)Assert.AreEqual(Case.Value.Context, Resolver.Explain(Placed.PackagePath).Context, FString::Printf(TEXT("%s is explained by the context that placed it"), *Case.Key));
        }

        const TPair<const TCHAR*, const TCHAR*> Unmatched[] = {
            {TEXT("a level no rule describes"), TEXT("/Game/Characters/Jimmy/Rigs/SK_Jimmy")},
            {TEXT("a name the context refuses"), TEXT("/Game/Maps/L_Demo")},
            {TEXT("a redirected prefix where it does not belong"), TEXT("/Game/Characters/Jimmy/PER_Jimmy")},
        };
        for(const auto& Stray : Unmatched)
        {
            (void)Assert.AreEqual(ETheExplanationOutcome::Unmatched, Resolver.Explain(Stray.Value).Outcome, FString::Printf(TEXT("%s is unmatched: %s"), Stray.Key, Stray.Value));
        }
    }

    // Guards: placement refuses rather than guesses - an unknown prefix or context, missing facts all named at once, no config.
    TEST_METHOD(RefusesRatherThanGuesses)
    {
        using namespace TheQuartermasterPlacementTests;

        FThePlacementResolver Resolver;
        FString Error;
        ASSERT_THAT(IsTrue(Resolver.LoadConfigFromString(LayeredConfig, Error), FString::Printf(TEXT("the layered config loads: %s"), *Error)));

        const auto Unknown = Resolver.Resolve(Facts(TEXT("XYZ_Thing"), TEXT("ui-widget")));
        (void)Assert.AreEqual(EThePlacementOutcome::Rejected, Unknown.Outcome, TEXT("an unknown prefix is rejected"));
        (void)Assert.IsTrue(Unknown.Error.Contains(TEXT("XYZ")), FString::Printf(TEXT("the refusal names the offending prefix (%s)"), *Unknown.Error));

        (void)Assert.AreEqual(EThePlacementOutcome::Rejected, Resolver.Resolve(Facts(TEXT("SM_Thing"), TEXT("not-a-context"))).Outcome, TEXT("an unknown context is rejected"));

        const auto Incomplete = Resolver.Resolve(Facts(TEXT("SM_Tower01"), TEXT("library"), {{TEXT("geography"), TEXT("City")}}));
        (void)Assert.AreEqual(EThePlacementOutcome::NeedsFacts, Incomplete.Outcome, TEXT("incomplete facts do not place"));
        (void)Assert.AreEqual(2, Incomplete.MissingFacts.Num(), TEXT("both missing facts are named at once"));
        (void)Assert.IsTrue(Incomplete.MissingFacts.Contains(TEXT("category")) && Incomplete.MissingFacts.Contains(TEXT("family")), TEXT("category and family are the facts named"));

        const auto Map = Resolver.Resolve(Facts(TEXT("L_Demo"), TEXT("map")));
        (void)Assert.AreEqual(FString(TEXT("/Game/Maps/L_Demo")), Map.PackagePath, FString::Printf(TEXT("an unprefixed context places without a prefix (%s)"), *Map.Error));

        (void)Assert.AreEqual(EThePlacementOutcome::Rejected, FThePlacementResolver().Resolve(Facts(TEXT("SM_Thing"), TEXT("library"))).Outcome, TEXT("a resolver with no config refuses"));
    }

    // Guards: Describe reports the taxonomy as the config declares it - templates unexpanded, required facts, prefix demands.
    TEST_METHOD(DescribesItsOwnStructure)
    {
        using namespace TheQuartermasterPlacementTests;

        FThePlacementResolver Resolver;
        FString Error;
        ASSERT_THAT(IsTrue(Resolver.LoadConfigFromString(LayeredConfig, Error), FString::Printf(TEXT("the layered config loads: %s"), *Error)));

        const auto Structure = Resolver.Describe();
        (void)Assert.AreEqual(FString(TEXT("/Game")), Structure.ContentRoot, TEXT("the content root"));
        for(const auto& Rule : Structure.Rules)
        {
            (void)Assert.IsTrue(Structure.Contexts.Contains(Rule.Context), FString::Printf(TEXT("context '%s' is in the context list"), *Rule.Context));
        }
        (void)Assert.IsTrue(Structure.PrefixKinds.Contains(TEXT("SM")), TEXT("the prefix table is carried"));
        (void)Assert.IsTrue(Structure.KindDirectories.Contains(TEXT("mesh")), TEXT("the kind directories are carried"));

        const auto Library = Structure.Rules.FindByPredicate([](const FThePlacementRuleInfo& Rule) { return Rule.Context == TEXT("library"); });
        ASSERT_THAT(IsNotNull(Library, TEXT("the library rule is described")));
        (void)Assert.AreEqual(FString(TEXT("Assets/{geography}/{category}/{family}/{kind_dir}")), Library->FolderTemplate, TEXT("its template is the config's, unexpanded"));
        (void)Assert.AreEqual(3, Library->Requires.Num(), TEXT("its required facts are named"));
        (void)Assert.IsTrue(Library->bRequiresPrefix, TEXT("a library asset carries a prefix"));

        const auto Map = Structure.Rules.FindByPredicate([](const FThePlacementRuleInfo& Rule) { return Rule.Context == TEXT("map"); });
        ASSERT_THAT(IsNotNull(Map, TEXT("the map rule is described")));
        (void)Assert.IsFalse(Map->bRequiresPrefix, TEXT("a map carries none"));
    }

    // Guards: Explain recovers the facts, prefix and kind a path was placed with, the more literal rule winning, and fits no foreign path.
    TEST_METHOD(ReadsAPathBackToItsRule)
    {
        using namespace TheQuartermasterPlacementTests;

        FThePlacementResolver Layered;
        FString Error;
        ASSERT_THAT(IsTrue(Layered.LoadConfigFromString(LayeredConfig, Error), FString::Printf(TEXT("the layered config loads: %s"), *Error)));

        const auto Placed = Layered.Resolve(Facts(TEXT("SM_Tower01"), TEXT("library"), {{TEXT("geography"), TEXT("City")}, {TEXT("category"), TEXT("Buildings")}, {TEXT("family"), TEXT("Tower01")}}));
        ASSERT_THAT(AreEqual(EThePlacementOutcome::Placed, Placed.Outcome, FString::Printf(TEXT("the library facts place (%s)"), *Placed.Error)));
        const auto Read = Layered.Explain(Placed.PackagePath);
        (void)Assert.AreEqual(ETheExplanationOutcome::Matched, Read.Outcome, TEXT("the path it produced is explained"));
        (void)Assert.AreEqual(FString(TEXT("library")), Read.Context, TEXT("back to the same context"));
        (void)Assert.AreEqual(FString(TEXT("City")), Read.Facts.FindRef(TEXT("geography")), TEXT("with the geography recovered"));
        (void)Assert.AreEqual(FString(TEXT("Buildings")), Read.Facts.FindRef(TEXT("category")), TEXT("with the category recovered"));
        (void)Assert.AreEqual(FString(TEXT("Tower01")), Read.Facts.FindRef(TEXT("family")), TEXT("with the family recovered"));
        (void)Assert.AreEqual(FString(TEXT("SM")), Read.Prefix, TEXT("and the prefix read off the name"));
        (void)Assert.AreEqual(FString(TEXT("mesh")), Read.Kind, TEXT("and its kind"));

        const auto Shared = Layered.Explain(TEXT("/Game/Assets/Shared/Textures/Utility/T_Noise"));
        (void)Assert.AreEqual(FString(TEXT("shared")), Shared.Context, TEXT("the more literal rule wins over the free-form one"));
        (void)Assert.AreEqual(FString(TEXT("Utility")), Shared.Facts.FindRef(TEXT("category")), TEXT("its category is recovered"));

        const auto Map = Layered.Explain(TEXT("/Game/Maps/L_Demo"));
        (void)Assert.AreEqual(ETheExplanationOutcome::Matched, Map.Outcome, TEXT("a map is explained"));
        (void)Assert.AreEqual(FString(TEXT("map")), Map.Context, TEXT("as a map"));

        FThePlacementResolver Forms;
        ASSERT_THAT(IsTrue(Forms.LoadConfig(FormsFixturePath(), Error), FString::Printf(TEXT("the forms fixture loads: %s"), *Error)));
        const auto Persona = Forms.Explain(TEXT("/Game/Narrative/Personas/PER_Jimmy"));
        (void)Assert.AreEqual(FString(TEXT("persona")), Persona.Context, TEXT("the fixture's persona is explained"));
        (void)Assert.AreEqual(0, Persona.Facts.Num(), TEXT("a fully literal rule captures no facts"));
        (void)Assert.AreEqual(ETheExplanationOutcome::Unmatched, Forms.Explain(Placed.PackagePath).Outcome, TEXT("another taxonomy's path is not fitted to this one"));
    }

    // Guards: a path no rule accounts for is refused with its near misses, and a tie between rules is reported, never picked.
    TEST_METHOD(RefusesAPathItCannotAccountFor)
    {
        using namespace TheQuartermasterPlacementTests;

        FThePlacementResolver Resolver;
        FString Error;
        ASSERT_THAT(IsTrue(Resolver.LoadConfigFromString(LayeredConfig, Error), FString::Printf(TEXT("the layered config loads: %s"), *Error)));

        const auto Legacy = Resolver.Explain(TEXT("/Game/VendorPack/Text/ST_Menu"));
        (void)Assert.AreEqual(ETheExplanationOutcome::Unmatched, Legacy.Outcome, TEXT("an unmigrated path matches nothing"));
        (void)Assert.IsTrue(Legacy.Error.Contains(TEXT("VendorPack")), FString::Printf(TEXT("and the refusal names the path (%s)"), *Legacy.Error));

        const auto TooDeep = Resolver.Explain(TEXT("/Game/Assets/City/Buildings/Kits/Set01/Meshes/Parts/SM_Wall01"));
        (void)Assert.AreEqual(ETheExplanationOutcome::Unmatched, TooDeep.Outcome, TEXT("a deeper route than any rule declares is refused"));
        (void)Assert.IsTrue(TooDeep.CandidateContexts.Contains(TEXT("library")), TEXT("the rules sharing its first segment are named as near misses"));

        (void)Assert.AreEqual(ETheExplanationOutcome::Unmatched, Resolver.Explain(TEXT("/Engine/Maps/L_Thing")).Outcome, TEXT("a path outside the content root is refused"));
        (void)Assert.AreEqual(ETheExplanationOutcome::Unmatched, Resolver.Explain(TEXT("/Game/SM_Loose")).Outcome, TEXT("a package with no folder is refused"));

        FThePlacementResolver Ambiguous;
        const auto TwoWays = TEXT(R"json({
            "content_root": "/Game",
            "prefixes": { "SM": "mesh" },
            "rules": [
                { "context": "by-owner",  "folder": "Assets/{owner}" },
                { "context": "by-family", "folder": "Assets/{family}" }
            ]
        })json");
        ASSERT_THAT(IsTrue(Ambiguous.LoadConfigFromString(TwoWays, Error), FString::Printf(TEXT("the ambiguous config loads: %s"), *Error)));
        const auto Tie = Ambiguous.Explain(TEXT("/Game/Assets/Tower01/SM_Tower01"));
        (void)Assert.AreEqual(ETheExplanationOutcome::Ambiguous, Tie.Outcome, TEXT("a tie is reported as ambiguous"));
        (void)Assert.AreEqual(2, Tie.CandidateContexts.Num(), TEXT("and both contexts are named"));
    }

    // Guards: a top-level exception folder is outside the taxonomy, compared as the content browser compares, and nothing else is.
    TEST_METHOD(TopLevelExceptionsAreOutsideTheTaxonomy)
    {
        FThePlacementResolver Resolver;
        FString Error;
        const auto Json = TEXT(R"json({
            "content_root": "/Game",
            "top_level": { "roots": ["Assets"], "exceptions": ["_Tests", "_Quarantine"] },
            "prefixes": { "SM": "mesh" },
            "rules": [{ "context": "library", "folder": "Assets/{category}/{entity}", "requires": ["category", "entity"] }]
        })json");
        ASSERT_THAT(IsTrue(Resolver.LoadConfigFromString(Json, Error), FString::Printf(TEXT("the config loads: %s"), *Error)));

        const TPair<const TCHAR*, bool> Rows[] = {
            {TEXT("/Game/_Tests/TheMCP/BP_Probe"), true},
            {TEXT("/Game/_tests/themcp/BP_Probe.BP_Probe"), true},
            {TEXT("/Game/Assets/Town/Barrel/SM_Barrel"), false},
            {TEXT("/Game/_TestsExtra/BP_Probe"), false},
            {TEXT("/Engine/_Tests/BP_Probe"), false},
        };
        for(const auto& Row : Rows)
        {
            (void)Assert.AreEqual(Row.Value, Resolver.IsOutsideTaxonomy(Row.Key), FString::Printf(TEXT("'%s' is %s the taxonomy"), Row.Key, Row.Value ? TEXT("outside") : TEXT("not outside")));
        }
        (void)Assert.AreEqual(2, Resolver.Describe().TopLevelExceptions.Num(), TEXT("Describe carries the exceptions"));
    }

    // Guards: a listed plugin's content root is judged by the game's layout; an unlisted or look-alike plugin is not under it.
    TEST_METHOD(PluginContentRootIsLaidOutLikeTheGame)
    {
        FThePlacementResolver Resolver;
        FString Error;
        const auto Json = TEXT(R"json({
            "content_root": "/Game",
            "plugin_content_roots": ["/TheGameKit/"],
            "top_level": { "roots": ["Assets"], "exceptions": ["_Tests"] },
            "prefixes": { "SM": "mesh" },
            "rules": [{ "context": "library", "folder": "Assets/{category}/{entity}", "requires": ["category", "entity"] }]
        })json");
        ASSERT_THAT(IsTrue(Resolver.LoadConfigFromString(Json, Error), FString::Printf(TEXT("the config loads: %s"), *Error)));

        const auto Kit = Resolver.Explain(TEXT("/TheGameKit/Assets/Town/Barrel/SM_Barrel"));
        (void)Assert.AreEqual(ETheExplanationOutcome::Matched, Kit.Outcome, FString::Printf(TEXT("a kit package is judged by the game's layout (%s)"), *Kit.Error));
        (void)Assert.AreEqual(FString(TEXT("/Game/Assets/Town/Barrel/SM_Barrel")), Resolver.AsContentPath(TEXT("/TheGameKit/Assets/Town/Barrel/SM_Barrel")), TEXT("its content path is the same path under the content root"));
        (void)Assert.IsTrue(Resolver.IsOutsideTaxonomy(TEXT("/TheGameKit/_Tests/BP_Probe")), TEXT("the kit's own _Tests folder is outside the taxonomy, as the game's is"));
        (void)Assert.AreNotEqual(ETheExplanationOutcome::Matched, Resolver.Explain(TEXT("/TheGameKit/Junk/SM_Barrel")).Outcome, TEXT("a kit package in a folder no rule explains is still refused"));
        const auto Other = Resolver.Explain(TEXT("/OtherPlugin/Assets/Town/Barrel/SM_Barrel"));
        (void)Assert.IsTrue(Other.Error.Contains(TEXT("not under the configured content root")), FString::Printf(TEXT("a plugin not listed is not under any root (%s)"), *Other.Error));
        (void)Assert.AreEqual(FString(TEXT("/TheGameKitExtra/Assets/SM_X")), Resolver.AsContentPath(TEXT("/TheGameKitExtra/Assets/SM_X")), TEXT("a plugin whose name only starts like a root is not under it"));
    }

    // Guards: a name takes its class's prefix unless it already carries one of the same kind, in the config's spelling.
    TEST_METHOD(ClassNamesItsPrefixByKind)
    {
        FThePlacementResolver Resolver;
        FString Error;
        const auto Json = TEXT(R"json({
            "content_root": "/Game",
            "prefixes": { "SM": "mesh", "SK": "mesh", "AS": "anim", "MM": "anim", "MF": "material", "PHYS": "physics", "S": "audio" },
            "class_prefixes": { "StaticMesh": "SM", "SkeletalMesh": "SK", "AnimSequence": "AS", "PhysicsAsset": "PHYS" },
            "rules": [{ "context": "library", "folder": "Assets/{category}/{entity}", "requires": ["category", "entity"] }]
        })json");
        ASSERT_THAT(IsTrue(Resolver.LoadConfigFromString(Json, Error), FString::Printf(TEXT("the config loads: %s"), *Error)));

        (void)Assert.AreEqual(FString(TEXT("SM")), Resolver.PrefixForClass(TEXT("StaticMesh")), TEXT("a class the config names answers its prefix"));
        (void)Assert.AreEqual(FString(), Resolver.PrefixForClass(TEXT("World")), TEXT("a class it does not name answers nothing"));

        struct FRow
        {
            const TCHAR* Label;
            const TCHAR* Name;
            const TCHAR* ClassPrefix;
            const TCHAR* Expected;
        };
        const FRow Rows[] = {
            {TEXT("a vendor name with no declared prefix gets the class's"), TEXT("Cliff_01"), TEXT("SM"), TEXT("SM_Cliff_01")},
            {TEXT("our own prefix stays"), TEXT("SM_Cliff01"), TEXT("SM"), TEXT("SM_Cliff01")},
            {TEXT("a pack's MM_ sequence keeps its prefix - same kind"), TEXT("MM_Run_Fwd"), TEXT("AS"), TEXT("MM_Run_Fwd")},
            {TEXT("a pack's MF_ physics asset is renamed - another kind"), TEXT("MF_Body"), TEXT("PHYS"), TEXT("PHYS_Body")},
            {TEXT("a static-mesh prefix on a skeletal mesh stays - both are meshes"), TEXT("SM_Hero"), TEXT("SK"), TEXT("SM_Hero")},
            {TEXT("an audio prefix on a mesh is replaced"), TEXT("S_Rock"), TEXT("SM"), TEXT("SM_Rock")},
            {TEXT("no prefix for the class leaves the name alone"), TEXT("Rock"), TEXT(""), TEXT("Rock")},
            {TEXT("an undeclared prefix leaves the name alone rather than inventing one"), TEXT("Rock"), TEXT("XX"), TEXT("Rock")},
            {TEXT("a pack's lower-case sm_ is our SM_ spelled its way"), TEXT("sm_Rock_01_01"), TEXT("SM"), TEXT("SM_Rock_01_01")},
            {TEXT("a lower-case mm_ keeps its kind and takes the config's case"), TEXT("mm_Run_Fwd"), TEXT("AS"), TEXT("MM_Run_Fwd")},
            {TEXT("a lower-case prefix of another kind is replaced like any other"), TEXT("s_Rock"), TEXT("SM"), TEXT("SM_Rock")},
        };
        for(const auto& Row : Rows)
        {
            // AreEqual compares case-sensitively, which is the point of the lower-case rows.
            (void)Assert.AreEqual(FString(Row.Expected), Resolver.NameWithPrefix(Row.Name, Row.ClassPrefix), FString::Printf(TEXT("%s: %s"), Row.Label, Row.Name));
        }

        (void)Assert.AreNotEqual(ETheExplanationOutcome::Matched, Resolver.Explain(TEXT("/Game/Assets/Nature/Rocks/sm_Rock_01_01")).Outcome, TEXT("Explain does not read a lower-case sm_ as the SM_ the naming table declares"));
        (void)Assert.AreEqual(ETheExplanationOutcome::Matched, Resolver.Explain(TEXT("/Game/Assets/Nature/Rocks/SM_Rock_01_01")).Outcome, TEXT("while our own spelling of the same path is explained"));
    }
};

#endif
