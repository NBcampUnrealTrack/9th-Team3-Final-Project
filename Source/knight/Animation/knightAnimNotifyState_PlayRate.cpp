#include "Animation/knightAnimNotifyState_PlayRate.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

void UknightAnimNotifyState_PlayRate::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	ScalePlayRate(MeshComp, Animation, PlayRate);
}

void UknightAnimNotifyState_PlayRate::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	ScalePlayRate(MeshComp, Animation, 1.f / PlayRate);
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UknightAnimNotifyState_PlayRate::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Play Rate x%.2g"), PlayRate);
}

void UknightAnimNotifyState_PlayRate::ScalePlayRate(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float Scale) const
{
	UAnimMontage* const Montage = Cast<UAnimMontage>(Animation);
	UAnimInstance* const AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	// 시작과 끝에서 곱하고 나누므로, 몽타주에 원래 걸린 속도(GA의 재생 속도 등)는 구간이 끝나면 그대로 돌아온다.
	if (FAnimMontageInstance* const Instance = Montage && AnimInstance ? AnimInstance->GetActiveInstanceForMontage(Montage) : nullptr)
	{
		Instance->SetPlayRate(Instance->GetPlayRate() * Scale);
	}
}
