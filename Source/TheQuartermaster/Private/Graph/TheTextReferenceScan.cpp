// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Graph/TheTextReferenceScan.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "TheQuartermasterModule.h"

namespace
{
/**
 * Product text only. Generated trees hold no authored reference: UHT writes sample paths into
 * Intermediate/, and Binaries/ has nothing a human wrote.
 */
const TCHAR* ScannedDirectories[] = {TEXT("Config"), TEXT("Source"), TEXT("Plugins")};
const TCHAR* SkippedDirectories[] = {TEXT("Intermediate"), TEXT("Binaries"), TEXT("Saved"), TEXT("DerivedDataCache")};

bool IsScannableFile(const FString& FilePath)
{
    static const TSet<FString> Suffixes = {
        TEXT("ini"), TEXT("uproject"), TEXT("uplugin"), TEXT("cpp"), TEXT("h"),
        TEXT("cs"), TEXT("py"), TEXT("ps1"), TEXT("cmd"), TEXT("json")
    };
    if(!Suffixes.Contains(FPaths::GetExtension(FilePath).ToLower()))
    {
        return false;
    }
    for(const TCHAR* Skipped : SkippedDirectories)
    {
        if(FilePath.Contains(FString::Printf(TEXT("/%s/"), Skipped)) || FilePath.Contains(FString::Printf(TEXT("\\%s\\"), Skipped)))
        {
            return false;
        }
    }
    return true;
}

/** Strips an object suffix (/Game/A/B.B) and trailing punctuation left by surrounding syntax. */
FString PackageOf(const FString& Token)
{
    FString Result = Token;
    while(Result.EndsWith(TEXT(".")) || Result.EndsWith(TEXT("/")) || Result.EndsWith(TEXT("-")))
    {
        Result.LeftChopInline(1);
    }
    int32 LastSlash = INDEX_NONE;
    Result.FindLastChar(TEXT('/'), LastSlash);
    const FString Leaf = LastSlash == INDEX_NONE ? Result : Result.Mid(LastSlash + 1);
    int32 Dot = INDEX_NONE;
    if(Leaf.FindChar(TEXT('.'), Dot))
    {
        Result.LeftInline(LastSlash + 1 + Dot);
    }
    return Result;
}
}

bool FTheTextReferenceScan::Scan(FTheTextScanResult& OutResult, FString& OutError)
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.SearchAllAssets(/*bSynchronousSearch*/ true);
    Registry.WaitForCompletion();

    TArray<FAssetData> AllAssets;
    Registry.GetAssetsByPath(FName(TEXT("/Game")), AllAssets, /*bRecursive*/ true, /*bIncludeOnlyOnDiskAssets*/ true);
    if(AllAssets.IsEmpty())
    {
        OutError = TEXT("the registry reported no packages under /Game; refusing to call every path unresolved");
        return false;
    }

    TSet<FString> KnownPackages;
    TSet<FString> KnownFolders;
    KnownPackages.Reserve(AllAssets.Num());
    for(const FAssetData& Asset : AllAssets)
    {
        const FString PackageName = Asset.PackageName.ToString();
        KnownPackages.Add(PackageName);

        // A token may name a folder rather than a package - editor folder colours, cook directory
        // rules, content-browser roots - and that resolves as long as the folder still holds
        // something.
        FString Folder = FPaths::GetPath(PackageName);
        while(Folder.StartsWith(TEXT("/Game")) && !KnownFolders.Contains(Folder))
        {
            KnownFolders.Add(Folder);
            Folder = FPaths::GetPath(Folder);
        }
    }

    const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    TArray<FString> Files;
    for(const TCHAR* Directory : ScannedDirectories)
    {
        TArray<FString> Found;
        IFileManager::Get().FindFilesRecursive(Found, *(ProjectDir / Directory), TEXT("*"), /*Files*/ true, /*Directories*/ false);
        Files.Append(MoveTemp(Found));
    }

    const FRegexPattern Pattern(TEXT("/Game/[A-Za-z0-9_./-]+"));
    for(const FString& File : Files)
    {
        if(!IsScannableFile(File))
        {
            continue;
        }
        FString Text;
        if(!FFileHelper::LoadFileToString(Text, *File))
        {
            continue;
        }
        OutResult.FilesScanned++;

        FString Relative = File;
        FPaths::MakePathRelativeTo(Relative, *ProjectDir);

        FRegexMatcher Matcher(Pattern, Text);
        while(Matcher.FindNext())
        {
            OutResult.TokensSeen++;
            const FString Package = PackageOf(Matcher.GetCaptureGroup(0));
            if(Package.IsEmpty())
            {
                continue;
            }
            TMap<FString, TArray<FString>>& Bucket = (KnownPackages.Contains(Package) || KnownFolders.Contains(Package))
                ? OutResult.ReferencedPackages
                : OutResult.UnresolvedPaths;
            Bucket.FindOrAdd(Package).AddUnique(Relative);
        }
    }

    UE_LOG(LogTheQuartermaster, Display, TEXT("THEQM_TEXTSCAN files=%d tokens=%d referenced=%d unresolved=%d"),
        OutResult.FilesScanned,
        OutResult.TokensSeen,
        OutResult.ReferencedPackages.Num(),
        OutResult.UnresolvedPaths.Num());
    return true;
}
