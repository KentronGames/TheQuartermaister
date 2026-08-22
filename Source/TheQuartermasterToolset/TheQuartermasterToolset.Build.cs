// (c) 2026 Kentron Cowboys. All rights reserved.

using UnrealBuildTool;

public class TheQuartermasterToolset : ModuleRules
{
    public TheQuartermasterToolset(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bWarningsAsErrors = true;

        // The second entry point is a separate module on purpose. ToolsetRegistry is an experimental
        // engine plugin, and the toolkit has to keep working as a commandlet in a project that does
        // not have it; a module boundary is what keeps that dependency out of the operations
        // themselves. Nothing here decides anything - every tool forwards to TheQuartermaster.
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "TheQuartermaster",
            "ToolsetRegistry"
        });
    }
}
