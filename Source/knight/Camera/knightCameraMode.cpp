#include "Camera/knightCameraMode.h"

#include "Camera/knightCameraComponent.h"
#include "GameFramework/Pawn.h"

void FknightCameraModeView::Blend(const FknightCameraModeView& Other, float OtherWeight)
{
	if (OtherWeight <= 0.f)
	{
		return;
	}
	if (OtherWeight >= 1.f)
	{
		*this = Other;
		return;
	}
	Location = FMath::Lerp(Location, Other.Location, OtherWeight);
	PivotLocation = FMath::Lerp(PivotLocation, Other.PivotLocation, OtherWeight);
	Rotation = Rotation + (Other.Rotation - Rotation).GetNormalized() * OtherWeight;
	FieldOfView = FMath::Lerp(FieldOfView, Other.FieldOfView, OtherWeight);
}

UknightCameraComponent* UknightCameraMode::GetCameraComponent() const
{
	return CastChecked<UknightCameraComponent>(GetOuter());
}

AActor* UknightCameraMode::GetTargetActor() const
{
	return GetCameraComponent()->GetOwner();
}

void UknightCameraMode::SetBlendWeight(float Weight)
{
	BlendWeight = FMath::Clamp(Weight, 0.f, 1.f);
	// Invert the blend function so the alpha continues smoothly from this weight.
	const float InvExponent = BlendExponent > 0.f ? 1.f / BlendExponent : 1.f;
	switch (BlendFunction)
	{
	case EknightCameraModeBlendFunction::EaseOut:
		BlendAlpha = 1.f - FMath::Pow(1.f - BlendWeight, InvExponent);
		break;
	case EknightCameraModeBlendFunction::EaseIn:
		BlendAlpha = FMath::Pow(BlendWeight, InvExponent);
		break;
	default:
		BlendAlpha = BlendWeight;
		break;
	}
}

void UknightCameraMode::UpdateCameraMode(float DeltaTime)
{
	UpdateView(DeltaTime);
	UpdateBlending(DeltaTime);
}

FVector UknightCameraMode::GetPivotLocation() const
{
	const AActor* const Target = GetTargetActor();
	return Target ? Target->GetActorLocation() + PivotOffset : FVector::ZeroVector;
}

FRotator UknightCameraMode::GetPivotRotation() const
{
	const AActor* const Target = GetTargetActor();
	if (const APawn* const Pawn = Cast<APawn>(Target))
	{
		return Pawn->GetViewRotation();
	}
	return Target ? Target->GetActorRotation() : FRotator::ZeroRotator;
}

void UknightCameraMode::UpdateView(float DeltaTime)
{
	FRotator PivotRotation = GetPivotRotation();
	PivotRotation.Pitch = FMath::ClampAngle(PivotRotation.Pitch, ViewPitchMin, ViewPitchMax);
	View.Location = GetPivotLocation();
	View.PivotLocation = View.Location;
	View.Rotation = PivotRotation;
	View.FieldOfView = FieldOfView;
}

void UknightCameraMode::UpdateBlending(float DeltaTime)
{
	if (BlendTime > 0.f)
	{
		BlendAlpha = FMath::Min(1.f, BlendAlpha + DeltaTime / BlendTime);
	}
	else
	{
		BlendAlpha = 1.f;
	}
	switch (BlendFunction)
	{
	case EknightCameraModeBlendFunction::EaseOut:
		BlendWeight = FMath::InterpEaseOut(0.f, 1.f, BlendAlpha, BlendExponent);
		break;
	case EknightCameraModeBlendFunction::EaseIn:
		BlendWeight = FMath::InterpEaseIn(0.f, 1.f, BlendAlpha, BlendExponent);
		break;
	case EknightCameraModeBlendFunction::EaseInOut:
		BlendWeight = FMath::InterpEaseInOut(0.f, 1.f, BlendAlpha, BlendExponent);
		break;
	default:
		BlendWeight = BlendAlpha;
		break;
	}
}

UknightCameraMode* UknightCameraModeStack::GetModeInstance(TSubclassOf<UknightCameraMode> ModeClass)
{
	for (UknightCameraMode* const Instance : Instances)
	{
		if (Instance && Instance->GetClass() == ModeClass)
		{
			return Instance;
		}
	}
	UknightCameraMode* const NewInstance = NewObject<UknightCameraMode>(GetOuter(), ModeClass, NAME_None, RF_NoFlags);
	Instances.Add(NewInstance);
	return NewInstance;
}

void UknightCameraModeStack::PushCameraMode(TSubclassOf<UknightCameraMode> ModeClass)
{
	if (!ModeClass)
	{
		return;
	}
	UknightCameraMode* const Mode = GetModeInstance(ModeClass);
	if (Stack.Num() > 0 && Stack[0] == Mode)
	{
		return;
	}

	// If the mode is already lower in the stack, start its blend from the weight it contributes now.
	float StartWeight = 0.f;
	const int32 ExistingIndex = Stack.IndexOfByKey(Mode);
	if (ExistingIndex != INDEX_NONE)
	{
		StartWeight = 1.f;
		for (int32 Index = 0; Index <= ExistingIndex; ++Index)
		{
			StartWeight *= Index == ExistingIndex ? Stack[Index]->GetBlendWeight() : 1.f - Stack[Index]->GetBlendWeight();
		}
		Stack.RemoveAt(ExistingIndex);
	}
	Mode->SetBlendWeight(Stack.Num() > 0 ? StartWeight : 1.f);
	Stack.Insert(Mode, 0);
	Mode->OnActivation();
}

bool UknightCameraModeStack::EvaluateStack(float DeltaTime, FknightCameraModeView& OutView)
{
	if (Stack.IsEmpty())
	{
		return false;
	}

	// Update top-down and drop everything under the first fully blended mode.
	int32 RemoveFrom = INDEX_NONE;
	for (int32 Index = 0; Index < Stack.Num(); ++Index)
	{
		Stack[Index]->UpdateCameraMode(DeltaTime);
		if (Stack[Index]->GetBlendWeight() >= 1.f)
		{
			RemoveFrom = Index + 1;
			break;
		}
	}
	if (RemoveFrom != INDEX_NONE && RemoveFrom < Stack.Num())
	{
		Stack.SetNum(RemoveFrom);
	}

	// Blend bottom-up.
	OutView = Stack.Last()->GetView();
	for (int32 Index = Stack.Num() - 2; Index >= 0; --Index)
	{
		OutView.Blend(Stack[Index]->GetView(), Stack[Index]->GetBlendWeight());
	}
	return true;
}
