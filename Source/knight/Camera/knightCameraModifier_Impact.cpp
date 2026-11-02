#include "Camera/knightCameraModifier_Impact.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Actor.h"
#include "Misc/App.h"

void UknightCameraModifier_Impact::Play(const FknightCameraImpactReaction& Reaction, float Scale, const FVector& Direction)
{
	if (Scale <= 0.f)
	{
		return;
	}
	FActiveReaction& Entry = Active.AddDefaulted_GetRef();
	Entry.Reaction = Reaction;
	Entry.Scale = Scale;
	Entry.Direction = Direction.GetSafeNormal();
}

float UknightCameraModifier_Impact::GetEnvelope(const FknightCameraImpactReaction& Reaction, float Age)
{
	if (Age < Reaction.AttackTime)
	{
		// 빠르게 치고 올라가야 타격 순간에 붙어 보인다.
		return FMath::Sin(Age / Reaction.AttackTime * HALF_PI);
	}
	const float HoldEnd = Reaction.AttackTime + Reaction.HoldTime;
	if (Age < HoldEnd)
	{
		return 1.f;
	}
	const float Release = FMath::Clamp((Age - HoldEnd) / FMath::Max(Reaction.ReleaseTime, KINDA_SMALL_NUMBER), 0.f, 1.f);
	return 1.f - FMath::InterpEaseInOut(0.f, 1.f, Release, 2.f);
}

bool UknightCameraModifier_Impact::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	CurrentFringe = 0.f;
	CurrentFOVDelta = 0.f;
	// 월드 시간은 레이드 시작 대기 같은 정지 중에 0이 되어 반응이 걸린 채로 남는다. 반응은 짧은 연출이라 실제 시간으로 돈다.
	DeltaTime = FApp::GetDeltaTime();
	if (Active.IsEmpty())
	{
		return Super::ModifyCamera(DeltaTime, InOutPOV);
	}

	const AActor* const ViewTarget = CameraOwner ? CameraOwner->GetViewTarget() : nullptr;
	const FVector BaseLocation = InOutPOV.Location;
	const FRotationMatrix ViewAxes(InOutPOV.Rotation);
	FVector Offset = FVector::ZeroVector;
	float PullTotal = 0.f;
	FVector PullOffset = FVector::ZeroVector;
	float FOVDelta = 0.f;

	for (int32 Index = Active.Num() - 1; Index >= 0; --Index)
	{
		FActiveReaction& Entry = Active[Index];
		Entry.Age += DeltaTime;
		const FknightCameraImpactReaction& Reaction = Entry.Reaction;
		if (Entry.Age >= Reaction.AttackTime + Reaction.HoldTime + Reaction.ReleaseTime)
		{
			Active.RemoveAtSwap(Index);
			continue;
		}
		const float Weight = GetEnvelope(Reaction, Entry.Age) * Entry.Scale;
		FOVDelta += Reaction.FOVPunch * Weight;
		Offset += (ViewAxes.GetScaledAxis(EAxis::Y) * Reaction.ShoulderOffset.X + ViewAxes.GetScaledAxis(EAxis::Z) * Reaction.ShoulderOffset.Y) * Weight;
		Offset += Entry.Direction * Reaction.Push * Weight;
		if (ViewTarget && Reaction.PullIn > 0.f)
		{
			const FVector Pivot = ViewTarget->GetActorLocation() + FVector(0.f, 0.f, Reaction.PullPivotHeight);
			const float Pull = Reaction.PullIn * Weight;
			PullOffset += (Pivot - BaseLocation) * Pull;
			PullTotal += Pull;
		}
		CurrentFringe = FMath::Max(CurrentFringe, Reaction.ChromaticAberration * Weight);
	}

	// 여러 당기기가 겹쳐도 캐릭터를 뚫고 지나가지 않게 전체 비율을 0.9로 묶는다.
	if (PullTotal > 0.9f)
	{
		PullOffset *= 0.9f / PullTotal;
	}
	InOutPOV.Location = BaseLocation + PullOffset + Offset;
	CurrentFOVDelta = FOVDelta;
	return Super::ModifyCamera(DeltaTime, InOutPOV);
}

void UknightCameraModifier_Impact::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings)
{
	if (CurrentFringe > KINDA_SMALL_NUMBER)
	{
		PostProcessSettings.bOverride_SceneFringeIntensity = true;
		PostProcessSettings.SceneFringeIntensity = CurrentFringe;
		PostProcessBlendWeight = 1.f;
	}
}
