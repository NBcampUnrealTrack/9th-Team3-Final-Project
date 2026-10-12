#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnightMaintenanceCameraRig.generated.h"

class ACameraActor;

/** Place one in L_MainMenu. Coordinates below are world space and can be edited per placed instance. */
UCLASS()
class KNIGHT_API AKnightMaintenanceCameraRig : public AActor
{
	GENERATED_BODY()
public:
	AKnightMaintenanceCameraRig();
	virtual void Tick(float DeltaSeconds) override;
	void SetMaintenanceView(bool bOpen);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Maintenance Camera")
	TObjectPtr<ACameraActor> ControlledCamera;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Maintenance Camera|World Space", meta=(MakeEditWidget=true))
	FVector MaintenanceWorldLocation = FVector(-260.f, 440.f, 230.f);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Maintenance Camera|World Space")
	FRotator MaintenanceWorldRotation = FRotator(-7.f, 0.f, 0.f);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Maintenance Camera", meta=(ClampMin=0.01))
	float TransitionSeconds = 0.65f;

private:
	FTransform OriginalTransform;
	FTransform FromTransform;
	FTransform ToTransform;
	float Elapsed = 0.f;
	bool bHasOriginal = false;
	bool bMoving = false;
	bool bOpening = false;
};
