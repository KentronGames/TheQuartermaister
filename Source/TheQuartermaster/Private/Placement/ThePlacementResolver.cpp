// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Placement/ThePlacementResolver.h"

#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "TheQuartermasterModule.h"
#include "TheQuartermasterSettings.h"

namespace
{
FThePlacementResult Reject(const FString& Error)
{
    FThePlacementResult Result;
    Result.Outcome = EThePlacementOutcome::Rejected;
    Result.Error = Error;
    return Result;
}

bool WholeSegmentToken(const FString& Segment, FString& OutToken)
{
    if(Segment.Len() < 3 || !Segment.StartsWith(TEXT("{")) || !Segment.EndsWith(TEXT("}")))
    {
        return false;
    }
    OutToken = Segment.Mid(1, Segment.Len() - 2);
    return !OutToken.Contains(TEXT("{")) && !OutToken.Contains(TEXT("}"));
}

void ReadStringMap(const TSharedPtr<FJsonObject>& Source, const FString& Field, TMap<FString, FString>& OutMap)
{
    const TSharedPtr<FJsonObject>* Object = nullptr;
    if(!Source->TryGetObjectField(Field, Object))
    {
        return;
    }
    for(const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Object)->Values)
    {
        FString Value;
        if(Pair.Value->TryGetString(Value))
        {
            OutMap.Add(Pair.Key, Value);
        }
    }
}
}

bool FThePlacementResolver::LoadConfig(const FString& FilePath, FString& OutError)
{
    FString Json;
    if(!FFileHelper::LoadFileToString(Json, *FPaths::ConvertRelativePathToFull(FilePath)))
    {
        OutError = FString::Printf(TEXT("cannot read placement config: %s"), *FilePath);
        return false;
    }
    return LoadConfigFromString(Json, OutError);
}

bool FThePlacementResolver::LoadProjectConfig(FThePlacementResolver& OutResolver, FString& OutError)
{
    const FString ConfigPath = UTheQuartermasterSettings::ResolvedPlacementConfigPath();
    if(ConfigPath.IsEmpty())
    {
        OutError = TEXT("no placement config: set PlacementConfigPath in the project's Editor settings");
        return false;
    }
    return OutResolver.LoadConfig(ConfigPath, OutError);
}

bool FThePlacementResolver::LoadConfigFromString(const FString& Json, FString& OutError)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if(!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("placement config is not valid JSON");
        return false;
    }

    PrefixKinds.Reset();
    KindDirectories.Reset();
    Contexts.Reset();
    Rules.Reset();
    UnprefixedContexts.Reset();

    Root->TryGetStringField(TEXT("content_root"), ContentRoot);
    ContentRoot.RemoveFromEnd(TEXT("/"));
    if(ContentRoot.IsEmpty())
    {
        ContentRoot = TEXT("/Game");
    }

    ReadStringMap(Root, TEXT("prefixes"), PrefixKinds);
    ReadStringMap(Root, TEXT("kind_directories"), KindDirectories);
    Root->TryGetStringArrayField(TEXT("contexts"), Contexts);
    Root->TryGetStringArrayField(TEXT("unprefixed_contexts"), UnprefixedContexts);

    const TArray<TSharedPtr<FJsonValue>>* RuleValues = nullptr;
    if(!Root->TryGetArrayField(TEXT("rules"), RuleValues))
    {
        OutError = TEXT("placement config declares no rules");
        return false;
    }
    for(const TSharedPtr<FJsonValue>& Value : *RuleValues)
    {
        const TSharedPtr<FJsonObject>* Object = nullptr;
        if(!Value->TryGetObject(Object))
        {
            continue;
        }
        FRule Rule;
        (*Object)->TryGetStringField(TEXT("context"), Rule.Context);
        (*Object)->TryGetStringField(TEXT("folder"), Rule.Folder);
        (*Object)->TryGetStringArrayField(TEXT("requires"), Rule.Requires);
        (*Object)->TryGetStringArrayField(TEXT("optional_facts"), Rule.OptionalFacts);
        if(Rule.Context.IsEmpty() || Rule.Folder.IsEmpty())
        {
            OutError = TEXT("every placement rule needs a context and a folder");
            return false;
        }
        if(!ReadFacts(*Object, Rule, OutError) || !ReadSub(*Object, Rule, OutError))
        {
            return false;
        }
        ReadNameRefusals(*Object, Rule);
        Rules.Add(MoveTemp(Rule));
    }

    const TArray<TSharedPtr<FJsonValue>>* RedirectValues = nullptr;
    if(Root->TryGetArrayField(TEXT("redirects"), RedirectValues))
    {
        for(const TSharedPtr<FJsonValue>& Value : *RedirectValues)
        {
            const TSharedPtr<FJsonObject>* Object = nullptr;
            if(!Value->TryGetObject(Object))
            {
                continue;
            }
            FRedirect Redirect;
            (*Object)->TryGetStringField(TEXT("context"), Redirect.Context);
            (*Object)->TryGetStringField(TEXT("prefix"), Redirect.Prefix);
            (*Object)->TryGetStringField(TEXT("to"), Redirect.To);
            (*Object)->TryGetStringField(TEXT("why"), Redirect.Why);
            if(Redirect.Context.IsEmpty() || Redirect.Prefix.IsEmpty() || Redirect.To.IsEmpty())
            {
                OutError = TEXT("every redirect needs a context, a prefix and a destination context");
                return false;
            }
            Redirects.Add(MoveTemp(Redirect));
        }
    }

    if(Contexts.IsEmpty())
    {
        for(const FRule& Rule : Rules)
        {
            Contexts.AddUnique(Rule.Context);
        }
    }
    return true;
}

bool FThePlacementResolver::ReadFacts(const TSharedPtr<FJsonObject>& Object, FRule& Rule, FString& OutError)
{
    const TSharedPtr<FJsonObject>* Facts = nullptr;
    if(!Object->TryGetObjectField(TEXT("facts"), Facts))
    {
        return true;
    }
    for(const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Facts)->Values)
    {
        const TSharedPtr<FJsonObject>* Body = nullptr;
        if(!Pair.Value->TryGetObject(Body))
        {
            OutError = FString::Printf(TEXT("fact '%s' of context '%s' is not an object"), *Pair.Key, *Rule.Context);
            return false;
        }
        FFactConstraint Constraint;
        (*Body)->TryGetStringArrayField(TEXT("one_of"), Constraint.OneOf);
        (*Body)->TryGetStringArrayField(TEXT("not"), Constraint.Excluded);
        (*Body)->TryGetStringField(TEXT("pattern"), Constraint.Pattern);
        (*Body)->TryGetStringField(TEXT("why"), Constraint.Why);
        int32 Spans = 1;
        if((*Body)->TryGetNumberField(TEXT("spans"), Spans) && Spans > 0)
        {
            Constraint.Spans = Spans;
        }
        ReadStringMap(*Body, TEXT("collapse"), Constraint.Collapse);
        Rule.Facts.Add(Pair.Key, MoveTemp(Constraint));
    }
    return true;
}

bool FThePlacementResolver::ReadSub(const TSharedPtr<FJsonObject>& Object, FRule& Rule, FString& OutError)
{
    const TSharedPtr<FJsonObject>* Sub = nullptr;
    if(!Object->TryGetObjectField(TEXT("sub"), Sub))
    {
        return true;
    }
    FString By;
    (*Sub)->TryGetStringField(TEXT("by"), By);
    if(!By.Equals(TEXT("kind"), ESearchCase::IgnoreCase) && !By.Equals(TEXT("prefix"), ESearchCase::IgnoreCase))
    {
        OutError = FString::Printf(TEXT("context '%s' declares a sub with no readable 'by' - it is either 'kind' or 'prefix'"), *Rule.Context);
        return false;
    }
    Rule.Sub.bDeclared = true;
    Rule.Sub.bByPrefix = By.Equals(TEXT("prefix"), ESearchCase::IgnoreCase);
    FString Use;
    (*Sub)->TryGetStringField(TEXT("use"), Use);
    Rule.Sub.bUseKindDirectories = Use.Equals(TEXT("kind_directories"), ESearchCase::IgnoreCase);
    (*Sub)->TryGetStringField(TEXT("default"), Rule.Sub.Default);
    (*Sub)->TryGetStringArrayField(TEXT("root_prefixes"), Rule.Sub.RootPrefixes);
    ReadStringMap(*Sub, TEXT("map"), Rule.Sub.Map);
    return true;
}

void FThePlacementResolver::ReadNameRefusals(const TSharedPtr<FJsonObject>& Object, FRule& Rule)
{
    const TArray<TSharedPtr<FJsonValue>>* Refusals = nullptr;
    if(!Object->TryGetArrayField(TEXT("name_refuses"), Refusals))
    {
        return;
    }
    for(const TSharedPtr<FJsonValue>& Value : *Refusals)
    {
        const TSharedPtr<FJsonObject>* Body = nullptr;
        if(!Value->TryGetObject(Body))
        {
            continue;
        }
        FNameRefusal Refusal;
        (*Body)->TryGetStringField(TEXT("starts_with"), Refusal.StartsWith);
        (*Body)->TryGetStringField(TEXT("why"), Refusal.Why);
        if(!Refusal.StartsWith.IsEmpty())
        {
            Rule.NameRefuses.Add(MoveTemp(Refusal));
        }
    }
}

FString FThePlacementResolver::SubFor(const FRule& Rule, const FString& Prefix, const FString& Kind, bool& bOutDeclared) const
{
    bOutDeclared = Rule.Sub.bDeclared;
    if(!Rule.Sub.bDeclared)
    {
        return FString();
    }
    if(!Prefix.IsEmpty() && Rule.Sub.RootPrefixes.Contains(Prefix))
    {
        return FString();
    }
    const FString& Key = Rule.Sub.bByPrefix ? Prefix : Kind;
    const TMap<FString, FString>& Table = Rule.Sub.bUseKindDirectories ? KindDirectories : Rule.Sub.Map;
    if(!Key.IsEmpty())
    {
        if(const FString* Found = Table.Find(Key))
        {
            return *Found;
        }
    }
    return Rule.Sub.Default;
}

FString FThePlacementResolver::RefusalByFacts(const FRule& Rule, const TMap<FString, FString>& Values) const
{
    for(const TPair<FString, FFactConstraint>& Pair : Rule.Facts)
    {
        const FString* Value = Values.Find(Pair.Key);
        if(!Value || Value->IsEmpty())
        {
            continue;
        }
        const FFactConstraint& Constraint = Pair.Value;
        if(Constraint.Excluded.Contains(*Value))
        {
            return FString::Printf(TEXT("'%s' is not a valid %s for context '%s'%s%s"), **Value, *Pair.Key, *Rule.Context, Constraint.Why.IsEmpty() ? TEXT("") : TEXT(" - "), *Constraint.Why);
        }
        if(Constraint.OneOf.Num() > 0 && !Constraint.OneOf.Contains(*Value))
        {
            return FString::Printf(TEXT("'%s' is not a %s for context '%s' - use one of %s"), **Value, *Pair.Key, *Rule.Context, *FString::Join(Constraint.OneOf, TEXT(", ")));
        }
        if(!Constraint.Pattern.IsEmpty())
        {
            const FRegexPattern Pattern(Constraint.Pattern);
            FRegexMatcher Matcher(Pattern, *Value);
            if(!Matcher.FindNext())
            {
                return FString::Printf(TEXT("%s '%s' does not have the shape %s"), *Pair.Key, **Value, *Constraint.Pattern);
            }
        }
    }
    return FString();
}

FString FThePlacementResolver::ParsePrefix(const FString& Name) const
{
    int32 Underscore = INDEX_NONE;
    if(!Name.FindChar(TEXT('_'), Underscore) || Underscore <= 0)
    {
        return FString();
    }
    const FString Candidate = Name.Left(Underscore);
    return PrefixKinds.Contains(Candidate) ? Candidate : FString();
}

FThePlacementResult FThePlacementResolver::Resolve(const FThePlacementFacts& Facts) const
{
    if(Rules.IsEmpty())
    {
        return Reject(TEXT("no placement config loaded"));
    }
    if(Facts.Name.IsEmpty())
    {
        return Reject(TEXT("name is required"));
    }
    if(!Contexts.Contains(Facts.Context))
    {
        return Reject(FString::Printf(TEXT("context '%s' is not in the configured set: %s"), *Facts.Context, *FString::Join(Contexts, TEXT(", "))));
    }

    FThePlacementResult Result;
    Result.Prefix = ParsePrefix(Facts.Name);
    if(!Result.Prefix.IsEmpty())
    {
        Result.Kind = PrefixKinds[Result.Prefix];
    }

    const bool bContextTakesNoPrefix = UnprefixedContexts.Contains(Facts.Context);
    if(Result.Prefix.IsEmpty() && !bContextTakesNoPrefix)
    {
        int32 Underscore = INDEX_NONE;
        if(Facts.Name.FindChar(TEXT('_'), Underscore) && Underscore > 0)
        {
            return Reject(FString::Printf(TEXT("'%s' carries prefix '%s', which the config's prefix table does not declare; add it there first"), *Facts.Name, *Facts.Name.Left(Underscore)));
        }
        return Reject(FString::Printf(TEXT("'%s' has no prefix, and context '%s' requires one"), *Facts.Name, *Facts.Context));
    }

    const FRule* Matched = Rules.FindByPredicate([&Facts](const FRule& Rule) { return Rule.Context == Facts.Context; });
    if(!Matched)
    {
        return Reject(FString::Printf(TEXT("no rule for context '%s'"), *Facts.Context));
    }

    for(const FNameRefusal& Refusal : Matched->NameRefuses)
    {
        if(Facts.Name.StartsWith(Refusal.StartsWith, ESearchCase::CaseSensitive))
        {
            return Reject(FString::Printf(TEXT("'%s' - %s"), *Facts.Name, *Refusal.Why));
        }
    }

    for(const FRedirect& Redirect : Redirects)
    {
        if(Redirect.Context == Facts.Context && Redirect.Prefix == Result.Prefix)
        {
            return Reject(FString::Printf(TEXT("'%s' does not belong in context '%s' - %s; use context '%s'"), *Facts.Name, *Facts.Context, *Redirect.Why, *Redirect.To));
        }
    }

    const FString Refusal = RefusalByFacts(*Matched, Facts.Values);
    if(!Refusal.IsEmpty())
    {
        return Reject(Refusal);
    }

    TArray<FString> Missing;
    for(const FString& Required : Matched->Requires)
    {
        const FString* Value = Facts.Values.Find(Required);
        if(!Value || Value->IsEmpty())
        {
            Missing.AddUnique(Required);
        }
    }

    if(Matched->Folder.Contains(TEXT("{kind_dir}")))
    {
        const FString* Directory = Result.Kind.IsEmpty() ? nullptr : KindDirectories.Find(Result.Kind);
        if(!Directory)
        {
            return Reject(FString::Printf(TEXT("context '%s' places by resource kind, but kind '%s' has no directory in kind_directories"), *Facts.Context, *Result.Kind));
        }
    }

    bool bSubDeclared = false;
    const FString Sub = SubFor(*Matched, Result.Prefix, Result.Kind, bSubDeclared);

    TArray<FString> TemplateSegments;
    Matched->Folder.ParseIntoArray(TemplateSegments, TEXT("/"), /*InCullEmpty*/ true);
    TArray<FString> Built;
    for(const FString& Segment : TemplateSegments)
    {
        FString Token;
        if(!WholeSegmentToken(Segment, Token))
        {
            FString Filled = Segment;
            for(const TPair<FString, FString>& Pair : Facts.Values)
            {
                Filled = Filled.Replace(*FString::Printf(TEXT("{%s}"), *Pair.Key), *Pair.Value);
            }
            int32 Open = INDEX_NONE;
            if(Filled.FindChar(TEXT('{'), Open))
            {
                int32 Close = INDEX_NONE;
                Filled.FindChar(TEXT('}'), Close);
                if(Close > Open)
                {
                    Missing.AddUnique(Filled.Mid(Open + 1, Close - Open - 1));
                }
                continue;
            }
            Built.Add(Filled);
            continue;
        }

        if(Token == TEXT("sub"))
        {
            if(!Sub.IsEmpty())
            {
                Built.Add(Sub);
            }
            continue;
        }
        if(Token == TEXT("kind_dir"))
        {
            const FString* Directory = Result.Kind.IsEmpty() ? nullptr : KindDirectories.Find(Result.Kind);
            Built.Add(Directory ? *Directory : FString());
            continue;
        }

        const FString* Raw = Facts.Values.Find(Token);
        const FFactConstraint* Constraint = Matched->Facts.Find(Token);
        const FString* Collapsed = (Raw && Constraint) ? Constraint->Collapse.Find(*Raw) : nullptr;
        const FString Value = Collapsed ? *Collapsed : (Raw ? *Raw : FString());
        if(!Value.IsEmpty())
        {
            Built.Add(Value);
            continue;
        }
        if(Collapsed || Matched->OptionalFacts.Contains(Token))
        {
            continue;
        }
        Missing.AddUnique(Token);
    }

    const FString Folder = FString::Join(Built.FilterByPredicate([](const FString& S) { return !S.IsEmpty(); }), TEXT("/"));

    if(!Missing.IsEmpty())
    {
        Missing.Sort();
        Result.Outcome = EThePlacementOutcome::NeedsFacts;
        Result.MissingFacts = MoveTemp(Missing);
        return Result;
    }

    Result.Outcome = EThePlacementOutcome::Placed;
    Result.Folder = Folder;
    Result.PackagePath = FString::Printf(TEXT("%s/%s/%s"), *ContentRoot, *Folder, *Facts.Name);
    return Result;
}

FThePlacementStructure FThePlacementResolver::Describe() const
{
    FThePlacementStructure Structure;
    Structure.ContentRoot = ContentRoot;
    Structure.Contexts = Contexts;
    Structure.PrefixKinds = PrefixKinds;
    Structure.KindDirectories = KindDirectories;

    Structure.Rules.Reserve(Rules.Num());
    for(const FRule& Rule : Rules)
    {
        FThePlacementRuleInfo Info;
        Info.Context = Rule.Context;
        Info.FolderTemplate = Rule.Folder;
        Info.Requires = Rule.Requires;
        Info.bRequiresPrefix = !UnprefixedContexts.Contains(Rule.Context);
        Structure.Rules.Add(MoveTemp(Info));
    }
    return Structure;
}

bool FThePlacementResolver::MatchTemplateSegments(const FRule& Rule, const TArray<FString>& Template, int32 TemplateAt, const TArray<FString>& Folder, int32 FolderAt, const FString& Sub, const FString& Kind, TMap<FString, FString>& OutFacts) const
{
    if(TemplateAt >= Template.Num())
    {
        return FolderAt >= Folder.Num();
    }

    const FString& Segment = Template[TemplateAt];
    FString Token;
    if(WholeSegmentToken(Segment, Token))
    {
        if(Token == TEXT("sub") || Token == TEXT("kind_dir"))
        {
            FString Expected = Sub;
            if(Token == TEXT("kind_dir"))
            {
                const FString* Directory = Kind.IsEmpty() ? nullptr : KindDirectories.Find(Kind);
                Expected = Directory ? *Directory : FString();
                if(Expected.IsEmpty())
                {
                    return false;
                }
            }
            if(Expected.IsEmpty())
            {
                return MatchTemplateSegments(Rule, Template, TemplateAt + 1, Folder, FolderAt, Sub, Kind, OutFacts);
            }
            if(FolderAt >= Folder.Num() || !Folder[FolderAt].Equals(Expected, ESearchCase::IgnoreCase))
            {
                return false;
            }
            return MatchTemplateSegments(Rule, Template, TemplateAt + 1, Folder, FolderAt + 1, Sub, Kind, OutFacts);
        }

        const FFactConstraint* Constraint = Rule.Facts.Find(Token);
        bool bCollapsible = false;
        if(Constraint)
        {
            for(const TPair<FString, FString>& Pair : Constraint->Collapse)
            {
                if(Pair.Value.IsEmpty())
                {
                    bCollapsible = true;
                    break;
                }
            }
        }
        if((bCollapsible || Rule.OptionalFacts.Contains(Token)) && MatchTemplateSegments(Rule, Template, TemplateAt + 1, Folder, FolderAt, Sub, Kind, OutFacts))
        {
            return true;
        }

        const int32 Most = FMath::Min(Constraint ? Constraint->Spans : 1, Folder.Num() - FolderAt);
        for(int32 Take = 1; Take <= Most; ++Take)
        {
            TArray<FString> Slice;
            for(int32 Index = 0; Index < Take; ++Index)
            {
                Slice.Add(Folder[FolderAt + Index]);
            }
            const FString Value = FString::Join(Slice, TEXT("/"));
            if(Value.IsEmpty())
            {
                continue;
            }
            if(Constraint)
            {
                if(Constraint->Excluded.Contains(Value))
                {
                    continue;
                }
                if(Constraint->OneOf.Num() > 0 && !Constraint->OneOf.Contains(Value))
                {
                    continue;
                }
                if(!Constraint->Pattern.IsEmpty())
                {
                    const FRegexPattern Pattern(Constraint->Pattern);
                    FRegexMatcher Matcher(Pattern, Value);
                    if(!Matcher.FindNext())
                    {
                        continue;
                    }
                }
            }
            TMap<FString, FString> Nested = OutFacts;
            Nested.Add(Token, Value);
            if(MatchTemplateSegments(Rule, Template, TemplateAt + 1, Folder, FolderAt + Take, Sub, Kind, Nested))
            {
                OutFacts = MoveTemp(Nested);
                return true;
            }
        }
        return false;
    }

    if(Segment.Contains(TEXT("{")))
    {
        return false;
    }
    if(FolderAt >= Folder.Num() || !Segment.Equals(Folder[FolderAt], ESearchCase::IgnoreCase))
    {
        return false;
    }
    return MatchTemplateSegments(Rule, Template, TemplateAt + 1, Folder, FolderAt + 1, Sub, Kind, OutFacts);
}

bool FThePlacementResolver::RuleAccountsForFolder(const FRule& Rule, const TArray<FString>& FolderSegments, const FString& Prefix, const FString& Kind, TMap<FString, FString>& OutFacts, int32& OutLiteralSegments) const
{
    TArray<FString> TemplateSegments;
    Rule.Folder.ParseIntoArray(TemplateSegments, TEXT("/"), /*InCullEmpty*/ true);

    OutFacts.Reset();
    OutLiteralSegments = 0;
    for(const FString& Segment : TemplateSegments)
    {
        if(!Segment.Contains(TEXT("{")))
        {
            ++OutLiteralSegments;
        }
    }

    bool bSubDeclared = false;
    const FString Sub = SubFor(Rule, Prefix, Kind, bSubDeclared);
    return MatchTemplateSegments(Rule, TemplateSegments, 0, FolderSegments, 0, Sub, Kind, OutFacts);
}

FThePathExplanation FThePlacementResolver::Explain(const FString& PackagePath) const
{
    FThePathExplanation Explanation;

    if(Rules.IsEmpty())
    {
        Explanation.Error = TEXT("no placement config loaded");
        return Explanation;
    }

    FString Trimmed = PackagePath;
    Trimmed.TrimStartAndEndInline();
    while(Trimmed.RemoveFromEnd(TEXT("/")))
    {
    }
    if(!Trimmed.StartsWith(ContentRoot + TEXT("/"), ESearchCase::IgnoreCase))
    {
        Explanation.Error = FString::Printf(TEXT("'%s' is not under the configured content root %s"), *PackagePath, *ContentRoot);
        return Explanation;
    }

    TArray<FString> Segments;
    Trimmed.RightChop(ContentRoot.Len() + 1).ParseIntoArray(Segments, TEXT("/"), /*InCullEmpty*/ true);
    if(Segments.Num() < 2)
    {
        Explanation.Error = FString::Printf(TEXT("'%s' names no folder below %s, so no rule can apply"), *PackagePath, *ContentRoot);
        return Explanation;
    }

    Explanation.Name = Segments.Pop();
    Explanation.Folder = FString::Join(Segments, TEXT("/"));
    Explanation.Prefix = ParsePrefix(Explanation.Name);
    if(!Explanation.Prefix.IsEmpty())
    {
        Explanation.Kind = PrefixKinds[Explanation.Prefix];
    }

    struct FMatch
    {
        const FRule* Rule = nullptr;
        TMap<FString, FString> Facts;
        int32 LiteralSegments = 0;
    };

    TArray<FMatch> Matches;
    for(const FRule& Rule : Rules)
    {
        if(Explanation.Prefix.IsEmpty() && !UnprefixedContexts.Contains(Rule.Context))
        {
            continue;
        }
        if(Rule.NameRefuses.ContainsByPredicate([&Explanation](const FNameRefusal& Refusal) { return Explanation.Name.StartsWith(Refusal.StartsWith, ESearchCase::CaseSensitive); }))
        {
            continue;
        }
        if(Redirects.ContainsByPredicate([&Rule, &Explanation](const FRedirect& Redirect) { return Redirect.Context == Rule.Context && Redirect.Prefix == Explanation.Prefix; }))
        {
            continue;
        }

        FMatch Match;
        if(RuleAccountsForFolder(Rule, Segments, Explanation.Prefix, Explanation.Kind, Match.Facts, Match.LiteralSegments))
        {
            Match.Rule = &Rule;
            Matches.Add(MoveTemp(Match));
        }
    }

    if(Matches.IsEmpty())
    {
        Explanation.Outcome = ETheExplanationOutcome::Unmatched;
        Explanation.Error = FString::Printf(TEXT("'%s' satisfies no rule in this taxonomy"), *PackagePath);
        for(const FRule& Rule : Rules)
        {
            TArray<FString> TemplateSegments;
            Rule.Folder.ParseIntoArray(TemplateSegments, TEXT("/"), /*InCullEmpty*/ true);
            if(!TemplateSegments.IsEmpty() && TemplateSegments[0].Equals(Segments[0], ESearchCase::IgnoreCase))
            {
                Explanation.CandidateContexts.AddUnique(Rule.Context);
            }
        }
        return Explanation;
    }

    int32 MostLiteral = 0;
    for(const FMatch& Match : Matches)
    {
        MostLiteral = FMath::Max(MostLiteral, Match.LiteralSegments);
    }

    TArray<const FMatch*> Best;
    for(const FMatch& Match : Matches)
    {
        if(Match.LiteralSegments == MostLiteral)
        {
            Best.Add(&Match);
        }
    }

    if(Best.Num() > 1)
    {
        Explanation.Outcome = ETheExplanationOutcome::Ambiguous;
        for(const FMatch* Match : Best)
        {
            Explanation.CandidateContexts.AddUnique(Match->Rule->Context);
        }
        Explanation.CandidateContexts.Sort();
        Explanation.Error = FString::Printf(TEXT("'%s' satisfies %d rules equally well: %s"), *PackagePath, Explanation.CandidateContexts.Num(), *FString::Join(Explanation.CandidateContexts, TEXT(", ")));
        return Explanation;
    }

    Explanation.Outcome = ETheExplanationOutcome::Matched;
    Explanation.Context = Best[0]->Rule->Context;
    Explanation.FolderTemplate = Best[0]->Rule->Folder;
    Explanation.Facts = Best[0]->Facts;
    return Explanation;
}
