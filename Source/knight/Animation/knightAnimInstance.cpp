#include "Animation/knightAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimMontage.h"
#include "Character/knightMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "System/knightGameplayTags.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	// (2026-10-22) PIE 콘솔 knight.Debug.Footsteps 1: 발 높이를 화면에 띄우고, 발소리가 난 자리에 초록 구를 그리고 로그를 남긴다.
	TAutoConsoleVariable<int32> CVarDebugFootsteps(TEXT("knight.Debug.Footsteps"), 0,
		TEXT("1: show foot heights, draw a sphere and log at each footstep."));
}

void UknightAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (ASC && !bAbilitySystemBound)
	{
		GameplayTagPropertyMap.Initialize(this, ASC);
		bAbilitySystemBound = true;
		BoundASC = ASC;
	}
}

void UknightAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningActor()))
	{
		InitializeWithAbilitySystem(ASC);
	}
}

void UknightAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// The ASC can be created after the anim instance initializes (spawned actors); bind on first sight.
	if (!bAbilitySystemBound)
	{
		InitializeWithAbilitySystem(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningActor()));
	}

	bTwoHandedStance = BoundASC.IsValid() && BoundASC->HasMatchingGameplayTag(knightGameplayTags::State_Stance_TwoHanded);

	const ACharacter* const Character = Cast<ACharacter>(GetOwningActor());
	const UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (Movement)
	{
		CachedVelocity = Movement->Velocity;
		bCachedIsFalling = Movement->IsFalling();
		bCachedHasAcceleration = !Movement->GetCurrentAcceleration().IsNearlyZero();
		bCachedPlayingRootMotion = bIgnoreRootMotionVelocity && Character->IsPlayingRootMotion();
	}

	// 공중 하체 고정: 검 뽑기 같은 몽타주는 땅 위 다리를 써서 고정하고, 공격 몽타주는 다리까지 따르게 한다.
	float LegLockTarget = 0.f;
	if (bCachedIsFalling)
	{
		UAnimMontage* const Montage = GetCurrentActiveMontage();
		LegLockTarget = (!Montage || AirLegLockMontages.Contains(Montage)) ? 1.f : 0.f;
	}
	AirLegLockAlpha = AirLegLockBlendTime > 0.f
		? FMath::FInterpConstantTo(AirLegLockAlpha, LegLockTarget, DeltaSeconds, 1.f / AirLegLockBlendTime)
		: LegLockTarget;

	if (Character && (FootstepWalkSound || FootstepRunSound))
	{
		UpdateFootsteps(DeltaSeconds, *Character);
	}
}

void UknightAnimInstance::UpdateFootsteps(const float DeltaSeconds, const ACharacter& Character)
{
	const USkeletalMeshComponent* const Mesh = GetSkelMeshComponent();
	const UWorld* const World = GetWorld();
	if (!Mesh || !World)
	{
		return;
	}
	if (FootStates.Num() != FootstepBones.Num())
	{
		FootStates.SetNum(FootstepBones.Num());
	}

	const bool bActive = !bCachedIsFalling && CachedVelocity.Size2D() >= FootstepMinSpeed;
	const UknightMovementComponent* const KnightMovement = Cast<UknightMovementComponent>(Character.GetCharacterMovement());
	USoundBase* const Sound = (KnightMovement && KnightMovement->IsRunning() && FootstepRunSound) ? FootstepRunSound.Get()
		: (FootstepWalkSound ? FootstepWalkSound.Get() : FootstepRunSound.Get());
	const double Now = World->GetTimeSeconds();
	const float MeshZ = Mesh->GetComponentLocation().Z;

	for (int32 Index = 0; Index < FootstepBones.Num(); ++Index)
	{
		FFootState& Foot = FootStates[Index];
		if (!Mesh->DoesSocketExist(FootstepBones[Index]))
		{
			continue;
		}
		const FVector FootLocation = Mesh->GetSocketLocation(FootstepBones[Index]);
		const float Height = FootLocation.Z - MeshZ;
		// 최저·최고 높이는 천천히 서로 다가가게(폭의 초당 50%) 해서 경사·걸음이 바뀌어도 따라가게 한다.
		if (Foot.MinHeight > Foot.MaxHeight)
		{
			Foot.MinHeight = Foot.MaxHeight = Height;
		}
		const float Shrink = (Foot.MaxHeight - Foot.MinHeight) * 0.5f * DeltaSeconds;
		Foot.MinHeight = FMath::Min(Foot.MinHeight + Shrink, Height);
		Foot.MaxHeight = FMath::Max(Foot.MaxHeight - Shrink, Height);
		const float Range = Foot.MaxHeight - Foot.MinHeight;
		if (CVarDebugFootsteps.GetValueOnGameThread() > 0 && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()) * 4 + Index, 0.f, FColor::Cyan,
				FString::Printf(TEXT("%s height %.1f  min %.1f  max %.1f  lifted %d  active %d"), *FootstepBones[Index].ToString(),
					Height, Foot.MinHeight, Foot.MaxHeight, Foot.bLifted ? 1 : 0, bActive ? 1 : 0));
		}
		if (!bActive)
		{
			Foot.bLifted = false;
			continue;
		}
		if (Range < FootstepMinLift)
		{
			Foot.bLifted = false;
			continue;
		}
		if (!Foot.bLifted)
		{
			Foot.bLifted = Height > Foot.MinHeight + Range * FootstepLiftRatio;
		}
		else if (Height <= Foot.MinHeight + Range * FootstepPlantRatio)
		{
			Foot.bLifted = false;
			if (Sound && Now - Foot.LastStepTime >= FootstepCooldown)
			{
				Foot.LastStepTime = Now;
				UGameplayStatics::PlaySoundAtLocation(&Character, Sound, FootLocation, FootstepVolume);
				if (CVarDebugFootsteps.GetValueOnGameThread() > 0)
				{
					DrawDebugSphere(World, FootLocation, 12.f, 8, FColor::Green, false, 0.5f);
					UE_LOG(LogTemp, Log, TEXT("[Footstep] %s %s"), *FootstepBones[Index].ToString(), *GetNameSafe(Sound));
				}
			}
		}
	}
}

void UknightAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
	Velocity = CachedVelocity;
	GroundSpeed = bCachedPlayingRootMotion ? 0.f : CachedVelocity.Size2D();
	bIsFalling = bCachedIsFalling;
	bHasAcceleration = bCachedHasAcceleration;
}

#if WITH_EDITOR
EDataValidationResult UknightAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	GameplayTagPropertyMap.IsDataValid(this, Context);
	return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
