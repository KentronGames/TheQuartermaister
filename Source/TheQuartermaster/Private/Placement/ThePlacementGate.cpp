// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Placement/ThePlacementGate.h"

#include "UObject/Class.h"

#include "Placement/ThePlacementResolver.h"
#include "TheQuartermasterModule.h"

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
        UE_LOG(LogTheQuartermaster, Verbose, TEXT("PlacementGate: %s - folder '%s' is not one of %d kind folder(s)"), *Package, *KindFolder, KindFolders.Num());
        return false;
    }
    const bool bMatched = Resolver.Explain(FamilyRoot / Name).Outcome == ETheExplanationOutcome::Matched;
    UE_LOG(LogTheQuartermaster, Verbose, TEXT("PlacementGate: %s - kind folder '%s', family root %s explains it=%d"), *Package, *KindFolder, *(FamilyRoot / Name), bMatched ? 1 : 0);
    return bMatched;
}

FString FThePlacementGate::NameFor(const UClass* AssetClass, const FString& Name)
{
    FThePlacementResolver Resolver;
    FString LoadError;
    if(!FThePlacementResolver::LoadProjectConfig(Resolver, LoadError))
    {
        UE_LOG(LogTheQuartermaster, Verbose, TEXT("PlacementGate.NameFor: '%s' kept - taxonomy did not load: %s"), *Name, *LoadError);
        return Name;
    }
    for(const UClass* Class = AssetClass; Class; Class = Class->GetSuperClass())
    {
        const FString Prefix = Resolver.PrefixForClass(Class->GetName());
        if(!Prefix.IsEmpty())
        {
            const FString Prefixed = Resolver.NameWithPrefix(Name, Prefix);
            UE_LOG(LogTheQuartermaster, Verbose, TEXT("PlacementGate.NameFor: '%s' (class %s) -> '%s' via prefix '%s' of %s"), *Name, AssetClass ? *AssetClass->GetName() : TEXT("<null>"), *Prefixed, *Prefix, *Class->GetName());
            return Prefixed;
        }
    }
    UE_LOG(LogTheQuartermaster, Verbose, TEXT("PlacementGate.NameFor: '%s' (class %s) kept - no prefix in its class chain"), *Name, AssetClass ? *AssetClass->GetName() : TEXT("<null>"));
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
            UE_LOG(LogTheQuartermaster, Verbose, TEXT("PlacementGate.Refusal: %s - outside the taxonomy, passed"), *Package);
            continue;
        }

        const FThePathExplanation Explanation = Resolver.Explain(Package);
        UE_LOG(LogTheQuartermaster, Verbose, TEXT("PlacementGate.Refusal: %s - outcome %d, %d candidate context(s), error '%s'"), *Package, static_cast<int32>(Explanation.Outcome), Explanation.CandidateContexts.Num(), *Explanation.Error);
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

    UE_LOG(LogTheQuartermaster, Verbose, TEXT("PlacementGate.Refusal: %d path(s) given, %d distinct, %d finding(s)"), PackagePaths.Num(), Seen.Num(), Findings.Num());
    if(Findings.IsEmpty())
    {
        return FString();
    }
    return FString::Printf(TEXT("placement refused before any write - %d destination(s) the taxonomy cannot account for, so the result would have to be relocated later:\n%s\nAsk PlaceAsset for the address and pass that instead."),
        Findings.Num(),
        *FString::Join(Findings, TEXT("\n")));
}
