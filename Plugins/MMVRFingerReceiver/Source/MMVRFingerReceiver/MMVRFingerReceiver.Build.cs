using UnrealBuildTool;
public class MMVRFingerReceiver : ModuleRules
{
    public MMVRFingerReceiver(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "MMVRFingerFusion" });
        PrivateDependencyModuleNames.AddRange(new[] { "OSC", "Networking", "Sockets" });
    }
}
