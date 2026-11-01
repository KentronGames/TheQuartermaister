// (c) 2026 Kentron Cowboys. All rights reserved.

using EpicGames.Core;
using UnrealBuildTool;

public class TheQuartermaster : ModuleRules
{
    public TheQuartermaster(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Strict only in a project of ours — one that mounts the platform, whatever it is called: a consumer's
        // toolchain or a newer engine must not turn a warning into a hard failure of THEIR build over a plugin
        // they cannot edit.
        bWarningsAsErrors = Target.ProjectFile != null
            && FileReference.Exists(FileReference.Combine(Target.ProjectFile.Directory, ".claude", "scripts", "roots.env"));

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings"
        });

        // Deliberately short. The quarantine works on packages and files, so it needs the registry
        // and UnrealEd's package tools and nothing else; every further dependency is a chance to
        // acquire a client project's assumptions, which is exactly what this plugin exists to avoid.
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "Json",
            // Locating the plugin's own Config/ at runtime, for the shipped placement taxonomies.
            "Projects",
            "UnrealEd"
        });

        if(Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("CQTest");
        }
    }
}
