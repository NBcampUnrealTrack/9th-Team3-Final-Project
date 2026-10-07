using UnrealBuildTool;
using System.IO;
public class JanyeolFireEditor : ModuleRules {
 public JanyeolFireEditor(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine","Niagara","JanyeolFire"});
  PrivateDependencyModuleNames.AddRange(new string[]{"UnrealEd","NiagaraEditor","NiagaraShader","AssetRegistry","RenderCore","RHI","VectorVM"});
  PrivateIncludePaths.Add(Path.Combine(GetModuleDirectory("Niagara"),"Internal"));
  PrivateIncludePaths.Add(Path.Combine(GetModuleDirectory("NiagaraShader"),"Internal"));
 }
}
