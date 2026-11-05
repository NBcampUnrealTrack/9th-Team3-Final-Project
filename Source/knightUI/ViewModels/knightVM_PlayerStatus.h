#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "Character/knightPartHealthComponent.h"
#include "Monster/knightMonsterPartComponent.h"
#include "knightVM_PlayerStatus.generated.h"

class UAbilitySystemComponent;
class UknightTargetLockComponent;
class UknightGroggyFeedbackComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FknightLockedPartConditionChanged, EknightPartCondition /*NewCondition*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FknightPlayerPartBroken, EknightBodyPart /*Part*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FknightPlayerGroggyPresentation, bool /*bImpact*/);
DECLARE_MULTICAST_DELEGATE(FknightPlayerKnockdown);

/**
 * The Knight's status for the HUD: health, boost gauge, overheat, the Knight's own parts, stagger and repair kits, and lock-on
 * (target name, locked part and its condition).
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
	/** The part carries its State.PartBroken.* tag (same rule as gameplay: broken at 0, repaired at 1 or more). */
	bool IsPartBroken(EknightBodyPart Part) const { return (BrokenParts & (1 << static_cast<int32>(Part))) != 0; }
	int32 GetBrokenParts() const { return BrokenParts; }
	/** Health of one of the Knight's parts, 0..1 (1 when the part has no maximum). */
	float GetPartHealthPercent(EknightBodyPart Part) const;
	float GetHeadHealthPercent() const { return HeadHealthPercent; }
	float GetTorsoHealthPercent() const { return TorsoHealthPercent; }
	float GetArmsHealthPercent() const { return ArmsHealthPercent; }
	float GetLegsHealthPercent() const { return LegsHealthPercent; }
	float GetStagger() const { return Stagger; }
	bool IsGroggy() const { return bGroggy; }
	int32 GetRepairKitCount() const { return RepairKitCount; }
	int32 GetMaxRepairKitCount() const { return MaxRepairKitCount; }
	float GetRepairKitGaugePercent() const { return RepairKitGaugePercent; }

	/** One of the Knight's parts just got its broken tag (not on rebind to an already broken part). */
	FknightPlayerPartBroken OnPlayerPartBroken;

	/** The Knight's groggy presentation on a target started its prediction slow (false) or landed (true). */
	FknightPlayerGroggyPresentation OnGroggyPresentation;

	/** Still in the groggy presentation (prediction slow, freeze or slow release). */
	bool IsGroggyPresentationActive() const;

	/** The Knight was knocked down (State.HitReact.Knockdown added). */
	FknightPlayerKnockdown OnPlayerKnockdown;

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

	void HandleGroggyChanged(const FGameplayTag Tag, int32 NewCount);
	void HandleKnockdownChanged(const FGameplayTag Tag, int32 NewCount);
	void HandlePartBrokenChanged(const FGameplayTag Tag, int32 NewCount, EknightBodyPart Part);
	void RefreshAll();
	void RefreshPercents();
	void RefreshKnightParts();

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

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	float HeadHealthPercent = 1.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	float TorsoHealthPercent = 1.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	float ArmsHealthPercent = 1.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	float LegsHealthPercent = 1.f;

	/** Bit per EknightBodyPart, set while the part is broken. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	int32 BrokenParts = 0;

	/** Poise taken from the Knight, 0..1; the Knight goes groggy at 1. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	float Stagger = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter="IsGroggy", Category="Knight|Status", meta=(AllowPrivateAccess=true))
	bool bGroggy = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	int32 RepairKitCount = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	int32 MaxRepairKitCount = 0;

	/** Progress to the next repair kit (perfect guards), 0..1. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Status", meta=(AllowPrivateAccess=true))
	float RepairKitGaugePercent = 0.f;

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<UknightTargetLockComponent> BoundTargetLock;
	TWeakObjectPtr<UknightMonsterPartComponent> BoundParts;
	/** 락온 셸을 켜 둔 몬스터. 락온이 다른 몬스터로 옮기거나 풀리면 끈다. */
	TWeakObjectPtr<class UknightMonsterPartShellComponent> ShellTarget;
	FGameplayTag LockedPartTag;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeHandles;
	FDelegateHandle OverheatedHandle;
	FDelegateHandle GroggyHandle;
	FDelegateHandle KnockdownHandle;
	TWeakObjectPtr<UknightGroggyFeedbackComponent> BoundGroggyFeedback;
	FDelegateHandle GroggyPresentationHandle;
	FDelegateHandle PartBrokenHandles[static_cast<int32>(EknightBodyPart::Count)];
};
