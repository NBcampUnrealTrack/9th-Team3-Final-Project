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
}
