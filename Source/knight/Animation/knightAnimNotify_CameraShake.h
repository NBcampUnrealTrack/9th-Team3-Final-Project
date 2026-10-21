#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "knightAnimNotify_CameraShake.generated.h"

class UCameraShakeBase;

/**
 * 가까운 플레이어의 화면을 흔든다(브레스의 지속 진동처럼 바닥을 찍지 않는 동작용). 길이와 세기는 Shake 에셋이 정한다.
 * 소켓 위치에서 InnerRadius 안은 그대로, OuterRadius에서 0이 되고, 플레이어의 화면 흔들림 설정을 따른다.
 */
UCLASS(meta=(DisplayName="Knight Camera Shake"))
class KNIGHT_API UknightAnimNotify_CameraShake : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, Category="Camera Shake")
	TSubclassOf<UCameraShakeBase> Shake;

	/** 흔들림 중심. None이면 메시 위치. */
	UPROPERTY(EditAnywhere, Category="Camera Shake")
	FName SocketName;

	UPROPERTY(EditAnywhere, Category="Camera Shake", meta=(ClampMin=0, Units="cm"))
	float InnerRadius = 2000.f;

	UPROPERTY(EditAnywhere, Category="Camera Shake", meta=(ClampMin=0, Units="cm"))
	float OuterRadius = 7000.f;
};
