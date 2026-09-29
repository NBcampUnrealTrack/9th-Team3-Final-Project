#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Raid/knightRaidTypes.h"
#include "knightVM_Raid.generated.h"

class AknightRaidGameState;

/** Raid phase and timer for the HUD and the result screen. Registered as "Raid". */
UCLASS(BlueprintType, DisplayName="Knight Raid ViewModel")
class KNIGHTUI_API UknightVM_Raid : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Bind(AknightRaidGameState* InRaidState);
	void Unbind();

	/** Called by the UI subsystem at a low rate; the timer is the only value that changes without an event. */
	void RefreshTimer();

	EknightRaidPhase GetPhase() const { return Phase; }
	bool HasTimeLimit() const { return bHasTimeLimit; }
	float GetRemainingTime() const { return RemainingTime; }
	FText GetRemainingTimeText() const { return RemainingTimeText; }

private:
	UFUNCTION()
	void HandlePhaseChanged(EknightRaidPhase OldPhase, EknightRaidPhase NewPhase);

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Raid", meta=(AllowPrivateAccess=true))
	EknightRaidPhase Phase = EknightRaidPhase::None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter="HasTimeLimit", Category="Knight|Raid", meta=(AllowPrivateAccess=true))
	bool bHasTimeLimit = false;

	/** Seconds left, or -1 without a time limit. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Raid", meta=(AllowPrivateAccess=true))
	float RemainingTime = -1.f;

	/** "mm:ss". Empty without a time limit. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category="Knight|Raid", meta=(AllowPrivateAccess=true))
	FText RemainingTimeText;

	TWeakObjectPtr<AknightRaidGameState> BoundRaidState;
};
