// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Graph/TheDependencyClosure.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Graph/TheTextReferenceScan.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "TheQuartermasterModule.h"

FString FTheDependencyClosure::TopLevelRootOf(const FString& PackageName)
{
    TArray<FString> Parts;
    PackageName.ParseIntoArray(Parts, TEXT("/"), /*InCullEmpty*/ true);
    if(Parts.Num() >= 2)
    {
        return FString::Printf(TEXT("/%s/%s"), *Parts[0], *Parts[1]);
    }
    return FString();
}

bool FTheDependencyClosure::IsExternalActorPackage(const FString& PackageName)
{
    return PackageName.Contains(TEXT("/__ExternalActors__/")) || PackageName.Contains(TEXT("/__ExternalObjects__/"));
}

bool FTheDependencyClosure::Solve(const FTheClosureRequest& Request, FTheClosureResult& OutResult, FString& OutError)
{
    if(Request.Roots.IsEmpty())
    {
        OutError = TEXT("no roots given");
        return false;
    }

    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    // Synchronous on purpose: a partially scanned registry answers "nothing references this" for
    // packages it has not read yet, which is the most dangerous wrong answer this code can give.
    Registry.SearchAllAssets(/*bSynchronousSearch*/ true);
    Registry.WaitForCompletion();

    TSet<FName> Candidates;
    for(const FString& Root : Request.Roots)
    {
        FString Clean = Root;
        Clean.RemoveFromEnd(TEXT("/"));

        TArray<FAssetData> Assets;
        Registry.GetAssetsByPath(FName(*Clean), Assets, /*bRecursive*/ true, /*bIncludeOnlyOnDiskAssets*/ true);
        if(Assets.IsEmpty())
        {
            OutError = FString::Printf(TEXT("root holds no packages: %s"), *Clean);
            return false;
        }

        for(const FAssetData& Asset : Assets)
        {
            const FString PackageName = Asset.PackageName.ToString();
            if(IsExternalActorPackage(PackageName))
            {
                OutResult.ExternalActorsExcluded++;
                continue;
            }
            Candidates.Add(Asset.PackageName);
        }
    }

    if(Candidates.IsEmpty())
    {
        OutError = TEXT("every candidate was an external actor package; nothing to solve");
        return false;
    }

    // All categories at once - Package, Manage and SearchableName - with no property filter.
    // Anything narrower would let a referencer through on a technicality.
    TMap<FName, TArray<FName>> Referencers;
    Referencers.Reserve(Candidates.Num());
    for(const FName& Package : Candidates)
    {
        TArray<FName> Found;
        Registry.GetReferencers(Package, Found, UE::AssetRegistry::EDependencyCategory::All, UE::AssetRegistry::FDependencyQuery());
        Found.RemoveAll([&Package](const FName& Value) { return Value == Package; });
        Found.Sort(FNameLexicalLess());
        Referencers.Add(Package, MoveTemp(Found));
    }

    // A text mention is modelled as an ordinary external referencer rather than as a separate
    // veto, so the fixpoint propagates it the same way: whatever the text-held package still uses
    // is held too.
    if(Request.bIncludeTextReferences)
    {
        FTheTextScanResult TextScan;
        if(!FTheTextReferenceScan::Scan(TextScan, OutError))
        {
            return false;
        }
        static const FName TextHolder(TEXT("__product_text__"));
        for(const FName& Package : Candidates)
        {
            if(const TArray<FString>* Files = TextScan.ReferencedPackages.Find(Package.ToString()))
            {
                Referencers[Package].Add(TextHolder);
                OutResult.TextHeldBy.Add(Package, *Files);
            }
        }
    }

    TSet<FName> Safe = Candidates;
    for(;;)
    {
        TArray<FName> Dropped;
        for(const FName& Package : Safe)
        {
            const TArray<FName>& Sources = Referencers[Package];
            const bool bHeldFromOutside = Sources.ContainsByPredicate([&Safe](const FName& Source) { return !Safe.Contains(Source); });
            if(bHeldFromOutside)
            {
                Dropped.Add(Package);
            }
        }
        if(Dropped.IsEmpty())
        {
            break;
        }
        for(const FName& Package : Dropped)
        {
            Safe.Remove(Package);
        }
        OutResult.FixpointRounds++;
    }

    OutResult.Candidates = Candidates.Array();
    OutResult.Candidates.Sort(FNameLexicalLess());
    OutResult.Safe = Safe.Array();
    OutResult.Safe.Sort(FNameLexicalLess());

    TMap<FString, int32> Holders;
    for(const FName& Package : OutResult.Candidates)
    {
        if(Safe.Contains(Package))
        {
            continue;
        }
        OutResult.Held.Add(Package);

        TArray<FName> Outside;
        for(const FName& Source : Referencers[Package])
        {
            if(!Candidates.Contains(Source))
            {
                Outside.Add(Source);
                Holders.FindOrAdd(TopLevelRootOf(Source.ToString()))++;
            }
        }
        OutResult.HeldBy.Add(Package, MoveTemp(Outside));
    }

    for(const TPair<FString, int32>& Pair : Holders)
    {
        OutResult.HoldingRoots.Add(Pair);
    }
    OutResult.HoldingRoots.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B)
    {
        return A.Value > B.Value;
    });

    UE_LOG(LogTheQuartermaster, Display, TEXT("THEQM_CLOSURE candidates=%d safe=%d held=%d text_held=%d rounds=%d external_actors_excluded=%d"),
        OutResult.Candidates.Num(),
        OutResult.Safe.Num(),
        OutResult.Held.Num(),
        OutResult.TextHeldBy.Num(),
        OutResult.FixpointRounds,
        OutResult.ExternalActorsExcluded);
    return true;
}

bool FTheDependencyClosure::WriteReport(const FTheClosureResult& Result, const FString& FilePath, FString& OutError)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("document_type"), TEXT("thequartermaster_dependency_closure"));
    Root->SetNumberField(TEXT("schema_version"), 1);

    // Stated in the artifact, not only in the code: a reader that treats this as permission to
    // delete has misread it. The mutating lane re-derives its own closure.
    Root->SetBoolField(TEXT("authorizes_mutation"), false);
    Root->SetStringField(TEXT("authorization_note"), TEXT("Planning artifact. The registry reflects on-disk state at last save; re-derive a fresh closure immediately before any mutation."));

    Root->SetNumberField(TEXT("candidate_count"), Result.Candidates.Num());
    Root->SetNumberField(TEXT("safe_count"), Result.Safe.Num());
    Root->SetNumberField(TEXT("held_count"), Result.Held.Num());
    Root->SetNumberField(TEXT("fixpoint_rounds"), Result.FixpointRounds);
    Root->SetNumberField(TEXT("external_actors_excluded"), Result.ExternalActorsExcluded);

    const auto NameArray = [](const TArray<FName>& Names)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Reserve(Names.Num());
        for(const FName& Name : Names)
        {
            Values.Add(MakeShared<FJsonValueString>(Name.ToString()));
        }
        return Values;
    };

    Root->SetArrayField(TEXT("safe_packages"), NameArray(Result.Safe));

    TSharedRef<FJsonObject> HeldBy = MakeShared<FJsonObject>();
    for(const TPair<FName, TArray<FName>>& Pair : Result.HeldBy)
    {
        HeldBy->SetArrayField(Pair.Key.ToString(), NameArray(Pair.Value));
    }
    Root->SetObjectField(TEXT("held_packages"), HeldBy);

    TSharedRef<FJsonObject> HoldingRoots = MakeShared<FJsonObject>();
    for(const TPair<FString, int32>& Pair : Result.HoldingRoots)
    {
        HoldingRoots->SetNumberField(Pair.Key, Pair.Value);
    }
    Root->SetObjectField(TEXT("holding_roots"), HoldingRoots);

    // Kept separate from held_packages: these are held by something the registry cannot see, so a
    // reader deciding whether to trust the verdict needs to know which kind of evidence held them.
    Root->SetNumberField(TEXT("text_held_count"), Result.TextHeldBy.Num());
    TSharedRef<FJsonObject> TextHeld = MakeShared<FJsonObject>();
    for(const TPair<FName, TArray<FString>>& Pair : Result.TextHeldBy)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        for(const FString& File : Pair.Value)
        {
            Values.Add(MakeShared<FJsonValueString>(File));
        }
        TextHeld->SetArrayField(Pair.Key.ToString(), Values);
    }
    Root->SetObjectField(TEXT("text_held_packages"), TextHeld);

    FString Serialized;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
    if(!FJsonSerializer::Serialize(Root, Writer))
    {
        OutError = TEXT("failed to serialise the closure report");
        return false;
    }

    const FString FullPath = FPaths::ConvertRelativePathToFull(FilePath);
    if(!FFileHelper::SaveStringToFile(Serialized, *FullPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FString::Printf(TEXT("failed to write %s"), *FullPath);
        return false;
    }
    UE_LOG(LogTheQuartermaster, Display, TEXT("THEQM_CLOSURE_REPORT %s"), *FullPath);
    return true;
}
