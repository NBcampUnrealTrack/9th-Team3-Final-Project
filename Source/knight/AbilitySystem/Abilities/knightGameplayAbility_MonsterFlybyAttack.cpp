#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterFlybyAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/knightPlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Environment/knightWaterRippleSettings.h"
#include "Environment/knightWaterRippleSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDebug.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "System/knightGameplayTags.h"
#include "knight.h"

namespace knightMonsterFlyby
{
	constexpr float TickInterval = 1.f / 60.f;
	constexpr int32 MaxTicks = 100000;
	/** 기둥 메시(SM_Janhyang_SonicPillar)의 높이와 아래 끝 반지름. */
	constexpr float PillarMeshSize = 100.f;

	template <typename T>
	T* Pick(bool bEnraged, const TObjectPtr<T>& Normal, const TObjectPtr<T>& Enraged)
	{
		return bEnraged && Enraged ? Enraged.Get() : Normal.Get();
	}

	/** Parent에 붙지만 회전과 크기는 월드 기준인 이펙트를 만든다(부모 메시의 스케일이 입자 위치·속도에 곱해지지 않게). */
	UNiagaraComponent* SpawnEffect(UNiagaraSystem* System, USceneComponent* Parent, FName Socket, const FRotator& WorldRotation)
	{
		if (!System || !Parent)
		{
			return nullptr;
		}
		UNiagaraComponent* const Component = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Parent, Socket, FVector::ZeroVector,
			FRotator::ZeroRotator, EAttachLocation::SnapToTarget, false, false, ENCPoolMethod::None);
		if (Component)
		{
			Component->SetUsingAbsoluteRotation(true);
			Component->SetUsingAbsoluteScale(true);
			Component->SetWorldRotation(WorldRotation);
			Component->SetWorldScale3D(FVector::OneVector);
			Component->Activate(true);
		}
		return Component;
	}

	/** bImmediate가 아니면 새 입자만 멈추고, 남은 입자가 수명을 다하면 사라진다. */
	void Release(TObjectPtr<UNiagaraComponent>& Component, bool bImmediate)
	{
		if (Component)
		{
			if (bImmediate)
			{
				Component->DestroyComponent();
			}
			else
			{
				Component->SetAutoDestroy(true);
				Component->Deactivate();
			}
		}
		Component = nullptr;
	}
}

UknightGameplayAbility_MonsterFlybyAttack::UknightGameplayAbility_MonsterFlybyAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Selection.MinRange = 800.f;
	Selection.MaxRange = 3000.f;
	Selection.MaxAngle = 180.f;
	HitParams.bCanBeParried = false;
}

void UknightGameplayAbility_MonsterFlybyAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bPassing = false;
	bRecovering = false;
	PhaseTime = 0.f;
	HitActors.Reset();
	SetWarning(true);
	const UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	bEnraged = ASC && ASC->HasMatchingGameplayTag(knightGameplayTags::State_Monster_Enraged);
	if (const ACharacter* const Monster = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		WarningComponent = knightMonsterFlyby::SpawnEffect(knightMonsterFlyby::Pick(bEnraged, Effects.Warning, Effects.EnragedWarning),
			Monster->GetMesh(), BeamBone, Monster->GetActorRotation());
		if (Effects.WarningSound)
		{
			WarningAudio = UGameplayStatics::SpawnSoundAttached(Effects.WarningSound, Monster->GetMesh(), BeamBone, FVector::ZeroVector,
				EAttachLocation::SnapToTarget, false, 1.f, Effects.WarningSoundPitch.X);
		}
	}
	WarningRippleTime = Effects.WarningRippleInterval;
	if (WarningMontage)
	{
		UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, WarningMontage);
		MontageTask->ReadyForActivation();
		PlayingWarning = WarningMontage;
	}
	UAbilityTask_Repeat* const Task = UAbilityTask_Repeat::RepeatAction(this, knightMonsterFlyby::TickInterval, knightMonsterFlyby::MaxTicks);
	Task->OnPerformAction.AddDynamic(this, &ThisClass::HandleTick);
	TickTask = Task;
	Task->ReadyForActivation();
}

void UknightGameplayAbility_MonsterFlybyAttack::HandleTick(int32 ActionNumber)
{
	// 몬스터 시간으로 센다. 히트스톱으로 몸이 멈춘 동안 시계만 흐르면 지나가기가 덜 간 채 끝난다.
	PhaseTime += GetMonsterTimeStep(knightMonsterFlyby::TickInterval);
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	if (!Monster)
	{
		return;
	}
	if (!bPassing)
	{
		const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
		const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
		UknightMonsterMovementComponent* const Movement = Monster->GetMonsterMovementComponent();
		if (Movement && Target)
		{
			Movement->TurnTowards(Target->GetActorLocation(), GetMonsterTimeStep(knightMonsterFlyby::TickInterval), TurnRate, 2.f);
		}
#if ENABLE_DRAW_DEBUG
		if (knightMonsterDebug::ShouldDrawAttacks() && Target)
		{
			const FVector From = Monster->GetActorLocation();
			const FVector Dir = (Target->GetActorLocation() - From).GetSafeNormal2D();
			const FVector To = Target->GetActorLocation() + Dir * PassOvershoot;
			DrawDebugLine(GetWorld(), From, To, FColor::Orange, false, knightMonsterFlyby::TickInterval, 0, HalfWidth * 0.2f);
		}
#endif
		UpdateWarningEffects();
		if (PhaseTime >= WarningTime)
		{
			StartPass();
		}
		return;
	}
	if (bRecovering)
	{
		if (PhaseTime >= RecoverDuration)
		{
			EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
		}
		return;
	}
	ApplyHits();
	UpdateBeamEffects();
	if (PhaseTime >= PassDuration)
	{
		StartRecover();
	}
}

void UknightGameplayAbility_MonsterFlybyAttack::StartRecover()
{
	UAnimInstance* const AnimInstance = GetCurrentActorInfo() ? GetCurrentActorInfo()->GetAnimInstance() : nullptr;
	UAnimMontage* const Montage = PlayingWarning.Get();
	if (!Montage || !AnimInstance || !AnimInstance->Montage_IsPlaying(Montage))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
		return;
	}
	// 기둥은 바로 끄고, 회복 동작은 제 속도로 끝까지(블렌드 아웃 전까지) 재생한다.
	StopEffects();
	bRecovering = true;
	PhaseTime = 0.f;
	AnimInstance->Montage_SetPosition(Montage, PassPoseEndTime);
	AnimInstance->Montage_SetPlayRate(Montage, 1.f);
	RecoverDuration = FMath::Max(0.f, Montage->GetPlayLength() - PassPoseEndTime - Montage->GetDefaultBlendOutTime());
}

void UknightGameplayAbility_MonsterFlybyAttack::StartPass()
{
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	SetWarning(false);
	StopWarningEffects();
	bPassing = true;
	PhaseTime = 0.f;
	if (PlayingWarning.IsValid() && PassPoseEndTime <= 0.f)
	{
		MontageStop(0.3f);
		PlayingWarning.Reset();
	}

	// 경고가 끝나는 순간의 타깃 위치로 방향·거리를 고정한다. 이후 타깃이 움직여도 일자로 지나간다.
	FVector Direction = Monster->GetActorForwardVector().GetSafeNormal2D();
	float Travel = PassOvershoot;
	if (Target)
	{
		const FVector ToTarget = Target->GetActorLocation() - Monster->GetActorLocation();
		Direction = ToTarget.GetSafeNormal2D(UE_SMALL_NUMBER, Direction);
		Travel += ToTarget.Size2D();
	}
	Monster->SetActorRotation(Direction.Rotation());
	PassDuration = Travel / PassSpeed;
	UAbilityTask_ApplyRootMotionConstantForce* const PassTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("Flyby"), Direction, PassSpeed, PassDuration, false, nullptr, ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f, false);
	PassTask->ReadyForActivation();
	// 쏘는 자세가 지나가기 끝까지 이어지도록 남은 자세 구간을 지나가는 시간에 맞춰 늘이거나 줄인다.
	UAnimInstance* const AnimInstance = GetCurrentActorInfo() ? GetCurrentActorInfo()->GetAnimInstance() : nullptr;
	if (PlayingWarning.IsValid() && AnimInstance && AnimInstance->Montage_IsPlaying(PlayingWarning.Get()))
	{
		const float Remaining = PassPoseEndTime - AnimInstance->Montage_GetPosition(PlayingWarning.Get());
		AnimInstance->Montage_SetPlayRate(PlayingWarning.Get(), FMath::Clamp(Remaining / FMath::Max(PassDuration, 0.1f), 0.1f, 2.f));
	}
	UE_LOG(LogknightAI, Log, TEXT("%s: flyby passes %.0f cm in %.2f s."), *GetNameSafe(Monster), Travel, PassDuration);
	StartBeamEffects();
}

void UknightGameplayAbility_MonsterFlybyAttack::GetBeam(FVector& OutChest, FVector& OutGround) const
{
	const ACharacter* const Monster = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Monster)
	{
		OutChest = OutGround = FVector::ZeroVector;
		return;
	}
	// 가슴 바로 아래 바닥(캡슐 바닥 높이)을 중심으로 한다. 공중에 뜬 메시 높이와 상관없이 바닥의 대상이 맞는다.
	const USkeletalMeshComponent* const Mesh = Monster->GetMesh();
	OutChest = Mesh && Mesh->DoesSocketExist(BeamBone) ? Mesh->GetSocketLocation(BeamBone) : Monster->GetActorLocation();
	const UCapsuleComponent* const Capsule = Monster->GetCapsuleComponent();
	OutGround = FVector(OutChest.X, OutChest.Y, Monster->GetActorLocation().Z - (Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f));
}

bool UknightGameplayAbility_MonsterFlybyAttack::FindBeamWater(const FVector& Chest, const FVector& Ground, float& OutSurfaceZ) const
{
	const UWorld* const World = GetWorld();
	return World && UknightWaterRippleSubsystem::FindWaterSurface(World, Ground, OutSurfaceZ) && OutSurfaceZ > Ground.Z - 50.f && OutSurfaceZ < Chest.Z;
}

void UknightGameplayAbility_MonsterFlybyAttack::UpdateWarningEffects()
{
	AActor* const Monster = GetAvatarActorFromActorInfo();
	USceneComponent* const Root = Monster ? Monster->GetRootComponent() : nullptr;
	if (!Root)
	{
		return;
	}
	const float Alpha = WarningTime > 0.f ? FMath::Clamp(PhaseTime / WarningTime, 0.f, 1.f) : 1.f;
	if (WarningAudio)
	{
		WarningAudio->SetPitchMultiplier(FMath::Lerp(Effects.WarningSoundPitch.X, Effects.WarningSoundPitch.Y, Alpha));
	}

	FVector Chest, Ground;
	GetBeam(Chest, Ground);
	float SurfaceZ = 0.f;
	const bool bWater = FindBeamWater(Chest, Ground, SurfaceZ);
	const FVector Bottom(Ground.X, Ground.Y, bWater ? SurfaceZ : Ground.Z);

	// 바닥과 수면을 오가면 이펙트를 바꾼다. 앞의 것은 남은 입자가 수명대로 사라진다.
	UNiagaraSystem* const Inflow = bWater && Effects.WarningWaterInflow ? Effects.WarningWaterInflow.Get() : Effects.WarningInflow.Get();
	if (InflowComponent && InflowComponent->GetAsset() != Inflow)
	{
		knightMonsterFlyby::Release(InflowComponent, false);
	}
	if (!InflowComponent && Inflow)
	{
		InflowComponent = knightMonsterFlyby::SpawnEffect(Inflow, Root, NAME_None, FRotator(0.f, Monster->GetActorRotation().Yaw, 0.f));
	}
	if (InflowComponent)
	{
		InflowComponent->SetRelativeLocation(Root->GetComponentTransform().InverseTransformPosition(Bottom));
		const float Height = FMath::Max(Chest.Z - Bottom.Z, knightMonsterFlyby::PillarMeshSize);
		InflowComponent->SetRelativeScale3D(FVector(1.f, 1.f, Height / FMath::Max(Effects.WarningInflowHeight, 1.f)));
	}

	// 같은 자리를 이어 눌러 잔물결 고리가 연달아 퍼지고, 발사가 가까워질수록 깊어진다.
	WarningRippleTime += knightMonsterFlyby::TickInterval;
	if (bWater && Effects.WarningRippleInterval > 0.f && Effects.WarningRippleRadius > 0.f && WarningRippleTime >= Effects.WarningRippleInterval)
	{
		WarningRippleTime = 0.f;
		if (UknightWaterRippleSubsystem* const Ripples = GetWorld()->GetSubsystem<UknightWaterRippleSubsystem>())
		{
			Ripples->AddImpulse(Bottom, Effects.WarningRippleRadius, FMath::Lerp(Effects.WarningRippleDepth.X, Effects.WarningRippleDepth.Y, Alpha));
		}
	}
}

void UknightGameplayAbility_MonsterFlybyAttack::StopWarningEffects()
{
	knightMonsterFlyby::Release(WarningComponent, true);
	knightMonsterFlyby::Release(InflowComponent, false);
	if (WarningAudio)
	{
		WarningAudio->FadeOut(Effects.WarningSoundFadeOut, 0.f);
	}
	WarningAudio = nullptr;
}

void UknightGameplayAbility_MonsterFlybyAttack::StartBeamEffects()
{
	AActor* const Monster = GetAvatarActorFromActorInfo();
	USceneComponent* const Root = Monster ? Monster->GetRootComponent() : nullptr;
	if (!Root)
	{
		return;
	}
	// 캡슐(스케일 1)에 붙여 몸과 함께 매 프레임 움직이고, 가슴 위치와 기둥 높이는 판정 틱마다 맞춘다.
	const FRotator Facing(0.f, Monster->GetActorRotation().Yaw, 0.f);
	UMaterialInterface* const Material = knightMonsterFlyby::Pick(bEnraged, Effects.PillarMaterial, Effects.EnragedPillarMaterial);
	if (Effects.PillarMesh && Material)
	{
		PillarComponent = NewObject<UStaticMeshComponent>(Monster, NAME_None, RF_Transient);
		PillarComponent->SetStaticMesh(Effects.PillarMesh);
		PillarComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PillarComponent->SetGenerateOverlapEvents(false);
		PillarComponent->SetCastShadow(false);
		PillarComponent->bReceivesDecals = false;
		PillarComponent->SetUsingAbsoluteRotation(true);
		PillarComponent->SetUsingAbsoluteScale(true);
		PillarComponent->SetupAttachment(Root);
		PillarComponent->RegisterComponent();
		PillarComponent->SetWorldRotation(Facing);
		PillarMaterialInstance = PillarComponent->CreateDynamicMaterialInstance(0, Material);
		if (PillarMaterialInstance)
		{
			PillarMaterialInstance->SetScalarParameterValue(TEXT("Fade"), 0.f);
		}
	}
	StreakComponent = knightMonsterFlyby::SpawnEffect(knightMonsterFlyby::Pick(bEnraged, Effects.PillarStreaks, Effects.EnragedPillarStreaks), Root, NAME_None, Facing);
	GroundComponent = knightMonsterFlyby::SpawnEffect(knightMonsterFlyby::Pick(bEnraged, Effects.Ground, Effects.EnragedGround), Root, NAME_None, Facing);
	const ACharacter* const Character = Cast<ACharacter>(Monster);
	if (Effects.BeamSound && Character)
	{
		BeamAudio = UGameplayStatics::SpawnSoundAttached(Effects.BeamSound, Character->GetMesh(), BeamBone);
	}
	bOverWater = false;
	bTrailStarted = false;
	UpdateBeamEffects();
}

void UknightGameplayAbility_MonsterFlybyAttack::UpdateBeamEffects()
{
	const AActor* const Monster = GetAvatarActorFromActorInfo();
	const USceneComponent* const Root = Monster ? Monster->GetRootComponent() : nullptr;
	if (!Root)
	{
		return;
	}
	FVector Chest, Ground;
	GetBeam(Chest, Ground);
	float BottomZ = Ground.Z;
	UpdateWater(Chest, Ground, BottomZ);
	const FVector Bottom(Ground.X, Ground.Y, BottomZ);
	const float Length = FMath::Max(Chest.Z - BottomZ, knightMonsterFlyby::PillarMeshSize);
	const FTransform& RootTransform = Root->GetComponentTransform();
	const FVector ChestLocal = RootTransform.InverseTransformPosition(Chest);

	if (PillarComponent)
	{
		const float Radius = (Effects.PillarRadius > 0.f ? Effects.PillarRadius : HalfWidth) / knightMonsterFlyby::PillarMeshSize;
		PillarComponent->SetRelativeLocation(ChestLocal);
		PillarComponent->SetRelativeScale3D(FVector(Radius, Radius, Length / knightMonsterFlyby::PillarMeshSize));
		if (PillarMaterialInstance)
		{
			// 지나가기 시작과 끝에서 FadeTime 동안 나타나고 사라진다.
			const float FadeTime = FMath::Max(Effects.FadeTime, KINDA_SMALL_NUMBER);
			const float Fade = FMath::Clamp(FMath::Min(PhaseTime, PassDuration - PhaseTime) / FadeTime, 0.f, 1.f);
			PillarMaterialInstance->SetScalarParameterValue(TEXT("Fade"), Fade);
		}
	}
	if (StreakComponent)
	{
		StreakComponent->SetRelativeLocation(ChestLocal);
		StreakComponent->SetRelativeScale3D(FVector(1.f, 1.f, Length / FMath::Max(Effects.PillarStreakLength, 1.f)));
	}
	if (GroundComponent)
	{
		GroundComponent->SetRelativeLocation(RootTransform.InverseTransformPosition(Bottom));
	}
	// 붙어 다니는 바람은 몸과 함께 움직이므로, 지나간 자리에는 따로 연기를 남긴다.
	UNiagaraSystem* const Trail = knightMonsterFlyby::Pick(bEnraged, Effects.GroundTrail, Effects.EnragedGroundTrail);
	if (Trail && (!bTrailStarted || FVector::DistSquared2D(Bottom, LastTrailPoint) >= FMath::Square(Effects.GroundTrailSpacing)))
	{
		bTrailStarted = true;
		LastTrailPoint = Bottom;
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Trail, Bottom, FRotator(0.f, Monster->GetActorRotation().Yaw, 0.f), FVector::OneVector,
			true, true, ENCPoolMethod::AutoRelease);
	}

	// 기둥 아래(띠 안쪽과 그 바깥 JitterMargin까지)에 있는 동안 화면이 자글거린다.
	if (Effects.JitterMargin >= 0.f)
	{
		const float Reach = HalfWidth + Effects.JitterMargin;
		AknightPlayerCameraManager::ForEachLocalCamera(Monster, [&Bottom, &Chest, Reach](AknightPlayerCameraManager& Camera)
		{
			const APlayerController* const PC = Camera.GetOwningPlayerController();
			const APawn* const Pawn = PC ? PC->GetPawn() : nullptr;
			const FVector Location = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
			if (Pawn && FVector::DistSquared2D(Location, Bottom) <= FMath::Square(Reach) && Location.Z <= Chest.Z)
			{
				Camera.HoldSonicJitter();
			}
		});
	}
}

void UknightGameplayAbility_MonsterFlybyAttack::UpdateWater(const FVector& Chest, const FVector& Ground, float& InOutBottomZ)
{
	UWorld* const World = GetWorld();
	float SurfaceZ = 0.f;
	if (!FindBeamWater(Chest, Ground, SurfaceZ))
	{
		bOverWater = false;
		if (SprayComponent && SprayComponent->IsActive())
		{
			SprayComponent->Deactivate();
		}
		return;
	}
	InOutBottomZ = SurfaceZ;
	const FVector Point(Ground.X, Ground.Y, SurfaceZ);

	// 물을 가르는 물보라 막은 몸(캡슐)에 붙여 닿는 자리와 함께 움직인다. 수면을 벗어나면 끄고 다시 닿으면 켠다.
	AActor* const Monster = GetAvatarActorFromActorInfo();
	USceneComponent* const Root = Monster ? Monster->GetRootComponent() : nullptr;
	if (!SprayComponent && Effects.WaterSpray && Root)
	{
		SprayComponent = knightMonsterFlyby::SpawnEffect(Effects.WaterSpray, Root, NAME_None, FRotator(0.f, Monster->GetActorRotation().Yaw, 0.f));
	}
	if (SprayComponent && Root)
	{
		SprayComponent->SetRelativeLocation(Root->GetComponentTransform().InverseTransformPosition(Point));
		if (!SprayComponent->IsActive())
		{
			SprayComponent->Activate(true);
		}
	}
	if (!bOverWater)
	{
		// 수면에 처음 닿으면 바로 물결과 물보라를 낸다.
		WaterTravel = Effects.WaterRippleSpacing;
		WaterSplashTravel = Effects.WaterSplashSpacing;
	}
	else
	{
		const float Step = FVector::Dist2D(Point, LastWaterPoint);
		WaterTravel += Step;
		WaterSplashTravel += Step;
	}
	bOverWater = true;
	LastWaterPoint = Point;

	UknightWaterRippleSubsystem* const Ripples = World->GetSubsystem<UknightWaterRippleSubsystem>();
	if (Ripples && Effects.WaterMaxHeight > 0.f)
	{
		// 촘촘히 이어 누르는 골이 평소 상한에 잘리지 않게, 지나가는 동안과 물결이 퍼지는 동안만 올린다.
		Ripples->BoostMaxHeight(Effects.WaterMaxHeight, 2.f);
	}
	if (Ripples && Effects.WaterRippleRadius > 0.f && Effects.WaterRippleDepth > 0.f && WaterTravel >= Effects.WaterRippleSpacing)
	{
		WaterTravel = 0.f;
		Ripples->AddImpulse(Point, Effects.WaterRippleRadius, Effects.WaterRippleDepth);
	}
	if (Effects.WaterSplashSpacing > 0.f && WaterSplashTravel >= Effects.WaterSplashSpacing)
	{
		WaterSplashTravel = 0.f;
		UNiagaraSystem* const Splash = Effects.WaterSplash ? Effects.WaterSplash.Get() : GetDefault<UknightWaterRippleSettings>()->SplashHeavySystem.LoadSynchronous();
		if (Splash)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Splash, Point, FRotator::ZeroRotator, FVector(Effects.WaterSplashScale), true, true,
				ENCPoolMethod::AutoRelease);
		}
	}
}

void UknightGameplayAbility_MonsterFlybyAttack::StopEffects()
{
	StopWarningEffects();
	knightMonsterFlyby::Release(StreakComponent, false);
	knightMonsterFlyby::Release(GroundComponent, false);
	knightMonsterFlyby::Release(SprayComponent, false);
	if (BeamAudio)
	{
		BeamAudio->FadeOut(Effects.BeamSoundFadeOut, 0.f);
	}
	BeamAudio = nullptr;
	if (PillarComponent)
	{
		PillarComponent->DestroyComponent();
	}
	PillarComponent = nullptr;
	PillarMaterialInstance = nullptr;
}

void UknightGameplayAbility_MonsterFlybyAttack::ApplyHits()
{
	ACharacter* const Monster = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Monster)
	{
		return;
	}
	FVector Chest, Center;
	GetBeam(Chest, Center);
	const int32 NewHits = UknightCombatLibrary::ApplyAreaMeleeHits(Monster, Center, FCollisionShape::MakeSphere(HalfWidth), HitParams, HitActors,
		[](const AActor&) { return true; },
		[this](AActor& Target, EknightHitOutcome Outcome) { HandleGuardPush(Target, Outcome); });
#if ENABLE_DRAW_DEBUG
	if (knightMonsterDebug::ShouldDrawAttacks())
	{
		DrawDebugSphere(GetWorld(), Center, HalfWidth, 12, NewHits == 0 ? FColor::Red : FColor::Magenta, false, 0.5f);
	}
#endif
}

void UknightGameplayAbility_MonsterFlybyAttack::SetWarning(bool bOn)
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || bOn == bWarningApplied)
	{
		return;
	}
	if (bOn)
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::State_Monster_Warning);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(knightGameplayTags::State_Monster_Warning);
	}
	bWarningApplied = bOn;
}

void UknightGameplayAbility_MonsterFlybyAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (TickTask.IsValid())
	{
		TickTask->EndTask();
	}
	TickTask.Reset();
	SetWarning(false);
	StopEffects();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
