#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "Monster/knightMonsterReadout.h"
#include "knightVM_BossStatus.generated.h"

class AknightMonsterCharacterBase;

DECLARE_MULTICAST_DELEGATE_OneParam(FknightOnMonsterReadout, EknightMonsterReadout);

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
	AknightMonsterCharacterBase* GetMonster() const { return BoundMonster.Get(); }

	/**
	 * 상태 태그가 붙는 순간(탈진, 다운·쓰러짐, 경직, 분노, 2페이즈) 한 번씩 알린다. HUD 센서 판독이 듣는다.
	 * 체력 기준(VitalsLow/Critical)은 여기서 보내지 않는다. 기준값을 가진 HUD가 HealthPercent로 직접 잰다.
	 */
	FknightOnMonsterReadout OnReadout;

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
	FDelegateHandle KnockdownHandle;
	FDelegateHandle CollapsedHandle;
	FDelegateHandle StaggerHandle;
	FDelegateHandle Phase2Handle;
};
