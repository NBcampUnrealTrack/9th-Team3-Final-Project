#include "Camera/knightCameraComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/knightCameraFadeHandler.h"
#include "Camera/knightCameraMode.h"
#include "Camera/knightCameraOcclusionHandler.h"
#include "Engine/World.h"
// (2026-10-21) 임시 진단 로그용. 원인 확인 후 지운다.
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/knightTargetLockComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Raid/knightRaidGameState.h"
#include "knight.h"

namespace
{
	// (2026-10-21) 시작 카메라·락온 조준점 문제 확인용 임시 로그. 1이면 0.25초마다 한 줄.
	TAutoConsoleVariable<int32> CVarCameraDebugLog(TEXT("knight.Debug.CameraLog"), 1,
		TEXT("1: log camera mode, view and lock-on point every 0.25s (temporary diagnostics)."), ECVF_Cheat);
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

	// Keep the component transform in sync so anything reading it (audio listener, debug) agrees with the view.
	SetWorldLocationAndRotation(CameraLocation, ModeView.Rotation);
	FieldOfView = ModeView.FieldOfView;

	DesiredView.Location = CameraLocation;
	DesiredView.Rotation = ModeView.Rotation;
	DesiredView.FOV = ModeView.FieldOfView;

	if (CVarCameraDebugLog.GetValueOnGameThread() != 0)
	{
		static double LastLogTime = 0.0;
		const UWorld* const World = GetWorld();
		const double Now = World ? World->GetRealTimeSeconds() : 0.0;
		if (World && (Now - LastLogTime >= 0.25 || Now < LastLogTime))
		{
			LastLogTime = Now;
			const AActor* const Owner = GetOwner();
			const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
			const AknightRaidGameState* const Raid = World->GetGameState<AknightRaidGameState>();
			FGameplayTagContainer Owned;
			if (ASC)
			{
				ASC->GetOwnedGameplayTags(Owned);
			}
			const UknightCameraMode* const Top = GetActiveCameraMode();
			FString LockText = TEXT("none");
			const UknightTargetLockComponent* const Lock = Owner ? Owner->FindComponentByClass<UknightTargetLockComponent>() : nullptr;
			FVector LockPoint;
			if (Lock && Lock->GetLockedTargetLocation(LockPoint))
			{
				const AActor* const Target = Lock->GetLockedTarget();
				const USceneComponent* const LockComp = Lock->GetLockedComponent();
				const ACharacter* const TargetChar = Cast<ACharacter>(Target);
				const USkeletalMeshComponent* const TargetMesh = TargetChar ? TargetChar->GetMesh() : nullptr;
				FVector2D Screen(-1.f, -1.f);
				if (const APlayerController* const PC = Cast<APlayerController>(Cast<APawn>(Owner) ? Cast<APawn>(Owner)->GetController() : nullptr))
				{
					PC->ProjectWorldLocationToScreen(LockPoint, Screen);
				}
				LockText = FString::Printf(TEXT("%s comp %s sock %s pt %s actor %s meshLoc %s meshBounds %s headBone %s anim %s montage %s screen %.0f,%.0f"),
					*GetNameSafe(Target), *GetNameSafe(LockComp), *Lock->GetLockedSocket().ToString(), *LockPoint.ToCompactString(),
					Target ? *Target->GetActorLocation().ToCompactString() : TEXT("-"),
					TargetMesh ? *TargetMesh->GetComponentLocation().ToCompactString() : TEXT("-"),
					TargetMesh ? *TargetMesh->Bounds.Origin.ToCompactString() : TEXT("-"),
					TargetMesh ? *TargetMesh->GetBoneLocation(Lock->GetLockedSocket()).ToCompactString() : TEXT("-"),
					TargetMesh && TargetMesh->GetAnimInstance() ? *GetNameSafe(TargetMesh->GetAnimInstance()->GetClass()) : TEXT("-"),
					TargetMesh && TargetMesh->GetAnimInstance() ? *GetNameSafe(TargetMesh->GetAnimInstance()->GetCurrentActiveMontage()) : TEXT("-"),
					Screen.X, Screen.Y);
			}
			UE_LOG(Logknight, Log, TEXT("CamDbg t=%.2f phase=%d mode=%s arm=%.0f fov=%.1f pitch=%.1f tags=[%s] lock: %s"),
				World->GetTimeSeconds(), Raid ? static_cast<int32>(Raid->GetRaidPhase()) : -1, *GetNameSafe(Top ? Top->GetClass() : nullptr),
				FVector::Dist(ModeView.PivotLocation, CameraLocation), ModeView.FieldOfView, ModeView.Rotation.Pitch,
				*Owned.ToStringSimple(), *LockText);
		}
	}
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
