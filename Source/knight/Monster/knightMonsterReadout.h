#pragma once

#include "CoreMinimal.h"
#include "knightMonsterReadout.generated.h"

/**
 * 몬스터 상태가 바뀌는 순간 HUD 센서 판독(UknightHUDMonsterReadout)이 짧게 띄우는 문구의 종류.
 * 보스 체력·상태 바를 두지 않는 대신 "지금 때릴 때다 / 위험해졌다 / 약해졌다"를 알려 준다.
 */
UENUM(BlueprintType)
enum class EknightMonsterReadout : uint8
{
	/** 탈진(State.Monster.Exhausted). 반격 기회. */
	Exhausted,
	/** 다운·쓰러짐(State.HitReact.Knockdown, State.Monster.Collapsed). 반격 기회, 약점 표시. */
	Stunned,
	/** 패링·부위 파괴 경직(State.HitReact.Stagger). 짧은 반격 기회. */
	Staggered,
	/** 분노(State.Monster.Enraged). 위험 상승. */
	Enraged,
	/** 2페이즈 진입(State.Monster.Phase2). 위험 상승. */
	Phase2,
	/** 체력이 첫 기준 아래로. */
	VitalsLow,
	/** 체력이 둘째 기준 아래로. */
	VitalsCritical,
};

/** 판독 문구. 위 줄은 센서 표기(영문), 아래 줄은 풀이. 비워 둔 줄은 HUD 기본값을 쓴다. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightMonsterReadoutText
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readout")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readout")
	FText Detail;

	/** HUD 메시지 로그의 [AI] 대사. 비우면 메시지 데이터(DA_HUDMessages)의 기본 대사를 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Readout")
	FText AssistantLine;
};
