using UnrealBuildTool;
public class JanyeolFire : ModuleRules {
 public JanyeolFire(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine","Niagara","ProceduralMeshComponent"});
 }
}
