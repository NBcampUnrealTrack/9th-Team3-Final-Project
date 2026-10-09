#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "knightGameMode.h"
#include "Raid/knightRaidTypes.h"
#include "knightRaidGameMode.generated.h"

class AknightMonsterCharacterBase;
class AknightRaidGameState;
class UknightRaidDefinition;
class USoundBase;

/**
 * GameMode for raid maps: Deploy -> Explore <-> Combat -> Success / Failed -> Returning.
 *
 * The raid rules come from the stage the main menu launched (FKnightStageDefinition::RaidDefinition), or from
 * DefaultRaidDefinition when a raid map is played directly in the editor. Target monsters are the placed
 * monsters whose definition matches the raid's TargetMonster (all monsters if it is empty).
 * On success or failure the result goes to UknightRaidSubsystem and the game returns to the main menu.
 */
UCLASS(abstract)
class KNIGHT_API AknightRaidGameMode : public AknightGameMode
{
	GENERATED_BODY()

public:
	AknightRaidGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void HandlePlayerDeath(AController* PlayerController) override;

	/** Explicit hooks for objectives and the player system. No monster dependency is required. */
	UFUNCTION(BlueprintCallable, Category="Knight|Raid")
	bool RequestStartRaid();

	UFUNCTION(BlueprintCallable, Category="Knight|Raid")
	void NotifyObjectiveCompleted();

	UFUNCTION(BlueprintCallable, Category="Knight|Raid")
	void NotifyPlayerDefeated();

	UFUNCTION(BlueprintCallable, Category="Knight|Raid")
	void RequestReturnToLobby();

	/** Test hook; expose only in development builds and test content. */
	UFUNCTION(BlueprintCallable, Category="Knight|Raid|Development", meta=(DevelopmentOnly))
	void DebugFinishRaid(bool bSuccess);

	/** Gives up the raid (pause menu "return to base"). */
	UFUNCTION(BlueprintCallable, Category="Knight|Raid")
	void AbandonRaid();

	/** A monster spawned after StartPlay (a second phase, a reinforcement) that also counts as a target. */
	UFUNCTION(BlueprintCallable, Category="Knight|Raid")
	void RegisterTargetMonster(AknightMonsterCharacterBase* Monster);

	const UknightRaidDefinition* GetRaidDefinition() const { return RaidDefinition; }

	/** Monsters that must be defeated. Filled in StartPlay, after actors begin play. */
	const TArray<TWeakObjectPtr<AknightMonsterCharacterBase>>& GetTargetMonsters() const { return TargetMonsters; }

protected:
	/** Raid rules for direct play in the editor, when no stage was launched from the menu. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Raid")
	TObjectPtr<const UknightRaidDefinition> DefaultRaidDefinition;

private:
	void ResolveRaidDefinition();
	void BindPlacedTargetMonsters();
	bool IsTargetMonster(const AknightMonsterCharacterBase* Monster) const;
	void BeginExploration();
	void HandleTimeUp();
	void FinishRaid(bool bSuccess, EknightRaidFailReason FailReason);
	void SetPhase(EknightRaidPhase NewPhase);
	AknightRaidGameState* GetRaidGameState() const;

	UFUNCTION()
	void HandleMonsterDefeated(AknightMonsterCharacterBase* Monster, AActor* Killer);

	UFUNCTION()
	void HandleMonsterTargetChanged(AActor* OldTarget, AActor* NewTarget);

	UFUNCTION()
	void HandlePartBroken(FGameplayTag PartTag, AActor* BreakInstigator);

	void HandleMonsterEnragedChanged(const FGameplayTag Tag, int32 NewCount);

	/** Picks the music state from the raid phase and the target monsters' rage, and the combat track (see CombatMusic). */
	void UpdateMusic();
	void UpdateCombatMusic(EknightRaidPhase Phase);
	void HandleMonsterHealthChanged(const FOnAttributeChangeData& Data);

	virtual void ReturnToMainMenu() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY()
	TObjectPtr<const UknightRaidDefinition> RaidDefinition;

	UPROPERTY()
	TArray<TWeakObjectPtr<AknightMonsterCharacterBase>> TargetMonsters;

	TSet<TWeakObjectPtr<AActor>> MonstersFightingPlayer;

	/** Combat track playing now; null before the first fight and after the raid ends. */
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> PlayingCombatTrack;
	bool bCombatMusicLowHealth = false;
	bool bCombatMusicEnded = false;
	FGameplayTagContainer BrokenParts;
	FTimerHandle PhaseTimer;
	FTimerHandle TimeLimitTimer;
};
