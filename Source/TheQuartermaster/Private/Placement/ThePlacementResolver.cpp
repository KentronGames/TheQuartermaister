// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Placement/ThePlacementResolver.h"

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

/** The placeholder name when a template segment is exactly one placeholder, e.g. {family}. */
bool WholeSegmentToken(const FString& Segment, FString& OutToken)
{
    if(Segment.Len() < 3 || !Segment.StartsWith(TEXT("{")) || !Segment.EndsWith(TEXT("}")))
    {
        return false;
    }
    OutToken = Segment.Mid(1, Segment.Len() - 2);
    // A segment such as {a}x{b} also starts and ends with braces without being one placeholder.
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
        // No default and no search. A guessed taxonomy is worse than none: it answers confidently
        // with another project's structure, and nothing in the answer says so.
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
        if(Rule.Context.IsEmpty() || Rule.Folder.IsEmpty())
        {
            OutError = TEXT("every placement rule needs a context and a folder");
            return false;
        }
        Rules.Add(MoveTemp(Rule));
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
        return Reject(FString::Printf(TEXT("context '%s' is not in the configured set: %s"),
            *Facts.Context, *FString::Join(Contexts, TEXT(", "))));
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
        // A name that LOOKS prefixed but carries an unknown prefix is a naming-table violation.
        // Guessing a folder for it would quietly launder the violation into the structure.
        if(Facts.Name.FindChar(TEXT('_'), Underscore) && Underscore > 0)
        {
            return Reject(FString::Printf(TEXT("'%s' carries prefix '%s', which the config's prefix table does not declare; add it there first"),
                *Facts.Name, *Facts.Name.Left(Underscore)));
        }
        return Reject(FString::Printf(TEXT("'%s' has no prefix, and context '%s' requires one"), *Facts.Name, *Facts.Context));
    }

    const FRule* Matched = Rules.FindByPredicate([&Facts](const FRule& Rule) { return Rule.Context == Facts.Context; });
    if(!Matched)
    {
        return Reject(FString::Printf(TEXT("no rule for context '%s'"), *Facts.Context));
    }

    // Facts are resolved as a set before any substitution, so the caller gets the WHOLE closed list
    // of what is missing in one answer instead of discovering it one round trip at a time.
    TArray<FString> Missing;
    for(const FString& Required : Matched->Requires)
    {
        const FString* Value = Facts.Values.Find(Required);
        if(!Value || Value->IsEmpty())
        {
            Missing.AddUnique(Required);
        }
    }

    FString Folder = Matched->Folder;
    if(Folder.Contains(TEXT("{kind_dir}")))
    {
        const FString* Directory = Result.Kind.IsEmpty() ? nullptr : KindDirectories.Find(Result.Kind);
        if(!Directory)
        {
            return Reject(FString::Printf(TEXT("context '%s' places by resource kind, but kind '%s' has no directory in kind_directories"),
                *Facts.Context, *Result.Kind));
        }
        Folder = Folder.Replace(TEXT("{kind_dir}"), **Directory);
    }

    for(const TPair<FString, FString>& Pair : Facts.Values)
    {
        Folder = Folder.Replace(*FString::Printf(TEXT("{%s}"), *Pair.Key), *Pair.Value);
    }

    int32 OpenBrace = INDEX_NONE;
    if(Folder.FindChar(TEXT('{'), OpenBrace))
    {
        int32 CloseBrace = INDEX_NONE;
        Folder.FindChar(TEXT('}'), CloseBrace);
        if(CloseBrace > OpenBrace)
        {
            Missing.AddUnique(Folder.Mid(OpenBrace + 1, CloseBrace - OpenBrace - 1));
        }
    }

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

bool FThePlacementResolver::RuleAccountsForFolder(const FRule& Rule, const TArray<FString>& FolderSegments,
    const FString& Kind, TMap<FString, FString>& OutFacts, int32& OutLiteralSegments) const
{
    TArray<FString> TemplateSegments;
    Rule.Folder.ParseIntoArray(TemplateSegments, TEXT("/"), /*InCullEmpty*/ true);
    if(TemplateSegments.Num() != FolderSegments.Num())
    {
        return false;
    }

    OutFacts.Reset();
    OutLiteralSegments = 0;

    for(int32 Index = 0; Index < TemplateSegments.Num(); ++Index)
    {
        const FString& Template = TemplateSegments[Index];
        const FString& Actual = FolderSegments[Index];

        FString Token;
        if(!WholeSegmentToken(Template, Token))
        {
            // A template segment that is neither a whole placeholder nor plain literal text cannot
            // be read backwards without guessing where the placeholder ends. Refuse the rule.
            if(Template.Contains(TEXT("{")))
            {
                return false;
            }
            if(!Template.Equals(Actual, ESearchCase::IgnoreCase))
            {
                return false;
            }
            ++OutLiteralSegments;
            continue;
        }

        if(Token == TEXT("kind_dir"))
        {
            // The kind comes from the asset's own prefix, so this segment is fixed by the name
            // rather than free: it is evidence, not a captured fact.
            const FString* Directory = Kind.IsEmpty() ? nullptr : KindDirectories.Find(Kind);
            if(!Directory || !Directory->Equals(Actual, ESearchCase::IgnoreCase))
            {
                return false;
            }
            ++OutLiteralSegments;
            continue;
        }

        if(Actual.IsEmpty())
        {
            return false;
        }
        OutFacts.Add(Token, Actual);
    }
    return true;
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
        // A context that demands a prefix cannot have produced a package whose name carries none;
        // Resolve would have refused it, so accepting it here would let the two disagree.
        if(Explanation.Prefix.IsEmpty() && !UnprefixedContexts.Contains(Rule.Context))
        {
            continue;
        }

        FMatch Match;
        if(RuleAccountsForFolder(Rule, Segments, Explanation.Kind, Match.Facts, Match.LiteralSegments))
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

    // More literal segments means the template pinned more of the path down, so it is the more
    // specific account of it. Equal specificity is a genuine ambiguity in the taxonomy.
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
        Explanation.Error = FString::Printf(TEXT("'%s' satisfies %d rules equally well: %s"),
            *PackagePath, Explanation.CandidateContexts.Num(), *FString::Join(Explanation.CandidateContexts, TEXT(", ")));
        return Explanation;
    }

    Explanation.Outcome = ETheExplanationOutcome::Matched;
    Explanation.Context = Best[0]->Rule->Context;
    Explanation.FolderTemplate = Best[0]->Rule->Folder;
    Explanation.Facts = Best[0]->Facts;
    return Explanation;
}
