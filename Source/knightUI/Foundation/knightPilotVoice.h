#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Raid/knightRaidTypes.h"
#include "UObject/Object.h"
#include "knightPilotVoice.generated.h"

class AknightRaidGameState;
class UAbilitySystemComponent;
class USoundBase;
struct FStreamableHandle;

/**
 * The pilot's voice lines (UknightUISettings, "Pilot Voice"), played through the audio subsystem's voice queue so they
 * never overlap:
 *   SystemActivated - once per raid, when the Knight is spawned and the loading screen is gone.
 *   EnemyDetected   - the first time the raid enters combat.
 *   CombatMode      - when the Knight draws the sword (CombatStateTag added), at most once per cooldown.
 *   HealthWarning   - once, when health first drops to the warning ratio or below. Jumps the queue.
 *   RaidSuccess     - 처치 연출의 토벌 확인 배너가 뜰 때(AknightRaidGameState 처치 연출 단계 Banner). 줄을 앞지른다.
 * Owned by UknightUISubsystem, which binds it with the possessed pawn and the raid like the HUD view models.
 */
UCLASS()
class KNIGHTUI_API UknightPilotVoice : public UObject
{
	GENERATED_BODY()

public:
	/** Loads every voice line in the background and keeps it, so the first play does not stall the game thread. */
	void Preload();

	void BindPawn(UAbilitySystemComponent* InASC);
	void BindRaid(AknightRaidGameState* InRaidState);
	void Unbind();

	/** Called at the view model refresh rate; plays SystemActivated once the raid is ready. */
	void Refresh();

private:
	void UnbindPawn();
	void UnbindRaid();
	void HandleCombatStateChanged(const FGameplayTag Tag, int32 NewCount);
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	UFUNCTION()
	void HandleRaidPhaseChanged(EknightRaidPhase OldPhase, EknightRaidPhase NewPhase);
	void HandleFinishStageChanged(EknightRaidFinishStage Stage);

	void Play(const TSoftObjectPtr<USoundBase>& Voice, bool bUrgent = false) const;

	TSharedPtr<FStreamableHandle> PreloadHandle;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<AknightRaidGameState> BoundRaidState;
	FGameplayTag BoundCombatTag;
	FDelegateHandle CombatStateHandle;
	FDelegateHandle HealthHandle;
	FDelegateHandle FinishStageHandle;

	bool bSystemActivatedPlayed = false;
	bool bEnemyDetectedPlayed = false;
	bool bHealthWarningPlayed = false;
	double LastCombatModeTime = -TNumericLimits<double>::Max();
};
