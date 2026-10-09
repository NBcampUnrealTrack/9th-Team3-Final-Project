#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "knightVM_BossStatus.generated.h"

class AknightMonsterCharacterBase;

/**
 * Status of the raid's target monster. Registered as "BossStatus". Health is exposed for designers who want
 * a bar; a Monster Hunter style HUD can show only name, rage and exhaustion instead.
 */
UCLASS(BlueprintType, DisplayName="Knight Boss Status ViewModel")
class KNIGHTUI_API UknightVM_BossStatus : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Bind(AknightMonsterCharacterBase* InMonster);
	void Unbind();

	bool HasMonster() const { return bHasMonster; }
	FText GetDisplayName() const { return DisplayName; }
	float GetHealthPercent() const { return HealthPercent; }
	bool IsEnraged() const { return bEnraged; }
	bool IsExhausted() const { return bExhausted; }
	bool IsDefeated() const { return bDefeated; }
	int32 GetBrokenPartCount() const { return BrokenPartCount; }

private:
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleStateTagChanged(const FGameplayTag Tag, int32 NewCount);

	UFUNCTION()
	void HandlePartBroken(FGameplayTag PartTag, AActor* BreakInstigator);

	UFUNCTION()
	void HandleDefeated(AknightMonsterCharacterBase* Monster, AActor* Killer);

	void RefreshAll();

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter="HasMonster", Category="Knight|Boss", meta=(AllowPrivateAccess=true))
	bool bHasMonster = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Boss", meta=(AllowPrivateAccess=true))
	FText DisplayName;

	/** 0..1 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Boss", meta=(AllowPrivateAccess=true))
	float HealthPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter="IsEnraged", Category="Knight|Boss", meta=(AllowPrivateAccess=true))
	bool bEnraged = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter="IsExhausted", Category="Knight|Boss", meta=(AllowPrivateAccess=true))
	bool bExhausted = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter="IsDefeated", Category="Knight|Boss", meta=(AllowPrivateAccess=true))
	bool bDefeated = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Boss", meta=(AllowPrivateAccess=true))
	int32 BrokenPartCount = 0;

	TWeakObjectPtr<AknightMonsterCharacterBase> BoundMonster;
	FDelegateHandle HealthHandle;
	FDelegateHandle MaxHealthHandle;
	FDelegateHandle EnragedHandle;
	FDelegateHandle ExhaustedHandle;
};
