using UnrealBuildTool;

/** Viewport tool for placing part meshes on the mech and writing FMechPartAttachment. In development. */
public class MechPartsWorkbenchEditor : ModuleRules
{
	public MechPartsWorkbenchEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"MechPartsWorkbenchRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd",
			"Slate",
			"SlateCore",
			// 워크벤치 창 (2026-10-10): 미리보기 창·장면, 파츠 목록, 속성 패널, Tools 메뉴, 개인 설정
			"InputCore",
			"EditorFramework",
			"AdvancedPreviewScene",
			"ContentBrowser",
			"PropertyEditor",
			"ToolMenus",
			"DeveloperSettings",
			// 파츠 목록 분류 (2026-10-11)
			"AssetRegistry",
			// 뼈대에 입히기 (2026-10-12): 스태틱 메시 → 메카 뼈대 스킨 가중치를 옮긴 스켈레탈 메시 (엔진 GeometryScripting 플러그인)
			"GeometryFramework",
			"GeometryScriptingCore",
			"GeometryScriptingEditor",
			"SkeletalMeshDescription",
			"AnimationCore",
			"GeometryCore"
		});
	}
}
