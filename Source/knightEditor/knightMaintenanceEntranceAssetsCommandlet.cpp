#include "knightMaintenanceEntranceAssetsCommandlet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "MaterialShared.h"
#include "ShaderCompiler.h"
#include "WidgetBlueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost(); Package->MarkPackageDirty();
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args);
	}
	struct FEntranceMaterial
	{
		UMaterial* Material;
		UMaterialExpressionCustom* Code;
		FEntranceMaterial(const TCHAR* Name, const TCHAR* Hlsl, bool bReveal)
		{
			const FString Path = FString(TEXT("/Game/Knight/UI/Menu/Maintenance/Entrance/")) + Name;
			Material = LoadObject<UMaterial>(nullptr, *(Path + TEXT(".") + Name), nullptr, LOAD_NoWarn);
			if (!Material)
			{
				Material = NewObject<UMaterial>(CreatePackage(*Path), Name, RF_Public | RF_Standalone);
				FAssetRegistryModule::AssetCreated(Material);
			}
			Material->PreEditChange(nullptr); Material->GetExpressionCollection().Empty();
			Material->MaterialDomain = MD_UI; Material->BlendMode = bReveal ? BLEND_AlphaComposite : BLEND_Additive;
			Code = NewObject<UMaterialExpressionCustom>(Material); Code->Code = Hlsl;
			Code->OutputType = bReveal ? CMOT_Float4 : CMOT_Float3; Code->Inputs.Empty();
			Code->Description = TEXT("Maintenance side entry: resolution-independent soft mask / explicit UI light");
			Material->GetExpressionCollection().AddExpression(Code);
			auto* UV = NewObject<UMaterialExpressionTextureCoordinate>(Material);
			Material->GetExpressionCollection().AddExpression(UV); Input(TEXT("UV"), UV);
			if (bReveal)
			{
				auto* Texture = NewObject<UMaterialExpressionTextureObjectParameter>(Material);
				Texture->ParameterName = TEXT("Texture");
				Texture->Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
				Material->GetExpressionCollection().AddExpression(Texture); Input(TEXT("Texture"), Texture);
				auto* RGB = NewObject<UMaterialExpressionComponentMask>(Material);
				RGB->R = RGB->G = RGB->B = true; RGB->A = false; RGB->Input.Connect(0, Code);
				Material->GetExpressionCollection().AddExpression(RGB);
				Material->GetEditorOnlyData()->EmissiveColor.Connect(0, RGB);
				auto* Alpha = NewObject<UMaterialExpressionComponentMask>(Material);
				Alpha->R = Alpha->G = Alpha->B = false; Alpha->A = true; Alpha->Input.Connect(0, Code);
				Material->GetExpressionCollection().AddExpression(Alpha);
				Material->GetEditorOnlyData()->Opacity.Connect(0, Alpha);
			}
			else
			{
				Material->GetEditorOnlyData()->EmissiveColor.Connect(0, Code);
				Material->GetEditorOnlyData()->Opacity.UseConstant = true;
				Material->GetEditorOnlyData()->Opacity.Constant = 1.f;
			}
		}
		void Input(const TCHAR* Name, UMaterialExpression* Expression)
		{
			FCustomInput In; In.InputName = Name; In.Input.Connect(0, Expression); Code->Inputs.Add(In);
		}
		void Scalar(const TCHAR* Name, float Default)
		{
			auto* P = NewObject<UMaterialExpressionScalarParameter>(Material);
			P->ParameterName = Name; P->DefaultValue = Default; P->Group = TEXT("Entrance");
			Material->GetExpressionCollection().AddExpression(P); Input(Name, P);
		}
	};
}

int32 UknightMaintenanceEntranceAssetsCommandlet::Main(const FString& Params)
{
	FEntranceMaterial Reveal(TEXT("M_UI_MaintenanceReveal"), TEXT(R"HLSL(
float4 panel = Texture2DSample(Texture, TextureSampler, UV);
float x = OriginX + UV.x * Width;
float mask = smoothstep(-max(Softness, 1.0), max(Softness, 1.0), (ScanX - x) * Direction);
// Retainer colors are premultiplied; fade color and alpha together.
return panel * mask;
)HLSL"), true);
	Reveal.Scalar(TEXT("OriginX"), 0); Reveal.Scalar(TEXT("Width"), 400);
	Reveal.Scalar(TEXT("Direction"), 1); Reveal.Scalar(TEXT("Softness"), 28); Reveal.Scalar(TEXT("ScanX"), 500);
	Reveal.Material->PostEditChange();
	FEntranceMaterial Scan(TEXT("M_UI_MaintenanceScan"), TEXT(R"HLSL(
float2 p = UV * float2(Width, Height);
float dx = (p.x - ScanX) * Direction;
float core = exp(-pow(dx / max(CoreWidth, 0.5), 2.0));
float halo = exp(-pow(dx / max(GlowWidth, 1.0), 2.0)) * 0.16;
float trails = 0;
[unroll] for (int i = 0; i < 8; ++i)
{
    float seed = frac(sin(float(i + 1) * 12.9898) * 43758.5453);
    float y = lerp(30.0, Height - 30.0, (float(i) + 0.25 + seed * 0.5) / 8.0);
    float length = 28.0 + seed * 56.0;
    float tail = smoothstep(-length, -length * 0.45, dx) * (1.0 - smoothstep(-2.0, 2.0, dx));
    trails += tail * exp(-pow((p.y - y) / (0.65 + seed), 2.0));
    halo += tail * exp(-pow((p.y - y) / 4.0, 2.0)) * 0.04 * TrailStrength;
}
// Quad includes a wide margin; fade well before its hard edge.
float edge = smoothstep(0.0, 18.0, p.y) * smoothstep(0.0, 18.0, Height - p.y);
edge *= smoothstep(0.0, 12.0, p.x) * smoothstep(0.0, 12.0, Width - p.x);
return edge * Intensity * (float3(1.0, 0.56, 0.09) * (core + trails * TrailStrength)
    + float3(1.0, 0.19, 0.005) * halo);
)HLSL"), false);
	Scan.Scalar(TEXT("Width"), 600); Scan.Scalar(TEXT("Height"), 1000); Scan.Scalar(TEXT("ScanX"), 300);
	Scan.Scalar(TEXT("Direction"), 1); Scan.Scalar(TEXT("CoreWidth"), 2); Scan.Scalar(TEXT("GlowWidth"), 24);
	Scan.Scalar(TEXT("Intensity"), .8f); Scan.Scalar(TEXT("TrailStrength"), .45f);
	Scan.Material->PostEditChange();
	GShaderCompilingManager->FinishAllCompilation();
	for (UMaterial* M : { Reveal.Material, Scan.Material })
	{
		const FMaterialResource* Resource = M->GetMaterialResource(GMaxRHIShaderPlatform);
		if (!Resource || Resource->GetCompileErrors().Num())
		{
			if (Resource) for (const FString& Error : Resource->GetCompileErrors()) UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
			return 1;
		}
		if (!Save(M)) return 1;
	}
	UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenance"));
	if (!BP || !BP->GeneratedClass) return 1;
	UObject* CDO = BP->GeneratedClass->GetDefaultObject();
	auto Bind = [CDO](const TCHAR* Name, UObject* Value)
	{
		auto* P = FindFProperty<FObjectPropertyBase>(CDO->GetClass(), Name);
		if (!P) return false;
		P->SetObjectPropertyValue_InContainer(CDO, Value); return true;
	};
	if (!Bind(TEXT("EntranceRevealMaterial"), Reveal.Material) || !Bind(TEXT("EntranceScanMaterial"), Scan.Material)) return 1;
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
	UE_LOG(LogTemp, Display, TEXT("MaintenanceEntrance assets compile errors=%d warnings=%d"), Results.NumErrors, Results.NumWarnings);
	if (Results.NumErrors || !Save(BP)) return 1;
	UE_LOG(LogTemp, Display, TEXT("MaintenanceEntrance assets COMPLETE: two UI materials and widget hard references"));
	return 0;
}
