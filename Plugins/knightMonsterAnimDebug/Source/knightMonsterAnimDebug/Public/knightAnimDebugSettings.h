#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "knightAnimDebugSettings.generated.h"

class APawn;
class UAnimNotifyState;
class UGameplayAbility;

/** 어빌리티의 float 프로퍼티 하나를 타임라인 구간 하나로 그린다. 어빌리티에 없는 프로퍼티는 건너뛴다. */
USTRUCT()
struct FknightAnimDebugPhaseProperty
{
	GENERATED_BODY()

	/** 어빌리티 클래스의 float 프로퍼티 이름(초 단위). */
	UPROPERTY(EditAnywhere, Category="Timeline")
	FName PropertyName;

	UPROPERTY(EditAnywhere, Category="Timeline")
	FString Label;

	UPROPERTY(EditAnywhere, Category="Timeline")
	FColor Color = FColor::White;

	/** 이 구간이 끝나는 순간에 판정이 일어난다(범위 공격의 예고). 타임라인에 판정 표시를 그린다. */
	UPROPERTY(EditAnywhere, Category="Timeline")
	bool bHitAtEnd = false;

	/** 이 구간 동안 판정이 유지된다(범위 공격의 지속 판정). 판정 표시 구간 바로 뒤에 둔다. */
	UPROPERTY(EditAnywhere, Category="Timeline")
	bool bHitWindow = false;
};

/** 머리 위와 패널에 표시할 속성. MaxName이 있으면 "값/최댓값"으로 표시한다. */
USTRUCT()
struct FknightAnimDebugAttribute
{
	GENERATED_BODY()

	/** 속성 세트의 FGameplayAttributeData 프로퍼티 이름. */
	UPROPERTY(EditAnywhere, Category="Attributes")
	FName Name;

	UPROPERTY(EditAnywhere, Category="Attributes")
	FName MaxName;
};

/**
 * 몬스터 애니메이션 디버거 설정 (프로젝트 설정 > Knight > Monster Anim Debug, DefaultGame.ini에 저장).
 *
 * 플러그인은 게임 모듈을 모르므로 무엇을 추적하고 어떤 프로퍼티를 읽을지는 모두 여기서 이름으로 정한다.
 * 클래스나 프로퍼티 이름이 바뀌면 이 설정만 고치면 된다.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Monster Anim Debug"))
class KNIGHTMONSTERANIMDEBUG_API UknightAnimDebugSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UknightAnimDebugSettings();

	//~ UDeveloperSettings
	virtual FName GetCategoryName() const override { return TEXT("Knight"); }

	/** 이 클래스의 폰을 추적한다. 비어 있으면 AI가 조종하고 어빌리티 시스템이 있는 폰을 모두 추적한다. */
	UPROPERTY(Config, EditAnywhere, Category="Tracking")
	TSoftClassPtr<APawn> TrackedPawnClass;

	/** 이 클래스(또는 하위 클래스)의 어빌리티만 기록하고 강제 실행 목록에 올린다. 비어 있으면 모든 어빌리티. */
	UPROPERTY(Config, EditAnywhere, Category="Tracking")
	TArray<TSoftClassPtr<UGameplayAbility>> PlayableAbilityClasses;

	/** 이 접두사로 시작하는 태그의 추가·제거를 기록하고 표시한다. */
	UPROPERTY(Config, EditAnywhere, Category="Tracking")
	TArray<FString> TagPrefixes;

	/** 위에서부터 순서대로 이어 붙여 어빌리티 타임라인을 만든다. */
	UPROPERTY(Config, EditAnywhere, Category="Timeline")
	TArray<FknightAnimDebugPhaseProperty> TimelinePhases;

	/** 범위 판정 구체: 몬스터 로컬 오프셋(FVector)과 반지름(float) 프로퍼티 이름. 둘 다 있는 어빌리티만 월드에 그린다. */
	UPROPERTY(Config, EditAnywhere, Category="Timeline")
	FName AreaOffsetProperty;

	UPROPERTY(Config, EditAnywhere, Category="Timeline")
	FName AreaRadiusProperty;

	/** 노티파이 스테이트 이름에 이 단어가 들어 있으면 판정 구간으로 보고 빨간색으로 그린다. */
	UPROPERTY(Config, EditAnywhere, Category="Timeline")
	TArray<FString> HitWindowNotifyKeywords;

	UPROPERTY(Config, EditAnywhere, Category="Display")
	TArray<FknightAnimDebugAttribute> Attributes;

	/** 이 접두사로 시작하는 knightPerfTracker 카운터를 성능 줄과 녹화에 넣는다. */
	UPROPERTY(Config, EditAnywhere, Category="Performance")
	TArray<FString> PerfCounterPrefixes;

	/** 몬스터 수 게이지. 있으면 TimeMs 카운터를 이 값으로 나눠 몬스터 1마리당 평균도 보여준다. */
	UPROPERTY(Config, EditAnywhere, Category="Performance")
	FName AliveCountGauge;

	/** 녹화 Markdown에 넣는 이벤트 행의 최대 개수. 전체 이벤트는 CSV에 모두 들어간다. */
	UPROPERTY(Config, EditAnywhere, Category="Recording", meta=(ClampMin=10))
	int32 MaxReportEventRows = 300;

	/**
	 * 커버리지 보드: 이 클래스의 어빌리티가 타이밍 프로퍼티(판정 표시 구간)를 갖지 않으면 몽타주가 판정을 맡는다고 보고,
	 * 판정 노티파이(HitWindowNotifyKeywords)가 없을 때 문제로 표시한다.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Coverage")
	TSoftClassPtr<UGameplayAbility> AttackAbilityClass;

	/** 비어 있어도 동작하는 몽타주 프로퍼티(반응의 RecoverMontage는 비면 RecoverTime으로 대신한다). 비어 있으면 '주의'로 표시한다. */
	UPROPERTY(Config, EditAnywhere, Category="Coverage")
	TArray<FName> OptionalMontageProperties;

	/** 어빌리티 판정 시각과 몽타주 판정 노티파이 시작이 이 값(초)보다 많이 어긋나면 '주의'로 표시한다. */
	UPROPERTY(Config, EditAnywhere, Category="Coverage", meta=(ClampMin=0, Units="s"))
	float HitAlignmentTolerance = 0.1f;

	/**
	 * 공격 미리보기: 판정 노티파이 스테이트에서 궤적을 만들 때 읽는 프로퍼티 이름. 노티파이 객체의 프로퍼티나
	 * 그 안의 구조체(예: TraceParams) 필드에서 찾는다.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Preview")
	FName TraceStartSocketProperty;

	UPROPERTY(Config, EditAnywhere, Category="Preview")
	FName TraceEndSocketProperty;

	UPROPERTY(Config, EditAnywhere, Category="Preview")
	FName TraceRadiusProperty;

	/**
	 * 공격 미리보기: 몽타주의 한 구간만 재생 속도를 바꾸는 노티파이 스테이트. 미리보기는 노티파이를 실행하지 않으므로
	 * 이 클래스의 구간을 읽어 게임과 같은 실제 시간축을 만들고, 타임라인에서 구간을 고칠 때도 이 클래스로 만든다.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Preview")
	TSoftClassPtr<UAnimNotifyState> PlayRateNotifyClass;

	/** PlayRateNotifyClass의 배율 float 프로퍼티 이름. 구간 안에서 몽타주 재생 속도에 곱해진다. */
	UPROPERTY(Config, EditAnywhere, Category="Preview")
	FName PlayRateProperty;

	/**
	 * 공격 미리보기: 판정 노티파이(HitWindowNotifyKeywords)가 없는 몽타주에서 판정 시점으로 볼 노티파이 클래스 이름 키워드.
	 * 내려찍기 같은 범위 공격은 몽타주 대신 어빌리티가 판정하므로, 착지 노티파이를 기준으로 어빌리티 판정 시각을 맞춘다.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Preview")
	TArray<FString> HitMarkerNotifyKeywords;

	/** 미리보기의 플레이어 더미 캡슐 기본 크기(cm). */
	UPROPERTY(Config, EditAnywhere, Category="Preview", meta=(ClampMin=1, Units="cm"))
	float DummyRadius = 42.f;

	UPROPERTY(Config, EditAnywhere, Category="Preview", meta=(ClampMin=1, Units="cm"))
	float DummyHalfHeight = 96.f;

	/**
	 * 전투 피드백 튜닝 탭에 보일 숫자 프로퍼티. 이름(또는 감싼 구조체 프로퍼티 이름)에 하나라도 들어가면 보인다.
	 * 감싼 구조체나 객체 이름이 걸리면 그 안의 숫자는 모두 보인다(FlinchImpact → 넉백 거리, 슬로 시간 / GuardPush 컴포넌트 → 밀림 거리).
	 */
	UPROPERTY(Config, EditAnywhere, Category="Feedback Tuning")
	TArray<FString> FeedbackPropertyKeywords;

	/** 이 클래스를 상속한 객체는 숫자 프로퍼티를 이름과 상관없이 모두 보인다(카메라 셰이크처럼 값 이름이 제각각인 것). */
	UPROPERTY(Config, EditAnywhere, Category="Feedback Tuning")
	TArray<TSoftClassPtr<UObject>> FeedbackAllPropertyClasses;

	/**
	 * 카메라 묶음. 이 클래스를 상속한 게임 클래스(엔진 클래스 제외)의 기본값을 모두 보인다. 모디파이어처럼 공격 중에만 생기는 것도
	 * 미리 바꿔 둘 수 있게 PIE의 인스턴스가 아니라 클래스 기본값을 잡는다.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Feedback Tuning")
	TArray<TSoftClassPtr<UObject>> FeedbackCameraClasses;

	/** 플레이어 공격을 다시 실행할 때 몬스터 몸통 가장자리에서 이만큼 떨어져 선다. */
	UPROPERTY(Config, EditAnywhere, Category="Feedback Tuning", meta=(ClampMin=0, Units="cm"))
	float FeedbackPlayerAttackGap = 150.f;

	/** 다시 실행 전에 플레이어를 몬스터 정면 이 거리에 세운다. */
	UPROPERTY(Config, EditAnywhere, Category="Feedback Tuning", meta=(ClampMin=0, Units="cm"))
	float FeedbackPlaceDistance = 900.f;

	/** false면 플레이어 묶음 값은 PIE에서 바꿔 보기만 하고 저장하지 않는다. */
	UPROPERTY(Config, EditAnywhere, Category="Feedback Tuning")
	bool bFeedbackAllowSavingPlayerValues = false;

	/** 카메라 묶음이어도 이 경로로 시작하는 에셋은 저장하지 않는다(플레이어 담당 에셋: 캐릭터 BP, 어빌리티·큐, 카메라 모드). */
	UPROPERTY(Config, EditAnywhere, Category="Feedback Tuning")
	TArray<FString> FeedbackLockedPathPrefixes;

	/**
	 * 카메라 반응 표 클래스. 이 클래스의 구조체 프로퍼티 하나가 순간 하나다(탭의 "카메라 반응" 목록).
	 * 미리보기는 카메라 매니저의 FeedbackImpactPreviewFunction(순간 이름, 세기)을 부른다.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Feedback Tuning")
	TSoftClassPtr<UObject> FeedbackImpactSettingsClass;

	UPROPERTY(Config, EditAnywhere, Category="Feedback Tuning")
	FName FeedbackImpactPreviewFunction;
};
