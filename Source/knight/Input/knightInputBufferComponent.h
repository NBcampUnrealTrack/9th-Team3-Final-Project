#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "knightInputBufferComponent.generated.h"

class UknightAbilitySystemComponent;

/**
 * Holds the most recent ability press that could not act yet, so input made slightly before a
 * combo or cancel window still counts. Latest press wins, as in Souls-likes.
 *
 * Per frame, after UknightAbilitySystemComponent::ProcessAbilityInput (see AknightPlayerController):
 *   - a fresh press whose ability just activated is consumed immediately;
 *   - otherwise the press is retried with TryActivateAbilityByInputTag until it activates or expires.
 * The ASC's activation groups decide when a retry may succeed (Window.Combo / Window.Cancel).
 *
 * An ability that chains combo steps internally, instead of re-activating, pulls its input with
 * ConsumeBufferedInput when its combo window opens.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightInputBufferComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightInputBufferComponent();

	/** Called on every ability input press. */
	void RecordInput(const FGameplayTag& InputTag);

	/** Called once per frame after the ASC processed input. */
	void ProcessBuffer(UknightAbilitySystemComponent* ASC);

	/** Removes and returns true if InputTag is buffered and not expired. */
	UFUNCTION(BlueprintCallable, Category="Knight|Input")
	bool ConsumeBufferedInput(FGameplayTag InputTag);

	UFUNCTION(BlueprintPure, Category="Knight|Input")
	bool HasBufferedInput(FGameplayTag InputTag) const;

	UFUNCTION(BlueprintCallable, Category="Knight|Input")
	void ClearBuffer();

	/** Currently buffered tag, or an empty tag. For the gameplay debugger. */
	FGameplayTag GetBufferedInputTag() const { return BufferedTag; }

protected:
	/**
	 * Seconds a press stays buffered.
	 * 0.35 → 0.5: 공격 판정 끝에서 가드·이동·점프까지 약 0.47초(양손)라, 판정 직후 미리 누른 입력이
	 * 기다리는 동안 사라져 다시 눌러야 했다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Input", meta=(ClampMin=0, Units="s"))
	float BufferDuration = 0.5f;

private:
	bool IsExpired() const;

	FGameplayTag BufferedTag;
	double BufferedTime = 0.0;
	/** Set on record; cleared the first time ProcessBuffer sees the press. */
	bool bFreshPress = false;
};
