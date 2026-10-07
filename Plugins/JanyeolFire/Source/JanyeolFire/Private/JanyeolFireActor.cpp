#include "JanyeolFireActor.h"
#include "Modules/ModuleManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"
IMPLEMENT_MODULE(FDefaultModuleImpl, JanyeolFire)

AJanyeolFireActor::AJanyeolFireActor() {
 PrimaryActorTick.bCanEverTick=true;
 Creature=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Janyeol"));
 SetRootComponent(Creature);
 Creature->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Creature->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 BackMantle=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ContinuousBackFlame"));
 BackMantle->SetupAttachment(Creature);
 BackMantle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 BackMantle->SetCastShadow(false);
 for(int i=0;i<16;i++) {
  auto C=CreateDefaultSubobject<UNiagaraComponent>(*FString::Printf(TEXT("Flame_%02d"),i));
  C->SetupAttachment(Creature); C->bTickInEditor=true; C->SetAutoActivate(true);
  Flames.Add(C);
 }
}
void AJanyeolFireActor::OnConstruction(const FTransform& T) {Super::OnConstruction(T); RefreshFire();}
#if WITH_EDITOR
void AJanyeolFireActor::PostEditChangeProperty(FPropertyChangedEvent& Event) {Super::PostEditChangeProperty(Event);RefreshFire();}
#endif
void AJanyeolFireActor::BeginPlay() {Super::BeginPlay(); RefreshFire();}
void AJanyeolFireActor::SetFireHeat(float Value) {Heat=FMath::Clamp(Value,0.f,1.f);}
void AJanyeolFireActor::Calm() {SetFireHeat(0);}
void AJanyeolFireActor::Enraged() {SetFireHeat(1);}
void AJanyeolFireActor::RefreshFire() {
 if(!Creature || !Creature->GetSkeletalMeshAsset())return;
 const auto& Ref=Creature->GetSkeletalMeshAsset()->GetRefSkeleton();
 auto Attach=[&](int I,const TCHAR* Bone,FVector Position) {
  int B=Ref.FindBoneIndex(FName(Bone)); if(B==INDEX_NONE)return;
  FTransform Rest=Ref.GetRefBonePose()[B];
  for(int P=Ref.GetParentIndex(B);P!=INDEX_NONE;P=Ref.GetParentIndex(P))Rest=Rest*Ref.GetRefBonePose()[P];
  Flames[I]->AttachToComponent(Creature,FAttachmentTransformRules::KeepRelativeTransform,FName(Bone));
  Flames[I]->SetRelativeLocation(Rest.InverseTransformPosition(Position));
  Flames[I]->SetRelativeRotation(Rest.GetRotation().Inverse());
 };
 Attach(0,TEXT("head"),FVector(-MouthAnchor.X,MouthAnchor.Y,MouthAnchor.Z));
 Attach(1,TEXT("head"),MouthAnchor);
 const TCHAR* Bones[]={TEXT("chest"),TEXT("chest"),TEXT("spine_02"),TEXT("spine_02"),TEXT("spine_01"),TEXT("pelvis"),TEXT("tail_01")};
 const FVector Points[]={FVector(-8,31,66),FVector(8,31,66),FVector(-7,12,61),FVector(7,12,61),FVector(0,-8,54),FVector(0,-27,46),FVector(0,-39,39)};
 for(int i=0;i<7;i++) {auto P=Points[i]+FVector(0,0,BackSurfaceLift);Attach(i+2,Bones[i],P);Attach(i+9,Bones[i],P);}
 if(FlameMaterial) {
  MouthMID=UMaterialInstanceDynamic::Create(MouthSmokeMaterial?MouthSmokeMaterial.Get():FlameMaterial.Get(),this);
  MouthRightMID=UMaterialInstanceDynamic::Create(MouthSmokeMaterial?MouthSmokeMaterial.Get():FlameMaterial.Get(),this);
  MouthMID->SetScalarParameterValue(TEXT("SmokePhase"),0.f);
  MouthMID->SetScalarParameterValue(TEXT("WaveAmount"),.8f);
  MouthRightMID->SetScalarParameterValue(TEXT("SmokePhase"),1.7f);
  MouthRightMID->SetScalarParameterValue(TEXT("WaveAmount"),1.f);
  LowMID=UMaterialInstanceDynamic::Create(FlameMaterial,this);
  RageMID=UMaterialInstanceDynamic::Create(FlameMaterial,this);
 }
 if(BodyHeatMaterial) {BodyMID=UMaterialInstanceDynamic::Create(BodyHeatMaterial,this);Creature->SetMaterial(0,BodyMID);}
 if(MantleMaterial){MantleMID=UMaterialInstanceDynamic::Create(MantleMaterial,this);BackMantle->SetMaterial(0,MantleMID);}
 BackMantle->SetVisibility(FireEnabled);
 for(int i=0;i<Flames.Num();i++) {
  auto C=Flames[i];
  C->SetAsset(i<2?MouthSystem:(i<9?BackLowSystem:BackRageSystem));
  C->SetVariableMaterial(TEXT("User.FireMaterial"),i==0?MouthMID:(i==1?MouthRightMID:(i<9?LowMID:RageMID)));
  C->SetVariableFloat(TEXT("User.Seed"),float(i)*13.37f);
  if(i<2)C->SetVariableVec3(TEXT("User.Flow"),i==0?FVector(-10.8,-18,18):FVector(11.5,-19,19));
  C->SetVisibility(FireEnabled);
  if(C->GetAsset() && FireEnabled)C->Activate(true);
 }
 if(PreviewAnimation) {
  Creature->SetAnimationMode(EAnimationMode::AnimationSingleNode);
  Creature->PlayAnimation(PreviewAnimation,true);
  Creature->SetPlayRate(PlaybackSpeed);
 }
 CurrentHeat=FMath::Clamp(Heat,0.f,1.f);ApplyHeat(0);
 UpdateMantle(true);
}
void AJanyeolFireActor::ApplyHeat(float Delta) {
 CurrentHeat=FMath::FInterpTo(CurrentHeat,FMath::Clamp(Heat,0.f,1.f),Delta,TransitionSpeed);
 const float Enabled=FireEnabled?1.f:0.f;
 if(MouthMID)MouthMID->SetScalarParameterValue(TEXT("FireStrength"),Enabled*(0.75f+CurrentHeat*0.4f));
 if(MouthRightMID)MouthRightMID->SetScalarParameterValue(TEXT("FireStrength"),Enabled*(0.72f+CurrentHeat*0.4f));
 if(LowMID)LowMID->SetScalarParameterValue(TEXT("FireStrength"),Enabled*(0.28f+CurrentHeat*0.12f));
 if(RageMID)RageMID->SetScalarParameterValue(TEXT("FireStrength"),Enabled*CurrentHeat*0.32f);
 if(BodyMID)BodyMID->SetScalarParameterValue(TEXT("Heat"),Enabled*(0.8f+CurrentHeat*1.8f));
 if(MantleMID){MantleMID->SetScalarParameterValue(TEXT("Heat"),CurrentHeat);MantleMID->SetScalarParameterValue(TEXT("FireStrength"),Enabled);}
}
void AJanyeolFireActor::Tick(float Delta) {
 Super::Tick(Delta);
#if WITH_EDITOR
 if(GetWorld() && GetWorld()->WorldType==EWorldType::Editor && AnimateInEditor && PreviewAnimation) {
  Creature->SetPlayRate(PlaybackSpeed);
  Creature->TickAnimation(Delta,false);Creature->RefreshBoneTransforms();
 }
#endif
 ApplyHeat(Delta);
 UpdateMantle(false);
}

// A single continuous sheet wraps the dorsal arc, skinned to the same spine chain.
// Niagara contributes only the loose trailing edges instead of separate fire columns.
void AJanyeolFireActor::UpdateMantle(bool Rebuild) {
 if(!Creature || !Creature->GetSkeletalMeshAsset() || !BackMantle)return;
 constexpr int Rows=33,Cols=25;
 const TCHAR* Names[]={TEXT("chest"),TEXT("spine_02"),TEXT("spine_01"),TEXT("pelvis"),TEXT("tail_01")};
 const float Ys[]={40,12,-10,-31,-65};
 const float Zs[]={73,67,61,51,34};
 const float Ws[]={22,28,28,23,9};
 const auto& Ref=Creature->GetSkeletalMeshAsset()->GetRefSkeleton();
 FTransform Rest[5],Pose[5];
 for(int k=0;k<5;k++){
  int B=Ref.FindBoneIndex(FName(Names[k]));if(B==INDEX_NONE)return;
  Rest[k]=Ref.GetRefBonePose()[B];
  for(int P=Ref.GetParentIndex(B);P!=INDEX_NONE;P=Ref.GetParentIndex(P))Rest[k]=Rest[k]*Ref.GetRefBonePose()[P];
  Pose[k]=Creature->GetSocketTransform(FName(Names[k]),RTS_Component);
 }
 TArray<FVector> V,N;TArray<FVector2D> UV;TArray<int32> Tris;
 TArray<FLinearColor> Colors;TArray<FProcMeshTangent> Tangents;
 V.Reserve(Rows*Cols);N.Reserve(Rows*Cols);UV.Reserve(Rows*Cols);
 const float T=GetWorld()?GetWorld()->GetTimeSeconds():0.f;
 for(int r=0;r<Rows;r++){
  const float Along=float(r)/(Rows-1),Y=FMath::Lerp(Ys[0],Ys[4],Along);
  int K=0;while(K<3 && Y<Ys[K+1])K++;
  const float A=FMath::Clamp((Ys[K]-Y)/(Ys[K]-Ys[K+1]),0.f,1.f);
  const float Top=FMath::Lerp(Zs[K],Zs[K+1],A),Width=FMath::Lerp(Ws[K],Ws[K+1],A);
  for(int c=0;c<Cols;c++){
   const float U=float(c)/(Cols-1),S=U*2-1;
   const float Envelope=FMath::Sin(Along*PI);
   const float Billow=(5.0f+12.f*CurrentHeat)*Envelope*(.65f+.35f*FMath::Sin(Along*11-T*2.2f+S*2));
   const FVector P(S*(Width+Billow*.24f),Y,Top-17*S*S+Billow);
   const FVector P0=Pose[K].TransformPosition(Rest[K].InverseTransformPosition(P));
   const FVector P1=Pose[K+1].TransformPosition(Rest[K+1].InverseTransformPosition(P));
   V.Add(FMath::Lerp(P0,P1,A));N.Add(FVector(S*.65f,0,1).GetSafeNormal());UV.Add(FVector2D(U,Along));
  }
 }
 if(Rebuild || !BackMantle->GetProcMeshSection(0)){
  for(int r=0;r<Rows-1;r++)for(int c=0;c<Cols-1;c++){
   int I=r*Cols+c;Tris.Append({I,I+Cols,I+1,I+1,I+Cols,I+Cols+1});
  }
  BackMantle->CreateMeshSection_LinearColor(0,V,Tris,N,UV,Colors,Tangents,false);
 }else BackMantle->UpdateMeshSection_LinearColor(0,V,N,UV,Colors,Tangents);
}
