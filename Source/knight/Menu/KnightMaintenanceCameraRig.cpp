#include "Menu/KnightMaintenanceCameraRig.h"
#include "Camera/CameraActor.h"

AKnightMaintenanceCameraRig::AKnightMaintenanceCameraRig()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AKnightMaintenanceCameraRig::SetMaintenanceView(bool bOpen)
{
	if (!ControlledCamera || (bOpen == bOpening && (bMoving || bHasOriginal))) return;
	if (bOpen && !bHasOriginal)
	{
		OriginalTransform = ControlledCamera->GetActorTransform();
		bHasOriginal = true;
	}
	if (!bHasOriginal) return;
	bOpening = bOpen;
	FromTransform = ControlledCamera->GetActorTransform();
	ToTransform = bOpen ? FTransform(MaintenanceWorldRotation, MaintenanceWorldLocation) : OriginalTransform;
	Elapsed = 0.f;
	bMoving = true;
	SetActorTickEnabled(true);
}

void AKnightMaintenanceCameraRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bMoving || !ControlledCamera) { SetActorTickEnabled(false); return; }
	Elapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(Elapsed / FMath::Max(TransitionSeconds, 0.01f), 0.f, 1.f);
	const float SmoothAlpha = Alpha * Alpha * (3.f - 2.f * Alpha);
	FTransform Blended;
	Blended.Blend(FromTransform, ToTransform, SmoothAlpha);
	ControlledCamera->SetActorTransform(Blended);
	if (Alpha >= 1.f)
	{
		bMoving = false;
		if (!bOpening) bHasOriginal = false;
		SetActorTickEnabled(false);
	}
}
