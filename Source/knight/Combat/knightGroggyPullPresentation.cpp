#include "Combat/knightGroggyFeedbackComponent.h"

#include "Camera/knightPlayerCameraManager.h"
#include "Camera/knightRadialPullMath.h"
#include "knightCharacter.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Raid/knightRaidGameState.h"

// SetupView uses the real, final view projection (including constrained aspect ratio,
// local-player split rect and FOV modifiers), not desktop/window dimensions.
class FknightGroggyPullViewExtension final : public FSceneViewExtensionBase
{
public:
	FknightGroggyPullViewExtension(const FAutoRegister& Auto, UknightGroggyFeedbackComponent* InOwner)
		: FSceneViewExtensionBase(Auto), Owner(InOwner) {}
	virtual void SetupView(FSceneViewFamily& Family, FSceneView& View) override
	{
		const APawn* Pawn = Owner.IsValid() ? Cast<APawn>(Owner->GetOwner()) : nullptr;
		const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		const ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
		if (Player && Player->ViewportClient && Family.RenderTarget == Player->ViewportClient->Viewport
			&& View.PlayerIndex == Player->GetControllerId()) Owner->ApplyRadialPullView(View);
	}
protected:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
	{
		const APawn* Pawn = Owner.IsValid() ? Cast<APawn>(Owner->GetOwner()) : nullptr;
		const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		const ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
		return Player && Player->ViewportClient && Player->ViewportClient->Viewport
			&& Context.Viewport == Player->ViewportClient->Viewport;
	}
private:
	TWeakObjectPtr<UknightGroggyFeedbackComponent> Owner;
};

void UknightGroggyFeedbackComponent::InitializeRadialPull()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (!RadialPullViewExtension)
		RadialPullViewExtension = FSceneViewExtensions::NewExtension<FknightGroggyPullViewExtension>(this);
	if (!RadialPullMID)
		if (UMaterialInterface* Base = RadialPullMaterial.Get())
			RadialPullMID = UMaterialInstanceDynamic::Create(Base, this);
}

void UknightGroggyFeedbackComponent::ShutdownRadialPull()
{
	CancelRadialPull();
	RadialPullViewExtension.Reset();
	RadialPullMID = nullptr;
}

void UknightGroggyFeedbackComponent::PlayRadialPull(AActor* Target, const FVector& WorldFocus)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	AknightPlayerCameraManager* Camera = PC ? Cast<AknightPlayerCameraManager>(PC->PlayerCameraManager) : nullptr;
	if (!Camera || !PC->IsLocalController() || !Target || RadialPullDuration <= 0.f) return;
	InitializeRadialPull();
	// At most one bounded pulse: retrigger from its current weight, never sum effects.
	RadialPullStartWeight = GetRadialPullWeight();
	RadialPullStartTime = FPlatformTime::Seconds();
	RadialPullTarget = Target;
	RadialPullViewTarget = PC->GetViewTarget();
	RadialPullWorldFocus = WorldFocus.ContainsNaN() ? Target->GetTargetLocation(GetOwner()) : WorldFocus;
	Camera->SetRadialPullSource(this);
}

void UknightGroggyFeedbackComponent::CancelRadialPull()
{
	RadialPullStartTime = -1.0;
	RadialPullStartWeight = 0.f;
	RadialPullTarget.Reset();
	RadialPullViewTarget.Reset();
	if (RadialPullMID) RadialPullMID->SetScalarParameterValue(TEXT("Amount"), 0.f);
}

float UknightGroggyFeedbackComponent::GetRadialPullWeight()
{
	if (RadialPullStartTime < 0.0) return 0.f;
	const UWorld* World = GetWorld();
	const AknightCharacter* Pawn = Cast<AknightCharacter>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const AknightMonsterCharacterBase* Boss = Cast<AknightMonsterCharacterBase>(RadialPullTarget.Get());
	const AknightRaidGameState* Raid = World ? World->GetGameState<AknightRaidGameState>() : nullptr;
	const EknightRaidPhase Phase = Raid ? Raid->GetRaidPhase() : EknightRaidPhase::Combat;
	const double Age = FPlatformTime::Seconds() - RadialPullStartTime;
	if (!World || World->IsPaused() || !Pawn || Pawn->IsDead() || !PC || !PC->IsLocalController()
		|| !RadialPullTarget.IsValid() || (Boss && Boss->IsDead())
		|| PC->GetViewTarget() != RadialPullViewTarget.Get() || Age >= RadialPullDuration
		|| (Phase != EknightRaidPhase::Explore && Phase != EknightRaidPhase::Combat))
	{
		CancelRadialPull();
		return 0.f;
	}
	return KnightRadialPull::Envelope(Age, RadialPullDuration, RadialPullAttackTime, RadialPullReleaseTime, RadialPullStartWeight);
}

float UknightGroggyFeedbackComponent::GetRadialPullFOVOffset()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const AknightPlayerCameraManager* Camera = PC ? Cast<AknightPlayerCameraManager>(PC->PlayerCameraManager) : nullptr;
	return Camera ? GetRadialPullWeight() * FMath::Clamp(RadialPullFOVDegrees, 0.f, 10.f)
		* FMath::Clamp(Camera->GetShakeScale(), 0.f, 2.f) : 0.f;
}

void UknightGroggyFeedbackComponent::ApplyRadialPullView(FSceneView& View)
{
	const float Weight = GetRadialPullWeight();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const AknightPlayerCameraManager* Camera = PC ? Cast<AknightPlayerCameraManager>(PC->PlayerCameraManager) : nullptr;
	const float Scale = Camera ? FMath::Clamp(Camera->GetShakeScale(), 0.f, 2.f) : 0.f;
	if (Weight <= 0.f || Scale <= 0.f || !RadialPullMID) return;
	// NDC maps directly to this view's 0..1 rectangle, without including letterbox bars.
	const FVector2D Focus = KnightRadialPull::ProjectFocus(RadialPullWorldFocus, View.ViewMatrices.GetWorldToClip());
	const float Aspect = float(View.UnscaledViewRect.Width()) / FMath::Max(View.UnscaledViewRect.Height(), 1);
	RadialPullMID->SetVectorParameterValue(TEXT("Focus"), FLinearColor(Focus.X, Focus.Y, Aspect, 0.f));
	RadialPullMID->SetScalarParameterValue(TEXT("Amount"), Weight);
	RadialPullMID->SetScalarParameterValue(TEXT("Radius"), FMath::Clamp(RadialPullClearRadius, 0.f, 1.f));
	RadialPullMID->SetScalarParameterValue(TEXT("Strength"), FMath::Clamp(RadialPullBlurStrength * Scale, 0.f, .12f));
	// UE's blendable interface reuses its per-view MID; this adds one node to the fresh
	// view, after camera/volume blending, preserving all existing blendables and grading.
	RadialPullMID->OverrideBlendableSettings(View, 1.f);
}
