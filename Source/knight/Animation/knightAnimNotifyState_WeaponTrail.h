#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "knightAnimNotifyState_WeaponTrail.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

/**
 * 구간 동안 칼날을 따라 궤적(띠 모양 나이아가라, 예: NS_Knight_SwordTrail)을 남긴다. 패링 반격 몽타주의 휘두르는 구간에 넣는다.
 *
 * 칼은 소유 액터에서 WeaponTag가 붙은 메시(무기 아이템이 붙인 것)다. 무기마다 길이가 달라서 칼 메시의 크기를 재서 맞춘다:
 * 가장 긴 축을 칼날 방향, 붙은 점(손잡이)에서 더 먼 끝을 칼끝으로 보고, 손잡이 쪽 BladeStartRatio만큼을 뺀 칼날 가운데에 붙인다.
 * 띠 폭(WidthParameter)은 그 칼날 길이 x WidthScale, 띠 방향(AxisParameter)은 칼날 방향으로 넣는다. 칼 메시가 없으면 아무것도 안 한다.
 * 구간이 끝나면(몽타주가 끊겨도) 새 궤적만 멈추고, 이미 남은 띠는 이펙트 수명대로 사라진다. 숫자는 모두 바뀔 수 있는 값.
 */
UCLASS(meta=(DisplayName="Knight Weapon Trail"))
class KNIGHT_API UknightAnimNotifyState_WeaponTrail : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 궤적 이펙트. 사용자 변수 Width(띠 폭)·AxisOffset(띠 방향)을 쓰는 띠 이펙트여야 한다(SlashHitVFX의 NS_Trail_Sword 계열). */
	UPROPERTY(EditAnywhere, Category="Weapon Trail")
	TObjectPtr<UNiagaraSystem> TrailEffect;

	/** 칼 메시를 찾을 컴포넌트 태그. 무기 아이템(UknightWeaponItemDefinition)이 붙인다. */
	UPROPERTY(EditAnywhere, Category="Weapon Trail")
	FName WeaponTag = TEXT("Weapon");

	/** 손잡이에서 칼끝까지 중 궤적에서 뺄 손잡이 쪽 비율. 0이면 손잡이부터, 0.3이면 칼끝 쪽 70%만. */
	UPROPERTY(EditAnywhere, Category="Weapon Trail", meta=(ClampMin=0, ClampMax=0.9))
	float BladeStartRatio = 0.3f;

	/** 띠 폭 = 칼날 길이 x 이 값. */
	UPROPERTY(EditAnywhere, Category="Weapon Trail", meta=(ClampMin=0.1))
	float WidthScale = 1.f;

	UPROPERTY(EditAnywhere, Category="Weapon Trail")
	FName WidthParameter = TEXT("Width");

	UPROPERTY(EditAnywhere, Category="Weapon Trail")
	FName AxisParameter = TEXT("AxisOffset");

private:
	/** 노티파이 객체는 같은 몽타주를 재생하는 모든 메시가 함께 쓰므로 메시마다 궤적을 따로 둔다. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TWeakObjectPtr<UNiagaraComponent>> Trails;

	void StopTrail(USkeletalMeshComponent* MeshComp);
};
