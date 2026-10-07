#include "JanyeolFireBuilder.h"
#include "Modules/ModuleManager.h"
#include "NiagaraSystem.h"
#include "NiagaraModule.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraConstants.h"
#include "NiagaraParameterStore.h"
#include "Stateless/NiagaraStatelessEmitter.h"
#include "Stateless/Modules/NiagaraStatelessModule_InitializeParticle.h"
#include "Stateless/Modules/NiagaraStatelessModule_AddVelocity.h"
#include "Stateless/Modules/NiagaraStatelessModule_ShapeLocation.h"
#include "Stateless/Modules/NiagaraStatelessModule_CurlNoiseForce.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleColor.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleSpriteSize.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleRibbonWidth.h"
#include "Stateless/Modules/NiagaraStatelessModule_AccelerationForce.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"
IMPLEMENT_MODULE(FDefaultModuleImpl, JanyeolFireEditor)

UNiagaraSystem* UJanyeolFireBuilder::BuildFlameSystem(const FString& Path,UMaterialInterface* Material,int32 Style) {
 UNiagaraSystem* Source=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Niagara/DefaultAssets/Templates/Systems/FountainLightweight.FountainLightweight"));
 if(!Source||!Material)return nullptr;
 auto Existing=LoadObject<UNiagaraSystem>(nullptr,*(Path+TEXT(".")+FPackageName::GetLongPackageAssetName(Path)));
 UPackage* Package=CreatePackage(*Path);
 auto Sys=Existing?Existing:Cast<UNiagaraSystem>(StaticDuplicateObject(Source,Package,*FPackageName::GetLongPackageAssetName(Path)));
 if(!Sys)return nullptr;
 Sys->SetFlags(RF_Public|RF_Standalone);
 auto& Store=Sys->GetExposedParameters();
 FNiagaraVariable MatVar(FNiagaraTypeDefinition(UMaterialInterface::StaticClass()),TEXT("User.FireMaterial"));
 Store.AddParameter(MatVar);Store.SetUObject(Material,MatVar);
 Store.SetParameterValue(0.f,FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.Seed")),true);
 if(Style==3)Store.SetParameterValue(FVector3f(15,-14,1),FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.Flow")),true);
 for(auto& H:Sys->GetEmitterHandles()) {
  auto E=H.GetStatelessEmitter();if(!E)continue;
  for(auto M:E->GetModules()) {
   const FString Name=M->GetClass()->GetName();
   bool On=Name.Contains(TEXT("InitializeParticle"))||Name.Contains(TEXT("AddVelocity"))||Name.Contains(TEXT("ShapeLocation"))||Name.Contains(TEXT("CurlNoiseForce"))||Name.Contains(TEXT("SolveVelocitiesAndForces"))||Name.Contains(TEXT("ScaleColor"))||Name.Contains(TEXT("ScaleSpriteSize"))||Name.Contains(TEXT("CalculateAccurateVelocity"));
   M->SetIsModuleEnabled(On && !Name.Contains(TEXT("BySpeed")));
   if(Style==3)M->SetIsModuleEnabled(Name.Contains(TEXT("InitializeParticle"))||Name.Contains(TEXT("AddVelocity"))||Name.Contains(TEXT("AccelerationForce"))||Name.Contains(TEXT("SolveVelocitiesAndForces"))||Name.Contains(TEXT("ScaleColor"))||Name.Contains(TEXT("ScaleRibbonWidth")));
  }
  for(int i=0;i<E->GetNumSpawnInfos();i++)E->GetSpawnInfoByIndex(i)->bEnabled=false;
  auto& Spawn=*E->GetSpawnInfoByIndex(0);Spawn.Type=ENiagaraStatelessSpawnInfoType::Rate;Spawn.Rate.InitConstant(Style==3?14.f:(Style==2?22.f:(Style==0?16.f:20.f)));Spawn.bEnabled=true;
  if(Style==3)Spawn.Rate.InitConstant(60.f);
  if(Style==1 || Style==2)Spawn.Rate.InitConstant(Style==1?9.f:12.f);
  if(auto M=Cast<UNiagaraStatelessModule_InitializeParticle>(E->GetModule(UNiagaraStatelessModule_InitializeParticle::StaticClass()))) {
   M->LifetimeDistribution.InitRange(Style==3?0.75f:(Style==2?0.42f:0.22f),Style==3?1.15f:(Style==2?0.8f:0.46f));
   M->SpriteSizeDistribution.InitConstant(Style==3?FVector2f(13,17):(Style==0?FVector2f(8,22):(Style==1?FVector2f(29,45):FVector2f(38,65))));
   M->SpriteRotationDistribution.InitRange(Style==3?-65.f:-12.f,Style==3?65.f:12.f);
   M->ColorDistribution=FNiagaraDistributionColor(FLinearColor::White);
   if(Style==3){M->LifetimeDistribution.InitConstant(.9f);M->bWriteRibbonWidth=true;M->RibbonWidthDistribution.InitConstant(7.f);}
  }
  if(auto M=Cast<UNiagaraStatelessModule_AddVelocity>(E->GetModule(UNiagaraStatelessModule_AddVelocity::StaticClass()))) {
   M->VelocityType=ENSM_VelocityType::Linear;M->CoordinateSpace=ENiagaraCoordinateSpace::Local;
   M->LinearVelocityDistribution.InitConstant(Style==3?FVector3f(0,-36,6):(Style==0?FVector3f(0,-65,4):(Style==1?FVector3f(0,-58,6):FVector3f(0,-65,18))));
   M->LinearVelocityScale.InitRange(0.7f,1.15f);
   if(Style==3){M->LinearVelocityScale.InitConstant(1.f);M->LinearVelocityDistribution.Mode=ENiagaraDistributionMode::Binding;M->LinearVelocityDistribution.ParameterBinding=FNiagaraVariableBase(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.Flow"));}
  }
  if(auto M=Cast<UNiagaraStatelessModule_ShapeLocation>(E->GetModule(UNiagaraStatelessModule_ShapeLocation::StaticClass()))) {
   M->ShapePrimitive=ENSM_ShapePrimitive::Sphere;M->SphereRadius.InitConstant(Style==0||Style==3?1.6f:4.f);
  }
  if(auto M=Cast<UNiagaraStatelessModule_CurlNoiseForce>(E->GetModule(UNiagaraStatelessModule_CurlNoiseForce::StaticClass()))) {M->NoiseStrength=Style==3?8.f:(Style==2?7.f:3.f);M->NoiseFrequency=0.025f;}
  if(Style==3){
   if(auto M=Cast<UNiagaraStatelessModule_AccelerationForce>(E->GetModule(UNiagaraStatelessModule_AccelerationForce::StaticClass()))){M->AccelerationDistribution.InitConstant(FVector3f(0,0,1));M->CoordinateSpace=ENiagaraCoordinateSpace::Local;}
   if(auto M=Cast<UNiagaraStatelessModule_ScaleRibbonWidth>(E->GetModule(UNiagaraStatelessModule_ScaleRibbonWidth::StaticClass()))){auto& D=M->ScaleDistribution;D.Mode=ENiagaraDistributionMode::UniformCurve;D.ChannelCurves.SetNum(1);D.ChannelCurves[0].Reset();D.ChannelCurves[0].AddKey(0,0.45f);D.ChannelCurves[0].AddKey(.3f,1.f);D.ChannelCurves[0].AddKey(1,1.9f);D.UpdateValuesFromDistribution();}
  }
  if(auto M=Cast<UNiagaraStatelessModule_ScaleColor>(E->GetModule(UNiagaraStatelessModule_ScaleColor::StaticClass()))) {
   auto& D=M->ScaleDistribution;D.Mode=ENiagaraDistributionMode::NonUniformCurve;D.ChannelCurves.SetNum(4);
   for(int c=0;c<4;c++) {D.ChannelCurves[c].Reset();D.ChannelCurves[c].AddKey(0,c==3?(Style==3?0.85f:0.f):1.f);D.ChannelCurves[c].AddKey(0.13f,1);D.ChannelCurves[c].AddKey(0.6f,c==3?0.7f:1.f);D.ChannelCurves[c].AddKey(1,c==3?0.f:1.f);}
   D.UpdateValuesFromDistribution();
  }
  if(auto M=Cast<UNiagaraStatelessModule_ScaleSpriteSize>(E->GetModule(UNiagaraStatelessModule_ScaleSpriteSize::StaticClass()))) {
   auto& D=M->ScaleDistribution;D.Mode=ENiagaraDistributionMode::NonUniformCurve;D.ChannelCurves.SetNum(2);
   for(int c=0;c<2;c++) {D.ChannelCurves[c].Reset();D.ChannelCurves[c].AddKey(0,Style==3?0.4f:0.5f);D.ChannelCurves[c].AddKey(0.2f,1);D.ChannelCurves[c].AddKey(1,Style==3?2.2f:0.15f);}
   D.UpdateValuesFromDistribution();
  }
  for(auto R:E->GetRenderers())if(auto Sprite=Cast<UNiagaraSpriteRendererProperties>(R)) {
   Sprite->Material=Material;Sprite->MaterialUserParamBinding.Parameter=MatVar;
   Sprite->Alignment=Style==3?ENiagaraSpriteAlignment::Unaligned:ENiagaraSpriteAlignment::VelocityAligned;
   Sprite->SortMode=ENiagaraSortMode::ViewDepth;
  }
  if(Style==3){
   TArray<UNiagaraRendererProperties*> Old=E->GetRenderers();for(auto R:Old)E->RemoveRenderer(R,FGuid());
   auto Ribbon=NewObject<UNiagaraRibbonRendererProperties>(E);
   Ribbon->Material=Material;Ribbon->MaterialUserParamBinding.Parameter=MatVar;
   Ribbon->FacingMode=ENiagaraRibbonFacingMode::Screen;Ribbon->Shape=ENiagaraRibbonShapeMode::Plane;
   Ribbon->RibbonLinkOrderBinding=FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
   Ribbon->U0OverrideBinding=FNiagaraConstants::GetAttributeDefaultBinding(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
   Ribbon->UV0Settings.bEnablePerParticleUOverride=true;
   Ribbon->TessellationMode=ENiagaraRibbonTessellationMode::Custom;Ribbon->TessellationFactor=4;Ribbon->CurveTension=0.1f;
   E->AddRenderer(Ribbon,FGuid());
  }
  if(auto P=FindFProperty<FStructProperty>(E->GetClass(),TEXT("FixedBounds"))) *P->ContainerPtrToValuePtr<FBox>(E)=FBox(FVector(-160),FVector(160));
  E->PostEditChange();
 }
 Sys->PostEditChange();Sys->RequestCompile(false);Sys->WaitForCompilationComplete();
 Sys->MarkPackageDirty();if(!Existing)FAssetRegistryModule::AssetCreated(Sys);
 return Sys;
}
FString UJanyeolFireBuilder::InspectFlameSystem(UNiagaraSystem* Sys) {
 if(!Sys)return TEXT("null");FString Out=Sys->GetPathName();
 for(auto& H:Sys->GetEmitterHandles()) {
  auto E=H.GetStatelessEmitter();if(!E){Out+=TEXT(" NO STATELESS EMITTER");continue;}
  Out+=FString::Printf(TEXT("\nEmitter %s spawn infos %d renderers %d"),*E->GetName(),E->GetNumSpawnInfos(),E->GetRenderers().Num());
  for(auto M:E->GetModules())Out+=FString::Printf(TEXT("\n %s enabled=%d"),*M->GetClass()->GetName(),M->IsModuleEnabled());
 }
 return Out;
}
