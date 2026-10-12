#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "Monster/knightMonsterPartComponent.h"
#include "knightVM_PlayerStatus.generated.h"

class UAbilitySystemComponent;
class UknightTargetLockComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FknightLockedPartConditionChanged, EknightPartCondition /*NewCondition*/);

/**
 * The Knight's status for the HUD: health, boost gauge, overheat and lock-on (target name, locked part and its condition,
 * the target's stagger gauge).
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
	bool HasLockedPart() const { return bHasLockedPart; }
	FText GetLockedPartName() const { return LockedPartName; }
	EknightPartCondition GetLockedPartCondition() const { return LockedPartCondition; }
	bool LockedTargetHasStagger() const { return bLockedTargetHasStagger; }
	float GetLockedTargetStagger() const { return LockedTargetStagger; }

	/** The locked part changed condition while it stays locked (not on lock or switch). For one-shot feedback like a flash. */
	FknightLockedPartConditionChanged OnLockedPartConditionChanged;

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void HandleOverheatedChanged(const FGameplayTag Tag, int32 NewCount);

	UFUNCTION()
	void HandleLockOnChanged(AActor* NewTarget, FGameplayTag PartTag);

	UFUNCTION()
	void HandlePartConditionChanged(FGameplayTag PartTag, EknightPartCondition NewCondition);

	void BindParts(UknightMonsterPartComponent* InParts);
	void BindTargetPoise(UAbilitySystemComponent* InTargetASC);
	void HandleTargetPoiseChanged(const FOnAttributeChangeData& Data);
	void RefreshTargetStagger();

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

	/** The lock is on a monster part (false for the body point or a non-monster target). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter="HasLockedPart", Category="Knight|Status", meta=(AllowPrivateAccess=true))
	bool bHasLockedPart = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	FText LockedPartName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	EknightPartCondition LockedPartCondition = EknightPartCondition::Unbreakable;

	/** The locked target has poise (a stagger gauge to show). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter="LockedTargetHasStagger", Category="Knight|Status", meta=(AllowPrivateAccess=true))
	bool bLockedTargetHasStagger = false;

	/** Poise taken from the locked target, 0..1; it staggers at 1. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	float LockedTargetStagger = 0.f;

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<UAbilitySystemComponent> BoundTargetASC;
	FDelegateHandle TargetPoiseHandle;
	FDelegateHandle TargetMaxPoiseHandle;
	TWeakObjectPtr<UknightTargetLockComponent> BoundTargetLock;
	TWeakObjectPtr<UknightMonsterPartComponent> BoundParts;
	FGameplayTag LockedPartTag;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeHandles;
	FDelegateHandle OverheatedHandle;
};
