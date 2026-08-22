// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheQuartermasterSettings.h"

#include "Misc/Paths.h"

#include "TheQuartermasterModule.h"

UTheQuartermasterSettings::UTheQuartermasterSettings()
{
    CategoryName = TEXT("Plugins");
    QuarantineRoot = DefaultQuarantineRoot();
}

FString UTheQuartermasterSettings::ResolvedQuarantineRoot()
{
    const UTheQuartermasterSettings* Settings = GetDefault<UTheQuartermasterSettings>();
    FString Root = Settings ? Settings->QuarantineRoot : FString();
    Root.TrimStartAndEndInline();
    Root.RemoveFromEnd(TEXT("/"));

    // /Game itself would make every folder in the project "already in quarantine", and a root
    // outside /Game cannot be expressed as a package path at all.
    const bool bUsable = Root.StartsWith(TEXT("/Game/")) && Root.Len() > FCString::Strlen(TEXT("/Game/"));
    if(!bUsable)
    {
        if(!Root.IsEmpty())
        {
            UE_LOG(LogTheQuartermaster, Warning, TEXT("QuarantineRoot '%s' is not a usable /Game/ subfolder; falling back to %s"), *Root, DefaultQuarantineRoot());
        }
        return DefaultQuarantineRoot();
    }
    return Root;
}

FString UTheQuartermasterSettings::ResolvedPlacementConfigPath()
{
    const UTheQuartermasterSettings* Settings = GetDefault<UTheQuartermasterSettings>();
    FString Path = Settings ? Settings->PlacementConfigPath : FString();
    Path.TrimStartAndEndInline();
    if(Path.IsEmpty())
    {
        return FString();
    }

    // Relative means relative to the project, not to whatever directory the process was started in:
    // a commandlet, the editor and a test runner all have different working directories.
    if(FPaths::IsRelative(Path))
    {
        Path = FPaths::ProjectDir() / Path;
    }
    return FPaths::ConvertRelativePathToFull(Path);
}
