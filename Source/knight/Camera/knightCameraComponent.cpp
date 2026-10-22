#include "Camera/knightCameraComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/knightCameraFadeHandler.h"
#include "Camera/knightCameraMode.h"
#include "Camera/knightCameraOcclusionHandler.h"
#include "Engine/World.h"

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

	// Keep the component transform in sync so anything reading it (audio listener, debug) agrees with the view.
	SetWorldLocationAndRotation(CameraLocation, ModeView.Rotation);
	FieldOfView = ModeView.FieldOfView;

	DesiredView.Location = CameraLocation;
	DesiredView.Rotation = ModeView.Rotation;
	DesiredView.FOV = ModeView.FieldOfView;

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
