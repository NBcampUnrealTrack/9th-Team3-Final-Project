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

float UknightAnimNotifyState_PlayRate::GetPlaySeconds(const UAnimMontage& Montage, float From, float To, float PlayRate)
{
	if (To <= From)
	{
		return 0.f;
	}
	// 구간 경계마다 끊어서, 조각마다 겹친 Play Rate 구간의 배율을 곱해 나눈다.
	TArray<float> Cuts = { From, To };
	for (const FAnimNotifyEvent& Event : Montage.Notifies)
	{
		if (Cast<UknightAnimNotifyState_PlayRate>(Event.NotifyStateClass))
		{
			Cuts.Add(FMath::Clamp(Event.GetTriggerTime(), From, To));
			Cuts.Add(FMath::Clamp(Event.GetEndTriggerTime(), From, To));
		}
	}
	Cuts.Sort();
	const float BaseRate = FMath::Max(Montage.RateScale * PlayRate, KINDA_SMALL_NUMBER);
	float Seconds = 0.f;
	for (int32 Index = 1; Index < Cuts.Num(); ++Index)
	{
		const float Length = Cuts[Index] - Cuts[Index - 1];
		if (Length <= 0.f)
		{
			continue;
		}
		const float Middle = (Cuts[Index] + Cuts[Index - 1]) * 0.5f;
		float Rate = BaseRate;
		for (const FAnimNotifyEvent& Event : Montage.Notifies)
		{
			const UknightAnimNotifyState_PlayRate* const Window = Cast<UknightAnimNotifyState_PlayRate>(Event.NotifyStateClass);
			if (Window && Middle >= Event.GetTriggerTime() && Middle < Event.GetEndTriggerTime())
			{
				Rate *= Window->PlayRate;
			}
		}
		Seconds += Length / Rate;
	}
	return Seconds;
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
