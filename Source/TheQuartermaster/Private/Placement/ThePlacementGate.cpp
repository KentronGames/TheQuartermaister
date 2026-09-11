// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Placement/ThePlacementGate.h"

#include "UObject/Class.h"

#include "Placement/ThePlacementResolver.h"

bool FThePlacementGate::IsExplainedAtFamilyRoot(const FThePlacementResolver& Resolver, const FString& Package)
{
    FString Folder;
    FString Name;
    if(!Package.Split(TEXT("/"), &Folder, &Name, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
    {
        return false;
    }
    FString FamilyRoot;
    FString KindFolder;
    if(!Folder.Split(TEXT("/"), &FamilyRoot, &KindFolder, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
    {
        return false;
    }

    TArray<FString> KindFolders;
    Resolver.Describe().KindDirectories.GenerateValueArray(KindFolders);
    if(!KindFolders.ContainsByPredicate([&KindFolder](const FString& Kind) { return Kind.Equals(KindFolder, ESearchCase::IgnoreCase); }))
    {
        return false;
    }
    return Resolver.Explain(FamilyRoot / Name).Outcome == ETheExplanationOutcome::Matched;
}

FString FThePlacementGate::NameFor(const UClass* AssetClass, const FString& Name)
{
    FThePlacementResolver Resolver;
    FString LoadError;
    if(!FThePlacementResolver::LoadProjectConfig(Resolver, LoadError))
    {
        return Name;
    }
    for(const UClass* Class = AssetClass; Class; Class = Class->GetSuperClass())
    {
        const FString Prefix = Resolver.PrefixForClass(Class->GetName());
        if(!Prefix.IsEmpty())
        {
            return Resolver.NameWithPrefix(Name, Prefix);
        }
    }
    return Name;
}

FString FThePlacementGate::Refusal(const TArray<FString>& PackagePaths)
{
    FThePlacementResolver Resolver;
    FString LoadError;
    if(!FThePlacementResolver::LoadProjectConfig(Resolver, LoadError))
    {
        return FString::Printf(TEXT("placement refused before any write - the taxonomy did not load: %s"), *LoadError);
    }

    TSet<FString> Seen;
    TArray<FString> Findings;
    for(const FString& Path : PackagePaths)
    {
        FString Package = Path;
        Package.Split(TEXT("."), &Package, nullptr);
        if(Package.IsEmpty() || Seen.Contains(Package))
        {
            continue;
        }
        Seen.Add(Package);
        if(Resolver.IsOutsideTaxonomy(Package))
        {
            continue;
        }

        const FThePathExplanation Explanation = Resolver.Explain(Package);
        if(Explanation.Outcome == ETheExplanationOutcome::Matched || IsExplainedAtFamilyRoot(Resolver, Package))
        {
            continue;
        }
        if(!Explanation.Error.IsEmpty())
        {
            Findings.Add(FString::Printf(TEXT("%s - %s"), *Package, *Explanation.Error));
        }
        else if(Explanation.Outcome == ETheExplanationOutcome::Ambiguous)
        {
            Findings.Add(FString::Printf(TEXT("%s - several rules explain it equally (%s)"), *Package, *FString::Join(Explanation.CandidateContexts, TEXT(", "))));
        }
        else
        {
            const FString Candidates = Explanation.CandidateContexts.IsEmpty() ? FString(TEXT("none share its first folder")) : FString::Join(Explanation.CandidateContexts, TEXT(", "));
            Findings.Add(FString::Printf(TEXT("%s - no taxonomy rule explains it (nearest contexts: %s)"), *Package, *Candidates));
        }
    }

    if(Findings.IsEmpty())
    {
        return FString();
    }
    return FString::Printf(TEXT("placement refused before any write - %d destination(s) the taxonomy cannot account for, so the result would have to be relocated later:\n%s\nAsk PlaceAsset for the address and pass that instead."),
        Findings.Num(),
        *FString::Join(Findings, TEXT("\n")));
}
