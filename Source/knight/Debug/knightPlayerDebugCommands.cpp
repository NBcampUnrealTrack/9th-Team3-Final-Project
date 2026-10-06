// 플레이어 피해·사망 테스트용 콘솔 명령 (개발 빌드 전용, 치트 명령)
//
// 사용법 (PIE 중 콘솔 ~ 키):
//   knight.Player.Kill          남은 체력만큼 피해를 넣어 즉시 사망시킨다
//   knight.Player.Damage <양>   지정한 양만큼 피해를 넣는다
//   knight.Player.SetStat <스탯 이름> <값>
//                               플레이어 스탯의 기본값을 바꾼다 (예: knight.Player.SetStat Weight 100)
//   knight.Player.Run <1|0>     달리기를 켜고 끈다 (키 연결 전 테스트용)
//
// Kill·Damage는 실제 피격과 같은 GAS 경로를 거치므로 아래 흐름 전체를 확인할 수 있다.
//   체력 감소 → 체력 0 → AknightCharacter::HandleOutOfHealth → 레이드 NotifyPlayerDefeated → 레이드 Failed
// 실행 전후의 체력, 사망 여부, 레이드 단계를 Logknight 로그로 출력한다.
// SetStat·Run은 실행 후 스탯 값과 현재 지상 최대 속도, 중량 계수를 로그로 출력한다.

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AttributeSet.h"
#include "Character/knightMovementComponent.h"
#include "GameFramework/Character.h"
#include "UObject/UnrealType.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "UObject/StrongObjectPtr.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/CombatInterface.h"
#include "Raid/knightRaidGameState.h"
#include "knight.h"

namespace knightPlayerDebug
{
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

	// 피해량을 GE에 넘길 때 쓰는 이름표 (SetByCaller).
	static const FName DebugDamageName(TEXT("KnightDebugDamage"));

	// 테스트 전용 피해 GE. "Damage 속성에 +(넘겨받은 피해량)"을 즉시 적용한다.
	// 에셋을 따로 만들지 않고 코드에서 처음 한 번만 만든 뒤 계속 재사용한다.
	static const UGameplayEffect* GetDebugDamageEffect()
	{
		static TStrongObjectPtr<UGameplayEffect> Effect;
		if (!Effect.IsValid())
		{
			UGameplayEffect* const NewEffect = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("GE_KnightDebugDamage"));
			NewEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

			// 피해량은 고정값이 아니라 적용할 때마다 이름표로 넘겨받는다.
			FSetByCallerFloat SetByCaller;
			SetByCaller.DataName = DebugDamageName;
			FGameplayModifierInfo Modifier;
			Modifier.Attribute = UknightCoreAttributeSet::GetDamageAttribute();
			Modifier.ModifierOp = EGameplayModOp::Additive;
			Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
			NewEffect->Modifiers.Add(Modifier);

			Effect.Reset(NewEffect);
		}
		return Effect.Get();
	}

	// 첫 번째 플레이어에게 피해를 넣고 전후 상태를 로그로 남긴다.
	// Amount가 음수면 남은 체력 전부를 피해로 넣는다 (= Kill).
	static void ApplyDebugDamage(UWorld* World, float Amount, const TCHAR* CommandName)
	{
		const APlayerController* const PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* const Pawn = PC ? PC->GetPawn() : nullptr;
		UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		if (!ASC || !ASC->GetSet<UknightCoreAttributeSet>())
		{
			UE_LOG(Logknight, Warning, TEXT("%s: no player pawn with an ability system and core attributes."), CommandName);
			return;
		}

		const ICombatInterface* const Combat = Cast<ICombatInterface>(Pawn);
		const float OldHealth = ASC->GetNumericAttribute(UknightCoreAttributeSet::GetHealthAttribute());
		const float Damage = Amount < 0.f ? OldHealth : Amount;
		UE_LOG(Logknight, Log, TEXT("%s: before  Health=%.1f  Dead=%d  RaidPhase=%s"),
			CommandName, OldHealth, Combat && Combat->IsDead(), PhaseName(World));

		// ApplyModToAttribute는 속성 값만 직접 바꾸고 PostGameplayEffectExecute(피해→체력 처리)를 부르지 않는다.
		// 그래서 실제 피격처럼 처리되도록 GE를 만들어 적용한다.
		FGameplayEffectSpec Spec(GetDebugDamageEffect(), ASC->MakeEffectContext(), 1.f);
		Spec.SetSetByCallerMagnitude(DebugDamageName, Damage);
		ASC->ApplyGameplayEffectSpecToSelf(Spec);

		UE_LOG(Logknight, Log, TEXT("%s: after   Health=%.1f  Dead=%d  RaidPhase=%s  (damage %.1f)"),
			CommandName, ASC->GetNumericAttribute(UknightCoreAttributeSet::GetHealthAttribute()),
			Combat && Combat->IsDead(), PhaseName(World), Damage);
	}

	// knight.Player.Kill
	static void KillCommand(const TArray<FString>& Args, UWorld* World)
	{
		ApplyDebugDamage(World, -1.f, TEXT("knight.Player.Kill"));
	}

	// knight.Player.Damage <양>. 양이 없거나 0 이하면 경고만 출력한다.
	static void DamageCommand(const TArray<FString>& Args, UWorld* World)
	{
		const float Amount = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 0.f;
		if (Amount <= 0.f)
		{
			UE_LOG(Logknight, Warning, TEXT("knight.Player.Damage <Amount>: Amount must be above zero."));
			return;
		}
		ApplyDebugDamage(World, Amount, TEXT("knight.Player.Damage"));
	}

	// 첫 번째 플레이어 폰. 없으면 경고를 남기고 null.
	static APawn* GetPlayerPawn(UWorld* World, const TCHAR* CommandName)
	{
		const APlayerController* const PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* const Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			UE_LOG(Logknight, Warning, TEXT("%s: no player pawn."), CommandName);
		}
		return Pawn;
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
			UE_LOG(Logknight, Log, TEXT("%s: Mode=%s  Run=%d  MaxSpeed=%.1f  WeightFactor=%.2f"),
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
			UE_LOG(Logknight, Warning, TEXT("%s <StatName> <Value>: e.g. knight.Player.SetStat Weight 100"), CommandName);
			return;
		}
		APawn* const Pawn = GetPlayerPawn(World, CommandName);
		UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		if (!ASC)
		{
			UE_LOG(Logknight, Warning, TEXT("%s: the player has no ability system."), CommandName);
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
			UE_LOG(Logknight, Warning, TEXT("%s: no attribute named '%s' on the player."), CommandName, *Args[0]);
			return;
		}

		const float OldValue = ASC->GetNumericAttribute(Found);
		ASC->SetNumericAttributeBase(Found, FCString::Atof(*Args[1]));
		UE_LOG(Logknight, Log, TEXT("%s: %s %.2f -> %.2f"), CommandName, *Found.GetName(), OldValue, ASC->GetNumericAttribute(Found));
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
			UE_LOG(Logknight, Warning, TEXT("%s: the player does not use knightMovementComponent."), CommandName);
			return;
		}
		const bool bRun = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) != 0 : !Movement->GetWantsToRun();
		Movement->SetWantsToRun(bRun);
		LogMovement(Character, CommandName);
	}

	// 콘솔 명령 등록. ECVF_Cheat라 개발 빌드에서만 동작한다.
	static FAutoConsoleCommandWithWorldAndArgs KillCmd(
		TEXT("knight.Player.Kill"),
		TEXT("knight.Player.Kill. Deals the player's remaining Health as damage through GAS, exercising the real death path."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KillCommand),
		ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs DamageCmd(
		TEXT("knight.Player.Damage"),
		TEXT("knight.Player.Damage <Amount>. Deals Amount damage to the player through GAS."),
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
}
