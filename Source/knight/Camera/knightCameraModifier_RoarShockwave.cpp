#include "Camera/knightCameraModifier_RoarShockwave.h"

#include "Camera/knightPlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

UknightCameraModifier_RoarShockwave::UknightCameraModifier_RoarShockwave()
{
	Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Knight/Camera/PostProcess/M_PP_RoarShockwave.M_PP_RoarShockwave")));
}

void UknightCameraModifier_RoarShockwave::StartRoar(USceneComponent* Source, FName Socket)
{
	RoarSource = Source;
	RoarSocket = Socket;
	bRoaring = true;
	SinceLastPulse = 0.f;
	PulsesSent = 0;
	Envelope = 1.f;
	SendPulse();
}

void UknightCameraModifier_RoarShockwave::StopRoar(const USceneComponent* Source)
{
	if (RoarSource.Get() == Source)
	{
		bRoaring = false;
	}
}

void UknightCameraModifier_RoarShockwave::SendPulse()
{
	if (Pulses.Num() >= MaxPulses)
	{
		Pulses.RemoveAt(0);
	}
	Pulses.Add({ 0.f, FMath::Pow(PulseDecay, static_cast<float>(PulsesSent)) });
	++PulsesSent;
}

bool UknightCameraModifier_RoarShockwave::IsVisible() const
{
	return !Pulses.IsEmpty() || Envelope > 0.f;
}

float UknightCameraModifier_RoarShockwave::GetStrengthScale(const FMinimalViewInfo& POV) const
{
	const AknightPlayerCameraManager* const KnightCamera = Cast<AknightPlayerCameraManager>(CameraOwner);
	const float SettingScale = KnightCamera ? KnightCamera->GetShakeScale() : 1.f;
	const float Meters = FVector::Dist(LastSourceLocation, POV.Location) / 100.f;
	const float Range = FMath::Max(MinStrengthDistance - FullStrengthDistance, KINDA_SMALL_NUMBER);
	const float DistanceScale = FMath::Lerp(1.f, MinDistanceScale, FMath::Clamp((Meters - FullStrengthDistance) / Range, 0.f, 1.f));
	return SettingScale * DistanceScale;
}

float UknightCameraModifier_RoarShockwave::GetAspectRatio() const
{
	const APlayerController* const PC = CameraOwner ? CameraOwner->GetOwningPlayerController() : nullptr;
	const ULocalPlayer* const LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (LocalPlayer && LocalPlayer->ViewportClient)
	{
		FVector2D Size;
		LocalPlayer->ViewportClient->GetViewportSize(Size);
		if (Size.X > 0.f && Size.Y > 0.f)
		{
			return Size.X / Size.Y;
		}
	}
	return 16.f / 9.f;
}

FVector2D UknightCameraModifier_RoarShockwave::ProjectToScreen(const FMinimalViewInfo& POV, const FVector& WorldLocation, float AspectRatio) const
{
	// FOV is horizontal (the default MaintainXFOV constraint), so the vertical extent is divided by the aspect ratio.
	const FVector Local = POV.Rotation.UnrotateVector(WorldLocation - POV.Location);
	const float TanHalfFOV = FMath::Tan(FMath::DegreesToRadians(POV.FOV * 0.5f));
	FVector2D NDC;
	if (Local.X > 1.f)
	{
		NDC = FVector2D(Local.Y / (Local.X * TanHalfFOV), Local.Z * AspectRatio / (Local.X * TanHalfFOV));
	}
	else
	{
		// Behind the camera: start the rings just off the matching screen edge.
		NDC = FVector2D(Local.Y, Local.Z).GetSafeNormal() * 1.5f;
	}
	NDC.X = FMath::Clamp(NDC.X, -3.f, 3.f);
	NDC.Y = FMath::Clamp(NDC.Y, -3.f, 3.f);
	return FVector2D(0.5f + 0.5f * NDC.X, 0.5f - 0.5f * NDC.Y);
}

bool UknightCameraModifier_RoarShockwave::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	for (FPulse& Pulse : Pulses)
	{
		Pulse.Age += DeltaTime;
	}
	Pulses.RemoveAll([this](const FPulse& Pulse) { return Pulse.Age > PulseLifetime; });

	USceneComponent* const Source = RoarSource.Get();
	if (bRoaring && !Source)
	{
		bRoaring = false;
	}
	if (bRoaring)
	{
		Envelope = 1.f;
		SinceLastPulse += DeltaTime;
		if (SinceLastPulse >= PulseInterval)
		{
			SinceLastPulse -= PulseInterval;
			SendPulse();
		}
	}
	else
	{
		Envelope = FMath::Max(0.f, Envelope - DeltaTime / FadeOutTime);
	}
	if (Source)
	{
		LastSourceLocation = RoarSocket != NAME_None && Source->DoesSocketExist(RoarSocket) ? Source->GetSocketLocation(RoarSocket) : Source->Bounds.Origin;
	}

	if (IsVisible())
	{
		const float Scale = GetStrengthScale(InOutPOV);

		float Strongest = 0.f;
		FVector4 Ages(-1.f, -1.f, -1.f, -1.f);
		FVector4 MoreAges(-1.f, -1.f, -1.f, -1.f);
		FVector4 Strengths(0.f, 0.f, 0.f, 0.f);
		FVector4 MoreStrengths(0.f, 0.f, 0.f, 0.f);
		for (int32 Index = 0; Index < Pulses.Num(); ++Index)
		{
			const FPulse& Pulse = Pulses[Index];
			(Index < 4 ? Ages : MoreAges)[Index % 4] = Pulse.Age;
			(Index < 4 ? Strengths : MoreStrengths)[Index % 4] = Pulse.Strength;
			const float Life = 1.f - Pulse.Age / PulseLifetime;
			Strongest = FMath::Max(Strongest, Life * Life * Pulse.Strength);
		}

		// Shake follows the newest strong ring, with a low rumble kept on for the rest of the roar.
		ShakeTime += DeltaTime;
		const float ShakeAngle = Shake * 0.6f * Scale * FMath::Max(Strongest, Envelope * 0.25f);
		InOutPOV.Rotation.Pitch += ShakeAngle * FMath::PerlinNoise1D(ShakeTime * 15.f);
		InOutPOV.Rotation.Yaw += ShakeAngle * 0.8f * FMath::PerlinNoise1D(ShakeTime * 15.f + 37.1f);

		if (!MaterialInstance)
		{
			if (UMaterialInterface* const Base = Material.LoadSynchronous())
			{
				MaterialInstance = UMaterialInstanceDynamic::Create(Base, this);
			}
		}
		if (MaterialInstance)
		{
			NoiseSeed = (NoiseSeed + 1) % 997;
			const FVector2D Center = ProjectToScreen(InOutPOV, LastSourceLocation, GetAspectRatio());
			MaterialInstance->SetVectorParameterValue(TEXT("Center"), FLinearColor(Center.X, Center.Y, NoiseSeed, 0.f));
			MaterialInstance->SetVectorParameterValue(TEXT("Ages0"), FLinearColor(Ages.X, Ages.Y, Ages.Z, Ages.W));
			MaterialInstance->SetVectorParameterValue(TEXT("Ages1"), FLinearColor(MoreAges.X, MoreAges.Y, MoreAges.Z, MoreAges.W));
			MaterialInstance->SetVectorParameterValue(TEXT("Strengths0"), FLinearColor(Strengths.X, Strengths.Y, Strengths.Z, Strengths.W));
			MaterialInstance->SetVectorParameterValue(TEXT("Strengths1"), FLinearColor(MoreStrengths.X, MoreStrengths.Y, MoreStrengths.Z, MoreStrengths.W));
			MaterialInstance->SetVectorParameterValue(TEXT("Look0"), FLinearColor(RingSpeed, RingWidth, Distortion * Scale, ChromaticAberration * Scale));
			MaterialInstance->SetVectorParameterValue(TEXT("Look1"), FLinearColor(Jitter * Scale, PulseLifetime, Envelope, 0.f));
		}
		CurrentScale = Scale;
	}

	return Super::ModifyCamera(DeltaTime, InOutPOV);
}

void UknightCameraModifier_RoarShockwave::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings)
{
	if (MaterialInstance && IsVisible() && CurrentScale > 0.f)
	{
		PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, MaterialInstance));
		PostProcessBlendWeight = 1.f;
	}
}
