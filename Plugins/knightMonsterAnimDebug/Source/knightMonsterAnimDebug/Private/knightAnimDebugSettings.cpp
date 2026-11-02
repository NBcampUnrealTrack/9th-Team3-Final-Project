#include "knightAnimDebugSettings.h"

#include "Abilities/GameplayAbility.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameFramework/Pawn.h"

UknightAnimDebugSettings::UknightAnimDebugSettings()
{
	TrackedPawnClass = TSoftClassPtr<APawn>(FSoftObjectPath(TEXT("/Script/knight.knightMonsterCharacterBase")));

	PlayableAbilityClasses = {
		TSoftClassPtr<UGameplayAbility>(FSoftObjectPath(TEXT("/Script/knight.knightGameplayAbility_MonsterAttack"))),
		TSoftClassPtr<UGameplayAbility>(FSoftObjectPath(TEXT("/Script/knight.knightGameplayAbility_MonsterReaction")))
	};

	TagPrefixes = { TEXT("State.") };

	// 범위 공격: 예고 → (판정) → 지속 → 후딜. 반응: 유지 → 회복. 클래스마다 둘 중 한 묶음만 있다.
	auto AddPhase = [this](const TCHAR* PropertyName, const TCHAR* Label, FColor Color, bool bHitAtEnd, bool bHitWindow = false)
	{
		FknightAnimDebugPhaseProperty& Phase = TimelinePhases.AddDefaulted_GetRef();
		Phase.PropertyName = PropertyName;
		Phase.Label = Label;
		Phase.Color = Color;
		Phase.bHitAtEnd = bHitAtEnd;
		Phase.bHitWindow = bHitWindow;
	};
	AddPhase(TEXT("WindupTime"), TEXT("예고"), FColor(255, 160, 40), true);
	AddPhase(TEXT("ActiveTime"), TEXT("지속"), FColor(230, 60, 60), false, true);
	AddPhase(TEXT("RecoveryTime"), TEXT("후딜"), FColor(80, 140, 255), false);
	AddPhase(TEXT("HoldTime"), TEXT("유지"), FColor(180, 100, 255), false);
	AddPhase(TEXT("RecoverTime"), TEXT("회복"), FColor(60, 200, 180), false);

	AreaOffsetProperty = TEXT("AreaOffset");
	AreaRadiusProperty = TEXT("AreaRadius");
	HitWindowNotifyKeywords = { TEXT("MeleeTrace") };

	auto AddAttribute = [this](const TCHAR* Name, const TCHAR* MaxName)
	{
		FknightAnimDebugAttribute& Attribute = Attributes.AddDefaulted_GetRef();
		Attribute.Name = Name;
		Attribute.MaxName = MaxName;
	};
	AddAttribute(TEXT("Health"), TEXT("MaxHealth"));
	AddAttribute(TEXT("Poise"), TEXT("MaxPoise"));

	PerfCounterPrefixes = { TEXT("Monster."), TEXT("AI.") };
	AliveCountGauge = TEXT("Monster.Alive");

	AttackAbilityClass = TSoftClassPtr<UGameplayAbility>(FSoftObjectPath(TEXT("/Script/knight.knightGameplayAbility_MonsterAttack")));
	OptionalMontageProperties = { TEXT("RecoverMontage") };

	TraceStartSocketProperty = TEXT("StartSocket");
	TraceEndSocketProperty = TEXT("EndSocket");
	TraceRadiusProperty = TEXT("Radius");
	PlayRateNotifyClass = TSoftClassPtr<UAnimNotifyState>(FSoftObjectPath(TEXT("/Script/knight.knightAnimNotifyState_PlayRate")));
	PlayRateProperty = TEXT("PlayRate");
	HitMarkerNotifyKeywords = { TEXT("GroundImpact") };

	FeedbackPropertyKeywords = {
		TEXT("HitStop"), TEXT("Shake"), TEXT("Knockback"), TEXT("SlowMo"), TEXT("Poise"), TEXT("Groggy"),
		TEXT("Flinch"), TEXT("Stagger"), TEXT("Push"), TEXT("TimeDilation"), TEXT("ForceFeedback"), TEXT("Dust"), TEXT("Splash"), TEXT("Parried")
	};
	FeedbackAllPropertyClasses = { TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/Script/Engine.CameraShakePattern"))) };
	FeedbackLockedPathPrefixes = { TEXT("/Game/Knight/Blueprints/"), TEXT("/Game/Knight/Abilities/"), TEXT("/Game/Knight/Camera/CM_") };
	FeedbackImpactSettingsClass = TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/Script/knight.knightCameraImpactSettings")));
	FeedbackImpactPreviewFunction = TEXT("PreviewImpactReaction");
	// 카메라 모드는 카메라 컴포넌트가 실제로 만든 인스턴스만 잡는다(쓰지 않는 모드의 값이 섞이지 않게).
	FeedbackCameraClasses = { TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/Script/Engine.CameraModifier"))) };
}
