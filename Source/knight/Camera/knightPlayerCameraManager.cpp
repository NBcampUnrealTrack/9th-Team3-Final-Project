#include "Camera/knightPlayerCameraManager.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/CameraShakeBase.h"
#include "Engine/AssetManager.h"
#include "Engine/Scene.h"
#include "Engine/StreamableManager.h"
#include "HAL/PlatformTime.h"
#include "Camera/knightCameraImpactSettings.h"
#include "Camera/knightCameraModifier_Darkness.h"
#include "Camera/knightCameraModifier_Impact.h"
#include "Camera/knightCameraModifier_Deafen.h"
#include "Camera/knightCameraModifier_RoarShockwave.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Settings/knightSettingsSubsystem.h"
#include "Settings/knightSharedSettings.h"
#include "System/knightGameplayTags.h"
#include "Combat/knightGroggyFeedbackComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogknightCameraImpact, Log, All);

namespace knightCamera
{
	template <typename ModifierType>
	ModifierType* FindOrAddModifier(AknightPlayerCameraManager& Camera, TSubclassOf<ModifierType> Class)
	{
		if (!Class)
		{
			return nullptr;
		}
		ModifierType* Modifier = Cast<ModifierType>(Camera.FindCameraModifierByClass(Class));
		return Modifier ? Modifier : Cast<ModifierType>(Camera.AddNewCameraModifier(Class));
	}
}

AknightPlayerCameraManager::AknightPlayerCameraManager()
{
	ViewPitchMin = -70.f;
	ViewPitchMax = 60.f;
	RoarShockwaveClass = UknightCameraModifier_RoarShockwave::StaticClass();
	DarknessClass = UknightCameraModifier_Darkness::StaticClass();
	DeafenClass = UknightCameraModifier_Deafen::StaticClass();
	ImpactSettings = TSoftObjectPtr<UknightCameraImpactSettings>(FSoftObjectPath(TEXT("/Game/Knight/Camera/DA_CameraImpact.DA_CameraImpact")));
	ImpactModifierClass = UknightCameraModifier_Impact::StaticClass();
}

void AknightPlayerCameraManager::BeginPlay()
{
	Super::BeginPlay();
	// The modifiers load their materials and sounds on first use, which stalls on the game thread (and waits for every
	// async load in flight). Load them now, during the raid loading hold.
	TArray<FSoftObjectPath> Paths;
	if (RoarShockwaveClass)
	{
		RoarShockwaveClass.GetDefaultObject()->GatherPreloadAssets(Paths);
	}
	if (DarknessClass)
	{
		DarknessClass.GetDefaultObject()->GatherPreloadAssets(Paths);
	}
	if (DeafenClass)
	{
		DeafenClass.GetDefaultObject()->GatherPreloadAssets(Paths);
	}
	if (!Paths.IsEmpty())
	{
		PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority);
	}
	// 작은 데이터 에셋 하나라 바로 읽는다. 첫 타격 전에 있어야 한다.
	LoadedImpactSettings = ImpactSettings.LoadSynchronous();
}

void AknightPlayerCameraManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindImpactEvents();
	Super::EndPlay(EndPlayReason);
}

void AknightPlayerCameraManager::RefreshImpactBinding()
{
	const APawn* const Pawn = PCOwner ? PCOwner->GetPawn() : nullptr;
	UAbilitySystemComponent* const ASC = LoadedImpactSettings ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) : nullptr;
	if (ASC == ImpactASC.Get())
	{
		return;
	}
	UnbindImpactEvents();
	if (!ASC)
	{
		return;
	}
	ImpactASC = ASC;
	ImpactEffectHandle = ASC->OnGameplayEffectAppliedDelegateToTarget.AddUObject(this, &ThisClass::HandleImpactEffectApplied);
	static const FGameplayTag ParrySucceeded = FGameplayTag::RequestGameplayTag(TEXT("Event.ParrySucceeded"), false);
	for (const FGameplayTag& Tag : { knightGameplayTags::Event_Damaged.GetTag(), knightGameplayTags::Event_PoiseBroken.GetTag(),
		knightGameplayTags::Event_Guarded.GetTag(), knightGameplayTags::Event_GuardBroken.GetTag(), ParrySucceeded })
	{
		if (Tag.IsValid())
		{
			ImpactEventHandles.Emplace(Tag, ASC->GenericGameplayEventCallbacks.FindOrAdd(Tag).AddUObject(this, &ThisClass::HandleImpactEvent));
		}
	}
}

void AknightPlayerCameraManager::UnbindImpactEvents()
{
	if (UAbilitySystemComponent* const ASC = ImpactASC.Get())
	{
		ASC->OnGameplayEffectAppliedDelegateToTarget.Remove(ImpactEffectHandle);
		for (const TPair<FGameplayTag, FDelegateHandle>& Pair : ImpactEventHandles)
		{
			if (FGameplayEventMulticastDelegate* const Delegate = ASC->GenericGameplayEventCallbacks.Find(Pair.Key))
			{
				Delegate->Remove(Pair.Value);
			}
		}
	}
	ImpactASC.Reset();
	ImpactEffectHandle.Reset();
	ImpactEventHandles.Reset();
}

FVector AknightPlayerCameraManager::GetDirectionFrom(const AActor* Source) const
{
	const AActor* const Target = GetViewTarget();
	return Source && Target ? (Target->GetActorLocation() - Source->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
}

void AknightPlayerCameraManager::HandleImpactEffectApplied(UAbilitySystemComponent* Target, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
{
	// 내가 다른 대상에게 근접 피해를 줬다. 근접 타격만 그로기 피해(SetByCaller)를 싣고 오므로 그걸로 가른다(화상 같은 지속 피해는 제외).
	const UknightCameraImpactSettings* const Settings = LoadedImpactSettings;
	if (!Settings || !Target || Target == ImpactASC.Get())
	{
		return;
	}
	const float Poise = Spec.GetSetByCallerMagnitude(knightGameplayTags::SetByCaller_PoiseDamage, false, -1.f);
	if (Poise < 0.f)
	{
		return;
	}
	// 때린 쪽에서 맞은 쪽으로 민다.
	const FVector Direction = -GetDirectionFrom(Target->GetAvatarActor());
	const FknightCameraImpactReaction& Heavy = Settings->PlayerHeavyHit;
	if (Heavy.bEnabled && Poise >= Heavy.MinStrength)
	{
		PlayImpactReaction(TEXT("PlayerHeavyHit"), Heavy, Poise, Direction);
		return;
	}
	PlayImpactReaction(TEXT("PlayerHit"), Settings->PlayerHit, Poise, Direction);
}

void AknightPlayerCameraManager::HandleImpactEvent(const FGameplayEventData* Payload)
{
	const UknightCameraImpactSettings* const Settings = LoadedImpactSettings;
	const UWorld* const World = GetWorld();
	if (!Settings || !Payload || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const FVector Direction = GetDirectionFrom(Payload->Instigator.Get());
	const FGameplayTag Tag = Payload->EventTag;
	if (Tag == knightGameplayTags::Event_Damaged)
	{
		// 가드·패링하고 남은 피해에는 맞은 반응을 겹치지 않는다.
		if (Now - LastDefenseEventTime > Settings->DefenseDamageSuppressTime)
		{
			PlayImpactReaction(TEXT("Damaged"), Settings->Damaged, Payload->EventMagnitude, Direction);
		}
		return;
	}
	if (Tag == knightGameplayTags::Event_PoiseBroken)
	{
		PlayImpactReaction(TEXT("PoiseBroken"), Settings->PoiseBroken, Payload->EventMagnitude, Direction);
		return;
	}
	LastDefenseEventTime = Now;
	if (Tag == knightGameplayTags::Event_Guarded)
	{
		PlayImpactReaction(TEXT("Guarded"), Settings->Guarded, 1.f, Direction);
	}
	else if (Tag == knightGameplayTags::Event_GuardBroken)
	{
		PlayImpactReaction(TEXT("GuardBroken"), Settings->GuardBroken, 1.f, Direction);
	}
	else
	{
		PlayImpactReaction(TEXT("Parried"), Settings->Parried, 1.f, Direction);
	}
}

void AknightPlayerCameraManager::PlayImpactReaction(FName Moment, const FknightCameraImpactReaction& Reaction, float Strength,
	const FVector& Direction, bool bIgnoreGates)
{
	const UWorld* const World = GetWorld();
	if (!World || (!bIgnoreGates && (!Reaction.bEnabled || Strength < Reaction.MinStrength)))
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (!bIgnoreGates)
	{
		double& LastTime = LastImpactTimes.FindOrAdd(Moment, -UE_BIG_NUMBER);
		if (Now - LastTime < Reaction.Cooldown)
		{
			return;
		}
		LastTime = Now;
	}
	const float Alpha = Reaction.FullStrength > Reaction.MinStrength
		? FMath::Clamp((Strength - Reaction.MinStrength) / (Reaction.FullStrength - Reaction.MinStrength), 0.f, 1.f)
		: 1.f;
	// 움직임과 색 갈라짐 모두 화면 흔들림 설정을 따른다(0이면 끈다).
	const float Scale = FMath::Lerp(Reaction.MinScale, 1.f, Alpha) * GetShakeScale();
	if (UknightCameraModifier_Impact* const Modifier = knightCamera::FindOrAddModifier(*this, ImpactModifierClass))
	{
		Modifier->Play(Reaction, Scale, Direction);
		UE_LOG(LogknightCameraImpact, Verbose, TEXT("%s strength %.1f scale %.2f"), *Moment.ToString(), Strength, Scale);
	}
}

void AknightPlayerCameraManager::PreviewImpactReaction(FName Moment, float Strength)
{
	const UknightCameraImpactSettings* const Settings = LoadedImpactSettings ? LoadedImpactSettings.Get() : ImpactSettings.LoadSynchronous();
	const FStructProperty* const Property = Settings ? FindFProperty<FStructProperty>(Settings->GetClass(), Moment) : nullptr;
	if (!Property || Property->Struct != FknightCameraImpactReaction::StaticStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("Camera impact reaction '%s' not found."), *Moment.ToString());
		return;
	}
	const FknightCameraImpactReaction& Reaction = *Property->ContainerPtrToValuePtr<FknightCameraImpactReaction>(Settings);
	const float PreviewStrength = Strength > 0.f ? Strength : FMath::Max3(Reaction.FullStrength, Reaction.MinStrength, 1.f);
	// 미리보기는 카메라가 보는 방향으로 민다.
	PlayImpactReaction(Moment, Reaction, PreviewStrength, GetCameraRotation().Vector().GetSafeNormal2D(), true);
}

void AknightPlayerCameraManager::SetRadialPullSource(UknightGroggyFeedbackComponent* Source)
{
	RadialPullSource = Source;
}

void AknightPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);
	const ULocalPlayer* const LocalPlayer = PCOwner ? PCOwner->GetLocalPlayer() : nullptr;
	UknightSettingsSubsystem* const Settings = LocalPlayer ? LocalPlayer->GetSubsystem<UknightSettingsSubsystem>() : nullptr;
	if (Settings)
	{
		float Offset, Brightness, Gamma;
		Settings->GetEffectiveDisplaySettings(Offset, Brightness, Gamma);
		if (!FMath::IsNearlyZero(Offset))
		{
			// Apply after the active camera mode or menu camera has produced its designed FOV.
			OutVT.POV.FOV = FMath::Clamp(OutVT.POV.FOV + Offset, 60.f, 110.f);
		}
	}

	if (bKnockdownZoomActive)
	{
		if (!KnockdownViewTarget.IsValid() || OutVT.Target != KnockdownViewTarget.Get())
		{
			CancelKnockdownZoom();
			return;
		}
		const double Now = FPlatformTime::Seconds();
		if (bKnockdownZoomReturning && Now - KnockdownZoomReturnStartTime >= KnockdownZoomReturn)
		{
			CancelKnockdownZoom();
			return;
		}
		// OutVT contains this frame's camera mode and user FOV; never subtract from last frame's zoomed FOV.
		const float ZoomWeight = GetKnockdownZoomWeight(Now);
		const float ClampedWeight = FMath::Clamp(ZoomWeight, 0.f, 1.f);
		const float BaseFOV = OutVT.POV.FOV;
		OutVT.POV.FOV = FMath::Clamp(BaseFOV - GetKnockdownZoomDegrees(Now) * ZoomWeight, FMath::Min(BaseFOV, 64.f), 115.f);

		// 시네마틱 로우-와이드 구도 보정: FOV를 좁혀 집중감을 주되, 카메라를 살짝 뒤로 물리고 낮춰
		// 캐릭터의 머리만 보이지 않고 전신 실루엣과 대검 궤적·보스가 한 프레임에 묵직하게 담기도록 한다.
		if (ClampedWeight > KINDA_SMALL_NUMBER && (KnockdownFramingPullBack > 0.f || !FMath::IsNearlyZero(KnockdownFramingHeightOffset)))
		{
			const FRotationMatrix ViewAxes(OutVT.POV.Rotation);
			FVector DesiredLoc = OutVT.POV.Location
				- ViewAxes.GetUnitAxis(EAxis::X) * (KnockdownFramingPullBack * ClampedWeight)
				+ FVector(0.f, 0.f, KnockdownFramingHeightOffset * ClampedWeight);

			if (UWorld* const World = GetWorld())
			{
				FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(KnightGroggyFraming), false, OutVT.Target);
				FHitResult Hit;
				if (World->SweepSingleByChannel(Hit, OutVT.POV.Location, DesiredLoc, FQuat::Identity, ECC_Camera,
					FCollisionShape::MakeSphere(12.f), TraceParams))
				{
					DesiredLoc = Hit.Location;
				}
			}
			OutVT.POV.Location = DesiredLoc;
			OutVT.POV.Rotation.Pitch = FMath::ClampAngle(
				OutVT.POV.Rotation.Pitch + KnockdownFramingPitchOffset * ClampedWeight, ViewPitchMin, ViewPitchMax);
		}
	}
	if (UknightGroggyFeedbackComponent* const Pull = RadialPullSource.Get())
	{
		// Compose on this frame's underlying view (user FOV, lock-on and existing zoom).
		const float BaseFOV = OutVT.POV.FOV;
		OutVT.POV.FOV = FMath::Max(FMath::Min(BaseFOV, 62.f), BaseFOV - Pull->GetRadialPullFOVOffset());
	}
	// 전투 순간 카메라 반응의 시야각은 사용자 FOV 보정과 다른 줌을 모두 더한 뒤에 뺀다(110도 자르기에 묻히지 않게).
	if (const UknightCameraModifier_Impact* const Impact = ImpactModifierClass
		? Cast<UknightCameraModifier_Impact>(FindCameraModifierByClass(ImpactModifierClass)) : nullptr)
	{
		OutVT.POV.FOV = FMath::Clamp(OutVT.POV.FOV - Impact->GetFOVDelta(), 5.f, 170.f);
	}
}

float AknightPlayerCameraManager::GetKnockdownZoomDegrees(double Now) const
{
	if (!bKnockdownPunched || KnockdownPunchDegrees <= 0.f)
	{
		return KnockdownZoomDegrees;
	}
	const float Alpha = KnockdownPunchTime > 0.f
		? FMath::Clamp(static_cast<float>((Now - KnockdownPunchStartTime) / KnockdownPunchTime), 0.f, 1.f) : 1.f;
	// 확 당기는 느낌: 처음에 거의 다 가고 끝에 멈춘다.
	return KnockdownZoomDegrees + KnockdownPunchDegrees * (1.f - FMath::Square(1.f - Alpha));
}

void AknightPlayerCameraManager::PunchKnockdownZoom(float ExtraDegrees, float Seconds)
{
	if (!bKnockdownZoomActive || bKnockdownZoomReturning || ExtraDegrees <= 0.f)
	{
		return;
	}
	bKnockdownPunched = true;
	KnockdownPunchDegrees = ExtraDegrees;
	KnockdownPunchTime = FMath::Max(Seconds, 0.f);
	KnockdownPunchStartTime = FPlatformTime::Seconds();
}

float AknightPlayerCameraManager::GetKnockdownZoomWeight(double Now) const
{
	if (bKnockdownZoomReturning)
	{
		const float Alpha = KnockdownZoomReturn > 0.f
			? FMath::Clamp(static_cast<float>((Now - KnockdownZoomReturnStartTime) / KnockdownZoomReturn), 0.f, 1.f) : 1.f;
		// Explosive snap-out on release with a subtle shockwave wide-FOV rebound (-0.12) before settling.
		const float FastOut = 1.f - FMath::Pow(1.f - Alpha, 2.6f);
		const float Rebound = FMath::Sin(Alpha * PI) * (1.f - Alpha) * 0.14f;
		return KnockdownZoomReturnStartWeight * ((1.f - FastOut) - Rebound);
	}
	const float Alpha = KnockdownZoomEnter > 0.f
		? FMath::Clamp(static_cast<float>((Now - KnockdownZoomStartTime) / KnockdownZoomEnter), 0.f, 1.f) : 1.f;
	return Alpha * Alpha * (3.f - 2.f * Alpha);
}

void AknightPlayerCameraManager::StartKnockdownZoom(AActor* InViewTarget, float Degrees, float EnterSeconds, double StartRealTime)
{
	CancelKnockdownZoom();
	if (!InViewTarget || Degrees <= 0.f)
	{
		return;
	}
	KnockdownViewTarget = InViewTarget;
	KnockdownZoomDegrees = Degrees;
	KnockdownZoomEnter = FMath::Max(EnterSeconds, 0.f);
	KnockdownZoomStartTime = StartRealTime;
	bKnockdownZoomActive = true;
}

void AknightPlayerCameraManager::ReturnKnockdownZoom(float ReturnSeconds)
{
	if (!bKnockdownZoomActive || bKnockdownZoomReturning)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	KnockdownZoomReturnStartWeight = GetKnockdownZoomWeight(Now);
	KnockdownZoomReturnStartTime = Now;
	KnockdownZoomReturn = FMath::Max(ReturnSeconds, 0.f);
	bKnockdownZoomReturning = true;
}

void AknightPlayerCameraManager::CancelKnockdownZoom()
{
	bKnockdownZoomActive = false;
	bKnockdownZoomReturning = false;
	bKnockdownPunched = false;
	KnockdownViewTarget.Reset();
}

void AknightPlayerCameraManager::UpdateCamera(float DeltaTime)
{
	RefreshImpactBinding();
	Super::UpdateCamera(DeltaTime);

	if (bKnockdownZoomActive)
	{
		const float ShakeScale = FMath::Clamp(GetShakeScale(), 0.f, 2.f);
		if (ShakeScale > KINDA_SMALL_NUMBER)
		{
			const double Now = FPlatformTime::Seconds();
			FPostProcessSettings ImpactPP;
			if (!bKnockdownZoomReturning)
			{
				const float Weight = FMath::Clamp(GetKnockdownZoomWeight(Now), 0.f, 1.f) * ShakeScale;
				ImpactPP.bOverride_VignetteIntensity = true;
				ImpactPP.VignetteIntensity = FMath::Lerp(0.4f, 0.72f, Weight);
				ImpactPP.bOverride_SceneFringeIntensity = true;
				ImpactPP.SceneFringeIntensity = 2.4f * Weight;
				// (2026-10-20) 슬로 동안 색을 빼서(집중) 맞는 순간 색이 터지게 한다. 맞은 뒤(펀치~멈춤)에는 채도·대비를 올리고 색수차를 크게.
				ImpactPP.bOverride_ColorSaturation = true;
				if (bKnockdownPunched)
				{
					ImpactPP.ColorSaturation = FVector4(1.25f, 1.25f, 1.25f, 1.f);
					ImpactPP.bOverride_ColorContrast = true;
					ImpactPP.ColorContrast = FVector4(1.15f, 1.15f, 1.15f, 1.f);
					ImpactPP.SceneFringeIntensity = 4.5f * Weight;
				}
				else
				{
					const float Sat = FMath::Lerp(1.f, 0.55f, Weight);
					ImpactPP.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
				}
				AddCachedPPBlend(ImpactPP, FMath::Clamp(Weight, 0.f, 1.f), VTBlendOrder_Override);
			}
			else
			{
				const float ReturnAlpha = KnockdownZoomReturn > 0.f
					? FMath::Clamp(static_cast<float>((Now - KnockdownZoomReturnStartTime) / KnockdownZoomReturn), 0.f, 1.f) : 1.f;
				const float Pulse = FMath::Pow(1.f - ReturnAlpha, 1.4f) * KnockdownZoomReturnStartWeight * ShakeScale;
				if (Pulse > KINDA_SMALL_NUMBER)
				{
					ImpactPP.bOverride_SceneFringeIntensity = true;
					ImpactPP.SceneFringeIntensity = 5.0f * Pulse;
					ImpactPP.bOverride_VignetteIntensity = true;
					ImpactPP.VignetteIntensity = FMath::Lerp(0.4f, 0.68f, Pulse);
					AddCachedPPBlend(ImpactPP, FMath::Clamp(Pulse, 0.f, 1.f), VTBlendOrder_Override);
				}
			}
		}
	}

	// Brightness/gamma compose on the final game view after all volume/camera blends.
	// A camera PP override would replace authored grading and depend on camera ticks.
}

float AknightPlayerCameraManager::GetShakeScale() const
{
	const ULocalPlayer* const LocalPlayer = PCOwner ? PCOwner->GetLocalPlayer() : nullptr;
	UknightSettingsSubsystem* const Settings = LocalPlayer ? LocalPlayer->GetSubsystem<UknightSettingsSubsystem>() : nullptr;
	const UknightSharedSettings* const Shared = Settings ? Settings->GetSharedSettings() : nullptr;
	return Shared ? Shared->CameraShakeScale : 1.f;
}

UCameraShakeBase* AknightPlayerCameraManager::StartCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, const FAddCameraShakeParams& Params)
{
	// Every shake (gameplay cues, abilities, the helpers below) passes through here.
	FAddCameraShakeParams Scaled = Params;
	Scaled.Scale *= GetShakeScale();
	if (Scaled.Scale <= 0.f)
	{
		return nullptr;
	}
	// 몬스터 큰 공격의 착지 셰이크에는 카메라 반응을 같이 건다. 세기는 설정 배율을 곱하기 전 값(거리 감쇠만 반영).
	if (LoadedImpactSettings && ShakeClass && ShakeClass == LoadedImpactSettings->MonsterImpactShake)
	{
		PlayImpactReaction(TEXT("MonsterImpact"), LoadedImpactSettings->MonsterImpact, Params.Scale, FVector::ZeroVector);
	}
	return Super::StartCameraShake(ShakeClass, Scaled);
}

void AknightPlayerCameraManager::PlayHitShake(float Intensity)
{
	if (HitShake && Intensity > 0.f)
	{
		StartCameraShake(HitShake, Intensity);
	}
}

void AknightPlayerCameraManager::PlayHeavyImpactShake(float Intensity)
{
	if (HeavyImpactShake && Intensity > 0.f)
	{
		StartCameraShake(HeavyImpactShake, Intensity);
	}
}

void AknightPlayerCameraManager::StartRoarShockwave(USceneComponent* Source, FName Socket)
{
	if (!RoarShockwaveClass)
	{
		return;
	}
	UknightCameraModifier_RoarShockwave* Modifier = Cast<UknightCameraModifier_RoarShockwave>(FindCameraModifierByClass(RoarShockwaveClass));
	if (!Modifier)
	{
		Modifier = Cast<UknightCameraModifier_RoarShockwave>(AddNewCameraModifier(RoarShockwaveClass));
	}
	if (Modifier)
	{
		Modifier->StartRoar(Source, Socket);
	}
}

void AknightPlayerCameraManager::StopRoarShockwave(const USceneComponent* Source)
{
	if (UknightCameraModifier_RoarShockwave* const Modifier = RoarShockwaveClass ? Cast<UknightCameraModifier_RoarShockwave>(FindCameraModifierByClass(RoarShockwaveClass)) : nullptr)
	{
		Modifier->StopRoar(Source);
	}
}

void AknightPlayerCameraManager::StartDarkness(UPrimitiveComponent* Source)
{
	if (UknightCameraModifier_Darkness* const Modifier = knightCamera::FindOrAddModifier(*this, DarknessClass))
	{
		Modifier->StartDarkness(Source);
	}
	SetLockOnBlocked(true);
}

void AknightPlayerCameraManager::StopDarkness(const UPrimitiveComponent* Source)
{
	if (UknightCameraModifier_Darkness* const Modifier = DarknessClass ? Cast<UknightCameraModifier_Darkness>(FindCameraModifierByClass(DarknessClass)) : nullptr)
	{
		Modifier->StopDarkness(Source);
	}
	SetLockOnBlocked(false);
}

void AknightPlayerCameraManager::SetLockOnBlocked(bool bBlocked) const
{
	const APawn* const Pawn = PCOwner ? PCOwner->GetPawn() : nullptr;
	if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
	{
		ASC->SetLooseGameplayTagCount(knightGameplayTags::State_LockOn_Blocked, bBlocked ? 1 : 0);
	}
}

void AknightPlayerCameraManager::SetDarknessSilhouette(const UPrimitiveComponent* Source, bool bVisible)
{
	if (UknightCameraModifier_Darkness* const Modifier = DarknessClass ? Cast<UknightCameraModifier_Darkness>(FindCameraModifierByClass(DarknessClass)) : nullptr)
	{
		Modifier->SetSilhouette(Source, bVisible);
	}
}

void AknightPlayerCameraManager::SetDarknessWarning(const UPrimitiveComponent* Source, const FVector& Location, float Strength)
{
	if (UknightCameraModifier_Darkness* const Modifier = DarknessClass ? Cast<UknightCameraModifier_Darkness>(FindCameraModifierByClass(DarknessClass)) : nullptr)
	{
		Modifier->SetWarning(Source, Location, Strength);
	}
}

void AknightPlayerCameraManager::StartDeafen(float Seconds)
{
	if (UknightCameraModifier_Deafen* const Modifier = knightCamera::FindOrAddModifier(*this, DeafenClass))
	{
		Modifier->StartDeafen(Seconds);
	}
}

void AknightPlayerCameraManager::StartSonicNearMiss()
{
	if (UknightCameraModifier_Deafen* const Modifier = knightCamera::FindOrAddModifier(*this, DeafenClass))
	{
		Modifier->StartNearMiss();
	}
}

void AknightPlayerCameraManager::HoldSonicJitter()
{
	if (UknightCameraModifier_Deafen* const Modifier = knightCamera::FindOrAddModifier(*this, DeafenClass))
	{
		Modifier->HoldJitter();
	}
}

void AknightPlayerCameraManager::ForEachLocalCamera(const UObject* WorldContext, TFunctionRef<void(AknightPlayerCameraManager&)> Function)
{
	UWorld* const World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* const PC = It->Get();
		if (AknightPlayerCameraManager* const Camera = PC && PC->IsLocalController() ? Cast<AknightPlayerCameraManager>(PC->PlayerCameraManager) : nullptr)
		{
			Function(*Camera);
		}
	}
}
