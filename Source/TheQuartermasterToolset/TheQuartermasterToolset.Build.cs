// (c) 2026 Kentron Cowboys. All rights reserved.

using EpicGames.Core;
using UnrealBuildTool;

public class TheQuartermasterToolset : ModuleRules
{
    public TheQuartermasterToolset(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Strict only in a project of ours — one that mounts the platform, whatever it is called: a consumer's
        // toolchain or a newer engine must not turn a warning into a hard failure of THEIR build over a plugin
        // they cannot edit.
        bWarningsAsErrors = Target.ProjectFile != null
            && FileReference.Exists(FileReference.Combine(Target.ProjectFile.Directory, ".claude", "scripts", "roots.env"));

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
