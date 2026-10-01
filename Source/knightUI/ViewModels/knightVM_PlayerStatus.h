#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "knightVM_PlayerStatus.generated.h"

class UAbilitySystemComponent;
class UknightTargetLockComponent;

/**
 * The Knight's status for the HUD: health, boost gauge, overheat and lock-on.
 * Registered in the global view model collection as "PlayerStatus" by UknightUISubsystem and rebound when the
 * possessed pawn changes. Values only update on change; widgets bind to them with MVVM bindings.
 */
UCLASS(BlueprintType, DisplayName="Knight Player Status ViewModel")
class KNIGHTUI_API UknightVM_PlayerStatus : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Bind(UAbilitySystemComponent* InASC, UknightTargetLockComponent* InTargetLock);
	void Unbind();

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	float GetHealthPercent() const { return HealthPercent; }
	float GetBoostGauge() const { return BoostGauge; }
	float GetMaxBoostGauge() const { return MaxBoostGauge; }
	float GetBoostPercent() const { return BoostPercent; }
	bool IsOverheated() const { return bOverheated; }
	bool IsLockedOn() const { return bLockedOn; }
	FText GetLockedTargetName() const { return LockedTargetName; }

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void HandleOverheatedChanged(const FGameplayTag Tag, int32 NewCount);

	UFUNCTION()
	void HandleLockOnChanged(AActor* NewTarget, FGameplayTag PartTag);

	void RefreshAll();
	void RefreshPercents();

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	float MaxHealth = 0.f;

	/** 0..1 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	float HealthPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	float BoostGauge = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	float MaxBoostGauge = 0.f;

	/** 0..1 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	float BoostPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter="IsOverheated", Category="Knight|Status", meta=(AllowPrivateAccess=true))
	bool bOverheated = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter="IsLockedOn", Category="Knight|Status", meta=(AllowPrivateAccess=true))
	bool bLockedOn = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	FText LockedTargetName;

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<UknightTargetLockComponent> BoundTargetLock;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeHandles;
	FDelegateHandle OverheatedHandle;
};
