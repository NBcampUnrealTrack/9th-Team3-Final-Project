#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "knightAnimNode_LegPlacement.generated.h"

/** 다리 하나. 발 뼈만 지정하면 그 부모(무릎·팔꿈치)와 조부모(허벅지·위팔)로 2본 IK 체인을 만든다. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightLegPlacementLeg
{
	GENERATED_BODY()

	/** IK 끝 뼈(발목). 잔열이면 front_paw_L, hind_paw_L처럼 발가락이 아닌 발목 뼈. */
	UPROPERTY(EditAnywhere, Category="Leg")
	FBoneReference FootBone;

	/** 접지점(발 뼈의 자손 중 가장 낮은 뼈, 보통 발가락 끝)에서 실제 발바닥까지의 높이(스켈레톤 단위). 발가락 끝이 발바닥보다 높으면 양수. */
	UPROPERTY(EditAnywhere, Category="Leg")
	float SoleOffset = 0.f;

	// 아래는 런타임 상태(직렬화하지 않음).
	FBoneReference JointBone;
	FBoneReference RootBone;
	/** 발 뼈의 자손(발가락·발톱). 이 중 가장 낮은 뼈를 접지점으로 본다. 없으면 발 뼈 자신. */
	TArray<FBoneReference> ContactBones;
	/** 캡슐 바닥(애니메이션이 가정한 평지) 대비 발밑 지면 높이. 월드 cm, 보간된 값. */
	float Offset = 0.f;
	bool bChainValid = false;

	/** 접지 커브 이름 "FootLock_<발 뼈>". 값이 1(섞인 포즈가 모두 디딤)일 때 발이 땅에 닿은 것으로 본다. */
	FName LockCurve;
	/** 고정한 발의 월드 위치(XY만 쓴다)와, 고정한 순간 컴포넌트의 요(도). */
	FVector LockedLocation = FVector::ZeroVector;
	double LockedYaw = 0.0;
	/** 1 = 고정 위치, 0 = 애니메이션 위치. 고정할 때 바로 1, 풀 때 FootLockReleaseTime 동안 0으로. */
	float LockAlpha = 0.f;
	bool bLocked = false;
	/** 다리가 닿지 않아 강제로 풀었으면, 커브가 한 번 내려갈(발을 들) 때까지 다시 고정하지 않는다. */
	bool bWaitForLift = false;

	/** 기준 포즈의 체인 길이(허벅지 ~ 발목, 컴포넌트 공간). 거리 기준을 몬스터 크기에 맞춘다. */
	double LengthCS = 0.0;
	/** 자동 발 딛기: 고정된 발을 들어 몸 기준 제자리로 옮기는 중. */
	bool bStepping = false;
	FVector StepFrom = FVector::ZeroVector;
	double StepFromYaw = 0.0;
	float StepElapsed = 0.f;
	/** 이번 풀기에 걸리는 시간(멀리 떨어진 발일수록 길다)과, 걷던 중에 풀렸을 때 남은 드는 높이(컴포넌트 공간). */
	float ReleaseTime = 0.f;
	double ReleaseLift = 0.0;
};

/**
 * 큰 몬스터의 발을 지형에 맞춘다. 캡슐 하나가 몸 전체를 받치므로 경사나 요철에서 발이 뜨거나 묻힌다.
 * 다리마다 발밑 지면 높이를 재서 (1) 골반을 그 지면 평면에 맞춰 내리고 기울인 뒤 (2) 남은 차이를 다리별
 * 2본 IK로 맞춘다. 평지에서는 모든 높이 차가 0이라 포즈를 바꾸지 않는다.
 * 접지 붙이기: 애니메이션의 발이 땅 근처에 있으면(GroundSnapRatio) 발가락 끝을 땅 높이에 붙인다. 평지에서도 적용된다.
 *
 * 지면 트레이스는 게임 스레드(PreUpdate)에서 이전 프레임의 발 위치로 하고, 포즈 계산은 워커 스레드에서 한다.
 * 다리 수는 자유다. 3개 미만이면 기울이기 없이 가장 낮은 발만큼 골반을 내린다(2족).
 *
 * 발 고정: 애니메이션에 "FootLock_<발 뼈>" 커브가 있으면, 커브가 켜진(발이 땅에 닿은) 동안 발의 월드 XY와 요를
 * 붙잡아 둔다. 몸이 코드로 돌거나 밀려도 디딘 발이 미끄러지지 않는다. 커브가 없는 애니메이션은 지금과 같다.
 * 자동 발 딛기: 몸이 많이 돌아 고정된 발이 몸에서 멀어지면, 그 발을 들어 몸 기준 제자리(조금 앞)로 옮겨 디딘다.
 * 한 번에 대각선 다리끼리만 든다.
 *
 * 콘솔: knight.Anim.LegPlacement 0 으로 끄고, knight.Anim.DrawLegPlacement 1 로 트레이스를 그린다.
 * knight.Anim.FootLock 0 으로 발 고정만 끈다. 그리기를 켜면 고정된 발(노랑), 걷는 발(하늘색), 풀리는 발(주황)도 보인다.
 */
USTRUCT(BlueprintInternalUseOnly)
struct KNIGHT_API FAnimNode_knightLegPlacement : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	FAnimNode_knightLegPlacement()
	{
		// 멀리 있어 낮은 LOD로 그려질 때는 발 위치가 눈에 띄지 않으니 트레이스와 IK를 건너뛴다.
		LODThreshold = 2;
	}

	UPROPERTY(EditAnywhere, Category="Legs")
	TArray<FknightLegPlacementLeg> Legs;

	/** 몸통을 내리고 기울이는 뼈. 모든 다리 체인의 조상이어야 한다(네발짐승은 pelvis). */
	UPROPERTY(EditAnywhere, Category="Body")
	FBoneReference PelvisBone;

	/** 발밑 지면 평면의 기울기를 몸통에 얼마나 반영할지. 0이면 높이만 맞추고 기울이지 않는다. */
	UPROPERTY(EditAnywhere, Category="Body", meta=(ClampMin=0, ClampMax=1))
	float BodyTiltWeight = 1.f;

	UPROPERTY(EditAnywhere, Category="Body", meta=(ClampMin=0, ClampMax=45, Units="deg"))
	float MaxBodyTilt = 20.f;

	/** 캡슐 바닥보다 이만큼 높은 지면까지 발을 올린다(월드 cm). */
	UPROPERTY(EditAnywhere, Category="Trace", meta=(ClampMin=0, Units="cm"))
	float MaxStepUp = 200.f;

	/** 캡슐 바닥보다 이만큼 낮은 지면까지 발을 내린다(월드 cm). */
	UPROPERTY(EditAnywhere, Category="Trace", meta=(ClampMin=0, Units="cm"))
	float MaxStepDown = 300.f;

	/** 발밑을 훑는 구의 반지름(월드 cm). 발바닥 크기에 맞추면 작은 요철에 덜 떨린다. */
	UPROPERTY(EditAnywhere, Category="Trace", meta=(ClampMin=0, Units="cm"))
	float TraceRadius = 30.f;

	/** 높이 차가 목표를 따라가는 속도. 클수록 빨리 붙고, 작을수록 부드럽다. */
	UPROPERTY(EditAnywhere, Category="Trace", meta=(ClampMin=0))
	float InterpSpeed = 10.f;

	/**
	 * 애니메이션에서 발이 땅 근처(다리 길이의 이 비율 안)에 있으면 접지점을 땅에 딱 붙인다. 2배 높이까지 서서히 줄어 원래 높이로 돌아간다.
	 * 애니메이션이 디딘 발을 살짝 들고 있거나(잔열 대기 자세의 앞발) 땅에 묻고 있어도 바닥에 닿아 보인다. 0이면 끈다.
	 */
	UPROPERTY(EditAnywhere, Category="Ground Contact", meta=(ClampMin=0, ClampMax=0.5))
	float GroundSnapRatio = 0.1f;

	/** 접지 커브가 있는 애니메이션에서 디딘 발을 월드에 고정한다. */
	UPROPERTY(EditAnywhere, Category="Foot Lock")
	bool bEnableFootLock = true;

	/** 발을 들 때 고정 위치에서 애니메이션 위치로 돌아가는 최소 시간. */
	UPROPERTY(EditAnywhere, Category="Foot Lock", meta=(ClampMin=0, Units="s"))
	float FootLockReleaseTime = 0.15f;

	/** 풀릴 때 발이 애니메이션 위치로 돌아가는 최대 속도(초당 다리 길이 배수). 멀리 떨어진 발이 튀지 않게 풀기 시간을 늘린다. */
	UPROPERTY(EditAnywhere, Category="Foot Lock", meta=(ClampMin=0.1))
	float FootLockReleaseSpeed = 1.f;

	/** 고정 위치와 애니메이션 발 위치가 다리 길이의 이 비율만큼 벌어지면 다리가 닿지 않는 것으로 보고 푼다. */
	UPROPERTY(EditAnywhere, Category="Foot Lock", meta=(ClampMin=0))
	float MaxFootLockStretch = 0.6f;

	/** 고정된 발이 몸에서 멀어지면 들어서 옮겨 딛는다. 끄면 MaxFootLockStretch에서 풀리며 미끄러진다. */
	UPROPERTY(EditAnywhere, Category="Foot Lock|Auto Step")
	bool bEnableAutoStep = true;

	/**
	 * 몸이 이보다 빠르면 딛지 않는다. 달리기는 애니메이션이 스스로 발을 옮기므로, 디딘 발이 조금 끌려도 옮겨 디디면
	 * 걸음 사이에 가짜 걸음이 끼어 발이 튄다. 그때는 MaxFootLockStretch까지 버티다 풀린다.
	 */
	UPROPERTY(EditAnywhere, Category="Foot Lock|Auto Step", meta=(ClampMin=0, Units="cm/s"))
	float MaxAutoStepSpeed = 800.f;

	/** 고정 위치와 애니메이션 발 위치가 다리 길이의 이 비율만큼 벌어지면 딛기를 시작한다. */
	UPROPERTY(EditAnywhere, Category="Foot Lock|Auto Step", meta=(ClampMin=0))
	float StepTriggerRatio = 0.3f;

	/** 한 걸음에 걸리는 시간. */
	UPROPERTY(EditAnywhere, Category="Foot Lock|Auto Step", meta=(ClampMin=0.05, Units="s"))
	float StepDuration = 0.25f;

	/** 발을 드는 높이(다리 길이 비율). */
	UPROPERTY(EditAnywhere, Category="Foot Lock|Auto Step", meta=(ClampMin=0))
	float StepHeightRatio = 0.1f;

	/** 몸이 계속 움직인다고 보고 이 시간(StepDuration 배수)만큼 앞을 내다보고 딛는다. 클수록 보폭이 커지고 딛는 횟수가 준다. */
	UPROPERTY(EditAnywhere, Category="Foot Lock|Auto Step", meta=(ClampMin=0))
	float StepLead = 1.0f;

	/** 동시에 들 수 있는 발 수. 같이 드는 발은 대각선(좌우·앞뒤가 모두 반대)이어야 한다. */
	UPROPERTY(EditAnywhere, Category="Foot Lock|Auto Step", meta=(ClampMin=1))
	int32 MaxSimultaneousSteps = 2;

	/** 발 뼈의 접지 커브 이름 "FootLock_<발 뼈>". 에디터의 커브 굽기 도구도 이 이름으로 쓴다. */
	static FName MakeFootLockCurveName(FName FootBone);

	//~ FAnimNode_Base
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

	//~ FAnimNode_SkeletalControlBase
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

	/** 고정을 풀고, 지금 보이는 발 위치에서 애니메이션 위치까지의 거리로 풀기 시간을 정한다. */
	void BeginFootRelease(FknightLegPlacementLeg& Leg, const FVector& AnimFootWorld, double Lift);

	/** 다리 하나의 고정·풀기·걸음 진행을 갱신한다. 걸음 시작은 여기서 하지 않는다(다리끼리 순서를 정해야 해서). */
	void UpdateFootLockState(FknightLegPlacementLeg& Leg, bool bPlanted, const FVector& AnimFootWorld, const FTransform& ComponentToWorld);

	/** 고정·걸음 상태로 IK 목표(컴포넌트 공간)와 발 요 보정(도)을 고친다. */
	void ApplyFootLock(const FknightLegPlacementLeg& Leg, const FVector& AnimFootWorld, const FTransform& ComponentToWorld,
		FVector& InOutEffectorCS, double& OutYawCorrection) const;

	/** 몸이 지금 속도와 회전 속도로 Time초 더 움직였을 때 애니메이션 발이 있을 월드 위치. */
	FVector PredictFootWorld(const FVector& AnimFootWorld, const FTransform& ComponentToWorld, float Time) const;

	/** 다리 길이(월드 cm). */
	double LegLengthWorld(const FknightLegPlacementLeg& Leg) const { return Leg.LengthCS / FMath::Max(WorldToComponent, UE_KINDA_SMALL_NUMBER); }

	/** 월드 cm를 컴포넌트 공간 단위로 바꾼다(메시 스케일). 게임 스레드에서 갱신. */
	float WorldToComponent = 1.f;

	/** 땅 위에 있고 발 고정이 켜져 있는지. 게임 스레드(PreUpdate)에서 갱신. */
	bool bFootLockActive = false;

	/** 접지 붙이기 강도. 땅 위면 1, 공중이면 0으로 InterpSpeed로 따라간다. 게임 스레드(PreUpdate)에서 갱신. */
	float GroundSnapAlpha = 0.f;

	/** 이번 프레임 DeltaTime(UpdateInternal)과, 평가를 건너뛴 업데이트 수(LOD 등으로 끊기면 고정을 버린다). */
	float FootLockDeltaTime = 0.f;
	int32 UpdatesSinceEvaluate = 0;

	/** 걸음 착지점을 내다보려고 재는 컴포넌트의 속도(월드 cm/s)와 요 회전 속도(도/s). 평가 스레드에서 갱신. */
	FVector PrevComponentLocation = FVector::ZeroVector;
	double PrevComponentYaw = 0.0;
	bool bHasPrevComponent = false;
	FVector ComponentVelocity = FVector::ZeroVector;
	double ComponentYawRate = 0.0;
};
