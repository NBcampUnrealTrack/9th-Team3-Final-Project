using UnrealBuildTool;

/** Editor-only tooling: data validation, asset utilities and editor extensions. */
public class knightEditor : ModuleRules
{
	public knightEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"knight"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"knightInventory",
			"GameplayAbilities",
			"GameplayTags",
			// 몬스터 공격 미리보기 플러그인의 공격 선택 시뮬레이터 인터페이스(knightMonsterAttackSelectionSimulator).
			"knightMonsterAnimDebugEditor",
			"UnrealEd",
			// UI 위젯 블루프린트 트리 생성 도구(knightUIEditorLibrary).
			"UMG",
			"UMGEditor",
			"CommonUI",
			"knightUI",
			"Slate",
			"SlateCore",
			"SlateRHIRenderer",
			// 몬스터 레시피 생성기(knightMonsterRecipeLibrary).
			"AIModule",
			"AnimGraph",
			"AnimGraphRuntime",
			"AssetRegistry",
			"AssetTools",
			"PropertyEditor",
			"BlueprintGraph",
			"GameplayStateTreeModule",
			"PropertyBindingUtils",
			"StateTreeEditorModule",
			"StateTreeModule",
			// 몬스터 피직스 에셋 생성(CreateMonsterPhysicsAsset).
			"MeshUtilitiesCommon",
			"MeshUtilitiesEngine",
			"PhysicsUtilities",
			// 몬스터 연출 Niagara 생성(knightMonsterFXBuilder). 경량 이미터 헤더는 Niagara의 Internal 폴더에 있다.
			"Niagara",
			"NiagaraCore",
			"NiagaraShader",
			"RenderCore",
			"RHI",
			"VectorVM",
			// 부위 와이어프레임 셸 굽기(knightPartShellBaker): 메시 단순화와 스켈레탈 메시 생성.
			"AnimationCore",
			"DynamicMesh",
			"GeometryCore",
			"MeshConversion",
			"MeshDescription",
			"ModelingComponentsEditorOnly",
			"SkeletalMeshDescription"
		});

		PrivateIncludePaths.Add(System.IO.Path.Combine(GetModuleDirectory("Niagara"), "Internal"));
		PrivateIncludePaths.Add(System.IO.Path.Combine(GetModuleDirectory("NiagaraShader"), "Internal"));
	}
}
