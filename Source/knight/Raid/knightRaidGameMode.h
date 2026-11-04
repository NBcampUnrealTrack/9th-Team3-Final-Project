#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "knightGameMode.h"
#include "Raid/knightRaidTypes.h"
#include "knightRaidGameMode.generated.h"

class AknightMonsterCharacterBase;
class AknightRaidGameState;
class UknightRaidDefinition;
class USoundBase;
struct FStreamableHandle;

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

	/** Accepts a confirmed broken part on this run's target once. Head/tail rewards read the resulting tags. */
	UFUNCTION(BlueprintCallable, Category="Knight|Raid")
	bool RecordBrokenPart(FGuid RunId, FName BossId, AknightMonsterCharacterBase* Monster, FGameplayTag PartTag);

	const UknightRaidDefinition* GetRaidDefinition() const { return RaidDefinition; }

	/** Monsters that must be defeated. Filled in StartPlay, after actors begin play. */
	const TArray<TWeakObjectPtr<AknightMonsterCharacterBase>>& GetTargetMonsters() const { return TargetMonsters; }

protected:
	/** Raid rules for direct play in the editor, when no stage was launched from the menu. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Raid")
	TObjectPtr<const UknightRaidDefinition> DefaultRaidDefinition;

	/**
	 * 레이드에 들어갈 때 월드가 안정될 때까지 로딩 화면(레이드 단계 None)을 유지하고 게임을 일시정지해 둔다.
	 * 전에는 맵이 올라온 프레임에 바로 출격해서, 텍스처·셰이더가 덜 준비된 채 로딩 화면이 걷히고 몬스터가 먼저 움직였다
	 * ('로딩이 끝나기 전에 시작'). 끝 = MinLoadingHoldTime이 지나고, 스트리밍을 기다리는 텍스처가 MaxStreamingResources 이하이고,
	 * (에디터만) 셰이더 컴파일과 추적 가능한 PSO 준비가 끝나고, 진행 중인 비동기 로드(미리 불러오는 큐·화면 효과·전투 음악)가 없을 때. 시간 초과 시 미완성 레이드를 시작하지 않고 로비로 돌아간다.
	 * Path Tracing RTPSO 같은 독립적인 배경 컴파일은 이 카운터로 추적되지 않는다.
	 * 확인: 출력 로그 Logknight의 "raid loading hold" 줄.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Raid|Loading")
	bool bHoldLoadingUntilReady = true;

	/** 로딩 화면을 최소한 이만큼(실제 초) 더 유지한다. 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Raid|Loading", meta=(ClampMin=0, Units="s", EditCondition="bHoldLoadingUntilReady"))
	float MinLoadingHoldTime = 0.5f;

	/** 준비가 끝나지 않으면 이 시간(실제 초)에 레이드를 중단한다. 무한 로딩 방지용. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Raid|Loading", meta=(ClampMin=0, Units="s", EditCondition="bHoldLoadingUntilReady"))
	float MaxLoadingHoldTime = 90.f;

	/** 스트리밍을 기다리는 텍스처 등이 이 개수 이하면 준비된 것으로 본다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Raid|Loading", meta=(ClampMin=0, EditCondition="bHoldLoadingUntilReady"))
	int32 MaxStreamingResources = 0;

private:
	/** 로딩 유지 중 0.1초마다(일시정지와 상관없는 코어 틱): 준비됐으면 일시정지를 풀고 출격한다. 계속 기다리면 true. */
	bool TickLoadingHold(float DeltaTime);

	/** Deploy 단계로 들어가고 출격 시간 뒤 탐색을 시작한다. */
	void BeginDeploy();

	FTSTicker::FDelegateHandle LoadingHoldTicker;
	double LoadingHoldStartTime = 0.0;
	bool bPausedForLoading = false;

	/** Keeps CombatMusic and CombatMusicLowHealth loaded so the first fight does not load them on the game thread. */
	TSharedPtr<FStreamableHandle> CombatMusicHandle;

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
	void HandlePartBroken(FGameplayTag PartTag, AActor* BreakInstigator, AActor* BrokenMonster);

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
