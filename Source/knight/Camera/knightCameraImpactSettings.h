#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "knightCameraImpactSettings.generated.h"

class UCameraShakeBase;

/**
 * 전투 순간 하나에 붙는 카메라 반응. 셰이크와 별개로 시야각, 카메라 위치, 화면 가장자리 색을 잠깐 바꿨다가 돌려놓는다.
 * 세기는 순간마다 뜻이 다르다(UknightCameraImpactSettings의 각 항목 설명 참고).
 */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightCameraImpactReaction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact")
	bool bEnabled = false;

	/** 세기가 이 값 이상일 때만 반응한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact|Strength", meta=(ClampMin=0))
	float MinStrength = 0.f;

	/** 이 세기에서 반응이 100%. MinStrength에서는 MinScale이고 그 사이는 비례. FullStrength가 MinStrength 이하면 항상 100%. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact|Strength", meta=(ClampMin=0))
	float FullStrength = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact|Strength", meta=(ClampMin=0, ClampMax=1))
	float MinScale = 0.5f;

	/** 시야각을 이만큼 좁힌다(도). 음수면 넓힌다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact|View", meta=(Units="deg"))
	float FOVPunch = 0.f;

	/** 카메라를 캐릭터 쪽으로 (카메라~캐릭터 거리)의 이 비율만큼 당긴다. 강한 공격에서 등 뒤로 확 다가오는 연출. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact|View", meta=(ClampMin=0, ClampMax=0.9))
	float PullIn = 0.f;

	/** 당길 때 향하는 점: 보고 있는 캐릭터 위치에서 이만큼 위. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact|View", meta=(Units="cm"))
	float PullPivotHeight = 60.f;

	/** 반응하는 동안 카메라를 화면 기준으로 옮기는 양(X 오른쪽, Y 위). 당기기와 같이 쓰면 어깨 너머 구도가 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact|View")
	FVector2D ShoulderOffset = FVector2D::ZeroVector;

	/** 타격 방향으로 카메라를 미는 양. 때릴 때는 타깃 쪽으로, 맞거나 막을 때는 때린 쪽 반대로 밀린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact|View", meta=(Units="cm"))
	float Push = 0.f;

	/** 화면 가장자리 색 갈라짐(포스트 프로세스 SceneFringeIntensity). 0~5. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact|Screen", meta=(ClampMin=0, ClampMax=5))
	float ChromaticAberration = 0.f;

	/** 0에서 최대까지 가는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact|Timing", meta=(ClampMin=0, Units="s"))
	float AttackTime = 0.05f;

	/** 최대로 머무는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact|Timing", meta=(ClampMin=0, Units="s"))
	float HoldTime = 0.08f;

	/** 원래대로 돌아오는 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact|Timing", meta=(ClampMin=0.01, Units="s"))
	float ReleaseTime = 0.3f;

	/** 같은 반응을 다시 하기까지 최소 간격. 한 번 휘둘러 여러 곳을 맞혀도 한 번만 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Impact|Timing", meta=(ClampMin=0, Units="s"))
	float Cooldown = 0.1f;
};

/**
 * 전투 순간별 카메라 반응 표. AknightPlayerCameraManager가 보고 있는 플레이어의 전투 이벤트를 듣고 해당 항목을 재생한다.
 * 항목 이름(프로퍼티 이름)이 순간 이름이고, AknightPlayerCameraManager::PreviewImpactReaction에 그대로 넘긴다.
 */
UCLASS(BlueprintType)
class KNIGHT_API UknightCameraImpactSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	UknightCameraImpactSettings();

	/** 플레이어가 때렸다. 세기 = 그로기 피해(스탠스·차지 배율 포함). PlayerHeavyHit 조건을 넘으면 그쪽만 재생한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player Attack")
	FknightCameraImpactReaction PlayerHit;

	/** 플레이어의 강한 타격. 세기 = 그로기 피해. MinStrength 이상이면 PlayerHit 대신 재생한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player Attack")
	FknightCameraImpactReaction PlayerHeavyHit;

	/** 플레이어가 맞았다(Event.Damaged). 세기 = 피해량. 가드·패링 직후의 피해는 무시한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player Defense")
	FknightCameraImpactReaction Damaged;

	/** 플레이어가 그로기로 넘어졌다(Event.PoiseBroken). 세기 = 그로기 피해. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player Defense")
	FknightCameraImpactReaction PoiseBroken;

	/** 플레이어가 가드로 막았다(Event.Guarded). 세기 = 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player Defense")
	FknightCameraImpactReaction Guarded;

	/** 플레이어가 패링에 성공했다(Event.ParrySucceeded). 세기 = 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player Defense")
	FknightCameraImpactReaction Parried;

	/** 플레이어의 가드가 깨졌다(Event.GuardBroken). 세기 = 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player Defense")
	FknightCameraImpactReaction GuardBroken;

	/** 몬스터의 큰 공격이 땅을 쳤다(MonsterImpactShake가 시작됨). 세기 = 셰이크 세기(거리 감쇠 포함). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Monster")
	FknightCameraImpactReaction MonsterImpact;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Monster")
	TSubclassOf<UCameraShakeBase> MonsterImpactShake;

	/** 가드·패링·가드 깨짐 뒤 이 시간 안에 오는 Event.Damaged(막고 남은 피해)는 Damaged 반응을 하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player Defense", meta=(ClampMin=0, Units="s"))
	float DefenseDamageSuppressTime = 0.2f;
};
