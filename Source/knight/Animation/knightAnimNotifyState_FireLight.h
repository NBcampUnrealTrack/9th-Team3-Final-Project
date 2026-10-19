#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "knightAnimNotifyState_FireLight.generated.h"

class UPointLightComponent;

/**
 * 구간 동안 불빛(포인트 라이트)을 소켓에 붙여 주변 바닥과 캐릭터를 비춘다. 브레스처럼 불을 뿜는 동작용 연출이다.
 *
 * 라이트는 소켓 기준 LocationOffset/RotationOffset(Timed Niagara Effect와 같은 값)에서 그 방향 앞으로 Distance(월드 cm)만큼
 * 떨어진 곳에 둔다. 처음 FadeIn초 동안 켜지고 마지막 FadeOut초 동안 꺼지며, 그 사이 Flicker 비율만큼 흔들린다.
 * 비용 때문에 그림자는 기본으로 끈다.
 */
UCLASS(meta=(DisplayName="Knight Fire Light"))
class KNIGHT_API UknightAnimNotifyState_FireLight : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UknightAnimNotifyState_FireLight();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, Category="Fire Light")
	FName SocketName;

	UPROPERTY(EditAnywhere, Category="Fire Light")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category="Fire Light")
	FRotator RotationOffset = FRotator::ZeroRotator;

	/** 소켓 방향(RotationOffset 적용 후 +X)으로 라이트를 내보내는 거리. 메시 스케일과 관계없이 월드 cm다. */
	UPROPERTY(EditAnywhere, Category="Fire Light", meta=(Units="cm"))
	float Distance = 0.f;

	UPROPERTY(EditAnywhere, Category="Fire Light")
	FLinearColor Color = FLinearColor(1.f, 0.42f, 0.12f);

	UPROPERTY(EditAnywhere, Category="Fire Light", meta=(ClampMin=0, Units="cd"))
	float Intensity = 50000.f;

	UPROPERTY(EditAnywhere, Category="Fire Light", meta=(ClampMin=0, Units="cm"))
	float AttenuationRadius = 3000.f;

	/** 깜빡임 세기(0이면 일정, 0.3이면 70~100%). */
	UPROPERTY(EditAnywhere, Category="Fire Light", meta=(ClampMin=0, ClampMax=1))
	float Flicker = 0.3f;

	UPROPERTY(EditAnywhere, Category="Fire Light", meta=(ClampMin=0))
	float FlickerSpeed = 12.f;

	UPROPERTY(EditAnywhere, Category="Fire Light", meta=(ClampMin=0, Units="s"))
	float FadeIn = 0.15f;

	UPROPERTY(EditAnywhere, Category="Fire Light", meta=(ClampMin=0, Units="s"))
	float FadeOut = 0.3f;

	UPROPERTY(EditAnywhere, Category="Fire Light")
	bool bCastShadows = false;

private:
	struct FActiveLight
	{
		TWeakObjectPtr<UPointLightComponent> Light;
		float Elapsed = 0.f;
		float Duration = 0.f;
	};

	/** 노티파이 객체는 같은 몽타주를 재생하는 모든 메시가 함께 쓰므로 메시마다 라이트를 따로 둔다. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FActiveLight> ActiveLights;

	void UpdateLight(const FActiveLight& Active) const;
	void RemoveLight(USkeletalMeshComponent* MeshComp);
};
