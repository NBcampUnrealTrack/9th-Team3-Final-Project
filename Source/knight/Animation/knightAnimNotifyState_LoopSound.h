#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "knightAnimNotifyState_LoopSound.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * 구간 동안 반복 소리를 소켓에 붙여 틀고, 구간이 끝나면(몽타주가 끊겨도) FadeOut초 동안 잦아들게 끈다.
 * 브레스 분출처럼 길이가 정해진 동작의 지속음용이다. Sound는 반복(Looping)으로 설정된 사운드여야 한다.
 */
UCLASS(meta=(DisplayName="Knight Loop Sound"))
class KNIGHT_API UknightAnimNotifyState_LoopSound : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UknightAnimNotifyState_LoopSound();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, Category="Loop Sound")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, Category="Loop Sound")
	FName SocketName;

	UPROPERTY(EditAnywhere, Category="Loop Sound", meta=(ClampMin=0))
	float VolumeMultiplier = 1.f;

	UPROPERTY(EditAnywhere, Category="Loop Sound", meta=(ClampMin=0, Units="s"))
	float FadeIn = 0.1f;

	UPROPERTY(EditAnywhere, Category="Loop Sound", meta=(ClampMin=0, Units="s"))
	float FadeOut = 0.3f;

private:
	/** 노티파이 객체는 같은 몽타주를 재생하는 모든 메시가 함께 쓰므로 메시마다 소리를 따로 둔다. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TWeakObjectPtr<UAudioComponent>> ActiveSounds;

	void StopSound(USkeletalMeshComponent* MeshComp);
};
