#include "AbilitySystem/Abilities/knightGameplayAbility_HitReact.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_StanceSwitch.h"
#include "Character/knightMovementComponent.h"
#include "GameFramework/Character.h"
#include "Camera/CameraShakeBase.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "System/knightGameplayTags.h"
#include "Monster/knightMonsterGuardPushComponent.h"
#include "knight.h"

UAnimMontage* FknightDirectionalMontages::Pick(const FVector& AvatarForward, const FVector& AvatarRight, const FVector& ToInstigator) const
{
	const FVector Direction = ToInstigator.GetSafeNormal2D();
	const float ForwardDot = FVector::DotProduct(AvatarForward.GetSafeNormal2D(), Direction);
	const float RightDot = FVector::DotProduct(AvatarRight.GetSafeNormal2D(), Direction);

	UAnimMontage* Chosen = nullptr;
	if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
	{
		Chosen = ForwardDot >= 0.f ? Front.Get() : Back.Get();
	}
	else
	{
		Chosen = RightDot >= 0.f ? Right.Get() : Left.Get();
	}
	return Chosen ? Chosen : Front.Get();
}

UknightGameplayAbility_HitReact::UknightGameplayAbility_HitReact(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationGroup = EknightAbilityActivationGroup::Independent;
	bRetriggerInstancedAbility = true;

	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_HitReact));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);

	FGameplayTagContainer Interrupted;
	Interrupted.AddTag(knightGameplayTags::Ability_Attack);
	Interrupted.AddTag(knightGameplayTags::Ability_Guard);
	Interrupted.AddTag(knightGameplayTags::Ability_Boost_Dash);
	Interrupted.AddTag(knightGameplayTags::Ability_Boost_Flight);
	CancelAbilitiesWithTag.AppendTags(Interrupted);
	BlockAbilitiesWithTag.AppendTags(Interrupted);

	// (2026-11-02) Event.GuardBroken: 몬스터의 가드 브레이크 공격을 막았다(UknightMonsterGuardPushComponent).
	for (const FGameplayTag& EventTag : { knightGameplayTags::Event_Damaged.GetTag(), knightGameplayTags::Event_PoiseBroken.GetTag(), knightGameplayTags::Event_Parried.GetTag(),
		knightMonsterGuardPush::Event_GuardBroken.GetTag() })
	{
		FAbilityTriggerData Trigger;
		Trigger.TriggerTag = EventTag;
		Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(Trigger);
	}
}

bool UknightGameplayAbility_HitReact::ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* Payload) const
{
	if (!Super::ShouldAbilityRespondToEvent(ActorInfo, Payload) || !Payload || !ActorInfo)
	{
		return false;
	}
	// 그로기(기절) 중에는 맞아도 피해만 받고 다른 반응 없이 기절이 이어진다(2026-10-16 사용자 결정).
	if (const UAbilitySystemComponent* const OwnerASC = ActorInfo->AbilitySystemComponent.Get();
		OwnerASC && OwnerASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Groggy))
	{
		return false;
	}
	if (Payload->EventTag != knightGameplayTags::Event_Damaged)
	{
		return true;
	}
	// Flinch filters. Stagger, knockdown and parry recoil always play.
	const UAbilitySystemComponent* const ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC || Payload->EventMagnitude < MinFlinchDamage)
	{
		return false;
	}
	return !ASC->HasMatchingGameplayTag(knightGameplayTags::State_SuperArmor)
		&& !ASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Stagger)
		&& !ASC->HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown);
}

EknightHitReactType UknightGameplayAbility_HitReact::ResolveType(const FGameplayEventData& Payload) const
{
	if (Payload.EventTag == knightGameplayTags::Event_PoiseBroken)
	{
		// (2026-10-16) 그로기 반복 몽타주가 있으면 그로기가 0이 될 때 기절한다.
		if (GroggyLoopMontage)
		{
			return EknightHitReactType::Groggy;
		}
		return Payload.EventMagnitude >= KnockdownPoiseDamage ? EknightHitReactType::Knockdown : EknightHitReactType::Stagger;
	}
	if (Payload.EventTag == knightGameplayTags::Event_Parried)
	{
		return EknightHitReactType::Stagger;
	}
	if (Payload.EventTag == knightMonsterGuardPush::Event_GuardBroken)
	{
		return EknightHitReactType::GuardBreak;
	}
	return EknightHitReactType::Flinch;
}

const FknightDirectionalMontages& UknightGameplayAbility_HitReact::GetMontages(EknightHitReactType Type) const
{
	switch (Type)
	{
	case EknightHitReactType::Stagger:
		return StaggerMontages;
	case EknightHitReactType::Knockdown:
		return KnockdownMontages;
	case EknightHitReactType::GuardBreak:
		return GuardBreakMontages.Front ? GuardBreakMontages : StaggerMontages;
	default:
		return FlinchMontages;
	}
}

FGameplayTag UknightGameplayAbility_HitReact::GetStateTag(EknightHitReactType Type)
{
	switch (Type)
	{
	case EknightHitReactType::Stagger:
	case EknightHitReactType::GuardBreak:
		return knightGameplayTags::State_HitReact_Stagger;
	case EknightHitReactType::Knockdown:
		return knightGameplayTags::State_HitReact_Knockdown;
	case EknightHitReactType::Groggy:
		return knightGameplayTags::State_HitReact_Groggy;
	default:
		return knightGameplayTags::State_HitReact_Flinch;
	}
}

void UknightGameplayAbility_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* const Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!TriggerEventData || !Avatar || !ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveType = ResolveType(*TriggerEventData);
	ASC->AddLooseGameplayTag(GetStateTag(ActiveType));
	bStateTagApplied = true;

	const AActor* const EventInstigator = TriggerEventData->Instigator.Get();
	ApplyImpactFeel(*Avatar, EventInstigator);
	if (ActiveType == EknightHitReactType::Groggy)
	{
		StartGroggy();
		return;
	}
	if (ActiveType == EknightHitReactType::GuardBreak)
	{
		StartGuardBreak(*Avatar);
	}
	const FVector ToInstigator = EventInstigator ? EventInstigator->GetActorLocation() - Avatar->GetActorLocation() : Avatar->GetActorForwardVector();
	UAnimMontage* const Montage = GetMontages(ActiveType).Pick(Avatar->GetActorForwardVector(), Avatar->GetActorRightVector(), ToInstigator);
	if (!Montage)
	{
		UE_LOG(LogknightAbility, Verbose, TEXT("%s: no montage for hit reaction %d."), *GetName(), static_cast<int32>(ActiveType));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const float PlayRate = ActiveType == EknightHitReactType::GuardBreak ? GuardBreakPlayRate : 1.f;
	UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, PlayRate);
	Task->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->OnCancelled.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->ReadyForActivation();
}

const FknightHitImpactFeel& UknightGameplayAbility_HitReact::GetImpactFeel(EknightHitReactType Type) const
{
	switch (Type)
	{
	case EknightHitReactType::Stagger:
	case EknightHitReactType::Groggy:
		return StaggerImpact;
	case EknightHitReactType::Knockdown:
		return KnockdownImpact;
	case EknightHitReactType::GuardBreak:
		return GuardBreakImpact;
	default:
		return FlinchImpact;
	}
}

void UknightGameplayAbility_HitReact::StartGuardBreak(AActor& Avatar)
{
	// 비틀거리는 동안 걸어 나가지 않게 이동 입력을 잠근다(그로기와 같은 잠금, EndAbility가 푼다).
	const ACharacter* const Character = Cast<ACharacter>(&Avatar);
	if (UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr)
	{
		Movement->SetMoveInputLocked(true);
		bGroggyMoveLockApplied = true;
	}
	if (GuardBreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(&Avatar, GuardBreakSound, Avatar.GetActorLocation(), GuardBreakSoundVolume);
	}
	UE_LOG(LogknightCombat, Log, TEXT("%s: guard broken."), *Avatar.GetName());
}

UCurveFloat* UknightGameplayAbility_HitReact::GetKnockbackStrengthCurve(const float Power)
{
	const float N = FMath::Clamp(Power, 1.f, 8.f);
	if (N <= 1.01f)
	{
		return nullptr;
	}
	const int32 Key = FMath::RoundToInt(N * 100.f);
	if (const TObjectPtr<UCurveFloat>* const Found = KnockbackCurves.Find(Key))
	{
		return Found->Get();
	}
	// 위치 1 - (1 - t)^N 의 기울기 = 속도 N(1 - t)^(N - 1). 0..1 평균이 1이라 거리 = 세기(평균 속도) × 시간 그대로.
	// 직선 보간이라 촘촘히 찍는다. 끝(t = 1)은 0이고, 마지막은 FinishVelocity(0)로 딱 멈춘다.
	UCurveFloat* const Curve = NewObject<UCurveFloat>(this, NAME_None, RF_Transient);
	constexpr int32 Samples = 24;
	for (int32 Index = 0; Index <= Samples; ++Index)
	{
		const float T = static_cast<float>(Index) / Samples;
		const FKeyHandle Handle = Curve->FloatCurve.AddKey(T, N * FMath::Pow(1.f - T, N - 1.f));
		Curve->FloatCurve.SetKeyInterpMode(Handle, RCIM_Linear);
	}
	KnockbackCurves.Add(Key, Curve);
	return Curve;
}

void UknightGameplayAbility_HitReact::ApplyImpactFeel(AActor& Avatar, const AActor* EventInstigator)
{
	const FknightHitImpactFeel& Feel = GetImpactFeel(ActiveType);

	// 넉백: 때린 쪽에서 멀어지는 방향. 몽타주 루트 모션에 더해지도록 더하기(Additive)로 건다.
	if (Feel.KnockbackDistance > 0.f && EventInstigator)
	{
		const FVector Away = (Avatar.GetActorLocation() - EventInstigator->GetActorLocation()).GetSafeNormal2D();
		if (!Away.IsNearlyZero())
		{
			const float Time = FMath::Max(Feel.KnockbackTime, 0.02f);
			// (2026-11-04) 등속 대신 Ease-Out: 처음에 최고 속도로 튕겨 나가 끝에서 급제동한다. 1이면 예전처럼 등속(커브 없음).
			UCurveFloat* const Curve = GetKnockbackStrengthCurve(Feel.KnockbackEasePower);
			UAbilityTask_ApplyRootMotionConstantForce* const Task = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
				this, TEXT("HitKnockback"), Away, Feel.KnockbackDistance / Time, Time, true, Curve,
				ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f, true);
			Task->ReadyForActivation();
			UE_LOG(LogknightAbility, Verbose, TEXT("%s: knockback %.0fcm in %.2fs, ease power %.1f (start speed %.0f cm/s)"), *GetName(),
				Feel.KnockbackDistance, Time, Feel.KnockbackEasePower, Feel.KnockbackDistance / Time * (Curve ? FMath::Max(Feel.KnockbackEasePower, 1.f) : 1.f));
		}
	}

	// 흔들림·슬로는 플레이어가 맞았을 때만.
	const APawn* const Pawn = Cast<APawn>(&Avatar);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}
	if (Feel.CameraShake && Feel.CameraShakeScale > 0.f)
	{
		if (APlayerController* const PC = Cast<APlayerController>(Pawn->GetController()))
		{
			PC->ClientStartCameraShake(Feel.CameraShake, Feel.CameraShakeScale);
		}
	}
	UWorld* const World = Avatar.GetWorld();
	if (World && Feel.SlowMoDuration > 0.f && Feel.SlowMoDilation < 1.f)
	{
		UGameplayStatics::SetGlobalTimeDilation(World, Feel.SlowMoDilation);
		// 타이머도 느려진 시간으로 세므로, 실제 시간 SlowMoDuration에 맞추려고 배율을 곱한다.
		FTimerHandle RestoreTimer;
		World->GetTimerManager().SetTimer(RestoreTimer, FTimerDelegate::CreateWeakLambda(World, [World]()
		{
			UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
		}), Feel.SlowMoDuration * Feel.SlowMoDilation, false);
	}
}

void UknightGameplayAbility_HitReact::OnMontageFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UknightGameplayAbility_HitReact::StartGroggy()
{
	// 기절하는 동안 이동 입력을 잠근다(몽타주에 루트 모션이 없어도 걸어 나가지 않게).
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr)
	{
		Movement->SetMoveInputLocked(true);
		bGroggyMoveLockApplied = true;
	}
	UE_LOG(LogknightAbility, Log, TEXT("%s: groggy (stun %.1fs)."), *GetNameSafe(GetAvatarActorFromActorInfo()), GroggyDuration);
	if (GroggyStartMontage)
	{
		GroggyPhase = EGroggyPhase::Start;
		PlayGroggyMontage(GroggyStartMontage);
	}
	else
	{
		EnterGroggyLoop();
	}
}

void UknightGameplayAbility_HitReact::EnterGroggyLoop()
{
	GroggyPhase = EGroggyPhase::Loop;
	GroggyLoopStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	PlayGroggyMontage(GroggyLoopMontage);
	// 반복 시간은 시작 동작이 끝난 뒤부터 센다.
	UAbilityTask_WaitDelay* const Delay = UAbilityTask_WaitDelay::WaitDelay(this, GroggyDuration);
	Delay->OnFinish.AddDynamic(this, &ThisClass::OnGroggyTimeUp);
	Delay->ReadyForActivation();
}

void UknightGameplayAbility_HitReact::PlayGroggyMontage(UAnimMontage* Montage)
{
	// 앞 몽타주의 태스크를 먼저 끝내 콜백을 끊는다. 다음 몽타주가 앞 것을 끊어도 '중단'으로 처리되지 않게.
	if (UAbilityTask_PlayMontageAndWait* const OldTask = GroggyMontageTask.Get())
	{
		OldTask->EndTask();
	}
	UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
	Task->OnBlendOut.AddDynamic(this, &ThisClass::OnGroggyMontageDone);
	Task->OnCompleted.AddDynamic(this, &ThisClass::OnGroggyMontageDone);
	// 다른 몽타주에 끊기면(예상 밖) 기절을 끝낸다.
	Task->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->OnCancelled.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->ReadyForActivation();
	GroggyMontageTask = Task;
}

void UknightGameplayAbility_HitReact::OnGroggyMontageDone()
{
	switch (GroggyPhase)
	{
	case EGroggyPhase::Start:
		EnterGroggyLoop();
		break;
	case EGroggyPhase::Loop:
	{
		// 반복 몽타주가 시간보다 짧으면 다시 재생한다. 남은 시간이 블렌드 아웃보다 짧으면 다시 틀지 않고 바로 끝 동작으로 넘어간다
		// (반복 애니 길이가 GroggyDuration과 같을 때 끝나기 직전에 처음부터 다시 시작하지 않게).
		const float Elapsed = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - GroggyLoopStartTime) : GroggyDuration;
		const float BlendOut = GroggyLoopMontage ? GroggyLoopMontage->BlendOut.GetBlendTime() : 0.f;
		if (GroggyDuration - Elapsed <= BlendOut + KINDA_SMALL_NUMBER)
		{
			OnGroggyTimeUp();
		}
		else
		{
			PlayGroggyMontage(GroggyLoopMontage);
		}
		break;
	}
	default:
		OnMontageFinished();
		break;
	}
}

void UknightGameplayAbility_HitReact::OnGroggyTimeUp()
{
	// 반복이 먼저 끝나 이미 끝 동작으로 넘어갔으면 시간 알림은 무시한다.
	if (GroggyPhase != EGroggyPhase::Loop)
	{
		return;
	}
	GroggyPhase = EGroggyPhase::End;
	// 끝 동작은 지금 스탠스에 맞춘다(검을 뽑으면 한손이 기본).
	const bool bTwoHanded = UknightGameplayAbility_StanceSwitch::IsTwoHanded(GetAbilitySystemComponentFromActorInfo());
	UAnimMontage* const EndMontage = !bTwoHanded && GroggyEndMontageOneHanded ? GroggyEndMontageOneHanded.Get() : GroggyEndMontage.Get();
	if (EndMontage)
	{
		PlayGroggyMontage(EndMontage);
	}
	else
	{
		OnMontageFinished();
	}
}

void UknightGameplayAbility_HitReact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	GroggyPhase = EGroggyPhase::None;
	GroggyMontageTask.Reset();
	if (bGroggyMoveLockApplied)
	{
		bGroggyMoveLockApplied = false;
		const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (UknightMovementComponent* const Movement = Character ? Cast<UknightMovementComponent>(Character->GetCharacterMovement()) : nullptr)
		{
			Movement->SetMoveInputLocked(false);
		}
	}
	if (bStateTagApplied)
	{
		bStateTagApplied = false;
		if (UAbilitySystemComponent* const ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->RemoveLooseGameplayTag(GetStateTag(ActiveType));
			// 가드 브레이크는 그로기가 무너진 게 아니라서 그로기를 채우지 않는다.
			if (ActiveType != EknightHitReactType::Flinch && ActiveType != EknightHitReactType::GuardBreak
				&& PoiseRecoveryEffect && !ASC->HasMatchingGameplayTag(knightGameplayTags::State_Dead))
			{
				ASC->ApplyGameplayEffectToSelf(PoiseRecoveryEffect->GetDefaultObject<UGameplayEffect>(), 1.f, ASC->MakeEffectContext());
			}
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
