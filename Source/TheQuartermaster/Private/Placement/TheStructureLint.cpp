// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Placement/TheStructureLint.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Graph/TheDependencyClosure.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "TheQuartermasterModule.h"

namespace
{
/** A pattern segment matches one path segment: `*` matches any, anything else matches itself. */
bool SegmentMatches(const FString& PatternSegment, const FString& PathSegment)
{
    return PatternSegment == TEXT("*") || PatternSegment.Equals(PathSegment, ESearchCase::IgnoreCase);
}

/** Number of leading segments matched by the pattern, or INDEX_NONE when it does not apply. */
int32 MatchedDepth(const TArray<FString>& Segments, const FString& Pattern, bool bExactLength)
{
    TArray<FString> PatternSegments;
    Pattern.ParseIntoArray(PatternSegments, TEXT("/"), /*InCullEmpty*/ true);
    if(PatternSegments.IsEmpty())
    {
        return INDEX_NONE;
    }
    const bool bLengthFits = bExactLength
        ? Segments.Num() == PatternSegments.Num()
        : Segments.Num() > PatternSegments.Num();
    if(!bLengthFits)
    {
        return INDEX_NONE;
    }
    for(int32 Index = 0; Index < PatternSegments.Num(); ++Index)
    {
        if(!SegmentMatches(PatternSegments[Index], Segments[Index]))
        {
            return INDEX_NONE;
        }
    }
    return PatternSegments.Num();
}

ETheLintSeverity SeverityOf(const FString& Rule, const FTheLintConfig& Config)
{
    const ETheLintSeverity* Configured = Config.RuleSeverity.Find(Rule);
    return Configured ? *Configured : ETheLintSeverity::Error;
}
}

FString FTheStructureLint::FamilyOf(const FString& PackageName, const FTheLintConfig& Config)
{
    const FString Prefix = Config.ContentRoot / Config.FamilyRoot / TEXT("");
    if(!PackageName.StartsWith(Prefix))
    {
        return FString();
    }
    const FString SharedPrefix = Config.ContentRoot / Config.SharedFolder / TEXT("");
    if(PackageName.StartsWith(SharedPrefix))
    {
        return FString();
    }

    TArray<FString> Segments;
    PackageName.RightChop(Prefix.Len()).ParseIntoArray(Segments, TEXT("/"), /*InCullEmpty*/ true);

    for(const FString& Pattern : Config.FamilyPatterns)
    {
        // The last segment is the asset itself, so naming a family needs the pattern to be matched
        // by segments that all sit above it.
        const int32 Depth = MatchedDepth(Segments, Pattern, /*bExactLength*/ false);
        if(Depth == INDEX_NONE)
        {
            continue;
        }
        TArray<FString> FamilySegments(Segments);
        FamilySegments.SetNum(Depth);
        return FString::Join(FamilySegments, TEXT("/"));
    }

    return FString();
}

bool FTheStructureLint::IsAssembly(const FString& Family, const FTheLintConfig& Config)
{
    if(Family.IsEmpty())
    {
        return false;
    }
    TArray<FString> Segments;
    Family.ParseIntoArray(Segments, TEXT("/"), /*InCullEmpty*/ true);
    for(const FString& Pattern : Config.AssemblyPatterns)
    {
        // A family is already the exact path, so the pattern has to cover all of it.
        if(MatchedDepth(Segments, Pattern, /*bExactLength*/ true) != INDEX_NONE)
        {
            return true;
        }
    }
    return false;
}

bool FTheStructureLint::LoadConfig(const FString& FilePath, FTheLintConfig& OutConfig, FString& OutError)
{
    FString Json;
    if(!FFileHelper::LoadFileToString(Json, *FPaths::ConvertRelativePathToFull(FilePath)))
    {
        OutError = FString::Printf(TEXT("cannot read config: %s"), *FilePath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if(!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("config is not valid JSON");
        return false;
    }

    Root->TryGetStringField(TEXT("content_root"), OutConfig.ContentRoot);
    OutConfig.ContentRoot.RemoveFromEnd(TEXT("/"));

    const TSharedPtr<FJsonObject>* Lint = nullptr;
    if(!Root->TryGetObjectField(TEXT("lint"), Lint))
    {
        OutError = TEXT("config declares no lint section");
        return false;
    }
    (*Lint)->TryGetStringField(TEXT("shared_folder"), OutConfig.SharedFolder);
    (*Lint)->TryGetStringField(TEXT("family_root"), OutConfig.FamilyRoot);
    (*Lint)->TryGetStringArrayField(TEXT("forbidden_folder_prefixes"), OutConfig.ForbiddenFolderPrefixes);

    (*Lint)->TryGetStringArrayField(TEXT("family_patterns"), OutConfig.FamilyPatterns);
    (*Lint)->TryGetStringArrayField(TEXT("assembly_patterns"), OutConfig.AssemblyPatterns);
    int32 Depth = 0;
    if(OutConfig.FamilyPatterns.IsEmpty() && (*Lint)->TryGetNumberField(TEXT("family_depth"), Depth))
    {
        // A uniform depth is the one-pattern case, kept so an existing config still loads.
        TArray<FString> Wildcards;
        for(int32 Index = 0; Index < FMath::Max(1, Depth); ++Index)
        {
            Wildcards.Add(TEXT("*"));
        }
        OutConfig.FamilyPatterns.Add(FString::Join(Wildcards, TEXT("/")));
    }
    if(OutConfig.FamilyPatterns.IsEmpty())
    {
        OutError = TEXT("lint config declares neither family_patterns nor family_depth");
        return false;
    }

    const TSharedPtr<FJsonObject>* Severities = nullptr;
    if((*Lint)->TryGetObjectField(TEXT("rule_severity"), Severities))
    {
        for(const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Severities)->Values)
        {
            const FString Value = Pair.Value.IsValid() ? Pair.Value->AsString() : FString();
            if(Value.Equals(TEXT("warning"), ESearchCase::IgnoreCase))
            {
                OutConfig.RuleSeverity.Add(Pair.Key, ETheLintSeverity::Warning);
            }
            else if(Value.Equals(TEXT("error"), ESearchCase::IgnoreCase))
            {
                OutConfig.RuleSeverity.Add(Pair.Key, ETheLintSeverity::Error);
            }
            else
            {
                OutError = FString::Printf(TEXT("rule_severity['%s'] must be 'error' or 'warning', got '%s'"),
                    *Pair.Key, *Value);
                return false;
            }
        }
    }

    TArray<FString> Acknowledged;
    (*Lint)->TryGetStringArrayField(TEXT("acknowledged_cross_family"), Acknowledged);
    OutConfig.AcknowledgedCrossFamily.Append(Acknowledged);
    return true;
}

bool FTheStructureLint::Run(const FTheLintConfig& Config, FTheLintResult& OutResult, FString& OutError)
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.SearchAllAssets(/*bSynchronousSearch*/ true);
    Registry.WaitForCompletion();

    TArray<FAssetData> Assets;
    Registry.GetAssetsByPath(FName(*Config.ContentRoot), Assets, /*bRecursive*/ true, /*bIncludeOnlyOnDiskAssets*/ true);
    if(Assets.IsEmpty())
    {
        OutError = FString::Printf(TEXT("the registry reported no packages under %s"), *Config.ContentRoot);
        return false;
    }

    const FString SharedPrefix = Config.ContentRoot / Config.SharedFolder / TEXT("");
    const FString FamilyPrefix = Config.ContentRoot / Config.FamilyRoot / TEXT("");

    for(const FAssetData& Asset : Assets)
    {
        const FString PackageName = Asset.PackageName.ToString();
        if(FTheDependencyClosure::IsExternalActorPackage(PackageName))
        {
            continue;
        }
        OutResult.PackagesChecked++;

        for(const FString& Forbidden : Config.ForbiddenFolderPrefixes)
        {
            if(PackageName.Contains(FString::Printf(TEXT("/%s"), *Forbidden)))
            {
                const FString Rule = TEXT("forbidden-folder-prefix");
                OutResult.Findings.Add({Rule, PackageName,
                    FString::Printf(TEXT("a path segment starts with '%s'"), *Forbidden),
                    SeverityOf(Rule, Config)});
                break;
            }
        }

        const FString Family = FamilyOf(PackageName, Config);
        const bool bIsShared = PackageName.StartsWith(SharedPrefix);
        if(Family.IsEmpty() && !bIsShared)
        {
            // Under the family root but on no declared route: the taxonomy has grown a shape it
            // does not describe. Reporting it is the point — the alternative is a package silently
            // exempt from every directional rule, which is how the mis-named-family defect hid.
            if(PackageName.StartsWith(FamilyPrefix))
            {
                const FString Rule = TEXT("unrouted-package");
                OutResult.Findings.Add({Rule, PackageName,
                    TEXT("matches no family pattern, so it belongs to no family"),
                    SeverityOf(Rule, Config)});
            }
            continue;
        }

        TArray<FName> Dependencies;
        Registry.GetDependencies(Asset.PackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::FDependencyQuery());
        for(const FName& Target : Dependencies)
        {
            const FString TargetName = Target.ToString();
            const FString TargetFamily = FamilyOf(TargetName, Config);
            if(TargetFamily.IsEmpty())
            {
                continue;
            }

            // Shared may not reach into a family: the moment it does, the family cannot be moved or
            // deleted without breaking something that claims to belong to no one.
            if(bIsShared)
            {
                const FString Rule = TEXT("shared-depends-on-family");
                OutResult.Findings.Add({Rule, PackageName,
                    FString::Printf(TEXT("depends on family '%s' via %s"), *TargetFamily, *TargetName),
                    SeverityOf(Rule, Config)});
                continue;
            }

            if(Family.IsEmpty() || TargetFamily == Family)
            {
                continue;
            }

            // Reaching into an assembly is the direction that costs something: a kit whose
            // internals are referenced from outside can no longer be revised as one thing.
            if(FTheStructureLint::IsAssembly(TargetFamily, Config))
            {
                const FString Rule = TEXT("depends-on-assembly");
                OutResult.Findings.Add({Rule, PackageName,
                    FString::Printf(TEXT("family '%s' reaches into assembly '%s' via %s"), *Family, *TargetFamily, *TargetName),
                    SeverityOf(Rule, Config)});
                continue;
            }

            // An assembly composing other families is the artifact doing its job, so the
            // family-to-family rule does not apply to it.
            if(FTheStructureLint::IsAssembly(Family, Config))
            {
                continue;
            }

            // Family-to-family makes both unextractable. A resource two families need belongs in
            // shared, not duplicated and not referenced sideways.
            const FString Pair = FString::Printf(TEXT("%s -> %s"), *Family, *TargetFamily);
            if(Config.AcknowledgedCrossFamily.Contains(Pair))
            {
                continue;
            }
            const FString Rule = TEXT("family-depends-on-family");
            OutResult.Findings.Add({Rule, PackageName,
                FString::Printf(TEXT("family '%s' depends on family '%s' via %s"), *Family, *TargetFamily, *TargetName),
                SeverityOf(Rule, Config)});
        }
    }

    UE_LOG(LogTheQuartermaster, Display, TEXT("THEQM_LINT checked=%d errors=%d warnings=%d"),
        OutResult.PackagesChecked,
        OutResult.CountOf(ETheLintSeverity::Error),
        OutResult.CountOf(ETheLintSeverity::Warning));
    return true;
}
