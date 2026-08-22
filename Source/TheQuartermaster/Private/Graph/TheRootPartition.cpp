// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Graph/TheRootPartition.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Graph/TheDependencyClosure.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "TheQuartermasterModule.h"

namespace
{
int64 SizeOnDisk(const FString& PackageName)
{
    FString Filename;
    if(!FPackageName::DoesPackageExist(PackageName, &Filename))
    {
        return 0;
    }
    const int64 Size = IFileManager::Get().FileSize(*Filename);
    return Size > 0 ? Size : 0;
}
}

bool FTheRootPartition::Survey(const TArray<FString>& Exclude, FTheRootPartitionResult& OutResult, FString& OutError)
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.SearchAllAssets(/*bSynchronousSearch*/ true);
    Registry.WaitForCompletion();

    TArray<FAssetData> Assets;
    Registry.GetAssetsByPath(FName(TEXT("/Game")), Assets, /*bRecursive*/ true, /*bIncludeOnlyOnDiskAssets*/ true);
    if(Assets.IsEmpty())
    {
        OutError = TEXT("the registry reported no packages under /Game");
        return false;
    }

    TSet<FString> Excluded(Exclude);
    TMap<FString, FTheRootProfile> Profiles;
    TMap<FName, FString> RootOfPackage;

    for(const FAssetData& Asset : Assets)
    {
        const FString PackageName = Asset.PackageName.ToString();
        const FString Root = FTheDependencyClosure::TopLevelRootOf(PackageName);
        if(Root.IsEmpty() || Excluded.Contains(Root))
        {
            continue;
        }

        FTheRootProfile& Profile = Profiles.FindOrAdd(Root);
        Profile.Root = Root;

        if(FTheDependencyClosure::IsExternalActorPackage(PackageName))
        {
            Profile.ExternalActors++;
            OutResult.TotalExternalActors++;
            continue;
        }

        if(RootOfPackage.Contains(Asset.PackageName))
        {
            continue;
        }
        RootOfPackage.Add(Asset.PackageName, Root);

        Profile.Packages++;
        Profile.Bytes += SizeOnDisk(PackageName);
        OutResult.TotalPackages++;
    }

    for(const TPair<FName, FString>& Entry : RootOfPackage)
    {
        const FName Package = Entry.Key;
        const FString& Root = Entry.Value;
        FTheRootProfile& Profile = Profiles[Root];

        TArray<FName> Referencers;
        Registry.GetReferencers(Package, Referencers, UE::AssetRegistry::EDependencyCategory::All, UE::AssetRegistry::FDependencyQuery());
        for(const FName& Source : Referencers)
        {
            if(FTheDependencyClosure::TopLevelRootOf(Source.ToString()) != Root)
            {
                Profile.InboundReferencers++;
            }
        }

        TArray<FName> Dependencies;
        Registry.GetDependencies(Package, Dependencies, UE::AssetRegistry::EDependencyCategory::All, UE::AssetRegistry::FDependencyQuery());
        for(const FName& Target : Dependencies)
        {
            if(FTheDependencyClosure::TopLevelRootOf(Target.ToString()) != Root)
            {
                Profile.OutboundDependencies++;
            }
        }
    }

    Profiles.GenerateValueArray(OutResult.Roots);
    for(FTheRootProfile& Profile : OutResult.Roots)
    {
        Profile.bIsIsland = Profile.InboundReferencers == 0 && Profile.Packages > 0;
        OutResult.TotalBytes += Profile.Bytes;
    }
    OutResult.Roots.Sort([](const FTheRootProfile& A, const FTheRootProfile& B)
    {
        return A.Packages > B.Packages;
    });

    int32 Islands = 0;
    for(const FTheRootProfile& Profile : OutResult.Roots)
    {
        Islands += Profile.bIsIsland ? 1 : 0;
    }

    UE_LOG(LogTheQuartermaster, Display, TEXT("THEQM_PARTITION roots=%d packages=%d external_actors=%d islands=%d mib=%.1f"),
        OutResult.Roots.Num(),
        OutResult.TotalPackages,
        OutResult.TotalExternalActors,
        Islands,
        static_cast<double>(OutResult.TotalBytes) / (1024.0 * 1024.0));
    return true;
}

bool FTheRootPartition::WriteReport(const FTheRootPartitionResult& Result, const FString& FilePath, FString& OutError)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("document_type"), TEXT("thequartermaster_root_partition"));
    Root->SetNumberField(TEXT("schema_version"), 1);
    Root->SetBoolField(TEXT("authorizes_mutation"), false);
    Root->SetNumberField(TEXT("total_packages"), Result.TotalPackages);
    Root->SetNumberField(TEXT("total_external_actors"), Result.TotalExternalActors);
    Root->SetNumberField(TEXT("total_bytes"), static_cast<double>(Result.TotalBytes));

    TArray<TSharedPtr<FJsonValue>> Roots;
    for(const FTheRootProfile& Profile : Result.Roots)
    {
        TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("root"), Profile.Root);
        Entry->SetNumberField(TEXT("packages"), Profile.Packages);
        Entry->SetNumberField(TEXT("bytes"), static_cast<double>(Profile.Bytes));
        Entry->SetNumberField(TEXT("inbound_referencers"), Profile.InboundReferencers);
        Entry->SetNumberField(TEXT("outbound_dependencies"), Profile.OutboundDependencies);
        Entry->SetNumberField(TEXT("external_actors"), Profile.ExternalActors);
        Entry->SetBoolField(TEXT("is_island"), Profile.bIsIsland);
        Roots.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Root->SetArrayField(TEXT("roots"), Roots);

    FString Serialized;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
    if(!FJsonSerializer::Serialize(Root, Writer))
    {
        OutError = TEXT("failed to serialise the partition report");
        return false;
    }

    const FString FullPath = FPaths::ConvertRelativePathToFull(FilePath);
    if(!FFileHelper::SaveStringToFile(Serialized, *FullPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FString::Printf(TEXT("failed to write %s"), *FullPath);
        return false;
    }
    UE_LOG(LogTheQuartermaster, Display, TEXT("THEQM_PARTITION_REPORT %s"), *FullPath);
    return true;
}
