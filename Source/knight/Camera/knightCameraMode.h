#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "knightCameraMode.generated.h"

class UknightCameraComponent;

/** What a camera mode produces each frame. Blended between modes by weight. */
struct FknightCameraModeView
{
	FVector Location = FVector::ZeroVector;
	/** Point the camera orbits. The occlusion sweep runs from here to Location. */
	FVector PivotLocation = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float FieldOfView = 80.f;

	void Blend(const FknightCameraModeView& Other, float OtherWeight);
};

UENUM(BlueprintType)
enum class EknightCameraModeBlendFunction : uint8
{
	Linear,
	/** Starts fast, settles slowly. Good for most transitions. */
	EaseOut,
	EaseIn,
	EaseInOut
};

/**
 * One camera behaviour (default follow, lock-on, boost, flight, parry punch-in). Stateless except for
 * blending, so one instance per class lives in the stack and is reused.
 * Subclass in Blueprint to make variants: Boost and Flight are ThirdPerson modes with other numbers.
 */
UCLASS(Abstract, Blueprintable)
class KNIGHT_API UknightCameraMode : public UObject
{
	GENERATED_BODY()

public:
	void UpdateCameraMode(float DeltaTime);

	const FknightCameraModeView& GetView() const { return View; }
	float GetBlendWeight() const { return BlendWeight; }
	void SetBlendWeight(float Weight);

	UknightCameraComponent* GetCameraComponent() const;
	AActor* GetTargetActor() const;

	/** Called when the mode is pushed on top of the stack (start of its blend in). */
	virtual void OnActivation() {}

protected:
	virtual void UpdateView(float DeltaTime);

	/** Point the camera orbits: actor location plus PivotOffset. */
	FVector GetPivotLocation() const;
	/** Control rotation for pawns, actor rotation otherwise. */
	FRotator GetPivotRotation() const;

	UPROPERTY(EditDefaultsOnly, Category="View", meta=(UIMin=5, UIMax=170, ClampMin=5, ClampMax=170, Units="deg"))
	float FieldOfView = 80.f;

	UPROPERTY(EditDefaultsOnly, Category="View", meta=(ClampMin=-89.9, ClampMax=89.9, Units="deg"))
	float ViewPitchMin = -60.f;

	UPROPERTY(EditDefaultsOnly, Category="View", meta=(ClampMin=-89.9, ClampMax=89.9, Units="deg"))
	float ViewPitchMax = 50.f;

	/** Offset from the actor location to the orbit pivot, in world space (mostly height). */
	UPROPERTY(EditDefaultsOnly, Category="View")
	FVector PivotOffset = FVector(0.f, 0.f, 90.f);

	UPROPERTY(EditDefaultsOnly, Category="Blending", meta=(ClampMin=0, Units="s"))
	float BlendTime = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category="Blending")
	EknightCameraModeBlendFunction BlendFunction = EknightCameraModeBlendFunction::EaseOut;

	UPROPERTY(EditDefaultsOnly, Category="Blending", meta=(ClampMin=1))
	float BlendExponent = 3.f;

	FknightCameraModeView View;

private:
	void UpdateBlending(float DeltaTime);

	float BlendAlpha = 1.f;
	float BlendWeight = 1.f;
};

/** Stack of active modes. The top mode blends in over its BlendTime; fully covered modes are dropped. */
UCLASS()
class KNIGHT_API UknightCameraModeStack : public UObject
{
	GENERATED_BODY()

public:
	void PushCameraMode(TSubclassOf<UknightCameraMode> ModeClass);
	bool EvaluateStack(float DeltaTime, FknightCameraModeView& OutView);

	/** Top mode, for debugging. */
	UknightCameraMode* GetTopMode() const { return Stack.Num() > 0 ? Stack[0] : nullptr; }

private:
	UknightCameraMode* GetModeInstance(TSubclassOf<UknightCameraMode> ModeClass);

	UPROPERTY()
	TArray<TObjectPtr<UknightCameraMode>> Instances;

	/** Index 0 is the top. */
	UPROPERTY()
	TArray<TObjectPtr<UknightCameraMode>> Stack;
};
