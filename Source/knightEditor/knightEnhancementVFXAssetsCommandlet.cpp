#include "knightEnhancementVFXAssetsCommandlet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
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
	const FString EnhancementVFXAssetFolder = TEXT("/Game/Knight/UI/Menu/Maintenance/Enhancement/VFX/");
	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
		const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		return UPackage::SavePackage(Package, Asset, *File, Args);
	}
	template<typename T> T* Asset(const TCHAR* Name)
	{
		const FString Path = EnhancementVFXAssetFolder + Name;
		if (T* Existing = LoadObject<T>(nullptr, *(Path + TEXT(".") + Name), nullptr, LOAD_NoWarn)) return Existing;
		T* Created = NewObject<T>(CreatePackage(*Path), Name, RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Created);
		return Created;
	}
	struct FMaskBuilder
	{
		UMaterial* Material;
		UMaterialExpressionCustom* Mask;
		FMaskBuilder(const TCHAR* Name, const TCHAR* Code)
		{
			Material = Asset<UMaterial>(Name);
			Material->PreEditChange(nullptr);
			Material->GetExpressionCollection().Empty();
			Material->MaterialDomain = MD_UI;
			Material->BlendMode = BLEND_Additive;
			Mask = NewObject<UMaterialExpressionCustom>(Material);
			Mask->Description = TEXT("Explicit soft light masks; no scene bloom dependency");
			Mask->OutputType = CMOT_Float3; Mask->Code = Code; Mask->Inputs.Empty();
			Material->GetExpressionCollection().AddExpression(Mask);
			auto* UV = NewObject<UMaterialExpressionTextureCoordinate>(Material);
			Material->GetExpressionCollection().AddExpression(UV);
			Input(TEXT("UV"), UV);
			Material->GetEditorOnlyData()->EmissiveColor.Connect(0, Mask);
			Material->GetEditorOnlyData()->Opacity.UseConstant = true;
			Material->GetEditorOnlyData()->Opacity.Constant = 1.f;
		}
		void Input(const TCHAR* Name, UMaterialExpression* Expression)
		{
			FCustomInput In; In.InputName = Name; In.Input.Connect(0, Expression); Mask->Inputs.Add(In);
		}
		void Scalar(const TCHAR* Name, float Value, const TCHAR* Group)
		{
			auto* P = NewObject<UMaterialExpressionScalarParameter>(Material);
			P->ParameterName = Name; P->DefaultValue = Value; P->Group = Group;
			P->MaterialExpressionEditorX = -400; P->MaterialExpressionEditorY = Mask->Inputs.Num() * 100;
			Material->GetExpressionCollection().AddExpression(P); Input(Name, P);
		}
		void Color(const TCHAR* Name, FLinearColor Value)
		{
			auto* P = NewObject<UMaterialExpressionVectorParameter>(Material);
			P->ParameterName = Name; P->DefaultValue = Value; P->Group = TEXT("Color");
			Material->GetExpressionCollection().AddExpression(P); Input(Name, P);
		}
		void Finish() { Material->PostEditChange(); }
	};
}

int32 UknightEnhancementVFXAssetsCommandlet::Main(const FString& Params)
{
	// Procedural masks remain resolution independent. The image quad includes padding for the halo/trail.
	FMaskBuilder Fragment(TEXT("M_UI_EnhancementFragment"), TEXT(R"HLSL(
float x = abs(UV.x - 0.5);
float y = UV.y - 0.5;
float cap = max(0.0, abs(y) - CoreLength * 0.5);
float core = exp(-pow(x / 0.055, 4.0) - pow(cap / 0.022, 2.0));
float haloCap = max(0.0, abs(y) - HaloLength * 0.5);
float halo = exp(-pow(x / max(HaloWidth, 0.001), 2.0) - pow(haloCap / 0.12, 2.0));
float tailY = y * TrailDirection;
float tail = exp(-pow(x / 0.09, 2.0)) * exp(-max(0.0, tailY) / max(TrailLength, 0.001));
tail *= smoothstep(-0.03, 0.03, tailY) * (1.0 - smoothstep(0.40, 0.50, abs(y)));
float edge = (1.0 - smoothstep(0.36, 0.50, x)) * (1.0 - smoothstep(0.43, 0.50, abs(y)));
return edge * (CoreColor * core * CoreBrightness * CoreOpacity
    + GlowColor * (halo * HaloBrightness * HaloOpacity + tail * TrailBrightness * TrailOpacity));
)HLSL"));
	Fragment.Color(TEXT("CoreColor"), FLinearColor(1.f, .62f, .12f));
	Fragment.Color(TEXT("GlowColor"), FLinearColor(1.f, .19f, .008f));
	Fragment.Scalar(TEXT("CoreBrightness"), 1.5f, TEXT("Core"));
	Fragment.Scalar(TEXT("CoreOpacity"), 1.f, TEXT("Core"));
	Fragment.Scalar(TEXT("CoreLength"), .24f, TEXT("Core"));
	Fragment.Scalar(TEXT("HaloBrightness"), .28f, TEXT("Halo"));
	Fragment.Scalar(TEXT("HaloOpacity"), .7f, TEXT("Halo"));
	Fragment.Scalar(TEXT("HaloLength"), .30f, TEXT("Halo"));
	Fragment.Scalar(TEXT("HaloWidth"), .23f, TEXT("Halo"));
	Fragment.Scalar(TEXT("TrailBrightness"), .60f, TEXT("Trail"));
	Fragment.Scalar(TEXT("TrailOpacity"), .65f, TEXT("Trail"));
	Fragment.Scalar(TEXT("TrailLength"), .16f, TEXT("Trail"));
	Fragment.Scalar(TEXT("TrailDirection"), 1.f, TEXT("Trail"));
	Fragment.Finish();
	FMaskBuilder Glow(TEXT("M_UI_EnhancementFieldGlow"), TEXT(R"HLSL(
float2 p = (UV - 0.5) * 2.0;
float radius = dot(p, p);
float mask = exp(-radius * 4.8) * (1.0 - smoothstep(0.55, 1.0, radius));
return GlowColor * mask * Brightness * Opacity;
)HLSL"));
	Glow.Color(TEXT("GlowColor"), FLinearColor(1.f, .16f, .004f));
	Glow.Scalar(TEXT("Brightness"), .15f, TEXT("Field"));
	Glow.Scalar(TEXT("Opacity"), .5f, TEXT("Field"));
	Glow.Finish();
	auto* Core = Asset<UMaterialInstanceConstant>(TEXT("MI_UI_EnhancementCore"));
	Core->SetParentEditorOnly(Fragment.Material); Core->PostEditChange();
	auto* Echo = Asset<UMaterialInstanceConstant>(TEXT("MI_UI_EnhancementEcho"));
	Echo->SetParentEditorOnly(Fragment.Material);
	Echo->SetScalarParameterValueEditorOnly(TEXT("CoreBrightness"), .65f);
	Echo->SetScalarParameterValueEditorOnly(TEXT("HaloBrightness"), .16f);
	Echo->SetScalarParameterValueEditorOnly(TEXT("CoreLength"), .34f);
	Echo->SetScalarParameterValueEditorOnly(TEXT("TrailOpacity"), .4f);
	Echo->PostEditChange();
	GShaderCompilingManager->FinishAllCompilation();
	for (UMaterial* M : { Fragment.Material, Glow.Material })
	{
		const FMaterialResource* R = M->GetMaterialResource(GMaxRHIShaderPlatform);
		if (!R || R->GetCompileErrors().Num())
		{
			if (R) for (const FString& Error : R->GetCompileErrors()) UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
			return 1;
		}
	}
	for (UObject* Object : { static_cast<UObject*>(Fragment.Material), static_cast<UObject*>(Glow.Material), static_cast<UObject*>(Core), static_cast<UObject*>(Echo) })
	{
		if (!Save(Object)) return 1;
		UE_LOG(LogTemp, Display, TEXT("EnhancementVFX saved %s"), *Object->GetPathName());
	}
	UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenance"));
	if (!BP || !BP->GeneratedClass) return 1;
	UObject* Defaults = BP->GeneratedClass->GetDefaultObject();
	auto Bind = [Defaults](const TCHAR* Name, UObject* Value)
	{
		auto* Property = FindFProperty<FObjectPropertyBase>(Defaults->GetClass(), Name);
		if (!Property) return false;
		Property->SetObjectPropertyValue_InContainer(Defaults, Value); return true;
	};
	if (!Bind(TEXT("EnhancementFragmentMaterial"), Core) || !Bind(TEXT("EnhancementEchoMaterial"), Echo)
		|| !Bind(TEXT("EnhancementFieldGlowMaterial"), Glow.Material)) return 1;
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
	UE_LOG(LogTemp, Display, TEXT("EnhancementVFX widget compile errors=%d warnings=%d"), Results.NumErrors, Results.NumWarnings);
	if (Results.NumErrors || !Save(BP)) return 1;
	UE_LOG(LogTemp, Display, TEXT("EnhancementVFX COMPLETE; 4 materials, widget hard references, no gameplay/save mutation"));
	return 0;
}
