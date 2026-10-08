#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "knightGameplayAbility_Interact.generated.h"

/**
 * 상호작용 (F, InputTag.Interact). 누른 순간 주변 InteractRadius 안에서 IInteractableInterface를 구현하고
 * CanInteract가 참인 액터 중, 정면에 가깝고 가까운 것을 고른다.
 *   - GetInteractionDuration()이 0이면 바로 Interact를 부른다.
 *   - 0보다 크면 그 시간 동안 키를 누르고 있어야 하고, 도중에 떼면 취소한다.
 * UI에서 표시할 대상을 미리 알고 싶으면 FindBestInteractable을 쓰면 된다 (같은 규칙).
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_Interact : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_Interact(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Interactor 주변에서 상호작용할 대상. 없으면 nullptr. */
	UFUNCTION(BlueprintCallable, Category="Knight|Interaction")
	static AActor* FindBestInteractable(AActor* Interactor, float Radius = 250.f, float MaxAngle = 100.f);

protected:
	UPROPERTY(EditDefaultsOnly, Category="Interaction", meta=(ClampMin=0, Units="cm"))
	float InteractRadius = 250.f;

	/** 정면 기준 이 각도 밖의 대상은 고르지 않는다 (뒤쪽 제외). */
	UPROPERTY(EditDefaultsOnly, Category="Interaction", meta=(ClampMin=1, ClampMax=180, Units="deg"))
	float MaxInteractAngle = 100.f;

private:
	UFUNCTION()
	void OnHoldFinished();

	UFUNCTION()
	void OnHoldReleased(float TimeHeld);

	void Finish(bool bWasCancelled);

	TWeakObjectPtr<AActor> Target;
	bool bDone = false;
};
