#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "Interfaces/TargetableInterface.h"
#include "knightTargetLockComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FknightLockOnTargetChanged, AActor*, NewTarget, FGameplayTag, PartTag);

/**
 * Lock-on for the player's pawn. Targets are ITargetableInterface actors that are hostile; giant
 * monsters expose several points (head, legs, tail) and SwitchTarget moves between them and
 * other targets by screen position.
 *
 * Until the camera modes land (phase 6), this component also turns the control rotation toward the
 * locked point. The lock-on camera mode will take over that job and read the target from here.
 *
 * Debug: knight.Combat.DrawLockOn 1
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightTargetLockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightTargetLockComponent();

	/** Locks the best target in view, or releases the current lock. Returns true if now locked. */
	UFUNCTION(BlueprintCallable, Category="Knight|LockOn")
	bool ToggleLockOn();

	UFUNCTION(BlueprintCallable, Category="Knight|LockOn")
	bool TryLockOn();

	UFUNCTION(BlueprintCallable, Category="Knight|LockOn")
	void ClearLockOn();

	/** Moves to the nearest target point to the right (or left) on screen. */
	UFUNCTION(BlueprintCallable, Category="Knight|LockOn")
	void SwitchTarget(bool bRight);

	UFUNCTION(BlueprintPure, Category="Knight|LockOn")
	bool IsLockedOn() const { return LockedActor.IsValid(); }

	UFUNCTION(BlueprintPure, Category="Knight|LockOn")
	AActor* GetLockedTarget() const { return LockedActor.Get(); }

	UFUNCTION(BlueprintPure, Category="Knight|LockOn")
	FGameplayTag GetLockedPartTag() const { return LockedPartTag; }

	bool GetLockedTargetLocation(FVector& OutLocation) const;

	/** Radius reported by the locked target, for camera framing. Zero without a lock. */
	float GetLockedTargetRadius() const;

	UPROPERTY(BlueprintAssignable, Category="Knight|LockOn")
	FknightLockOnTargetChanged OnLockOnTargetChanged;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn", meta=(Units="cm"))
	float LockOnRange = 4000.f;

	/** The lock breaks beyond this distance. Larger than LockOnRange so it does not flicker at the edge. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn", meta=(Units="cm"))
	float BreakRange = 5500.f;

	/** Initial lock only considers points within this angle of the view direction. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn", meta=(ClampMin=1, ClampMax=180, Units="deg"))
	float MaxLockAngle = 60.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn")
	bool bRequireLineOfSight = true;

	/** Seconds the target may stay out of sight before the lock breaks. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn", meta=(ClampMin=0, Units="s"))
	float LoseSightGraceTime = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn")
	TArray<TEnumAsByte<EObjectTypeQuery>> TargetObjectTypes;

	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Camera")
	bool bRotateControllerToTarget = true;

	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Camera", meta=(ClampMin=0))
	float ControlRotationInterpSpeed = 8.f;

	/** Added to the look-at pitch so the Knight stays in frame below a tall target. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Camera", meta=(Units="deg"))
	float PitchOffset = -8.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Camera", meta=(Units="deg"))
	float MinPitch = -50.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Camera", meta=(Units="deg"))
	float MaxPitch = 35.f;

private:
	struct FCandidate
	{
		TWeakObjectPtr<AActor> Actor;
		FknightTargetPoint Point;
	};

	void GatherCandidates(TArray<FCandidate>& OutCandidates) const;
	bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
	bool HasLineOfSight(const FVector& ViewLocation, const FVector& PointLocation, const AActor* Target) const;
	void SetLock(const FCandidate& Candidate);
	void UpdateControlRotation(float DeltaTime) const;
	/** Mirrors the lock onto the owner's ASC as State.LockedOn, which selects the lock-on camera mode. */
	void SetLockedOnTag(bool bLocked) const;

	TWeakObjectPtr<AActor> LockedActor;
	TWeakObjectPtr<USceneComponent> LockedComponent;
	FName LockedSocket;
	FGameplayTag LockedPartTag;
	float TimeOutOfSight = 0.f;
};
