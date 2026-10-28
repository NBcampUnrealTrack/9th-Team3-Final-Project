#include "Animation/knightAnimNotifyState_LoopSound.h"

#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UknightAnimNotifyState_LoopSound::UknightAnimNotifyState_LoopSound()
{
#if WITH_EDITORONLY_DATA
	bShouldFireInEditor = true;
#endif
}

void UknightAnimNotifyState_LoopSound::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp || !Sound)
	{
		return;
	}
	// 앞 구간이 끊긴 채 다시 시작하면 남은 소리부터 끈다.
	StopSound(MeshComp);
	const FName Socket = MeshComp->DoesSocketExist(SocketName) ? SocketName : NAME_None;
	if (UAudioComponent* const Audio = UGameplayStatics::SpawnSoundAttached(Sound, MeshComp, Socket, FVector::ZeroVector,
		EAttachLocation::KeepRelativeOffset, true, VolumeMultiplier))
	{
		Audio->FadeIn(FadeIn, VolumeMultiplier);
		ActiveSounds.Add(MeshComp, Audio);
	}
}

void UknightAnimNotifyState_LoopSound::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	StopSound(MeshComp);
}

FString UknightAnimNotifyState_LoopSound::GetNotifyName_Implementation() const
{
	return Sound ? FString::Printf(TEXT("Loop %s"), *Sound->GetName()) : TEXT("Knight Loop Sound");
}

void UknightAnimNotifyState_LoopSound::StopSound(USkeletalMeshComponent* MeshComp)
{
	TWeakObjectPtr<UAudioComponent> Audio;
	if (ActiveSounds.RemoveAndCopyValue(MeshComp, Audio) && Audio.IsValid())
	{
		Audio->FadeOut(FadeOut, 0.f);
	}
	// 메시가 먼저 사라진 항목도 정리한다.
	for (auto It = ActiveSounds.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
