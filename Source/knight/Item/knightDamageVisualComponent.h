#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "knightDamageVisualComponent.generated.h"

class UknightPartItemDefinition;

/**
 * Shows wear on the mech as it loses health: writes a 0..1 damage value to the custom primitive data of the
 * mech mesh and every equipped part mesh under DamageParameterName. Materials blend in scorches, cracks and
 * exposed frame from it. Cosmetic only; parts never break off.
 *
 * DamageSteps quantizes the value so materials can switch between a few authored states instead of fading.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightDamageVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightDamageVisualComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Damage Visual")
	FName DamageParameterName = TEXT("MechDamage");

	/** Number of visual states above pristine. Zero writes the continuous value. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Damage Visual", meta=(ClampMin=0, ClampMax=10))
	int32 DamageSteps = 3;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	UFUNCTION()
	void HandleEquipmentChanged(FGameplayTag SlotTag, UknightPartItemDefinition* Part);

	void Refresh();
	float ComputeDamage() const;

	FDelegateHandle HealthHandle;
	FDelegateHandle MaxHealthHandle;
	float AppliedDamage = -1.f;
};
