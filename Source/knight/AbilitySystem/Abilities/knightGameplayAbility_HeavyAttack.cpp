#include "AbilitySystem/Abilities/knightGameplayAbility_HeavyAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/knightAnimNotifyState_MeleeTrace.h"
#include "Components/AudioComponent.h"
#include "Combat/knightCombatLibrary.h"
#include "Combat/knightGroggyFeedbackComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Input/knightInputBufferComponent.h"
#include "MotionWarpingComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace
{
	// 락온 상태가 아닐 때 공격하면 화면 중앙(카메라/컨트롤러 수평 방향)을 바라보게 한다.
	void FaceScreenCenterForHeavyAttack(ACharacter* Character, const UAbilitySystemComponent* ASC)
	{
		if (!Character || (ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn)))
		{
			return;
		}
		if (const AController* const Controller = Character->GetController())
		{
			Character->SetActorRotation(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
		}
	}
}

UknightGameplayAbility_HeavyAttack::UknightGameplayAbility_HeavyAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EknightAbilityActivationPolicy::OnInputTriggered;
	// Independent while deciding, so the heavy step it starts is not blocked by this ability itself.
	ActivationGroup = EknightAbilityActivationGroup::Independent;
	SetAssetTags(FGameplayTagContainer(knightGameplayTags::Ability_Attack_Charge));
	ActivationBlockedTags.AddTag(knightGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(knightGameplayTags::State_HitReact);
	LoopHitParams.MotionValue = 15.f;
	LoopHitParams.PoiseDamage = 5.f;
	// (2026-10-26 사용자 결정) 80/40 → 55/25(짧게 누르면 강공격 1~2타 수준). 풀차지는 FullChargeMotionValue·FullChargePoiseDamage(120/60).
	ReleaseHitParams.MotionValue = 55.f;
	ReleaseHitParams.PoiseDamage = 25.f;
	// 차지를 시작할 때 쓰는 행동 에너지. 탭 강공격 콤보는 각 강공격 단계(일반 공격 BP)의 값을 쓴다. 2026-10-09 임시값.
	// (2026-10-26 사용자 결정) 시작 비용 20 → 0. 대신 모으는 동안 ChargeStaminaCost, 해방할 때 ReleaseStaminaCost를 쓴다.
	StaminaCost = 0.f;
}

void UknightGameplayAbility_HeavyAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	Phase = EPhase::Deciding;
	bReleased = false;
	ReleaseRateScale = 1.f;

	// 누르자마자 차지: 떼기 감시보다 먼저 차지를 시작해야, 이미 뗀 채 발동돼도 탭 콤보가 아니라 해방으로 간다.
	if (bChargeOnPress)
	{
		OnHoldThreshold();
		if (!IsActive())
		{
			return;
		}
	}

	UAbilityTask_WaitInputRelease* const Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	Release->OnRelease.AddDynamic(this, &ThisClass::OnReleased);
	Release->ReadyForActivation();

	if (!bChargeOnPress)
	{
		UAbilityTask_WaitDelay* const Hold = UAbilityTask_WaitDelay::WaitDelay(this, HoldThreshold);
		Hold->OnFinish.AddDynamic(this, &ThisClass::OnHoldThreshold);
		Hold->ReadyForActivation();
	}
}

void UknightGameplayAbility_HeavyAttack::OnReleased(float TimeHeld)
{
	bReleased = true;
	if (Phase == EPhase::Deciding)
	{
		// Tap: heavy combo step, retried for a short while if the current swing cannot be replaced yet.
		Phase = EPhase::TapBuffer;
		const UWorld* const World = GetWorld();
		TapDeadline = (World ? World->GetTimeSeconds() : 0.0) + TapBufferTime;
		TryHeavyStep();
	}
	else if (Phase == EPhase::WaitingCharge)
	{
		if (bChargeOnPress)
		{
			// 누르자마자 차지: 앞 공격 때문에 아직 못 모았으면 TapBufferTime 동안 더 보고(TryStartChargeWhileHeld),
			// 모을 수 있게 되는 순간 바로 가장 약한 해방을 한다.
			const UWorld* const World = GetWorld();
			TapDeadline = (World ? World->GetTimeSeconds() : 0.0) + TapBufferTime;
			return;
		}
		// (2026-10-28) 차지를 기다리다 떼면 아무것도 하지 않는다(예전에 HoldThreshold에서 차지를 못 시작했을 때와 같다).
		Finish(false);
	}
	else if (Phase == EPhase::Charging)
	{
		StopChargeEffects();
		// (2026-10-26) 마지막 틱 뒤에 모은 몫까지 쓰고 모으기 소모를 멈춘 뒤, 해방(돌진) 비용을 쓴다. 모자라도 쓴다(가불).
		TickChargeStamina();
		Phase = EPhase::Releasing;
		UWorld* const World = GetWorld();
		if (World)
		{
			World->GetTimerManager().ClearTimer(ChargeStaminaTimer);
		}
		ApplyStaminaCost(ReleaseStaminaCost);

		// (2026-10-26 사용자 결정) 해방부터는 일반 공격과 같은 캔슬 규칙을 받는다(교체 가능 배타 그룹, 클래스 설명 참고).
		// 모으는 동안 막아 둔 다른 공격도 풀어서, 다음 공격은 그룹 규칙(캔슬 창부터)대로 나간다.
		ChangeActivationGroupWhileActive(EknightAbilityActivationGroup::Exclusive_Replaceable);
		if (bAttackBlockApplied)
		{
			bAttackBlockApplied = false;
			if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo())
			{
				ASC->UnBlockAbilitiesWithTags(FGameplayTagContainer(knightGameplayTags::Ability_Attack));
			}
		}
		if (const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()); Character && World)
		{
			ReleaseMovePress.Reset(*Character);
			World->GetTimerManager().SetTimer(ReleaseMoveCancelTimer, this, &ThisClass::CheckReleaseMoveCancel, 0.02f, true);
		}

		const double Held = (World ? World->GetTimeSeconds() : ChargeStartTime) - ChargeStartTime;
		const float FullChargeTime = GetStanceMaxChargeTime();
		const float Alpha = FMath::Clamp(static_cast<float>(Held) / FullChargeTime, 0.f, 1.f);
		LastReleaseChargeAlpha = Alpha;
		// (2026-10-26) 풀차지 값(FullCharge*)이 있으면 ReleaseHitParams 값에서 그 값까지, 없으면 MaxChargeMultiplier배까지 오른다.
		const float MotionScale = FMath::Lerp(1.f, FullChargeMotionValue > 0.f && ReleaseHitParams.MotionValue > 0.f
			? FullChargeMotionValue / ReleaseHitParams.MotionValue : MaxChargeMultiplier, Alpha);
		const float PoiseScale = FMath::Lerp(1.f, FullChargePoiseDamage > 0.f && ReleaseHitParams.PoiseDamage > 0.f
			? FullChargePoiseDamage / ReleaseHitParams.PoiseDamage : MaxChargeMultiplier, Alpha);
		// (2026-10-20) 스탠스 보정(한손/양손)을 먼저 곱하고 차지 배율을 곱한다.
		ActiveHitParams = GetStanceScaledHitParams(ReleaseHitParams);
		ActiveHitParams.MotionValue *= MotionScale;
		ActiveHitParams.PoiseDamage *= PoiseScale;
		UE_LOG(LogknightAbility, Log, TEXT("%s: released after %.2fs charge (full at %.2fs), MV x%.2f (%.0f), poise x%.2f (%.0f), stamina %.1f + %.1f"),
			*GetName(), Held, FullChargeTime, MotionScale, ActiveHitParams.MotionValue, PoiseScale, ActiveHitParams.PoiseDamage, ChargeStaminaSpent, ReleaseStaminaCost);
		if (ChargeReleaseMontage)
		{
			AActor* const Avatar = GetAvatarActorFromActorInfo();
			FaceScreenCenterForHeavyAttack(Cast<ACharacter>(Avatar), GetAbilitySystemComponentFromActorInfo());
			// (2026-10-27 사용자 결정) 차지량에 따라 해방 모션이 빨라지고(풀차지 FullChargeReleaseRate배), 락온 중이면 돌진 거리도 는다.
			ReleaseRateScale = FMath::Lerp(1.f, FullChargeReleaseRate, Alpha);
			const UAbilitySystemComponent* const OwnerASC = GetAbilitySystemComponentFromActorInfo();
			const bool bLockedOn = OwnerASC && OwnerASC->HasMatchingGameplayTag(knightGameplayTags::State_LockedOn);
			const float WarpDistance = bLockedOn ? FMath::Lerp(MaxWarpDistance, FMath::Max(MaxWarpDistance, LockOnFullChargeWarpDistance), Alpha)
				: MaxWarpDistance;
			FVector WarpLocation;
			FRotator WarpRotation;
			float WarpTravel = 0.f;
			const bool bWarp = UknightCombatLibrary::SetAttackWarpTarget(Avatar, WarpTargetName, WarpDistance,
				WarpStopDistance, bUseSoftTarget, SoftTargetRange, SoftTargetAngle)
				&& UknightCombatLibrary::ComputeAttackWarp(Avatar, WarpDistance, WarpStopDistance,
					bUseSoftTarget, SoftTargetRange, SoftTargetAngle, WarpLocation, WarpRotation, WarpTravel);
			PlayMontage(ChargeReleaseMontage, TEXT("ChargeRelease"), false);
			// (2026-10-28 사용자 요청 '공격 전체에 잔상') 해방 시작부터 휘두르기 판정이 끝날 때까지 남긴다(락온·돌진과 상관없이).
			// 기계식 타이밍으로 실제 휘두르는 시각이 늦어지므로 시간이 아니라 몽타주 재생 위치(판정 끝)로 멈춘다(지면 균열과 같은 방식).
			if (bReleaseGhostTrail && bReleaseGhostTrailWholeSwing && Avatar)
			{
				float SwingEnd = 0.f;
				for (const FAnimNotifyEvent& Event : ChargeReleaseMontage->Notifies)
				{
					if (Event.NotifyStateClass && Event.NotifyStateClass->IsA<UknightAnimNotifyState_MeleeTrace>())
					{
						SwingEnd = FMath::Max(SwingEnd, Event.GetEndTriggerTime());
					}
				}
				if (SwingEnd > 0.f)
				{
					FknightGhostTrailSettings WholeSwingSettings = ReleaseGhostTrailSettings;
					// 대상 가까이 가도 끊지 않는다(남은 거리 검사가 늘 거짓이 되게).
					WholeSwingSettings.InRangeDistance = -1.f;
					const TWeakObjectPtr<ThisClass> WeakThis(this);
					// 시간은 넉넉히(몽타주 길이) 주고, 실제로는 판정 끝 위치를 지나면 멈춘다.
					AknightGhostTrailActor::StartGhostTrailForAbility(this, WholeSwingSettings, ChargeReleaseMontage->GetPlayLength(),
						Avatar->GetActorLocation(), [WeakThis, SwingEnd]()
						{
							const ThisClass* const Self = WeakThis.Get();
							const ACharacter* const Character = Self ? Cast<ACharacter>(Self->GetAvatarActorFromActorInfo()) : nullptr;
							const UAnimInstance* const Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
							return Anim && Anim->Montage_IsPlaying(Self->ChargeReleaseMontage)
								&& Anim->Montage_GetPosition(Self->ChargeReleaseMontage) >= SwingEnd;
						});
				}
			}
			if (bWarp && Avatar)
			{
				// 발사 몽타주와 같은 배속(행동 속도 x 해방 배속)을 넘겨야 돌진이 모션과 같이 끝난다. (2026-10-08)
				const float Rate = GetAttackSpeed() * ReleaseRateScale;
				// (2026-10-27) 먼 돌진이 순간 이동처럼 보이지 않게 거리만큼 시간을 늘린다(최소 0.25초, 휘두르기 직전까지만).
				const float LungeTime = FMath::Max(0.25f, WarpTravel / FMath::Max(ReleaseLungeSpeed, 1.f));
				TFunction<bool(FVector&)> TrackTarget;
				if (bTrackTargetDuringLunge)
				{
					// (2026-10-27 사용자 결정 '약간의 유도') 돌진하는 동안 대상의 지금 위치 앞으로 목표를 옮긴다.
					// 돌진을 시작한 자리에서 WarpDistance 밖으로는 가지 않는다(UknightGameplayAbility_MeleeAttack::UpdateTrackedWarpTarget과 같은 규칙).
					const FVector LungeStart = Avatar->GetActorLocation();
					TrackTarget = [WeakThis = TWeakObjectPtr<ThisClass>(this), LungeStart, WarpDistance](FVector& OutLocation)
					{
						ThisClass* const Self = WeakThis.Get();
						AActor* const Owner = Self ? Self->GetAvatarActorFromActorInfo() : nullptr;
						FRotator Rotation;
						float Travel = 0.f;
						if (!Owner || !UknightCombatLibrary::ComputeAttackWarp(Owner, WarpDistance + 10000.f, Self->WarpStopDistance,
							Self->bUseSoftTarget, Self->SoftTargetRange, Self->SoftTargetAngle, OutLocation, Rotation, Travel))
						{
							return false;
						}
						FVector FromStart = OutLocation - LungeStart;
						FromStart.Z = 0.f;
						if (FromStart.Size() > WarpDistance)
						{
							const FVector Clamped = LungeStart + FromStart.GetSafeNormal() * WarpDistance;
							OutLocation = FVector(Clamped.X, Clamped.Y, OutLocation.Z);
						}
						// 몽타주의 워프 구간(몸 돌리기)도 같은 지점을 보게 한다.
						if (UMotionWarpingComponent* const MotionWarping = Owner->FindComponentByClass<UMotionWarpingComponent>())
						{
							MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(Self->WarpTargetName, OutLocation, Rotation);
						}
						return true;
					};
				}
				UknightCombatLibrary::StartAttackLunge(this, ChargeReleaseMontage, Rate, WarpLocation, WarpTravel, LungeTime, 20.f, MoveTemp(TrackTarget));

				// (2026-10-27 사용자 요청) 돌진 잔상: 실제로 움직이는 동안(돌진 시작 ~ 휘두르기 직전)만 남긴다. 그 전 준비 동작에는 남기지 않는다.
				// 시작·길이는 StartAttackLunge와 같은 계산(워프 구간 끝에 도착하도록 LungeTime만큼).
				const float WindowSeconds = UknightCombatLibrary::GetWarpWindowEnd(ChargeReleaseMontage) / FMath::Max(Rate, 0.01f);
				if (bReleaseGhostTrail && !bReleaseGhostTrailWholeSwing && World && WindowSeconds > 0.f)
				{
					const float LungeDuration = FMath::Clamp(WindowSeconds, 0.05f, LungeTime);
					const float GhostDelay = WindowSeconds - LungeDuration;
					const auto StartGhost = [WeakThis = TWeakObjectPtr<ThisClass>(this), LungeDuration, WarpLocation]()
					{
						if (ThisClass* const Self = WeakThis.Get(); Self && Self->IsActive())
						{
							AknightGhostTrailActor::StartGhostTrailForAbility(Self, Self->ReleaseGhostTrailSettings, LungeDuration, WarpLocation);
						}
					};
					if (GhostDelay > KINDA_SMALL_NUMBER)
					{
						World->GetTimerManager().SetTimer(ReleaseGhostTimer, FTimerDelegate::CreateLambda(StartGhost), GhostDelay, false);
					}
					else
					{
						StartGhost();
					}
				}
			}
			if (bPlayChargeEffects && World)
			{
				// 기갑 타이밍(Windup 감속 + Lock 정지)·히트스탑·슬로모션으로 실제 휘두르기 시각이 늦춰지므로,
				// 단순 시간 타이머 대신 몽타주의 실제 MeleeTrace 구간(예: 0.47s~0.65s) 재생 위치를 추적해 검이 땅을 가르는 순간에 균열을 생성한다.
				ReleaseCrackTargetPosition = ReleaseGroundCrackDelay;
				for (const FAnimNotifyEvent& Event : ChargeReleaseMontage->Notifies)
				{
					if (Event.NotifyStateClass && Event.NotifyStateClass->IsA<UknightAnimNotifyState_MeleeTrace>())
					{
						const float TraceStart = Event.GetTriggerTime();
						const float TraceEnd = Event.GetEndTriggerTime();
						ReleaseCrackTargetPosition = FMath::Lerp(TraceStart, TraceEnd, FMath::Clamp(ReleaseGroundCrackSwingProgress, 0.f, 1.f));
						break;
					}
				}
				World->GetTimerManager().SetTimer(GroundCrackTimer, this, &ThisClass::CheckReleaseGroundCrackTiming, 0.015f, true);
			}
		}
		else
		{
			Finish(false);
		}
	}
}

void UknightGameplayAbility_HeavyAttack::TryHeavyStep()
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* const World = GetWorld();
	if (!ASC || !World || !IsActive())
	{
		return;
	}
	if (HeavyAbilityTag.IsValid() && ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(HeavyAbilityTag)))
	{
		Finish(false);
		return;
	}
	if (World->GetTimeSeconds() >= TapDeadline)
	{
		Finish(false);
		return;
	}
	World->GetTimerManager().SetTimer(TapTimer, this, &ThisClass::TryHeavyStep, 0.05f, false);
}

bool UknightGameplayAbility_HeavyAttack::CanStartCharge() const
{
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !ChargeLoopMontage)
	{
		return false;
	}
	// (2026-10-26) 시작 비용이 없어도 행동 에너지가 0보다 많아야 모으기 시작한다(0 이하면 행동 불가, 다른 행동과 같은 규칙).
	const FGameplayAttribute Gauge = UknightAttributeSet::GetBoostGaugeAttribute();
	if (ChargeStaminaCost + ReleaseStaminaCost > 0.f && ASC->HasAttributeSetForAttribute(Gauge) && ASC->GetNumericAttribute(Gauge) <= 0.f)
	{
		return false;
	}
	// (2026-10-14) 공격 후딜은 퀵스텝만 끊는다 → 그때는 캔슬 창이어도 차지를 시작하지 않는다(아래에서 실행 중인 공격에 걸림).
	// (2026-10-23) 후딜에 다음 공격을 허용하면(ASC bAttacksCancelAttackRecovery) 차지도 시작할 수 있다.
	const UknightAbilitySystemComponent* const KnightASC = GetKnightAbilitySystemComponentFromActorInfo();
	if (ASC->HasMatchingGameplayTag(knightGameplayTags::Window_Cancel) && !(KnightASC && KnightASC->IsAttackRecoveryBlockingAttacks()))
	{
		return true;
	}
	FGameplayTagContainer Busy;
	Busy.AddTag(knightGameplayTags::Ability_Guard);
	Busy.AddTag(knightGameplayTags::Ability_Boost_Dash);
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive() || !Spec.Ability || Spec.Ability == GetClass()->GetDefaultObject())
		{
			continue;
		}
		const FGameplayTagContainer& Tags = Spec.Ability->GetAssetTags();
		const bool bAttack = Tags.HasTag(knightGameplayTags::Ability_Attack) && !Tags.HasTagExact(knightGameplayTags::Ability_Attack_Charge);
		if (bAttack || Tags.HasAny(Busy))
		{
			return false;
		}
	}
	return true;
}

float UknightGameplayAbility_HeavyAttack::GetStanceMaxChargeTime() const
{
	const UknightAbilitySystemComponent* const ASC = GetKnightAbilitySystemComponentFromActorInfo();
	const float SpeedScale = bScaleChargeTimeByStance && ASC ? ASC->GetStanceModifiers().AttackSpeedScale : 1.f;
	return MaxChargeTime / FMath::Max(SpeedScale, 0.1f);
}

void UknightGameplayAbility_HeavyAttack::OnHoldThreshold()
{
	if (Phase != EPhase::Deciding || bReleased)
	{
		return;
	}
	if (CanStartCharge())
	{
		StartCharge();
		return;
	}
	// (2026-10-28 사용자 결정) 앞 공격이 아직 캔슬 창 전이면 바로 버리지 않고, 누르고 있는 동안 ChargeWaitTime까지 다시 본다.
	UWorld* const World = GetWorld();
	if (ChargeWaitTime <= 0.f || !World)
	{
		Finish(false);
		return;
	}
	Phase = EPhase::WaitingCharge;
	ChargeWaitStartTime = World->GetTimeSeconds();
	World->GetTimerManager().SetTimer(ChargeWaitTimer, this, &ThisClass::TryStartChargeWhileHeld, 0.05f, false);
}

void UknightGameplayAbility_HeavyAttack::TryStartChargeWhileHeld()
{
	UWorld* const World = GetWorld();
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!World || !ASC || !IsActive() || Phase != EPhase::WaitingCharge)
	{
		return;
	}
	// 맞거나 죽으면(발동을 막는 태그) 그만둔다. 맞을 때는 보통 피격 어빌리티가 이 어빌리티를 먼저 끊는다.
	if (ASC->HasAnyMatchingGameplayTags(ActivationBlockedTags))
	{
		Finish(false);
		return;
	}
	const double Waited = World->GetTimeSeconds() - ChargeWaitStartTime;
	if (CanStartCharge())
	{
		// 다음 플레이 로그에서 '기다렸다가 시작한 차지'를 셀 수 있게 남긴다.
		UE_LOG(LogknightAbility, Log, TEXT("%s: charge started after waiting %.2fs while held"), *GetName(), Waited);
		StartCharge();
		// 누르자마자 차지에서 기다리는 동안 이미 뗐으면 모으지 않고 바로 해방한다.
		if (bReleased && Phase == EPhase::Charging)
		{
			OnReleased(0.f);
		}
		return;
	}
	if (bReleased && World->GetTimeSeconds() >= TapDeadline)
	{
		UE_LOG(LogknightAbility, Log, TEXT("%s: released while waiting, gave up after %.2fs"), *GetName(), Waited);
		Finish(false);
		return;
	}
	if (Waited >= ChargeWaitTime)
	{
		UE_LOG(LogknightAbility, Log, TEXT("%s: charge wait timed out after %.2fs"), *GetName(), Waited);
		Finish(false);
		return;
	}
	World->GetTimerManager().SetTimer(ChargeWaitTimer, this, &ThisClass::TryStartChargeWhileHeld, 0.05f, false);
}

void UknightGameplayAbility_HeavyAttack::StartCharge()
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		Finish(true);
		return;
	}
	// 차지를 시작할 때 비용(행동 에너지 StaminaCost)을 낸다. 탭으로 끝나면 이 어빌리티는 비용을 내지 않는다. (2026-10-09)
	if (!CommitAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
	{
		Finish(true);
		return;
	}
	Phase = EPhase::Charging;
	// 락온 중이 아니면 화면 중앙(카메라 방향)을 바로 본다. 탭 콤보는 일반 공격 어빌리티가 같은 처리를 한다.
	FaceScreenCenterForHeavyAttack(Cast<ACharacter>(GetAvatarActorFromActorInfo()), ASC);
	const UWorld* const World = GetWorld();
	ChargeStartTime = World ? World->GetTimeSeconds() : 0.0;
	ActiveHitParams = GetStanceScaledHitParams(LoopHitParams);

	FGameplayTagContainer Interrupt;
	Interrupt.AddTag(knightGameplayTags::Ability_Attack);
	Interrupt.AddTag(knightGameplayTags::Ability_Guard);
	Interrupt.AddTag(knightGameplayTags::Ability_Boost_Dash);
	ASC->CancelAbilities(&Interrupt, nullptr, this);
	FGameplayTagContainer Blocked(knightGameplayTags::Ability_Attack);
	ASC->BlockAbilitiesWithTags(Blocked);
	bAttackBlockApplied = true;
	ASC->AddLooseGameplayTag(knightGameplayTags::State_Charging);
	bChargeStateApplied = true;

	// (2026-10-26) 모으는 동안 행동 에너지를 조금씩 쓴다(TickChargeStamina).
	ChargeStaminaSpent = 0.f;
	LastChargeStaminaTime = ChargeStartTime;
	if (ChargeStaminaCost > 0.f && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(ChargeStaminaTimer, this, &ThisClass::TickChargeStamina, 0.1f, true);
	}

	UAbilityTask_WaitGameplayEvent* const HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, knightGameplayTags::Event_Melee_Hit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &ThisClass::OnMeleeHitEvent);
	HitTask->ReadyForActivation();

	if (bPlayChargeEffects)
	{
		StartChargeEffects();
	}
	// (2026-10-28) 차지 유지 루프는 이펙트 설정과 따로 켠다. 시작 소리(몽타주 노티파이)가 잦아들 무렵 이어 받는다.
	if (ChargeLoopSound && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(ChargeLoopSoundTimer, this, &ThisClass::StartChargeLoopSound,
			FMath::Max(ChargeLoopSoundDelay, 0.01f), false);
	}

	if (ChargeStartMontage)
	{
		PlayMontage(ChargeStartMontage, TEXT("ChargeStart"), false);
	}
	else
	{
		PlayLoop();
	}
}

void UknightGameplayAbility_HeavyAttack::StartChargeEffects()
{
	StopChargeEffects();
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* const CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (!Character || !CharacterMesh)
	{
		return;
	}

	UNiagaraSystem* AuraAsset = ChargeAuraEffect;
	if (!AuraAsset)
	{
		AuraAsset = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Knight/Creatures/Shared/Effects/NS_Monster_WindStreak.NS_Monster_WindStreak"));
	}
	if (AuraAsset)
	{
		ActiveChargeAuraFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
			AuraAsset, CharacterMesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true, true);
	}

	UMeshComponent* WeaponMesh = nullptr;
	TArray<UMeshComponent*> Meshes;
	Character->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* const Mesh : Meshes)
	{
		if (Mesh != CharacterMesh && Mesh->ComponentHasTag(TEXT("Weapon")))
		{
			WeaponMesh = Mesh;
			break;
		}
	}

	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	const bool bTwoHanded = ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Stance_TwoHanded);
	UNiagaraSystem* SwordAsset = bTwoHanded ? TwoHandedChargeSwordEffect.Get() : OneHandedChargeSwordEffect.Get();
	if (!SwordAsset)
	{
		SwordAsset = LoadObject<UNiagaraSystem>(nullptr, bTwoHanded
			? TEXT("/Game/SlashTrail_SoftTofu/Niagara/Fire/NS_Sword_Fire.NS_Sword_Fire")
			: TEXT("/Game/SlashTrail_SoftTofu/Niagara/Scifi/NS_Sword_Scifi.NS_Sword_Scifi"));
	}
	if (SwordAsset)
	{
		UMeshComponent* const AttachTarget = WeaponMesh ? WeaponMesh : CharacterMesh;
		const FName AttachSocket = WeaponMesh ? NAME_None : FName(TEXT("weapon_r"));
		ActiveChargeSwordFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
			SwordAsset, AttachTarget, AttachSocket, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true, true);
	}

	if (UWorld* const World = GetWorld())
	{
		// (2026-10-28) 풀차지 시간은 스탠스 공격 속도를 따른다(GetStanceMaxChargeTime).
		if (const float FullChargeTime = GetStanceMaxChargeTime(); FullChargeTime > 0.f)
		{
			World->GetTimerManager().SetTimer(FullChargeTimer, this, &ThisClass::OnFullChargeReached, FullChargeTime, false);
		}
	}
}

void UknightGameplayAbility_HeavyAttack::StartChargeLoopSound()
{
	if (Phase != EPhase::Charging || !IsActive() || !ChargeLoopSound)
	{
		return;
	}
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}
	if (ActiveChargeLoopAudio)
	{
		ActiveChargeLoopAudio->Stop();
	}
	ActiveChargeLoopAudio = UGameplayStatics::SpawnSoundAttached(ChargeLoopSound, Mesh, NAME_None, FVector::ZeroVector,
		EAttachLocation::SnapToTarget, true, ChargeLoopVolume);
	if (ActiveChargeLoopAudio)
	{
		ActiveChargeLoopAudio->FadeIn(ChargeLoopFadeIn);
	}
}

void UknightGameplayAbility_HeavyAttack::StopChargeLoopSound()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeLoopSoundTimer);
	}
	if (ActiveChargeLoopAudio)
	{
		ActiveChargeLoopAudio->FadeOut(ChargeLoopFadeOut, 0.f);
		ActiveChargeLoopAudio = nullptr;
	}
}

void UknightGameplayAbility_HeavyAttack::StopChargeEffects()
{
	StopChargeLoopSound();
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FullChargeTimer);
	}
	if (ActiveChargeAuraFX)
	{
		ActiveChargeAuraFX->Deactivate();
		ActiveChargeAuraFX = nullptr;
	}
	if (ActiveChargeSwordFX)
	{
		ActiveChargeSwordFX->Deactivate();
		ActiveChargeSwordFX = nullptr;
	}
}

void UknightGameplayAbility_HeavyAttack::OnFullChargeReached()
{
	if (Phase != EPhase::Charging || !IsActive())
	{
		return;
	}
	if (ChargeFullSound)
	{
		if (const AActor* const Avatar = GetAvatarActorFromActorInfo())
		{
			UGameplayStatics::PlaySoundAtLocation(Avatar, ChargeFullSound, Avatar->GetActorLocation());
		}
	}
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* const CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (!Character || !CharacterMesh)
	{
		return;
	}

	UMeshComponent* WeaponMesh = nullptr;
	TArray<UMeshComponent*> Meshes;
	Character->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* const Mesh : Meshes)
	{
		if (Mesh != CharacterMesh && Mesh->ComponentHasTag(TEXT("Weapon")))
		{
			WeaponMesh = Mesh;
			break;
		}
	}
	UMeshComponent* const AttachTarget = WeaponMesh ? WeaponMesh : CharacterMesh;
	const FName AttachSocket = WeaponMesh ? NAME_None : FName(TEXT("weapon_r"));

	UNiagaraSystem* BurstAsset = FullChargeBurstEffect;
	if (!BurstAsset)
	{
		BurstAsset = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Knight/Abilities/Cues/NS_Knight_ParrySpark.NS_Knight_ParrySpark"));
	}
	if (BurstAsset)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			BurstAsset, AttachTarget, AttachSocket, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true, true);
	}

	UNiagaraSystem* DistortionAsset = FullChargeDistortionEffect;
	if (!DistortionAsset)
	{
		DistortionAsset = LoadObject<UNiagaraSystem>(nullptr,
			TEXT("/Game/SlashTrail_SoftTofu/Niagara/Distortion_Only/NS_Hit_Distortion_Once.NS_Hit_Distortion_Once"));
	}
	if (DistortionAsset)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			DistortionAsset, AttachTarget, AttachSocket, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true, true);
	}
}

void UknightGameplayAbility_HeavyAttack::CheckReleaseGroundCrackTiming()
{
	UWorld* const World = GetWorld();
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UAnimInstance* const Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!IsActive() || Phase != EPhase::Releasing || !ChargeReleaseMontage || !Anim || !Anim->Montage_IsPlaying(ChargeReleaseMontage))
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(GroundCrackTimer);
		}
		return;
	}

	if (Anim->Montage_GetPosition(ChargeReleaseMontage) >= ReleaseCrackTargetPosition)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(GroundCrackTimer);
		}
		SpawnReleaseGroundCrack();
	}
}

void UknightGameplayAbility_HeavyAttack::SpawnReleaseGroundCrack()
{
	AActor* const Avatar = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	if (!IsActive() || !Avatar || !World)
	{
		return;
	}

	const ACharacter* const Character = Cast<ACharacter>(Avatar);
	const float HalfHeight = Character && Character->GetCapsuleComponent()
		? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Forward = Avatar->GetActorForwardVector();
	const FVector StrikeOrigin = Avatar->GetActorLocation() + Forward * 180.f;

	FHitResult GroundHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(knightHeavyGroundCrack), false, Avatar);
	FVector ImpactLocation = StrikeOrigin - FVector(0.f, 0.f, HalfHeight);
	if (World->LineTraceSingleByObjectType(GroundHit, StrikeOrigin + FVector(0.f, 0.f, 120.f),
		StrikeOrigin - FVector(0.f, 0.f, 400.f), FCollisionObjectQueryParams(ECC_WorldStatic), QueryParams))
	{
		ImpactLocation = GroundHit.ImpactPoint;
	}

	const FRotator ImpactRotation = Avatar->GetActorRotation();
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	const bool bTwoHanded = ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Stance_TwoHanded);
	const bool bHeavyImpact = bTwoHanded || LastReleaseChargeAlpha >= 0.85f;

	UNiagaraSystem* CrackAsset = ReleaseGroundCrackEffect;
	if (!CrackAsset)
	{
		CrackAsset = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/SlashHitVFX/NS/NS_Hit_GroundCrack.NS_Hit_GroundCrack"));
	}
	if (CrackAsset)
	{
		const float CrackScale = FMath::Lerp(0.85f, bTwoHanded ? 1.45f : 1.2f, LastReleaseChargeAlpha);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, CrackAsset, ImpactLocation, ImpactRotation,
			FVector(CrackScale), true, true, ENCPoolMethod::AutoRelease);
	}

	if (bHeavyImpact)
	{
		UNiagaraSystem* RiftAsset = ReleaseGroundRiftEffect;
		if (!RiftAsset)
		{
			RiftAsset = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/SlashHitVFX/NS/NS_Hit_GroundRift.NS_Hit_GroundRift"));
		}
		if (RiftAsset)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, RiftAsset, ImpactLocation, ImpactRotation,
				FVector(bTwoHanded ? 1.15f : 0.95f), true, true, ENCPoolMethod::AutoRelease);
		}
	}

	UNiagaraSystem* DustAsset = ReleaseGroundDustEffect;
	if (!DustAsset)
	{
		DustAsset = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Knight/Creatures/Shared/Effects/NS_Monster_GroundDust.NS_Monster_GroundDust"));
	}
	if (DustAsset)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, DustAsset, ImpactLocation, ImpactRotation,
			FVector(FMath::Lerp(0.85f, 1.35f, LastReleaseChargeAlpha)), true, true, ENCPoolMethod::AutoRelease);
	}
}

void UknightGameplayAbility_HeavyAttack::PlayLoop()
{
	if (Phase == EPhase::Charging)
	{
		PlayMontage(ChargeLoopMontage, TEXT("ChargeLoop"), true);
	}
}

void UknightGameplayAbility_HeavyAttack::PlayMontage(UAnimMontage* Montage, FName TaskName, bool bLoopOnEnd)
{
	// 차지 시작·반복·발사 몽타주 모두 행동 속도 배속으로 재생한다. (2026-10-08)
	// (2026-10-27) 발사(해방)는 차지량에 따른 해방 배속(ReleaseRateScale)을 더 곱한다.
	const float Rate = GetAttackSpeed() * (Phase == EPhase::Releasing ? ReleaseRateScale : 1.f);
	UAbilityTask_PlayMontageAndWait* const Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TaskName, Montage, Rate);
	if (Phase == EPhase::Releasing)
	{
		Task->OnCompleted.AddDynamic(this, &ThisClass::OnReleaseFinished);
		Task->OnBlendOut.AddDynamic(this, &ThisClass::OnReleaseFinished);
		Task->OnInterrupted.AddDynamic(this, &ThisClass::OnReleaseFinished);
		Task->OnCancelled.AddDynamic(this, &ThisClass::OnReleaseFinished);
	}
	else if (bLoopOnEnd)
	{
		Task->OnCompleted.AddDynamic(this, &ThisClass::OnChargeLoopFinished);
		Task->OnBlendOut.AddDynamic(this, &ThisClass::OnChargeLoopFinished);
	}
	else
	{
		Task->OnCompleted.AddDynamic(this, &ThisClass::OnChargeStartFinished);
		Task->OnBlendOut.AddDynamic(this, &ThisClass::OnChargeStartFinished);
	}
	Task->ReadyForActivation();
	if (Phase == EPhase::Releasing)
	{
		// (2026-10-20) 차지 발사에만 기계식 타이밍(차지 시작·반복은 그대로).
		StartMechTiming(Montage, Rate);
	}
	if (AActor* const Avatar = GetAvatarActorFromActorInfo())
	{
		if (UknightGroggyFeedbackComponent* const Feedback = Avatar->FindComponentByClass<UknightGroggyFeedbackComponent>())
		{
			Feedback->WatchAttack(this, Montage, ActiveHitParams);
		}
	}
}

void UknightGameplayAbility_HeavyAttack::OnChargeStartFinished()
{
	PlayLoop();
}

void UknightGameplayAbility_HeavyAttack::OnChargeLoopFinished()
{
	PlayLoop();
}

void UknightGameplayAbility_HeavyAttack::OnReleaseFinished()
{
	Finish(false);
}

void UknightGameplayAbility_HeavyAttack::OnMeleeHitEvent(FGameplayEventData Payload)
{
	const FHitResult* const Hit = Payload.TargetData.Num() > 0 && Payload.TargetData.Get(0) ? Payload.TargetData.Get(0)->GetHitResult() : nullptr;
	if (Hit && (Phase == EPhase::Charging || Phase == EPhase::Releasing))
	{
		UknightCombatLibrary::ApplyMeleeHit(GetAvatarActorFromActorInfo(), *Hit, ActiveHitParams, this);
	}
}

void UknightGameplayAbility_HeavyAttack::TickChargeStamina()
{
	UWorld* const World = GetWorld();
	if (!World || Phase != EPhase::Charging || ChargeStaminaSpent >= ChargeStaminaCost)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(ChargeStaminaTimer);
		}
		return;
	}
	// 풀차지(MaxChargeTime)까지 ChargeStaminaCost를 고르게 나눠 쓴다. 풀차지 뒤에 더 누르고 있어도 더 쓰지 않는다.
	// (2026-10-28) 풀차지 시간은 스탠스를 따르므로(GetStanceMaxChargeTime) 합은 두 스탠스 모두 ChargeStaminaCost.
	const double Now = World->GetTimeSeconds();
	const float Amount = FMath::Min(ChargeStaminaCost * static_cast<float>(Now - LastChargeStaminaTime) / FMath::Max(GetStanceMaxChargeTime(), 0.1f),
		ChargeStaminaCost - ChargeStaminaSpent);
	LastChargeStaminaTime = Now;
	if (Amount > 0.f)
	{
		ChargeStaminaSpent += Amount;
		ApplyStaminaCost(Amount);
	}
}

void UknightGameplayAbility_HeavyAttack::CheckReleaseMoveCancel()
{
	UWorld* const World = GetWorld();
	const ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UknightAbilitySystemComponent* const KnightASC = GetKnightAbilitySystemComponentFromActorInfo();
	if (!World || !Character || !KnightASC || !IsActive() || Phase != EPhase::Releasing)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(ReleaseMoveCancelTimer);
		}
		return;
	}
	// 일반 공격(UknightGameplayAbility_MeleeAttack::CheckMoveCancel)과 같은 규칙: 캔슬 창 밖에서도 새 누름은 추적하고,
	// 캔슬 창이 열리고 후딜(퀵스텝만 되는 시간)이 지나면 새로 누른 이동 입력으로 해방 모션을 끊는다.
	ReleaseMovePress.Update(*Character);
	if (!KnightASC->HasMatchingGameplayTag(knightGameplayTags::Window_Cancel) || KnightASC->IsAttackRecoveryDashOnly())
	{
		return;
	}
	if (!ReleaseMovePress.HasFreshPress() || Character->GetLastMovementInputVector().IsNearlyZero())
	{
		return;
	}
	// 공격·가드·대시 입력이 버퍼에 남아 있으면 이동 캔슬보다 그 입력을 우선한다(일반 공격과 같음).
	if (const UknightInputBufferComponent* const InputBuffer = Character->FindComponentByClass<UknightInputBufferComponent>())
	{
		if (InputBuffer->HasBufferedInput(InputBuffer->GetBufferedInputTag()))
		{
			return;
		}
	}
	World->GetTimerManager().ClearTimer(ReleaseMoveCancelTimer);
	MontageStop(ReleaseMoveCancelBlendOut);
	Finish(false);
}

void UknightGameplayAbility_HeavyAttack::Finish(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, bWasCancelled);
	}
}

void UknightGameplayAbility_HeavyAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AActor* const Avatar = GetAvatarActorFromActorInfo())
	{
		if (UknightGroggyFeedbackComponent* const Feedback = Avatar->FindComponentByClass<UknightGroggyFeedbackComponent>())
		{
			Feedback->EndPredictionForAttack(this);
		}
	}
	StopChargeEffects();
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TapTimer);
		World->GetTimerManager().ClearTimer(ChargeWaitTimer);
		World->GetTimerManager().ClearTimer(GroundCrackTimer);
		World->GetTimerManager().ClearTimer(ChargeStaminaTimer);
		World->GetTimerManager().ClearTimer(ReleaseMoveCancelTimer);
		World->GetTimerManager().ClearTimer(ReleaseGhostTimer);
	}
	UAbilitySystemComponent* const OwnerASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (bAttackBlockApplied)
	{
		bAttackBlockApplied = false;
		if (OwnerASC)
		{
			OwnerASC->UnBlockAbilitiesWithTags(FGameplayTagContainer(knightGameplayTags::Ability_Attack));
		}
	}
	if (bChargeStateApplied)
	{
		bChargeStateApplied = false;
		if (OwnerASC)
		{
			OwnerASC->RemoveLooseGameplayTag(knightGameplayTags::State_Charging);
		}
		if (bWasCancelled)
		{
			MontageStop(0.2f);
		}
	}
	Phase = EPhase::Deciding;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	// (2026-10-26) 해방 때 바꾼 활성화 그룹을 되돌린다. 다음 발동은 다시 독립 그룹으로 시작한다(탭 강공격 단계를 막지 않게).
	// ASC의 그룹 수는 위 EndAbility가 바뀐 그룹 기준으로 이미 뺐다.
	ChangeActivationGroupWhileActive(EknightAbilityActivationGroup::Independent);
}
