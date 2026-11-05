#include "Animation/knightAnimNotifyState_WeaponTrail.h"

#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

void UknightAnimNotifyState_WeaponTrail::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	StopTrail(MeshComp);
	AActor* const Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!TrailEffect || !Owner)
	{
		return;
	}
	UMeshComponent* Weapon = nullptr;
	TArray<UMeshComponent*> Meshes;
	Owner->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* const Mesh : Meshes)
	{
		if (Mesh != MeshComp && Mesh->ComponentHasTag(WeaponTag))
		{
			Weapon = Mesh;
			break;
		}
	}
	if (!Weapon)
	{
		return;
	}

	// 칼 메시 자기 공간의 크기: 가장 긴 축이 칼날, 원점(손에 붙은 점)에서 더 먼 끝이 칼끝.
	const FBox Box = Weapon->CalcBounds(FTransform::Identity).GetBox();
	const FVector Size = Box.GetSize();
	const int32 AxisIndex = Size.X >= Size.Y && Size.X >= Size.Z ? 0 : (Size.Y >= Size.Z ? 1 : 2);
	const double Tip = FMath::Abs(Box.Max[AxisIndex]) >= FMath::Abs(Box.Min[AxisIndex]) ? Box.Max[AxisIndex] : Box.Min[AxisIndex];
	const double Start = Tip * BladeStartRatio;
	FVector Center = Box.GetCenter();
	Center[AxisIndex] = (Start + Tip) * 0.5;
	FVector Axis = FVector::ZeroVector;
	Axis[AxisIndex] = 1.0;
	// 띠 폭은 월드 단위라 칼 메시 크기(스케일)를 곱한다. 붙이는 위치는 붙인 컴포넌트 공간이라 그대로.
	const float BladeLength = static_cast<float>(FMath::Abs(Tip - Start) * Weapon->GetComponentScale()[AxisIndex]);

	UNiagaraComponent* const Trail = UNiagaraFunctionLibrary::SpawnSystemAttached(TrailEffect, Weapon, NAME_None, Center, FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset, true, false);
	if (!Trail)
	{
		return;
	}
	Trail->SetVariableFloat(WidthParameter, BladeLength * WidthScale);
	Trail->SetVariableVec3(AxisParameter, Axis);
	Trail->Activate(true);
	Trails.Add(MeshComp, Trail);
}

void UknightAnimNotifyState_WeaponTrail::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	StopTrail(MeshComp);
}

FString UknightAnimNotifyState_WeaponTrail::GetNotifyName_Implementation() const
{
	return TrailEffect ? FString::Printf(TEXT("Weapon Trail (%s)"), *TrailEffect->GetName()) : TEXT("Weapon Trail");
}

void UknightAnimNotifyState_WeaponTrail::StopTrail(USkeletalMeshComponent* MeshComp)
{
	TWeakObjectPtr<UNiagaraComponent> Trail;
	if (Trails.RemoveAndCopyValue(MeshComp, Trail) && Trail.IsValid())
	{
		// 새 궤적만 멈춘다. 남은 띠는 수명대로 사라지고, 다 사라지면 컴포넌트도 지워진다(bAutoDestroy).
		Trail->Deactivate();
	}
}
