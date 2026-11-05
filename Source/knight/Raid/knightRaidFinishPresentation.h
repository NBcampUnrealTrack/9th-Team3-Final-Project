#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Raid/knightRaidDefinition.h"
#include "Raid/knightRaidTypes.h"
#include "UObject/Object.h"
#include "knightRaidFinishPresentation.generated.h"

class ACameraActor;
class AknightMonsterCharacterBase;
class AknightRaidGameState;
class APlayerController;
class USoundMix;

/**
 * 마지막 대상 몬스터를 처치한 뒤의 연출. AknightRaidGameMode가 만들고 Start를 부른다. 설정은 FknightRaidFinishSettings.
 *
 * 1. Slow: 막타 히트스톱과 흰 섬광 뒤 전역 시간을 늦추고, 카메라 하나를 컷마다 옮겨 쓰러지는 몬스터를 여러 각도에서 보여 준다
 *    (Shots, 레터박스, 화면 효과, 효과음 늘어짐). 플레이어 입력을 막고 HUD는 이 단계를 보고 숨는다.
 * 2. Return: 시간이 원래 속도로 돌아오고, 플레이어 시점이 몬스터를 향한 채 블렌드로 돌아온다.
 * 3. Impact: 몸이 땅에 닿는 순간(ImpactTime, 공중에서 떨어지면 떨어진 뒤) 카메라 흔들림·이펙트·소리를 내고, 보스 BGM 엔딩을 시작하고,
 *    입력을 돌려준다.
 * 4. Banner: SettleTime 뒤 토벌 확인 배너(HUD)와 음성. 5. Done: 배너와 여운 뒤 결과 화면이 열리고 귀환 타이머가 돈다.
 *
 * 슬로모션 배율은 몸이 땅에 닿는 순간이 복귀가 끝난 뒤 원래 속도에서 오도록 정한다. 시간은 일시정지와 상관없는 코어 틱의 실제 시간으로
 * 세고, 게임이 일시정지되면 멈춘다. 단계는 AknightRaidGameState::GetFinishStage로 알린다.
 * 확인: 출력 로그 Logknight의 "Finish presentation" 줄.
 */
UCLASS()
class KNIGHT_API UknightRaidFinishPresentation : public UObject
{
	GENERATED_BODY()

public:
	/** 연출을 시작한다. 시작하지 못하면(플레이어 없음 등) false이고 단계는 None으로 남는다. */
	bool Start(AknightMonsterCharacterBase& Monster, APlayerController& PlayerController, AknightRaidGameState& RaidState,
		const FknightRaidFinishSettings& InSettings);

	/** 바로 끝낸다(맵을 떠날 때). 시간·시점·입력·소리를 돌려 놓는다. 단계는 바꾸지 않는다. */
	void Stop();

	/** 몸이 땅에 닿은 순간과 연출이 끝난 순간(결과 화면을 열 때). */
	TFunction<void()> OnImpact;
	TFunction<void()> OnDone;

private:
	bool Tick(float DeltaTime);
	void SetStage(EknightRaidFinishStage Stage);
	void BeginSlow();
	void BeginReturn();
	void BeginImpact();
	void UpdateShot();
	/** 지금 몬스터 몸 경계 중심(바라보는 점). */
	FVector GetBodyCenter() const;
	bool HasLanded() const;
	void PushSlowSound();
	void PopSlowSound();
	void RestoreInput();

	FknightRaidFinishSettings Settings;
	TWeakObjectPtr<AknightMonsterCharacterBase> Monster;
	TWeakObjectPtr<APlayerController> PlayerController;
	TWeakObjectPtr<AknightRaidGameState> RaidState;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> SlowSoundMix;

	FTSTicker::FDelegateHandle TickHandle;
	EknightRaidFinishStage Stage = EknightRaidFinishStage::None;

	/** 지금 단계에 들어온 뒤의 실제 시간. */
	float StageTime = 0.f;
	bool bHitStop = false;
	bool bInputIgnored = false;
	bool bSoundPushed = false;

	/** 몬스터가 죽은 게임 시각, 땅에 닿을 것으로 본 게임 시간(사망 뒤), 슬로모션 배율과 길이(실제 초). */
	double DeathGameTime = 0.0;
	float ExpectedImpactTime = 0.f;
	float SlowDilation = 0.25f;
	float SlowDuration = 4.5f;

	/** 시작할 때의 몸 크기(카메라 거리 기준)와 몬스터 정면 방향, 바라보는 점(부드럽게 따라감). */
	float BodyRadius = 500.f;
	float BodyHalfHeight = 300.f;
	float MonsterYaw = 0.f;
	FVector Focus = FVector::ZeroVector;
	int32 ShotIndex = INDEX_NONE;
};
