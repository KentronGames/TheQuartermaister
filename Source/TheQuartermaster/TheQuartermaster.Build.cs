// (c) 2026 Kentron Cowboys. All rights reserved.

using UnrealBuildTool;

public class TheQuartermaster : ModuleRules
{
    public TheQuartermaster(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bWarningsAsErrors = true;

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
    }
}
