// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aresic

using UnrealBuildTool;
using System.IO;

public class MMVROpenVRInput : ModuleRules
{
    public MMVROpenVRInput(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "LiveLinkInterface", "EnhancedInput", "Projects", "OpenVR" });
        // SteamVR opens these with native file IO, outside Unreal's pak/IoStore.
        string ConfigDir = Path.Combine(EngineDirectory, "Plugins", "Experimental", "LiveLinkOpenVR", "Config");
        foreach (string FileName in new[] { "livelinkopenvr_action_manifest.json", "livelinkopenvr_bindings_knuckles.json" })
            RuntimeDependencies.Add(Path.Combine(ConfigDir, FileName), StagedFileType.NonUFS);
    }
}
