#include "Animation/knightAnimNotifyState_PlayRate.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

FAnimMontageInstance* UknightAnimNotifyState_PlayRate::FindInstance(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) const
{
	const UAnimMontage* const Montage = Cast<UAnimMontage>(Animation);
	UAnimInstance* const AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	return (Montage && AnimInstance) ? AnimInstance->GetActiveInstanceForMontage(Montage) : nullptr;
}

void UknightAnimNotifyState_PlayRate::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (FAnimMontageInstance* const Instance = FindInstance(MeshComp, Animation))
	{
		Instance->SetPlayRate(Instance->GetPlayRate() * PlayRateScale);
	}
}

void UknightAnimNotifyState_PlayRate::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	// 몽타주가 중간에 끊겨 인스턴스가 사라졌으면 되돌릴 것도 없다.
	if (FAnimMontageInstance* const Instance = FindInstance(MeshComp, Animation))
	{
		if (PlayRateScale > KINDA_SMALL_NUMBER)
		{
			Instance->SetPlayRate(Instance->GetPlayRate() / PlayRateScale);
		}
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UknightAnimNotifyState_PlayRate::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Play Rate x%.2f"), PlayRateScale);
}
