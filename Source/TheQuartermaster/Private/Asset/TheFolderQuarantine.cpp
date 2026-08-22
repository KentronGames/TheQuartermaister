// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Asset/TheFolderQuarantine.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "UObject/CoreRedirects.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

#include "TheQuartermasterModule.h"
#include "TheQuartermasterSettings.h"

namespace
{
const TCHAR* OriginMarkerName = TEXT(".origin");
const TCHAR* RedirectListName = TEXT("TheQuartermaster.Quarantine");

FString GameFolderToDisk(const FString& GameFolder)
{
    FString Relative = GameFolder;
    Relative.RemoveFromStart(TEXT("/Game/"));
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / Relative);
}

FCoreRedirect MakeRedirect(const FString& OriginPrefix, const FString& QuarantinePrefix)
{
    return FCoreRedirect(ECoreRedirectFlags::Type_Package | ECoreRedirectFlags::Option_MatchSubstring, OriginPrefix, QuarantinePrefix);
}

void RegisterRedirect(const FString& OriginFolder, const FString& QuarantineFolder)
{
    const FCoreRedirect Redirect = MakeRedirect(OriginFolder + TEXT("/"), QuarantineFolder + TEXT("/"));
    FCoreRedirects::AddRedirectList(MakeArrayView(&Redirect, 1), RedirectListName);
}

void UnregisterRedirect(const FString& OriginFolder, const FString& QuarantineFolder)
{
    const FCoreRedirect Redirect = MakeRedirect(OriginFolder + TEXT("/"), QuarantineFolder + TEXT("/"));
    FCoreRedirects::RemoveRedirectList(MakeArrayView(&Redirect, 1), RedirectListName);
}

FString OriginMarkerPath(const FString& QuarantineFolder)
{
    return GameFolderToDisk(QuarantineFolder) / OriginMarkerName;
}

bool RefuseUnlessInsideQuarantine(const FString& Clean, FString& OutError)
{
    const FString Root = FTheFolderQuarantine::QuarantineRoot();
    if(Clean == Root)
    {
        OutError = FString::Printf(TEXT("refused: %s is the quarantine root itself - name a pack folder inside it, not the root"), *Clean);
        return false;
    }
    if(!Clean.StartsWith(Root / TEXT("")))
    {
        OutError = FString::Printf(TEXT("refused: %s is not under the quarantine root %s"), *Clean, *Root);
        return false;
    }
    return true;
}

bool UnloadAndCollect(const FString& Folder)
{
    TArray<UPackage*> ToUnload;
    const FString Prefix = Folder + TEXT("/");
    for(TObjectIterator<UPackage> It; It; ++It)
    {
        if(It->GetName().StartsWith(Prefix))
        {
            ToUnload.Add(*It);
        }
    }
    bool bAll = true;
    if(ToUnload.Num() > 0)
    {
        FText UnloadError;
        bAll = UPackageTools::UnloadPackages(ToUnload, UnloadError, /*bUnloadDirtyPackages*/ true);
    }
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    return bAll;
}

bool MoveDirOnDisk(const FString& SrcAbs, const FString& DestAbs, FString& OutError)
{
    IFileManager& FileManager = IFileManager::Get();
    if(!FileManager.DirectoryExists(*SrcAbs))
    {
        OutError = FString::Printf(TEXT("source folder not found on disk: %s"), *SrcAbs);
        return false;
    }
    FileManager.MakeDirectory(*FPaths::GetPath(DestAbs), /*Tree*/ true);

    if(FileManager.Move(*DestAbs, *SrcAbs, /*bReplace*/ false, /*bEvenIfReadOnly*/ true, /*bAttributes*/ true, /*bDoNotRetryOrError*/ true))
    {
        return true;
    }

    // The whole-directory move fails across volumes and on partially-locked trees, so fall back to
    // per-file moves and roll them back on the first failure. A half-moved folder is the one
    // outcome worth extra code to avoid: neither path would then hold a working pack.
    TArray<FString> Files;
    FileManager.FindFilesRecursive(Files, *SrcAbs, TEXT("*"), /*Files*/ true, /*Directories*/ false);
    TArray<TPair<FString, FString>> Moved;
    for(const FString& File : Files)
    {
        FString Relative = File;
        FPaths::MakePathRelativeTo(Relative, *(SrcAbs / TEXT("")));
        const FString Target = DestAbs / Relative;
        FileManager.MakeDirectory(*FPaths::GetPath(Target), /*Tree*/ true);
        if(!FileManager.Move(*Target, *File, false, true, true, true))
        {
            OutError = FString::Printf(TEXT("failed to move %s (is it open/locked? restart the editor and retry)"), *File);
            TArray<FString> Stranded;
            for(const TPair<FString, FString>& Pair : Moved)
            {
                if(!FileManager.Move(*Pair.Key, *Pair.Value, false, true, true, true))
                {
                    Stranded.Add(Pair.Value);
                }
            }
            if(Stranded.Num() > 0)
            {
                OutError = FString::Printf(TEXT("%s - and %d file(s) could not be put back either, so the folder is now SPLIT between %s and %s; move them back by hand before using either path: %s"),
                    *OutError,
                    Stranded.Num(),
                    *SrcAbs,
                    *DestAbs,
                    *FString::Join(Stranded, TEXT(", ")));
                UE_LOG(LogTheQuartermaster, Error, TEXT("%s"), *OutError);
                return false;
            }
            FileManager.DeleteDirectory(*DestAbs, /*RequireExists*/ false, /*Tree*/ true);
            return false;
        }
        Moved.Emplace(File, Target);
    }
    FileManager.DeleteDirectory(*SrcAbs, /*RequireExists*/ false, /*Tree*/ true);
    return true;
}
}

FString FTheFolderQuarantine::QuarantineRoot()
{
    return UTheQuartermasterSettings::ResolvedQuarantineRoot();
}

bool FTheFolderQuarantine::MoveToQuarantine(const FString& SourceFolder, FString& OutReport, bool& OutNeedsRestart, FString& OutError)
{
    OutNeedsRestart = false;

    FString Folder = SourceFolder;
    Folder.RemoveFromEnd(TEXT("/"));
    if(!Folder.StartsWith(TEXT("/Game/")))
    {
        OutError = TEXT("only /Game/ folders can be quarantined");
        return false;
    }
    if(Folder.StartsWith(QuarantineRoot()))
    {
        OutError = TEXT("already in quarantine");
        return false;
    }

    const FString Leaf = FPaths::GetCleanFilename(Folder);
    const FString Target = QuarantineRoot() / Leaf;
    const FString SrcAbs = GameFolderToDisk(Folder);
    const FString DestAbs = GameFolderToDisk(Target);
    if(IFileManager::Get().DirectoryExists(*DestAbs))
    {
        OutError = FString::Printf(TEXT("%s already exists in quarantine"), *Target);
        return false;
    }

    OutNeedsRestart = !UnloadAndCollect(Folder);

    if(!MoveDirOnDisk(SrcAbs, DestAbs, OutError))
    {
        return false;
    }

    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

    // Without the marker the redirect cannot be rebuilt after a restart, so a folder parked here
    // would silently break every reference into it. Roll the move back rather than leave that.
    if(!FFileHelper::SaveStringToFile(Folder, *OriginMarkerPath(Target), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        FString RollbackError;
        if(!MoveDirOnDisk(DestAbs, SrcAbs, RollbackError))
        {
            OutError = FString::Printf(TEXT("could not write origin marker %s, and rolling the move back failed too (%s) - %s is now in quarantine WITHOUT a redirect marker; restore it manually"), *OriginMarkerPath(Target), *RollbackError, *Target);
            UE_LOG(LogTheQuartermaster, Error, TEXT("%s"), *OutError);
            return false;
        }
        Registry.ScanPathsSynchronous({Folder}, /*bForceRescan*/ true);
        OutError = FString::Printf(TEXT("could not write origin marker %s - the move was rolled back, %s left untouched"), *OriginMarkerPath(Target), *Folder);
        UE_LOG(LogTheQuartermaster, Error, TEXT("%s"), *OutError);
        return false;
    }
    RegisterRedirect(Folder, Target);

    Registry.ScanPathsSynchronous({Target}, /*bForceRescan*/ true);
    Registry.ScanPathsSynchronous({Folder}, /*bForceRescan*/ true);

    OutReport = FString::Printf(TEXT("moved %s -> %s\nredirect %s/ -> %s/ added (no repo config touched)%s"),
        *Folder,
        *Target,
        *Folder,
        *Target,
        OutNeedsRestart ? TEXT("\nNOTE: a package could not be unloaded - restart the editor for a fully clean state") : TEXT(""));
    UE_LOG(LogTheQuartermaster, Display, TEXT("Quarantined %s -> %s"), *Folder, *Target);
    return true;
}

bool FTheFolderQuarantine::DeleteFromQuarantine(const FString& Folder, FString& OutReport, FString& OutError)
{
    FString Clean = Folder;
    Clean.RemoveFromEnd(TEXT("/"));
    if(!RefuseUnlessInsideQuarantine(Clean, OutError))
    {
        return false;
    }

    FString Origin;
    const bool bHasOrigin = FFileHelper::LoadFileToString(Origin, *OriginMarkerPath(Clean));
    Origin.TrimStartAndEndInline();

    if(!UnloadAndCollect(Clean))
    {
        OutError = FString::Printf(TEXT("refused to delete %s - a package under it could not be unloaded, and deleting a folder the editor still holds leaves half of it behind. Close what is open (or restart the editor) and retry"), *Clean);
        return false;
    }

    const FString DiskPath = GameFolderToDisk(Clean);
    IFileManager::Get().DeleteDirectory(*DiskPath, /*RequireExists*/ false, /*Tree*/ true);
    const bool bDiskGone = !IFileManager::Get().DirectoryExists(*DiskPath);
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().ScanPathsSynchronous({QuarantineRoot()}, /*bForceRescan*/ true);

    if(!bDiskGone)
    {
        OutError = FString::Printf(TEXT("failed to delete %s - files are still on disk; the redirect is left in place so what survived stays reachable"), *Clean);
        return false;
    }

    if(bHasOrigin && !Origin.IsEmpty())
    {
        UnregisterRedirect(Origin, Clean);
    }

    OutReport = FString::Printf(TEXT("deleted quarantine folder %s"), *Clean);
    UE_LOG(LogTheQuartermaster, Display, TEXT("Deleted quarantine folder %s"), *Clean);
    return true;
}

bool FTheFolderQuarantine::RestoreFromQuarantine(const FString& Folder, FString& OutReport, bool& OutNeedsRestart, FString& OutError)
{
    OutNeedsRestart = false;

    FString Clean = Folder;
    Clean.RemoveFromEnd(TEXT("/"));
    if(!RefuseUnlessInsideQuarantine(Clean, OutError))
    {
        return false;
    }

    FString Origin;
    if(!FFileHelper::LoadFileToString(Origin, *OriginMarkerPath(Clean)))
    {
        OutError = FString::Printf(TEXT("no origin marker in %s - nothing records where this pack came from, so it cannot be restored automatically"), *Clean);
        return false;
    }
    Origin.TrimStartAndEndInline();
    if(Origin.IsEmpty() || !Origin.StartsWith(TEXT("/Game/")))
    {
        OutError = FString::Printf(TEXT("the origin marker in %s does not record a /Game/ folder: '%s'"), *Clean, *Origin);
        return false;
    }

    const FString SrcAbs = GameFolderToDisk(Clean);
    const FString DestAbs = GameFolderToDisk(Origin);
    if(IFileManager::Get().DirectoryExists(*DestAbs))
    {
        OutError = FString::Printf(TEXT("%s already exists - the pack's original home is occupied, so restoring would merge two folders"), *Origin);
        return false;
    }

    OutNeedsRestart = !UnloadAndCollect(Clean);

    if(!MoveDirOnDisk(SrcAbs, DestAbs, OutError))
    {
        return false;
    }

    UnregisterRedirect(Origin, Clean);
    IFileManager::Get().Delete(*(DestAbs / OriginMarkerName), /*RequireExists*/ false, /*EvenReadOnly*/ true);

    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.ScanPathsSynchronous({Origin}, /*bForceRescan*/ true);
    Registry.ScanPathsSynchronous({Clean}, /*bForceRescan*/ true);

    OutReport = FString::Printf(TEXT("restored %s -> %s\nredirect %s/ -> %s/ dropped, origin marker removed%s"),
        *Clean,
        *Origin,
        *Origin,
        *Clean,
        OutNeedsRestart ? TEXT("\nNOTE: a package could not be unloaded - restart the editor for a fully clean state") : TEXT(""));
    UE_LOG(LogTheQuartermaster, Display, TEXT("Restored %s -> %s"), *Clean, *Origin);
    return true;
}

bool FTheFolderQuarantine::DropQuarantineBookkeepingAfterPackagesLeft(const FString& Folder, FString& OutReport)
{
    FString Clean = Folder;
    Clean.RemoveFromEnd(TEXT("/"));
    if(!Clean.StartsWith(QuarantineRoot() / TEXT("")))
    {
        return false;
    }

    FString Origin;
    if(!FFileHelper::LoadFileToString(Origin, *OriginMarkerPath(Clean)))
    {
        return false;
    }
    Origin.TrimStartAndEndInline();
    if(!Origin.IsEmpty())
    {
        UnregisterRedirect(Origin, Clean);
    }
    IFileManager::Get().Delete(*OriginMarkerPath(Clean), /*RequireExists*/ false, /*EvenReadOnly*/ true);
    OutReport = FString::Printf(TEXT("left quarantine: redirect %s/ -> %s/ dropped and the origin marker removed"), *Origin, *Clean);
    UE_LOG(LogTheQuartermaster, Display, TEXT("%s"), *OutReport);
    return true;
}

bool FTheFolderQuarantine::HoldsNothingButTheOriginMarker(const FString& Folder)
{
    FString Clean = Folder;
    Clean.RemoveFromEnd(TEXT("/"));
    if(!Clean.StartsWith(QuarantineRoot() / TEXT("")))
    {
        return false;
    }
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *GameFolderToDisk(Clean), TEXT("*"), /*Files*/ true, /*Directories*/ false);
    return Files.Num() == 1 && FPaths::GetCleanFilename(Files[0]) == OriginMarkerName;
}

bool FTheFolderQuarantine::VerifyRestoredFromQuarantine(const FString& SourceFolder, FString& OutReport, FString& OutError)
{
    FString Source = SourceFolder;
    Source.RemoveFromEnd(TEXT("/"));
    const FString Quarantined = QuarantineRoot() / FPaths::GetCleanFilename(Source);

    if(!IFileManager::Get().DirectoryExists(*GameFolderToDisk(Source)))
    {
        OutError = FString::Printf(TEXT("the pack is not back at its origin: %s does not exist on disk"), *Source);
        return false;
    }
    if(IFileManager::Get().DirectoryExists(*GameFolderToDisk(Quarantined)))
    {
        OutError = FString::Printf(TEXT("%s is still in quarantine - the restore did not complete"), *Quarantined);
        return false;
    }
    if(IFileManager::Get().FileExists(*(GameFolderToDisk(Source) / OriginMarkerName)))
    {
        OutError = FString::Printf(TEXT("the origin marker travelled back with the pack: %s - a restart would re-register the redirect it records"), *(Source / OriginMarkerName));
        return false;
    }

    const FCoreRedirectObjectName Probe(*(Source / TEXT("Probe")));
    const FCoreRedirectObjectName Resolved = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, Probe);
    if(Resolved != Probe)
    {
        OutError = FString::Printf(TEXT("a quarantine redirect is still live: %s resolves to %s"), *Probe.ToString(), *Resolved.ToString());
        return false;
    }

    OutReport = FString::Printf(TEXT("%s is back at its origin, out of quarantine, with no marker and no redirect left"), *Source);
    return true;
}

bool FTheFolderQuarantine::VerifyQuarantined(const FString& SourceFolder, FString& OutReport, FString& OutError)
{
    FString Source = SourceFolder;
    Source.RemoveFromEnd(TEXT("/"));
    const FString Target = QuarantineRoot() / FPaths::GetCleanFilename(Source);

    if(!IFileManager::Get().DirectoryExists(*GameFolderToDisk(Target)))
    {
        OutError = FString::Printf(TEXT("quarantined folder not found on disk: %s"), *Target);
        return false;
    }
    if(IFileManager::Get().DirectoryExists(*GameFolderToDisk(Source)))
    {
        OutError = FString::Printf(TEXT("source folder still on disk - the move did not complete: %s"), *Source);
        return false;
    }
    FString Recorded;
    if(!FFileHelper::LoadFileToString(Recorded, *OriginMarkerPath(Target)))
    {
        OutError = FString::Printf(TEXT("origin marker missing (redirect would not survive a restart): %s"), *OriginMarkerPath(Target));
        return false;
    }
    Recorded.TrimStartAndEndInline();
    if(Recorded != Source)
    {
        OutError = FString::Printf(TEXT("origin marker records %s, expected %s"), *Recorded, *Source);
        return false;
    }
    OutReport = FString::Printf(TEXT("%s is quarantined at %s; origin marker records the source"), *Source, *Target);
    return true;
}

bool FTheFolderQuarantine::VerifyQuarantineDeleted(const FString& Folder, FString& OutReport, FString& OutError)
{
    FString Clean = Folder;
    Clean.RemoveFromEnd(TEXT("/"));
    if(!RefuseUnlessInsideQuarantine(Clean, OutError))
    {
        return false;
    }
    if(IFileManager::Get().DirectoryExists(*GameFolderToDisk(Clean)))
    {
        OutError = FString::Printf(TEXT("folder still exists on disk: %s"), *Clean);
        return false;
    }
    OutReport = FString::Printf(TEXT("%s is gone from disk"), *Clean);
    return true;
}

void FTheFolderQuarantine::RehydrateRedirects()
{
    const FString Root = QuarantineRoot();
    const FString QuarantineDiskRoot = GameFolderToDisk(Root);
    if(!IFileManager::Get().DirectoryExists(*QuarantineDiskRoot))
    {
        return;
    }

    TArray<FString> Leaves;
    IFileManager::Get().FindFiles(Leaves, *(QuarantineDiskRoot / TEXT("*")), /*Files*/ false, /*Directories*/ true);
    for(const FString& Leaf : Leaves)
    {
        const FString QuarantineFolder = Root / Leaf;
        FString Origin;
        if(FFileHelper::LoadFileToString(Origin, *OriginMarkerPath(QuarantineFolder)))
        {
            Origin.TrimStartAndEndInline();
            if(!Origin.IsEmpty())
            {
                RegisterRedirect(Origin, QuarantineFolder);
            }
        }
    }
}
