#include "Camera/knightCameraComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/knightCameraFadeHandler.h"
#include "Camera/knightCameraMode.h"
#include "Camera/knightCameraOcclusionHandler.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// (2026-10-23) 공격 휘두르기 카메라 연출 켜고 끄기(PIE에서 비교용). BP의 bAttackCameraKick과 둘 다 켜져야 동작한다.
	TAutoConsoleVariable<int32> CVarAttackCameraKick(TEXT("knight.Camera.AttackKick"), 1,
		TEXT("1 = 공격 휘두르기 카메라 연출(준비 동작 조임, 휘두를 때 시야각 튐·앞으로 밀기), 0 = 끔."));
}

void UknightCameraComponent::StartAttackWindup(float Strength)
{
	const UWorld* const World = GetWorld();
	if (!World || !bAttackCameraKick || CVarAttackCameraKick.GetValueOnGameThread() == 0)
	{
		return;
	}
	WindupStartTime = World->GetRealTimeSeconds();
	WindupStrength = FMath::Max(0.f, Strength);
}

void UknightCameraComponent::StartAttackSwing(float Strength)
{
	const UWorld* const World = GetWorld();
	if (!World || !bAttackCameraKick || CVarAttackCameraKick.GetValueOnGameThread() == 0)
	{
		return;
	}
	WindupStartTime = -1.0;
	SwingStartTime = World->GetRealTimeSeconds();
	SwingStrength = FMath::Max(0.f, Strength);
}

void UknightCameraComponent::EvaluateAttackKick(float& OutFOV, float& OutPush) const
{
	OutFOV = 0.f;
	OutPush = 0.f;
	const UWorld* const World = GetWorld();
	if (!World || !bAttackCameraKick || CVarAttackCameraKick.GetValueOnGameThread() == 0)
	{
		return;
	}
	// 실제 시간 기준: 히트스탑·그로기 슬로 중에도 연출 길이가 바뀌지 않게.
	const double Now = World->GetRealTimeSeconds();
	if (WindupStartTime >= 0.0 && Now - WindupStartTime < WindupTimeout)
	{
		const float Blend = FMath::Clamp(static_cast<float>((Now - WindupStartTime) / WindupBlendTime), 0.f, 1.f);
		OutFOV += WindupFOV * WindupStrength * Blend;
	}
	if (SwingStartTime >= 0.0 && Now - SwingStartTime < SwingKickDuration)
	{
		// 휘두르는 순간 가장 크고, 남은 시간의 제곱으로 빠르게 돌아온다.
		const float Remaining = 1.f - static_cast<float>((Now - SwingStartTime) / SwingKickDuration);
		const float Kick = SwingStrength * Remaining * Remaining;
		OutFOV += SwingFOV * Kick;
		OutPush += SwingPush * Kick;
	}
}

UknightCameraComponent::UknightCameraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OcclusionHandler = CreateDefaultSubobject<UknightCameraOcclusionHandler>(TEXT("OcclusionHandler"));
	FadeHandler = CreateDefaultSubobject<UknightCameraFadeHandler>(TEXT("FadeHandler"));
}

void UknightCameraComponent::OnRegister()
{
	Super::OnRegister();
	if (!ModeStack)
	{
		ModeStack = NewObject<UknightCameraModeStack>(this);
	}
}

void UknightCameraComponent::OnUnregister()
{
	if (FadeHandler)
	{
		FadeHandler->RestoreAll();
	}
	Super::OnUnregister();
}

void UknightCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	if (!DefaultCameraMode || !ModeStack)
	{
		Super::GetCameraView(DeltaTime, DesiredView);
		return;
	}

	ModeStack->PushCameraMode(DetermineCameraMode());
	FknightCameraModeView ModeView;
	if (!ModeStack->EvaluateStack(DeltaTime, ModeView))
	{
		Super::GetCameraView(DeltaTime, DesiredView);
		return;
	}

	const FVector CameraLocation = OcclusionHandler
		? OcclusionHandler->ResolveCameraLocation(GetOwner(), ModeView.PivotLocation, ModeView.Location, DeltaTime)
		: ModeView.Location;
	if (FadeHandler)
	{
		FadeHandler->UpdateFade(GetOwner(), ModeView.PivotLocation, CameraLocation, DeltaTime);
	}

	// (2026-10-23) 벽에 막혀 당겨진 만큼 시야각을 넓히고, 공격 휘두르기 연출(시야각 + 앞으로 밀기)을 더한다.
	float KickFOV = 0.f;
	float KickPush = 0.f;
	EvaluateAttackKick(KickFOV, KickPush);
	const float ViewFOV = FMath::Clamp(ModeView.FieldOfView + (OcclusionHandler ? OcclusionHandler->GetCloseUpExtraFOV() : 0.f) + KickFOV, 5.f, 170.f);
	const FVector ViewLocation = CameraLocation + ModeView.Rotation.Vector() * KickPush;

	// Keep the component transform in sync so anything reading it (audio listener, debug) agrees with the view.
	SetWorldLocationAndRotation(ViewLocation, ModeView.Rotation);
	FieldOfView = ViewFOV;

	DesiredView.Location = ViewLocation;
	DesiredView.Rotation = ModeView.Rotation;
	DesiredView.FOV = ViewFOV;

	DesiredView.OrthoWidth = OrthoWidth;
	DesiredView.OrthoNearClipPlane = OrthoNearClipPlane;
	DesiredView.OrthoFarClipPlane = OrthoFarClipPlane;
	DesiredView.AspectRatio = AspectRatio;
	DesiredView.bConstrainAspectRatio = bConstrainAspectRatio;
	DesiredView.bUseFieldOfViewForLOD = bUseFieldOfViewForLOD;
	DesiredView.ProjectionMode = ProjectionMode;
	DesiredView.PostProcessBlendWeight = PostProcessBlendWeight;
	if (PostProcessBlendWeight > 0.f)
	{
		DesiredView.PostProcessSettings = PostProcessSettings;
	}
}

TSubclassOf<UknightCameraMode> UknightCameraComponent::DetermineCameraMode() const
{
	const UWorld* const World = GetWorld();
	if (TemporaryMode && World && World->GetTimeSeconds() < TemporaryModeEndTime)
	{
		return TemporaryMode;
	}
	if (const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		for (const FknightCameraModeRule& Rule : CameraModeRules)
		{
			if (Rule.CameraMode && Rule.RequiredTag.IsValid() && ASC->HasMatchingGameplayTag(Rule.RequiredTag))
			{
				return Rule.CameraMode;
			}
		}
	}
	return DefaultCameraMode;
}

void UknightCameraComponent::SetTemporaryCameraMode(TSubclassOf<UknightCameraMode> CameraMode, float Duration)
{
	const UWorld* const World = GetWorld();
	if (!CameraMode || !World || Duration <= 0.f)
	{
		return;
	}
	TemporaryMode = CameraMode;
	TemporaryModeEndTime = World->GetTimeSeconds() + Duration;
}

void UknightCameraComponent::ClearTemporaryCameraMode()
{
	TemporaryMode = nullptr;
	TemporaryModeEndTime = 0.0;
}

UknightCameraMode* UknightCameraComponent::GetActiveCameraMode() const
{
	return ModeStack ? ModeStack->GetTopMode() : nullptr;
}
