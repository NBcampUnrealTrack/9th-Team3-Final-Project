// 플레이어 테스트용 콘솔 명령 (개발 빌드 전용, 치트 명령)
// 몬스터 치트(knightMonsterCheats.cpp)와 같은 knightDev 모듈에 있어 Shipping 빌드에는 들어가지 않는다.
//
// 사용법 (PIE 중 콘솔 ~ 키):
//   knight.Player.Kill                     남은 체력만큼 피해를 넣어 즉시 사망시킨다
//   knight.Player.Damage <피해> [그로기]   지정한 양만큼 피해·그로기 피해를 넣는다 (예: knight.Player.Damage 0 30)
//   knight.Player.SetStat <스탯 이름> <값>  플레이어 스탯의 기본값을 바꾼다 (예: knight.Player.SetStat Weight 100)
//   knight.Player.Run <1|0>                달리기를 켜고 끈다 (키 연결 전 테스트용)
//   knight.Player.Status                   체력, 그로기, 회복 속도, 마지막 피격 후 경과 시간, 부위 체력, 수리 키트 개수를 출력한다
//   knight.Player.Easing                   출발·정지 Easing 값(출발 곡선, 정지 마찰·감속 등)을 출력한다
//   knight.Player.Easing <값 이름> <값>     무브먼트의 숫자 값을 바꾼다 (예: knight.Player.Easing StopBrakingFriction 1.5)
//   knight.Player.Easing Curve [값...]     출발 곡선을 바꾼다. 값은 속도 0%~100%에 고르게 놓인다
//                                          (예: Curve 0.35 1 0.6 → 0%, 50%, 100%). 값 없이 쓰면 곡선을 비워 일정한 가속
//   knight.Player.MoveLog [1|0]            출발·가속·정지에 걸린 시간과 거리를 화면과 로그에 찍는다. 인자가 없으면 켜고 끈다
//   knight.Player.AttackSpeed [배율]        (2026-10-14) 공격 몽타주 재생 속도 배율을 바꾼다 (예: 0.9). 인자가 없으면 지금 값 출력.
//                                          행동 속도(ActionSpeed)에 곱하며 발도·가드는 그대로. 새로 시작하는 공격부터 적용
//   knight.Player.RollBackThrust [1|0]     구를 때 등 부스터를 켜고 끈다 (기본 끔). 인자가 없으면 켜고 끄기를 뒤집는다
//   knight.Player.RollFootThrust [1|0]     구를 때 두 발 부스터를 켜고 끈다 (기본 켬). 인자가 없으면 켜고 끄기를 뒤집는다
//   knight.Player.RollThrustScale [Back|Foot <크기>]
//                                          구를 때 등·발 부스터 크기를 바꾼다 (예: knight.Player.RollThrustScale Foot 0.6).
//                                          인자가 없으면 지금 상태만 출력. 구르기 부스터 명령은 다음 구르기부터 적용
//
// 구르기 부스터 명령으로 바꾼 값도 지금 PIE의 플레이어에게만 적용된다. 정한 값은 GA_Knight_BoostDash의 Dash|Booster 칸에 넣는다.
//
// Easing으로 바꾼 값은 지금 PIE의 플레이어에게만 적용되고 PIE를 끝내면 사라진다.
// 마음에 드는 값은 BP_Knight의 CharacterMovement 컴포넌트(Knight|Movement|Easing)에 넣거나 C++ 기본값으로 옮긴다.
//
// Kill·Damage는 실제 피격과 같은 GAS 경로를 거치므로 아래 흐름 전체를 확인할 수 있다.
//   체력 감소 → 체력 0 → AknightCharacter::HandleOutOfHealth → 레이드 NotifyPlayerDefeated → 레이드 Failed
//   그로기 피해 → 마지막 피격 시각 기록 → 대기 시간 후 그로기 자연 회복
// 결과는 LogknightDev 로그로 출력한다.

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_BoostDash.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AttributeSet.h"
#include "Character/knightMovementComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/CombatInterface.h"
#include "Raid/knightRaidGameState.h"
#include "System/knightGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "knightDev.h"

namespace knightPlayerCheats
{
	// 첫 번째 플레이어 폰. 없으면 경고를 남기고 null.
	static APawn* GetPlayerPawn(UWorld* World, const TCHAR* CommandName)
	{
		const APlayerController* const PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* const Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s: no player pawn."), CommandName);
		}
		return Pawn;
	}

	// 로그에 찍을 현재 레이드 단계 이름. 레이드 맵이 아니면 "(no raid)".
	static const TCHAR* PhaseName(const UWorld* World)
	{
		const AknightRaidGameState* const RaidState = World->GetGameState<AknightRaidGameState>();
		if (!RaidState)
		{
			return TEXT("(no raid)");
		}
		switch (RaidState->GetRaidPhase())
		{
		case EknightRaidPhase::Deploy:    return TEXT("Deploy");
		case EknightRaidPhase::Explore:   return TEXT("Explore");
		case EknightRaidPhase::Combat:    return TEXT("Combat");
		case EknightRaidPhase::Success:   return TEXT("Success");
		case EknightRaidPhase::Failed:    return TEXT("Failed");
		case EknightRaidPhase::Returning: return TEXT("Returning");
		default:                          return TEXT("None");
		}
	}

	// 체력·그로기·사망·레이드 단계를 한 줄로 남긴다.
	static void LogVitals(const UWorld* World, const APawn* Pawn, const UAbilitySystemComponent* ASC, const TCHAR* CommandName, const TCHAR* Label)
	{
		const ICombatInterface* const Combat = Cast<ICombatInterface>(Pawn);
		UE_LOG(LogknightDev, Log, TEXT("%s: %s Health=%.1f  Poise=%.1f/%.1f  Dead=%d  RaidPhase=%s"),
			CommandName, Label,
			ASC->GetNumericAttribute(UknightCoreAttributeSet::GetHealthAttribute()),
			ASC->GetNumericAttribute(UknightCoreAttributeSet::GetPoiseAttribute()),
			ASC->GetNumericAttribute(UknightCoreAttributeSet::GetMaxPoiseAttribute()),
			Combat && Combat->IsDead(), PhaseName(World));
	}

	// 피해량·그로기 피해량을 GE에 넘길 때 쓰는 이름표 (SetByCaller).
	static const FName DebugDamageName(TEXT("KnightDebugDamage"));
	static const FName DebugPoiseDamageName(TEXT("KnightDebugPoiseDamage"));

	// 테스트 전용 피해 GE. "Damage에 +피해, PoiseDamage에 +그로기 피해"를 즉시 적용한다.
	// 에셋을 따로 만들지 않고 코드에서 처음 한 번만 만든 뒤 계속 재사용한다.
	static const UGameplayEffect* GetDebugDamageEffect()
	{
		static TStrongObjectPtr<UGameplayEffect> Effect;
		if (!Effect.IsValid())
		{
			UGameplayEffect* const NewEffect = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("GE_KnightDebugDamage"));
			NewEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

			// 양은 고정값이 아니라 적용할 때마다 이름표로 넘겨받는다.
			auto AddSetByCallerModifier = [NewEffect](const FGameplayAttribute& Attribute, FName DataName)
			{
				FSetByCallerFloat SetByCaller;
				SetByCaller.DataName = DataName;
				FGameplayModifierInfo Modifier;
				Modifier.Attribute = Attribute;
				Modifier.ModifierOp = EGameplayModOp::Additive;
				Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
				NewEffect->Modifiers.Add(Modifier);
			};
			AddSetByCallerModifier(UknightCoreAttributeSet::GetDamageAttribute(), DebugDamageName);
			AddSetByCallerModifier(UknightCoreAttributeSet::GetPoiseDamageAttribute(), DebugPoiseDamageName);

			Effect.Reset(NewEffect);
		}
		return Effect.Get();
	}

	// 첫 번째 플레이어에게 피해·그로기 피해를 넣고 전후 상태를 로그로 남긴다.
	// Damage가 음수면 남은 체력 전부를 피해로 넣는다 (= Kill).
	static void ApplyDebugDamage(UWorld* World, float Damage, float PoiseDamage, const TCHAR* CommandName)
	{
		APawn* const Pawn = GetPlayerPawn(World, CommandName);
		UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		if (!ASC || !ASC->GetSet<UknightCoreAttributeSet>())
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s: no player pawn with an ability system and core attributes."), CommandName);
			return;
		}

		if (Damage < 0.f)
		{
			Damage = ASC->GetNumericAttribute(UknightCoreAttributeSet::GetHealthAttribute());
		}
		LogVitals(World, Pawn, ASC, CommandName, TEXT("before"));

		// ApplyModToAttribute는 속성 값만 직접 바꾸고 PostGameplayEffectExecute(피해→체력 처리)를 부르지 않는다.
		// 그래서 실제 피격처럼 처리되도록 GE를 만들어 적용한다.
		FGameplayEffectSpec Spec(GetDebugDamageEffect(), ASC->MakeEffectContext(), 1.f);
		Spec.SetSetByCallerMagnitude(DebugDamageName, Damage);
		Spec.SetSetByCallerMagnitude(DebugPoiseDamageName, PoiseDamage);
		ASC->ApplyGameplayEffectSpecToSelf(Spec);

		LogVitals(World, Pawn, ASC, CommandName, TEXT("after "));
		UE_LOG(LogknightDev, Log, TEXT("%s: applied damage %.1f, poise damage %.1f"), CommandName, Damage, PoiseDamage);
	}

	// knight.Player.Kill
	static void KillCommand(const TArray<FString>& Args, UWorld* World)
	{
		ApplyDebugDamage(World, -1.f, 0.f, TEXT("knight.Player.Kill"));
	}

	// knight.Player.Damage <피해> [그로기]. 둘 다 0 이하면 경고만 출력한다.
	static void DamageCommand(const TArray<FString>& Args, UWorld* World)
	{
		const float Damage = Args.IsValidIndex(0) ? FMath::Max(0.f, FCString::Atof(*Args[0])) : 0.f;
		const float PoiseDamage = Args.IsValidIndex(1) ? FMath::Max(0.f, FCString::Atof(*Args[1])) : 0.f;
		if (Damage <= 0.f && PoiseDamage <= 0.f)
		{
			UE_LOG(LogknightDev, Warning, TEXT("knight.Player.Damage <Damage> [PoiseDamage]: at least one must be above zero."));
			return;
		}
		ApplyDebugDamage(World, Damage, PoiseDamage, TEXT("knight.Player.Damage"));
	}

	// 현재 이동 모드, 최대 속도, 중량 계수를 로그로 남긴다. 나이트 무브먼트가 아니면 아무것도 하지 않는다.
	// 이동 모드를 같이 찍는 이유: 비행 등 스탯을 따르지 않는 모드에서는 MaxSpeed가 엔진 기본값으로 나온다.
	static void LogMovement(const APawn* Pawn, const TCHAR* CommandName)
	{
		const ACharacter* const Character = Cast<ACharacter>(Pawn);
		const UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr;
		if (Movement)
		{
			const FString ModeName = StaticEnum<EMovementMode>()->GetNameStringByValue(Movement->MovementMode);
			UE_LOG(LogknightDev, Log, TEXT("%s: Mode=%s  Run=%d  MaxSpeed=%.1f  WeightFactor=%.2f"),
				CommandName, *ModeName, Movement->GetWantsToRun(), Movement->GetMaxSpeed(), Movement->GetWeightSpeedFactor());
		}
	}

	// knight.Player.SetStat <스탯 이름> <값>
	// 플레이어가 가진 모든 속성 세트에서 이름이 같은 속성을 찾아 기본값(Base)을 바꾼다.
	// 대소문자는 구분하지 않는다. 범위 제한(Clamp)은 속성 세트 규칙대로 적용된다.
	static void SetStatCommand(const TArray<FString>& Args, UWorld* World)
	{
		const TCHAR* const CommandName = TEXT("knight.Player.SetStat");
		if (Args.Num() < 2)
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s <StatName> <Value>: e.g. knight.Player.SetStat Weight 100"), CommandName);
			return;
		}
		APawn* const Pawn = GetPlayerPawn(World, CommandName);
		UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		if (!ASC)
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s: the player has no ability system."), CommandName);
			return;
		}

		// 속성 세트 클래스들의 FGameplayAttributeData 속성 중 이름이 맞는 것을 찾는다.
		FGameplayAttribute Found;
		for (const UAttributeSet* const Set : ASC->GetSpawnedAttributes())
		{
			for (TFieldIterator<FProperty> It(Set ? Set->GetClass() : nullptr); It && !Found.IsValid(); ++It)
			{
				if (FGameplayAttribute::IsGameplayAttributeDataProperty(*It) && It->GetName().Equals(Args[0], ESearchCase::IgnoreCase))
				{
					Found = FGameplayAttribute(*It);
				}
			}
		}
		if (!Found.IsValid())
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s: no attribute named '%s' on the player."), CommandName, *Args[0]);
			return;
		}

		const float OldValue = ASC->GetNumericAttribute(Found);
		ASC->SetNumericAttributeBase(Found, FCString::Atof(*Args[1]));
		UE_LOG(LogknightDev, Log, TEXT("%s: %s %.2f -> %.2f"), CommandName, *Found.GetName(), OldValue, ASC->GetNumericAttribute(Found));
		LogMovement(Pawn, CommandName);
	}

	// knight.Player.Run <1|0>. 인자가 없으면 현재 상태를 뒤집는다.
	static void RunCommand(const TArray<FString>& Args, UWorld* World)
	{
		const TCHAR* const CommandName = TEXT("knight.Player.Run");
		const ACharacter* const Character = Cast<ACharacter>(GetPlayerPawn(World, CommandName));
		UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr;
		if (!Movement)
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s: the player does not use knightMovementComponent."), CommandName);
			return;
		}
		const bool bRun = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) != 0 : !Movement->GetWantsToRun();
		Movement->SetWantsToRun(bRun);
		LogMovement(Character, CommandName);
	}

	// knight.Player.AttackSpeed [배율] (2026-10-14). 공격 몽타주 재생 속도 배율(ASC AttackAnimationSpeed)을 바꾼다.
	// 인자가 없으면 지금 값만 출력한다. 새로 시작하는 공격부터 적용되고, 지금 PIE의 플레이어에게만 적용된다.
	static void AttackSpeedCommand(const TArray<FString>& Args, UWorld* World)
	{
		const TCHAR* const CommandName = TEXT("knight.Player.AttackSpeed");
		UknightAbilitySystemComponent* const ASC = Cast<UknightAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPlayerPawn(World, CommandName)));
		if (!ASC)
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s: no player pawn with a knight ability system."), CommandName);
			return;
		}
		const float Before = ASC->GetAttackAnimationSpeed();
		if (Args.IsValidIndex(0))
		{
			ASC->SetAttackAnimationSpeed(FCString::Atof(*Args[0]));
		}
		UE_LOG(LogknightDev, Log, TEXT("%s: attack animation speed x%.2f -> x%.2f (multiplied with ActionSpeed; draw and guard unchanged)."),
			CommandName, Before, ASC->GetAttackAnimationSpeed());
	}

	// 플레이어 구르기 어빌리티 인스턴스(구르기 부스터 명령용). 없으면 경고를 남기고 null.
	static UGameplayAbility* GetRollAbility(UWorld* World, const TCHAR* CommandName)
	{
		if (const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPlayerPawn(World, CommandName)))
		{
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->IsA<UknightGameplayAbility_BoostDash>())
				{
					if (UGameplayAbility* const Instance = Spec.GetPrimaryInstance())
					{
						return Instance;
					}
				}
			}
		}
		UE_LOG(LogknightDev, Warning, TEXT("%s: the player has no roll ability instance."), CommandName);
		return nullptr;
	}

	// 구르기 부스터 칸은 보호된 칸이라 이름으로 찾아 바꾼다(게임 코드에 치트용 공개 함수를 늘리지 않으려고).
	// Part는 "Back" 또는 "Foot": bUseRoll<Part>Thrust(켬 여부), Roll<Part>Thrust.Scale(크기).
	static const FBoolProperty* FindRollThrustToggle(const UGameplayAbility* Ability, const TCHAR* Part)
	{
		return FindFProperty<FBoolProperty>(Ability->GetClass(), *FString::Printf(TEXT("bUseRoll%sThrust"), Part));
	}

	static float* FindRollThrustScale(UGameplayAbility* Ability, const TCHAR* Part)
	{
		const FStructProperty* const Settings = FindFProperty<FStructProperty>(Ability->GetClass(), *FString::Printf(TEXT("Roll%sThrust"), Part));
		const FFloatProperty* const Scale = Settings ? FindFProperty<FFloatProperty>(Settings->Struct, TEXT("Scale")) : nullptr;
		return Scale ? Scale->ContainerPtrToValuePtr<float>(Settings->ContainerPtrToValuePtr<void>(Ability)) : nullptr;
	}

	// 지금 등·발 부스터 상태를 한 줄로 찍는다.
	static void LogRollThrust(UGameplayAbility* Ability, const TCHAR* CommandName)
	{
		static const TCHAR* const Parts[] = { TEXT("Back"), TEXT("Foot") };
		FString Text;
		for (const TCHAR* const Part : Parts)
		{
			const FBoolProperty* const Toggle = FindRollThrustToggle(Ability, Part);
			const float* const Scale = FindRollThrustScale(Ability, Part);
			if (!Toggle || !Scale)
			{
				UE_LOG(LogknightDev, Warning, TEXT("%s: the roll ability has no %s thrust settings."), CommandName, Part);
				return;
			}
			Text += FString::Printf(TEXT("%s%s %s (scale %.2f)"), Text.IsEmpty() ? TEXT("") : TEXT(", "), Part,
				Toggle->GetPropertyValue_InContainer(Ability) ? TEXT("on") : TEXT("off"), *Scale);
		}
		UE_LOG(LogknightDev, Log, TEXT("%s: roll thrust %s. Applies from the next roll, this PIE only."), CommandName, *Text);
	}

	// knight.Player.RollBackThrust / RollFootThrust [1|0]. 인자가 없으면 켜고 끄기를 뒤집는다.
	static void SetRollThrustToggle(const TArray<FString>& Args, UWorld* World, const TCHAR* Part, const TCHAR* CommandName)
	{
		UGameplayAbility* const Ability = GetRollAbility(World, CommandName);
		const FBoolProperty* const Toggle = Ability ? FindRollThrustToggle(Ability, Part) : nullptr;
		if (!Toggle)
		{
			if (Ability)
			{
				UE_LOG(LogknightDev, Warning, TEXT("%s: the roll ability has no %s thrust toggle."), CommandName, Part);
			}
			return;
		}
		const bool bOn = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) != 0 : !Toggle->GetPropertyValue_InContainer(Ability);
		Toggle->SetPropertyValue_InContainer(Ability, bOn);
		LogRollThrust(Ability, CommandName);
	}

	static void RollBackThrustCommand(const TArray<FString>& Args, UWorld* World)
	{
		SetRollThrustToggle(Args, World, TEXT("Back"), TEXT("knight.Player.RollBackThrust"));
	}

	static void RollFootThrustCommand(const TArray<FString>& Args, UWorld* World)
	{
		SetRollThrustToggle(Args, World, TEXT("Foot"), TEXT("knight.Player.RollFootThrust"));
	}

	// knight.Player.RollThrustScale [Back|Foot <크기>]. 인자가 없으면 지금 상태만 찍는다.
	static void RollThrustScaleCommand(const TArray<FString>& Args, UWorld* World)
	{
		const TCHAR* const CommandName = TEXT("knight.Player.RollThrustScale");
		UGameplayAbility* const Ability = GetRollAbility(World, CommandName);
		if (!Ability)
		{
			return;
		}
		if (!Args.IsEmpty())
		{
			const TCHAR* const Part = Args[0].Equals(TEXT("Back"), ESearchCase::IgnoreCase) ? TEXT("Back")
				: Args[0].Equals(TEXT("Foot"), ESearchCase::IgnoreCase) ? TEXT("Foot") : nullptr;
			float* const Scale = Part && Args.IsValidIndex(1) ? FindRollThrustScale(Ability, Part) : nullptr;
			if (!Scale)
			{
				UE_LOG(LogknightDev, Warning, TEXT("%s Back|Foot <Scale> (e.g. %s Foot 0.6)."), CommandName, CommandName);
				return;
			}
			*Scale = FMath::Max(0.f, FCString::Atof(*Args[1]));
		}
		LogRollThrust(Ability, CommandName);
	}

	// knight.Player.Status. 그로기 회복을 확인할 때 피격 후 몇 번 입력해 Poise가 오르는지 본다.
	static void StatusCommand(const TArray<FString>& Args, UWorld* World)
	{
		const TCHAR* const CommandName = TEXT("knight.Player.Status");
		APawn* const Pawn = GetPlayerPawn(World, CommandName);
		const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		const UknightCoreAttributeSet* const CoreSet = ASC ? ASC->GetSet<UknightCoreAttributeSet>() : nullptr;
		if (!CoreSet)
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s: no player pawn with core attributes."), CommandName);
			return;
		}
		LogVitals(World, Pawn, ASC, CommandName, TEXT(""));
		UE_LOG(LogknightDev, Log, TEXT("%s: PoiseRecoveryRate=%.1f/s  SecondsSinceLastHit=%.2f  MaxHealth=%.1f  AttackPower=%.1f"),
			CommandName, CoreSet->GetPoiseRecoveryRate(), World->GetTimeSeconds() - CoreSet->GetLastHitTime(),
			CoreSet->GetMaxHealth(), CoreSet->GetAttackPower());
		// 받는 쪽 보정 (2026-10-10): 방어력, 받는 피해·그로기 배율(파츠 효과용, 기본 1.0)
		UE_LOG(LogknightDev, Log, TEXT("%s: Defense=%.1f  DamageTakenMultiplier=%.2f  PoiseDamageTakenMultiplier=%.2f"),
			CommandName, CoreSet->GetDefense(), CoreSet->GetDamageTakenMultiplier(), CoreSet->GetPoiseDamageTakenMultiplier());

		// 부위 체력: 현재/최대, 파괴 여부(B). 파괴 페널티가 걸리면 위 AttackPower·아래 MaxSpeed가 줄어든다.
		if (const UknightAttributeSet* const MechSet = ASC->GetSet<UknightAttributeSet>())
		{
			auto PartText = [ASC](const TCHAR* Name, float Health, float MaxHealth, const FGameplayTag& BrokenTag)
			{
				return FString::Printf(TEXT("%s=%.1f/%.1f%s"), Name, Health, MaxHealth, ASC->HasMatchingGameplayTag(BrokenTag) ? TEXT("(B)") : TEXT(""));
			};
			UE_LOG(LogknightDev, Log, TEXT("%s: Parts %s  %s  %s  %s"), CommandName,
				*PartText(TEXT("Head"), MechSet->GetHeadHealth(), MechSet->GetMaxHeadHealth(), knightGameplayTags::State_PartBroken_Head),
				*PartText(TEXT("Torso"), MechSet->GetTorsoHealth(), MechSet->GetMaxTorsoHealth(), knightGameplayTags::State_PartBroken_Torso),
				*PartText(TEXT("Arms"), MechSet->GetArmsHealth(), MechSet->GetMaxArmsHealth(), knightGameplayTags::State_PartBroken_Arms),
				*PartText(TEXT("Legs"), MechSet->GetLegsHealth(), MechSet->GetMaxLegsHealth(), knightGameplayTags::State_PartBroken_Legs));
			// 수리 키트 (2026-10-09). 보충: knight.Player.SetStat RepairKitCount 3
			UE_LOG(LogknightDev, Log, TEXT("%s: RepairKits=%.0f  RepairEfficiency=%.2f  Repairing=%d"), CommandName,
				MechSet->GetRepairKitCount(), MechSet->GetRepairEfficiency(), ASC->HasMatchingGameplayTag(knightGameplayTags::State_Repairing));
		}
		LogMovement(Pawn, CommandName);
	}

	// 플레이어의 나이트 무브먼트. 없으면 경고를 남기고 null.
	static UknightMovementComponent* GetPlayerMovement(UWorld* World, const TCHAR* CommandName)
	{
		const ACharacter* const Character = Cast<ACharacter>(GetPlayerPawn(World, CommandName));
		UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr;
		if (Character && !Movement)
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s: the player does not use knightMovementComponent."), CommandName);
		}
		return Movement;
	}

	// 무브먼트의 float 값을 이름으로 찾는다(대소문자 무시). Stop 값처럼 protected인 값도 읽고 쓰려고 리플렉션을 쓴다.
	static FFloatProperty* FindMovementFloat(const UknightMovementComponent* Movement, const FString& Name)
	{
		for (TFieldIterator<FFloatProperty> It(Movement->GetClass()); It; ++It)
		{
			if (It->GetName().Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	// 출발 곡선(StartAccelerationCurve). 무브먼트의 protected 값이라 리플렉션으로 꺼낸다.
	static FRuntimeFloatCurve* FindStartCurve(UknightMovementComponent* Movement)
	{
		const FStructProperty* const Property = FindFProperty<FStructProperty>(Movement->GetClass(), TEXT("StartAccelerationCurve"));
		return Property && Property->Struct == FRuntimeFloatCurve::StaticStruct()
			? Property->ContainerPtrToValuePtr<FRuntimeFloatCurve>(Movement)
			: nullptr;
	}

	// 출발·정지 Easing에 쓰이는 값을 남긴다. 곡선은 '속도%:가속 배율'로 찍는다.
	static void LogEasing(UknightMovementComponent* Movement, const TCHAR* CommandName)
	{
		auto Value = [Movement](const TCHAR* Name)
		{
			const FFloatProperty* const Property = FindMovementFloat(Movement, Name);
			return Property ? Property->GetPropertyValue_InContainer(Movement) : -1.f;
		};

		FString CurveText = TEXT("empty = constant acceleration");
		if (const FRuntimeFloatCurve* const Curve = FindStartCurve(Movement))
		{
			TArray<FString> Keys;
			for (const FRichCurveKey& Key : Curve->GetRichCurveConst()->GetConstRefOfKeys())
			{
				Keys.Add(FString::Printf(TEXT("%.0f%%:%.2f"), Key.Time * 100.f, Key.Value));
			}
			if (Keys.Num() > 0)
			{
				CurveText = FString::Join(Keys, TEXT(", "));
			}
		}

		UE_LOG(LogknightDev, Log, TEXT("%s: start   MaxAcceleration=%.0f  StartAccelerationCurve=[%s]"),
			CommandName, Value(TEXT("MaxAcceleration")), *CurveText);
		UE_LOG(LogknightDev, Log, TEXT("%s: stop    StopBrakingFriction=%.2f  StopBrakingDeceleration=%.0f  BrakingFrictionFactor=%.2f"),
			CommandName, Value(TEXT("StopBrakingFriction")), Value(TEXT("StopBrakingDeceleration")), Value(TEXT("BrakingFrictionFactor")));
		UE_LOG(LogknightDev, Log, TEXT("%s: locked  GroundFriction=%.2f  BrakingDecelerationWalking=%.0f  (guard, quickstep recovery; locked now=%d)"),
			CommandName, Value(TEXT("GroundFriction")), Value(TEXT("BrakingDecelerationWalking")), Movement->IsMoveInputLocked());
		LogMovement(Movement->GetCharacterOwner(), CommandName);
	}

	// knight.Player.Easing [값 이름 값 | Curve 값...]. 인자가 없으면 지금 값만 출력한다.
	static void EasingCommand(const TArray<FString>& Args, UWorld* World)
	{
		const TCHAR* const CommandName = TEXT("knight.Player.Easing");
		UknightMovementComponent* const Movement = GetPlayerMovement(World, CommandName);
		if (!Movement)
		{
			return;
		}

		if (Args.IsValidIndex(0) && Args[0].Equals(TEXT("Curve"), ESearchCase::IgnoreCase))
		{
			FRuntimeFloatCurve* const Curve = FindStartCurve(Movement);
			if (!Curve)
			{
				UE_LOG(LogknightDev, Warning, TEXT("%s: StartAccelerationCurve not found."), CommandName);
				return;
			}
			// 커브 에셋이 지정돼 있으면 에셋을 고치지 않도록, 이 플레이어에서만 에셋 연결을 끊고 키를 직접 넣는다.
			Curve->ExternalCurve = nullptr;
			Curve->EditorCurveData.Reset();
			const int32 KeyCount = Args.Num() - 1;
			for (int32 Index = 0; Index < KeyCount; ++Index)
			{
				const float Time = KeyCount > 1 ? static_cast<float>(Index) / (KeyCount - 1) : 0.f;
				Curve->EditorCurveData.AddKey(Time, FCString::Atof(*Args[Index + 1]));
			}
		}
		else if (Args.Num() >= 2)
		{
			FFloatProperty* const Property = FindMovementFloat(Movement, Args[0]);
			if (!Property)
			{
				UE_LOG(LogknightDev, Warning, TEXT("%s: the movement component has no float value named '%s'."), CommandName, *Args[0]);
				return;
			}
			const float OldValue = Property->GetPropertyValue_InContainer(Movement);
			Property->SetPropertyValue_InContainer(Movement, FCString::Atof(*Args[1]));
			UE_LOG(LogknightDev, Log, TEXT("%s: %s %.2f -> %.2f"), CommandName, *Property->GetName(), OldValue, Property->GetPropertyValue_InContainer(Movement));
		}
		else if (Args.Num() == 1)
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s [<Name> <Value> | Curve <Values...>]: e.g. knight.Player.Easing StopBrakingFriction 1.5"), CommandName);
		}
		LogEasing(Movement, CommandName);
	}

	// knight.Player.MoveLog 측정 상태. 한 번에 한 플레이어만 잰다.
	struct FMoveLogState
	{
		// SpeedUp = 최고 속도까지 가속, SlowDown = 최고 속도가 낮아져 줄어듦(달리기 → 걷기), Stop = 방향키를 떼고 멈춤
		enum class EPhase : uint8 { None, SpeedUp, SlowDown, Stop };

		TWeakObjectPtr<UknightMovementComponent> Movement;
		FTSTicker::FDelegateHandle TickerHandle;
		EPhase Phase = EPhase::None;
		double StartTime = 0.0;
		FVector StartLocation = FVector::ZeroVector;
		float StartSpeed = 0.f;
		float TargetSpeed = 0.f;
		double Time50 = -1.0;
		double Time90 = -1.0;
		bool bLocked = false;
	};
	static FMoveLogState MoveLogState;

	// 측정 결과를 로그와 화면에 남긴다. 화면 글꼴에 한글이 없을 수 있어 영어로 찍는다.
	static void PrintMoveLog(const FString& Text)
	{
		UE_LOG(LogknightDev, Log, TEXT("knight.Player.MoveLog: %s"), *Text);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Cyan, Text);
		}
	}

	// 매 프레임 속도를 보고 가속·감속·정지 구간의 시간과 거리를 잰다. 프레임 단위라 ±1프레임 오차가 있다.
	static bool TickMoveLog(float DeltaTime)
	{
		using EPhase = FMoveLogState::EPhase;
		FMoveLogState& State = MoveLogState;
		const UknightMovementComponent* const Movement = State.Movement.Get();
		const UWorld* const World = Movement ? Movement->GetWorld() : nullptr;
		if (!World || !Movement->UpdatedComponent)
		{
			// PIE가 끝나 플레이어가 사라지면 측정도 끝낸다.
			State = FMoveLogState();
			return false;
		}

		// 루트 모션(퀵스텝·공격)이나 공중에서는 재지 않는다. 끝나면 그때 속도에서 다시 잰다.
		if (Movement->HasAnimRootMotion() || Movement->CurrentRootMotion.HasActiveRootMotionSources() || !Movement->IsMovingOnGround())
		{
			State.Phase = EPhase::None;
			return true;
		}

		constexpr float StoppedSpeed = 1.f;
		const double Now = World->GetTimeSeconds();
		const FVector Location = Movement->UpdatedComponent->GetComponentLocation();
		const float Speed = static_cast<float>(Movement->Velocity.Size2D());
		const float MaxSpeed = Movement->GetMaxSpeed();
		const bool bHasInput = !Movement->GetCurrentAcceleration().IsNearlyZero();
		const float Elapsed = static_cast<float>(Now - State.StartTime);
		const float Moved = static_cast<float>(FVector::Dist2D(Location, State.StartLocation));

		// 진행 중인 측정을 이어 가거나 끝낸다.
		if (State.Phase == EPhase::SpeedUp)
		{
			if (!bHasInput || Speed > MaxSpeed * 1.01f)
			{
				// 도중에 키를 떼거나 최고 속도가 낮아지면 이번 측정은 버린다.
				State.Phase = EPhase::None;
			}
			else
			{
				State.TargetSpeed = MaxSpeed;
				const float Range = State.TargetSpeed - State.StartSpeed;
				const float Progress = Range > 1.f ? (Speed - State.StartSpeed) / Range : 1.f;
				if (State.Time50 < 0.0 && Progress >= 0.5f)
				{
					State.Time50 = Now;
				}
				if (State.Time90 < 0.0 && Progress >= 0.9f)
				{
					State.Time90 = Now;
				}
				if (Speed >= State.TargetSpeed * 0.99f)
				{
					PrintMoveLog(FString::Printf(TEXT("speed up %.0f -> %.0f: 50%% %.2fs, 90%% %.2fs, reached %.2fs, moved %.0fcm"),
						State.StartSpeed, State.TargetSpeed, State.Time50 - State.StartTime, State.Time90 - State.StartTime, Elapsed, Moved));
					State.Phase = EPhase::None;
				}
			}
		}
		else if (State.Phase == EPhase::SlowDown)
		{
			if (!bHasInput)
			{
				State.Phase = EPhase::None;
			}
			else if (Speed <= MaxSpeed * 1.01f)
			{
				PrintMoveLog(FString::Printf(TEXT("slow down %.0f -> %.0f (lower top speed, e.g. run -> walk): %.2fs, moved %.0fcm"),
					State.StartSpeed, MaxSpeed, Elapsed, Moved));
				State.Phase = EPhase::None;
			}
		}
		else if (State.Phase == EPhase::Stop)
		{
			State.bLocked |= Movement->IsMoveInputLocked();
			if (bHasInput)
			{
				State.Phase = EPhase::None;
			}
			else if (Speed < StoppedSpeed)
			{
				PrintMoveLog(FString::Printf(TEXT("stop %.0f -> 0: %.2fs, slid %.0fcm%s"), State.StartSpeed, Elapsed, Moved,
					State.bLocked ? TEXT(" (move input locked: guard / quickstep recovery)") : TEXT("")));
				State.Phase = EPhase::None;
			}
		}

		// 새 측정을 시작한다. 방향 전환으로 속도가 10% 넘게 떨어졌다가 다시 붙는 것도 가속으로 잰다.
		if (State.Phase == EPhase::None)
		{
			EPhase NewPhase = EPhase::None;
			if (!bHasInput && Speed >= StoppedSpeed)
			{
				NewPhase = EPhase::Stop;
			}
			else if (bHasInput && Speed < MaxSpeed * 0.9f)
			{
				NewPhase = EPhase::SpeedUp;
			}
			else if (bHasInput && Speed > MaxSpeed * 1.01f)
			{
				NewPhase = EPhase::SlowDown;
			}
			if (NewPhase != EPhase::None)
			{
				State.Phase = NewPhase;
				State.StartTime = Now;
				State.StartLocation = Location;
				State.StartSpeed = Speed;
				State.TargetSpeed = MaxSpeed;
				State.Time50 = -1.0;
				State.Time90 = -1.0;
				State.bLocked = false;
			}
		}
		return true;
	}

	// knight.Player.MoveLog [1|0]. 인자가 없으면 켜고 끈다.
	static void MoveLogCommand(const TArray<FString>& Args, UWorld* World)
	{
		const TCHAR* const CommandName = TEXT("knight.Player.MoveLog");
		const bool bWasOn = MoveLogState.TickerHandle.IsValid();
		const bool bOn = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) != 0 : !bWasOn;
		if (bWasOn)
		{
			FTSTicker::GetCoreTicker().RemoveTicker(MoveLogState.TickerHandle);
			MoveLogState = FMoveLogState();
		}
		if (bOn)
		{
			UknightMovementComponent* const Movement = GetPlayerMovement(World, CommandName);
			if (!Movement)
			{
				return;
			}
			MoveLogState.Movement = Movement;
			MoveLogState.TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickMoveLog));
		}
		UE_LOG(LogknightDev, Log, TEXT("%s: %s"), CommandName,
			bOn ? TEXT("on. Speed up, slow down and stop times are printed on screen and in the log.") : TEXT("off"));
	}

	// 콘솔 명령 등록. ECVF_Cheat라 개발 빌드에서만 동작한다.
	static FAutoConsoleCommandWithWorldAndArgs KillCmd(
		TEXT("knight.Player.Kill"),
		TEXT("knight.Player.Kill. Deals the player's remaining Health as damage through GAS, exercising the real death path."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KillCommand),
		ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs DamageCmd(
		TEXT("knight.Player.Damage"),
		TEXT("knight.Player.Damage <Damage> [PoiseDamage]. Deals damage and poise damage to the player through GAS."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DamageCommand),
		ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs SetStatCmd(
		TEXT("knight.Player.SetStat"),
		TEXT("knight.Player.SetStat <StatName> <Value>. Sets the base value of a player attribute by name (e.g. Weight, WalkSpeed)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetStatCommand),
		ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs RunCmd(
		TEXT("knight.Player.Run"),
		TEXT("knight.Player.Run <1|0>. Turns running on or off for testing before the run key is bound. No argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunCommand),
		ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs AttackSpeedCmd(
		TEXT("knight.Player.AttackSpeed"),
		TEXT("knight.Player.AttackSpeed [Multiplier]. Sets the attack montage play rate multiplier (e.g. 0.9) for this PIE. No argument logs the current value."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AttackSpeedCommand),
		ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs RollBackThrustCmd(
		TEXT("knight.Player.RollBackThrust"),
		TEXT("knight.Player.RollBackThrust [1|0]. Turns the back booster on roll start on or off for this PIE. No argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RollBackThrustCommand),
		ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs RollFootThrustCmd(
		TEXT("knight.Player.RollFootThrust"),
		TEXT("knight.Player.RollFootThrust [1|0]. Turns the foot boosters on roll start on or off for this PIE. No argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RollFootThrustCommand),
		ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs RollThrustScaleCmd(
		TEXT("knight.Player.RollThrustScale"),
		TEXT("knight.Player.RollThrustScale [Back|Foot <Scale>]. Sets the roll back or foot booster size for this PIE (e.g. Foot 0.6). No argument logs both."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RollThrustScaleCommand),
		ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs StatusCmd(
		TEXT("knight.Player.Status"),
		TEXT("knight.Player.Status. Logs health, poise, poise recovery rate, time since the last hit and movement."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StatusCommand),
		ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs EasingCmd(
		TEXT("knight.Player.Easing"),
		TEXT("knight.Player.Easing [<Name> <Value> | Curve <Values...>]. Logs or changes the player's start/stop easing for this PIE only. ")
		TEXT("e.g. knight.Player.Easing StopBrakingFriction 1.5, knight.Player.Easing Curve 0.35 1 0.6"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&EasingCommand),
		ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs MoveLogCmd(
		TEXT("knight.Player.MoveLog"),
		TEXT("knight.Player.MoveLog [1|0]. Prints how long the player takes to speed up, slow down and stop, and how far it slides. No argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MoveLogCommand),
		ECVF_Cheat);
}
