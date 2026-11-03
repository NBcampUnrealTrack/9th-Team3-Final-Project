#include "Animation/knightAnimNotifyState_AttackTrail.h"

#include "Components/SkeletalMeshComponent.h"
#include "Monster/knightAttackTrailComponent.h"

UknightAnimNotifyState_AttackTrail::UknightAnimNotifyState_AttackTrail()
{
#if WITH_EDITORONLY_DATA
	bShouldFireInEditor = true;
#endif
}

void UknightAnimNotifyState_AttackTrail::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp || !MeshComp->GetWorld() || MeshComp->GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// 앞 구간이 끊긴 채 다시 시작하면 남은 궤적은 그대로 사라지게 두고 새로 그린다.
	if (const TWeakObjectPtr<UknightAttackTrailComponent>* const Previous = ActiveTrails.Find(MeshComp))
	{
		if (Previous->IsValid())
		{
			(*Previous)->Stop();
		}
	}

	UknightAttackTrailComponent* const Trail = NewObject<UknightAttackTrailComponent>(MeshComp->GetOwner() ? static_cast<UObject*>(MeshComp->GetOwner()) : MeshComp);
	Trail->SetMaterial(0, Material);
	Trail->RegisterComponent();

	UknightAttackTrailComponent::FSettings Settings;
	Settings.BaseSocket = BaseSocket;
	Settings.TipSocket = TipSocket;
	Settings.BaseRatio = BaseRatio;
	Settings.TipExtension = TipExtension;
	Settings.Lifetime = Lifetime;
	Settings.FadeTime = FadeIn;
	Settings.MinSpeed = MinSpeed;
	Settings.FullSpeed = FullSpeed;
	Trail->Start(MeshComp, Settings);
	ActiveTrails.Add(MeshComp, Trail);
}

void UknightAnimNotifyState_AttackTrail::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	TWeakObjectPtr<UknightAttackTrailComponent> Trail;
	if (ActiveTrails.RemoveAndCopyValue(MeshComp, Trail) && Trail.IsValid())
	{
		Trail->Stop();
	}
	// 메시가 먼저 사라진 항목도 정리한다.
	for (auto It = ActiveTrails.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

FString UknightAnimNotifyState_AttackTrail::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Attack Trail (%s-%s)"), *BaseSocket.ToString(), *TipSocket.ToString());
}
