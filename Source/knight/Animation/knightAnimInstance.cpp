#include "Animation/knightAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimMontage.h"
#include "Character/knightMovementComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Environment/knightWaterRippleSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
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

	if (Character && (FootstepWalkSound || FootstepRunSound || FootstepWalkSounds.Num() > 0 || FootstepRunSounds.Num() > 0))
	{
		UpdateFootsteps(DeltaSeconds, *Character);
	}
	if (ServoLoopSound)
	{
		UpdateServoLoop(DeltaSeconds);
	}
}

USoundBase* UknightAnimInstance::PickVariation(const TArray<TObjectPtr<USoundBase>>& Sounds, int32& LastIndex)
{
	if (Sounds.Num() == 0)
	{
		return nullptr;
	}
	int32 Index = FMath::RandRange(0, Sounds.Num() - 1);
	if (Sounds.Num() > 1 && Index == LastIndex)
	{
		Index = (Index + FMath::RandRange(1, Sounds.Num() - 1)) % Sounds.Num();
	}
	LastIndex = Index;
	return Sounds[Index];
}

void UknightAnimInstance::UpdateServoLoop(const float DeltaSeconds)
{
	const UWorld* const World = GetWorld();
	USkeletalMeshComponent* const Mesh = GetSkelMeshComponent();
	if (!World || !World->IsGameWorld() || !Mesh)
	{
		return;
	}
	const float SpeedAlpha = FMath::Clamp(static_cast<float>(CachedVelocity.Size2D()) / FMath::Max(ServoLoopFullSpeed, 1.f), 0.f, 1.f);
	const float TargetVolume = bCachedIsFalling ? 0.f : ServoLoopMaxVolume * SpeedAlpha;
	ServoLoopCurrentVolume = FMath::FInterpConstantTo(ServoLoopCurrentVolume, TargetVolume, DeltaSeconds,
		FMath::Max(ServoLoopMaxVolume, 0.01f) * ServoLoopFadeRate);

	if (ServoLoopCurrentVolume <= KINDA_SMALL_NUMBER)
	{
		// 멈췄을 때는 소리 채널을 차지하지 않게 끈다.
		if (ServoLoopAudio && ServoLoopAudio->IsPlaying())
		{
			ServoLoopAudio->Stop();
		}
		return;
	}
	if (!ServoLoopAudio)
	{
		ServoLoopAudio = UGameplayStatics::SpawnSoundAttached(ServoLoopSound, Mesh, NAME_None, FVector::ZeroVector,
			EAttachLocation::SnapToTarget, true, ServoLoopCurrentVolume, 1.f, 0.f, nullptr, nullptr, false);
	}
	if (!ServoLoopAudio)
	{
		return;
	}
	if (!ServoLoopAudio->IsPlaying())
	{
		// 루프 어디서나 시작해 멈췄다 출발할 때마다 같은 소리로 시작하지 않게 한다.
		ServoLoopAudio->Play(FMath::FRandRange(0.f, 2.5f));
	}
	ServoLoopAudio->SetVolumeMultiplier(ServoLoopCurrentVolume);
	ServoLoopAudio->SetPitchMultiplier(FMath::Lerp(ServoLoopPitchSlow, ServoLoopPitchFast, SpeedAlpha));
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
	const bool bRunning = KnightMovement && KnightMovement->IsRunning();
	USoundBase* const Sound = (bRunning && FootstepRunSound) ? FootstepRunSound.Get()
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
#if !UE_BUILD_SHIPPING
		if (CVarDebugFootsteps.GetValueOnGameThread() > 0 && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()) * 4 + Index, 0.f, FColor::Cyan,
				FString::Printf(TEXT("%s height %.1f  min %.1f  max %.1f  lifted %d  active %d"), *FootstepBones[Index].ToString(),
					Height, Foot.MinHeight, Foot.MaxHeight, Foot.bLifted ? 1 : 0, bActive ? 1 : 0));
		}
#endif
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
			if (Now - Foot.LastStepTime >= FootstepCooldown)
			{
				Foot.LastStepTime = Now;
				// (2026-10-28) 변주 배열이 있으면 거기서 고르고, 물에 잠긴 발이면 물 발소리를 겹치거나 바꾼다.
				USoundBase* StepSound = bRunning
					? (FootstepRunSounds.Num() > 0 ? PickVariation(FootstepRunSounds, LastRunIndex) : Sound)
					: (FootstepWalkSounds.Num() > 0 ? PickVariation(FootstepWalkSounds, LastWalkIndex) : Sound);
				bool bInWater = false;
				bool bDeepWater = false;
				if (FootstepWaterSounds.Num() > 0 || FootstepDeepWaterSounds.Num() > 0)
				{
					float SurfaceZ = 0.f;
					if (UknightWaterRippleSubsystem::FindWaterSurface(World, FootLocation, SurfaceZ) && SurfaceZ > FootLocation.Z)
					{
						bInWater = true;
						bDeepWater = FootstepDeepWaterSounds.Num() > 0 && SurfaceZ - FootLocation.Z >= FootstepDeepWaterDepth;
					}
				}
				if (bDeepWater)
				{
					StepSound = PickVariation(FootstepDeepWaterSounds, LastDeepWaterIndex);
				}
				const float StepPitch = 1.f + FMath::FRandRange(-FootstepPitchVariance, FootstepPitchVariance);
				if (StepSound)
				{
					UGameplayStatics::PlaySoundAtLocation(&Character, StepSound, FootLocation, FootstepVolume, StepPitch);
				}
				if (bInWater && !bDeepWater)
				{
					if (USoundBase* const WaterSound = PickVariation(FootstepWaterSounds, LastWaterIndex))
					{
						UGameplayStatics::PlaySoundAtLocation(&Character, WaterSound, FootLocation, FootstepWaterVolume, StepPitch);
					}
				}
				if (bSpawnFootstepRunDust && KnightMovement && KnightMovement->IsRunning())
				{
					UNiagaraSystem* DustAsset = FootstepRunDustEffect;
					if (!DustAsset)
					{
						DustAsset = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Knight/Creatures/Shared/Effects/NS_Monster_RunDust.NS_Monster_RunDust"));
					}
					if (DustAsset)
					{
						UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, DustAsset, FootLocation, Character.GetActorRotation(),
							FVector(FootstepRunDustScale), true, true, ENCPoolMethod::AutoRelease);
					}
				}
#if !UE_BUILD_SHIPPING
				if (CVarDebugFootsteps.GetValueOnGameThread() > 0)
				{
					DrawDebugSphere(World, FootLocation, 12.f, 8, FColor::Green, false, 0.5f);
					UE_LOG(LogTemp, Log, TEXT("[Footstep] %s %s water %d deep %d"), *FootstepBones[Index].ToString(), *GetNameSafe(StepSound),
						bInWater ? 1 : 0, bDeepWater ? 1 : 0);
				}
#endif
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
