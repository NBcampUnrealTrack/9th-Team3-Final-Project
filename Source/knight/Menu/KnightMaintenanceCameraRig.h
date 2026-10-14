#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "KnightMaintenanceCameraRig.generated.h"

class ACameraActor;

/** 부위별 클로즈업: 기준 뼈를 Distance 거리에서 바라본다. */
USTRUCT(BlueprintType)
struct FKnightMaintenanceShot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Maintenance Camera")
	FGameplayTag Slot;

	/** 선택 시점의 실제 포즈에서 위치를 읽는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Maintenance Camera")
	FName Bone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Maintenance Camera", meta=(ClampMin=50, Units="cm"))
	float Distance = 400.f;

	/** 뼈 높이 보정. 음수면 아래쪽. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Maintenance Camera", meta=(Units="cm"))
	float HeightOffset = 0.f;

	/** 전신 시점 방향 기준 회전. 양수면 화면 왼쪽으로. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Maintenance Camera", meta=(Units="deg"))
	float Yaw = 0.f;

	/** 내려다보는 각도. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Maintenance Camera", meta=(Units="deg"))
	float Pitch = 0.f;
};

/**
 * 정비 카메라. L_MainMenu에 하나 배치.
 * 진입 시 전신, 부위 선택 시 클로즈업. 클로즈업끼리는 호 이동, 이동 중 재선택은 속도 유지.
 */
UCLASS()
class KNIGHT_API AKnightMaintenanceCameraRig : public AActor
{
	GENERATED_BODY()
public:
	AKnightMaintenanceCameraRig();
	virtual void Tick(float DeltaSeconds) override;
	void SetMaintenanceView(bool bInOpen);

	/** 빈 태그면 전신 시점. 정비 화면이 닫혀 있으면 무시. */
	UFUNCTION(BlueprintCallable, Category="Maintenance Camera")
	void FocusSlot(FGameplayTag Slot);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Maintenance Camera")
	TObjectPtr<ACameraActor> ControlledCamera;

	/** 비출 메카. 스켈레탈 메시 뼈를 기준으로 쓴다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Maintenance Camera")
	TObjectPtr<AActor> FocusActor;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Maintenance Camera|World Space", meta=(MakeEditWidget=true))
	FVector MaintenanceWorldLocation = FVector(-260.f, 440.f, 230.f);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Maintenance Camera|World Space")
	FRotator MaintenanceWorldRotation = FRotator(-7.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Maintenance Camera")
	TArray<FKnightMaintenanceShot> Shots;

	/** 이동 시간: 거리에 따라 Min~Max. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Maintenance Camera|Motion", meta=(ClampMin=0.05, Units="s"))
	float MinTransitionSeconds = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Maintenance Camera|Motion", meta=(ClampMin=0.05, Units="s"))
	float MaxTransitionSeconds = 0.8f;

	/** 이 거리 이상이면 최대 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Maintenance Camera|Motion", meta=(ClampMin=1, Units="cm"))
	float FullTransitionDistance = 600.f;

	/** 호 이동 시 뒤로 빠지는 정도(이동 거리 비율). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Maintenance Camera|Motion", meta=(ClampMin=0, ClampMax=1))
	float ArcStrength = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Maintenance Camera|Motion", meta=(ClampMin=0, Units="cm"))
	float MaxArcPullBack = 160.f;

	/** 최종 스무딩. 이동 중 재선택 시 꺾임 완화. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Maintenance Camera|Motion", meta=(ClampMin=0, Units="s"))
	float SmoothingHalfLife = 0.05f;

private:
	/** 위치 + 바라보는 점. 회전은 이 둘로 계산(롤 없음). */
	struct FView
	{
		FVector Location = FVector::ZeroVector;
		FVector Focus = FVector::ForwardVector;
	};

	static FView MakeView(const FVector& Location, const FRotator& Rotation, float FocusDistance);
	FView OverviewView() const;
	bool ShotView(FGameplayTag Slot, FView& OutView) const;
	float FocusDistanceFrom(const FVector& Location) const;
	void StartMove(const FView& Target, bool bArc);
	FView EvaluatePath(float Alpha) const;
	void ApplyToCamera(const FView& View);

	FView Original;
	FView PathStart;
	FView PathEnd;
	FView PathPoint;
	FView Smoothed;
	FVector PathVelocity = FVector::ZeroVector;
	FVector SmoothedVelocity = FVector::ZeroVector;
	FVector SmoothedFocusVelocity = FVector::ZeroVector;
	FVector ArcOffset = FVector::ZeroVector;
	float StartSlope = 0.f;
	float Duration = 0.f;
	float Elapsed = 0.f;
	bool bHasOriginal = false;
	bool bMoving = false;
	bool bOpen = false;
	bool bAtShot = false;
};
