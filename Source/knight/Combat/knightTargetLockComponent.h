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
 *
 * (2026-10-14 추가)
 * - 시점 입력으로 대상 바꾸기(다크소울 방식): 락온 중 마우스를 휙 움직이거나 패드 오른쪽 스틱을 한쪽으로 튕기면
 *   화면에서 그 방향(상하좌우)에 있는 다른 지점(같은 몬스터의 다른 부위 또는 다른 적)으로 바꾼다. 캐릭터의 시점 입력이
 *   AddSwitchLookInput으로 넘어온다. 휠 전환(SwitchTarget)은 그대로. 조절 값은 Knight|LockOn|Switch.
 * - 잠근 지점이 대상의 락온 지점 목록에서 빠지면(잘려 나간 꼬리 등) 같은 대상의 가장 가까운 지점으로 옮긴다. 남은 지점이 없으면 푼다.
 * - 확인: 출력 로그 LogknightCombat의 "lock-on switch" / "lock-on point ... is gone" 줄.
 *
 * (2026-10-19) 아래 '가장 가까운 부위로 다시 잡기'는 사용자 요청으로 기본 끔(bReLockNearestOnPress). 기본은 누르면 켜기/끄기.
 * (2026-10-15 사용자 결정) 락온 키(PressLockOn, 가운데 버튼):
 * - 락온이 아닐 때: 지금처럼 보고 있는 쪽 대상을 잡는다(TryLockOn).
 * - 락온 중: 카메라 앞쪽(MaxLockAngle 안)에서 플레이어와 가장 가까운 부위로 다시 잡는다. 그게 지금 잡은 부위면 푼다.
 * - LockOnDoublePressTime 안에 두 번 누르면 푼다.
 * - 마우스 이동 전환은 '불편하다'는 의견으로 기본 끔(bSwitchByMouseMove). 패드 스틱 전환·휠 전환은 그대로.
 * - 확인: 출력 로그 LogknightCombat의 "lock-on press" 줄.
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

	/**
	 * (2026-10-15) 락온 키를 한 번 눌렀을 때. 락온이 아니면 TryLockOn, 락온 중이면 플레이어와 가장 가까운 부위로 다시 잡고
	 * (같은 부위면 해제), LockOnDoublePressTime 안에 두 번 누르면 해제. 지금 락온 중이면 true.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|LockOn")
	bool PressLockOn();

	UFUNCTION(BlueprintCallable, Category="Knight|LockOn")
	bool TryLockOn();

	UFUNCTION(BlueprintCallable, Category="Knight|LockOn")
	void ClearLockOn();

	/** Moves to the nearest target point to the right (or left) on screen. */
	UFUNCTION(BlueprintCallable, Category="Knight|LockOn")
	void SwitchTarget(bool bRight);

	/**
	 * 지금 잠근 지점에서 화면상 ScreenDirection 쪽(오른쪽 +X, 아래 +Y)으로 SwitchMaxAngle 안에 있는 지점 중
	 * 방향이 잘 맞고 가까운 지점으로 바꾼다. 바꿨으면 true.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|LockOn")
	bool SwitchTargetInDirection(FVector2D ScreenDirection);

	/**
	 * 시점 입력을 받아 락온 중이면 대상 전환을 판단한다(캐릭터의 ApplyLookInput이 매번 부른다). 락온이 아닐 때도 불러야
	 * 락온 직전부터 움직이던 마우스·스틱을 전환으로 잘못 치지 않는다.
	 * LookInput은 카메라를 돌리는 값과 같은 방향(X = 오른쪽, Y = 음수면 위)이라 화면 방향으로 그대로 쓴다.
	 * - 마우스(bFromStick = false): 감도·Y축 반전이 적용된 값. SwitchMouseWindow 안에 모인 크기가 SwitchMouseThreshold를 넘으면 전환.
	 * - 패드(bFromStick = true): 감도와 상관없는 스틱 기울기(Y축 반전만 적용). SwitchStickThreshold 이상 튕기면 전환,
	 *   스틱을 가운데로 되돌려야(SwitchStickRearmThreshold 아래 또는 입력이 끊김) 다음 전환이 된다.
	 */
	void AddSwitchLookInput(const FVector2D& LookInput, bool bFromStick);

	UFUNCTION(BlueprintPure, Category="Knight|LockOn")
	bool IsLockedOn() const { return LockedActor.IsValid(); }

	UFUNCTION(BlueprintPure, Category="Knight|LockOn")
	AActor* GetLockedTarget() const { return LockedActor.Get(); }

	UFUNCTION(BlueprintPure, Category="Knight|LockOn")
	FGameplayTag GetLockedPartTag() const { return LockedPartTag; }

	/** 잠근 지점의 컴포넌트·소켓(뼈). 락온 부위 보정(knightCombatLibrary)이 맞은 부위를 바꿀 때 쓴다. */
	USceneComponent* GetLockedComponent() const { return LockedComponent.Get(); }
	FName GetLockedSocket() const { return LockedSocket; }
	float GetLockOnPartAssistRadius() const { return LockOnPartAssistRadius; }

	bool GetLockedTargetLocation(FVector& OutLocation) const;

	/**
	 * 카메라와 이동(스트레이프) 기준이 되는 안정적인 보스 중심 위치.
	 * 부위 소켓(머리·꼬리 등)의 요동이나 자동 부위 전환에 흔들리지 않도록 대상 중심부를 반환한다.
	 */
	UFUNCTION(BlueprintPure, Category="Knight|LockOn")
	bool GetLockedCameraLocation(FVector& OutLocation) const;

	/** 자동 부위 타겟팅이 켜져 있을 때(수동 핀 고정 중이 아니면) 현재 플레이어 위치에서 가장 알맞은 부위로 즉시 갱신한다. */
	void RefreshAutoNearestPart(bool bIgnoreCooldown = false);

	/** Radius reported by the locked target, for camera framing. Zero without a lock. */
	float GetLockedTargetRadius() const;

	/**
	 * Target for an attack made without a lock: the closest hostile target point within Range whose horizontal
	 * direction is within MaxAngle of Direction (the attack direction). Distance is measured to the target's
	 * surface for single-point targets (QueryTargetRadius), to the point itself for part points.
	 */
	bool FindSoftTarget(const FVector& Direction, float Range, float MaxAngle, FVector& OutLocation, AActor*& OutActor, FGameplayTag& OutPartTag) const;

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

	/**
	 * (2026-10-19 사용자 결정 '락온 중 키 누르면 해제 방식으로 다시') 끄면(기본) 락온 키는 켜기/끄기(ToggleLockOn)다.
	 * 켜면 10-15 방식: 락온 중 누르면 가장 가까운 부위로 다시 잡고, 같은 부위·두 번 누르기면 해제.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn")
	bool bReLockNearestOnPress = false;

	/** (2026-10-15) 락온 키를 이 시간(초, 실제 시간) 안에 두 번 누르면 락온을 푼다. bReLockNearestOnPress일 때만. 바뀔 수 있는 값. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn", meta=(ClampMin=0, Units="s", EditCondition="bReLockNearestOnPress"))
	float LockOnDoublePressTime = 0.3f;

	/**
	 * (2026-10-19 사용자 결정) 락온 부위 보정 거리(cm). 부위에 락온 중 같은 대상을 맞혔고, 맞은 지점이 락온 지점에서 이 거리 안이면
	 * 락온 부위에 맞은 것으로 친다(knightCombatLibrary::ApplyMeleeHit). 0이면 보정 없음. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn", meta=(ClampMin=0, Units="cm"))
	float LockOnPartAssistRadius = 250.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn")
	bool bRequireLineOfSight = true;

	/** Seconds the target may stay out of sight before the lock breaks. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn", meta=(ClampMin=0, Units="s"))
	float LoseSightGraceTime = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn")
	TArray<TEnumAsByte<EObjectTypeQuery>> TargetObjectTypes;

	/**
	 * 락온 대상의 공격 지점(표면에서 FarStopDistance 앞)까지 남은 거리가 이보다 멀면 State.LockOn.Far를 붙인다. (2026-10-12)
	 * 멀 때는 공격 키로 부스트 돌진 베기가 나가고, 가까우면 평소 공격이 나간다. 0이면 태그를 붙이지 않는다.
	 * 일반 공격의 돌진(MaxWarpDistance 300)보다 조금 멀게 둔다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Lunge", meta=(ClampMin=0, Units="cm"))
	float FarTagDistance = 450.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Lunge", meta=(ClampMin=0, Units="cm"))
	float FarStopDistance = 170.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Camera")
	bool bRotateControllerToTarget = true;

	/**
	 * (방안 1 + 수동 부위 락온 하이브리드) 카메라는 보스 중심부(75%)를 축으로 삼고 현재 선택한 부위(25%, CameraPartLocationWeight)만
	 * 부드럽게 반영해, 부위 조준감을 살리면서 머리·꼬리 요동으로 인한 카메라 멀미를 막는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Camera")
	bool bLockCameraToTargetCenter = true;

	/** 카메라 시점 계산 시 선택한 부위 위치를 섞는 비율(0 = 보스 중심 100%, 0.25 = 보스 중심 75% + 선택 부위 25%, 1 = 부위 100%). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Camera", meta=(ClampMin=0, ClampMax=1, EditCondition="bLockCameraToTargetCenter"))
	float CameraPartLocationWeight = 0.25f;

	/** 카메라 중심점이 보스 이동·부위 전환을 따라가는 보간 속도. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Camera", meta=(ClampMin=0, EditCondition="bLockCameraToTargetCenter"))
	float CameraFocusInterpSpeed = 10.f;

	/**
	 * 락온 중 플레이어 위치에 따라 가까운 부위를 자동으로 바꿀지 여부.
	 * 기본값 false: 플레이어가 직접 선택한 부위를 끝까지 유지한다(팀 피드백 반영).
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|AutoPart")
	bool bAutoSelectNearestPart = false;

	/** 자동 부위 전환 시 경계에서 마커가 깜빡이지 않도록 하는 거리 여유치(cm). 새 부위가 현재 부위보다 이 값 이상 더 가까워야 바뀐다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|AutoPart", meta=(ClampMin=0, Units="cm", EditCondition="bAutoSelectNearestPart"))
	float AutoPartSwitchHysteresis = 70.f;

	/** 자동 부위 전환 최소 간격(초). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|AutoPart", meta=(ClampMin=0, Units="s", EditCondition="bAutoSelectNearestPart"))
	float AutoPartSwitchMinInterval = 0.22f;

	/** 마우스 휠이나 패드 스틱으로 수동 부위 전환을 했을 때 자동 부위 변경을 멈추고 핀 고정하는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|AutoPart", meta=(ClampMin=0, Units="s", EditCondition="bAutoSelectNearestPart"))
	float ManualPartPinDuration = 4.5f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Camera", meta=(ClampMin=0))
	float ControlRotationInterpSpeed = 8.f;

	/** Added to the look-at pitch so the Knight stays in frame below a tall target. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Camera", meta=(Units="deg"))
	float PitchOffset = -8.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Camera", meta=(Units="deg"))
	float MinPitch = -50.f;

	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Camera", meta=(Units="deg"))
	float MaxPitch = 35.f;

	/** 락온 중 시점 입력(마우스·패드 오른쪽 스틱)으로 대상을 바꾼다(다크소울 방식). 끄면 휠로만 바꾼다. (2026-10-14) */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Switch")
	bool bSwitchByLookInput = true;

	/**
	 * 마우스 휠로 부위를 전환할 때 화면 좌우 좌표 대신 몬스터 부위 정의 순서(머리 ↔ 앞다리 ↔ 몸통 ↔ 뒷다리 ↔ 꼬리)대로
	 * 끝에서 처음으로 순환(Wrap-Around)하며 막힘없이 전환한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Switch")
	bool bCyclePartsInOrder = true;

	/**
	 * 마우스 이동으로도 바꿀지. (2026-10-15) '마우스 이동에 락온이 바뀌는 게 너무 불편하다'는 의견으로 기본 끔.
	 * 끄면 마우스는 휠·락온 키(가장 가까운 부위)로만 바꾸고, 패드 스틱 전환은 bSwitchByLookInput을 따른다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Switch", meta=(EditCondition="bSwitchByLookInput"))
	bool bSwitchByMouseMove = false;

	/**
	 * 마우스 전환 문턱: SwitchMouseWindow 안에 모인 시점 입력의 크기. 바뀔 수 있는 값(시험 후 조절).
	 * 감도 1에서 입력 1 ≈ 마우스 14픽셀(DefaultInput.ini Mouse 감도 0.07)이라 기본 5 ≈ 0.15초 안에 70픽셀.
	 * 작을수록 살짝만 움직여도 바뀌고, 클수록 크게 휙 움직여야 바뀐다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Switch", meta=(ClampMin=0.1))
	float SwitchMouseThreshold = 5.f;

	/** 마우스 입력을 모으는 시간(초). 이보다 오래 걸린 느린 움직임은 버려서 손 떨림으로는 바뀌지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Switch", meta=(ClampMin=0.01, Units="s"))
	float SwitchMouseWindow = 0.15f;

	/** 마우스로 한 번 바꾼 뒤(또는 그 방향에 대상이 없을 때) 입력을 무시하는 시간(초). 한 번 휙 = 한 칸. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Switch", meta=(ClampMin=0, Units="s"))
	float SwitchMouseCooldown = 0.3f;

	/** 패드 전환 문턱: 오른쪽 스틱 기울기(0~1, 데드존 0.2 뒤 값). 이만큼 튕기면 바꾼다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Switch", meta=(ClampMin=0.1, ClampMax=1))
	float SwitchStickThreshold = 0.8f;

	/** 스틱이 이 기울기 아래로 돌아오면(또는 데드존 안으로 들어가 입력이 끊기면) 다음 전환을 받는다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Switch", meta=(ClampMin=0, ClampMax=1))
	float SwitchStickRearmThreshold = 0.3f;

	/** 입력 방향에서 이 각도 안에 있는 지점만 고른다(화면 기준). 넓을수록 대충 그 쪽이어도 바뀐다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|LockOn|Switch", meta=(ClampMin=1, ClampMax=90, Units="deg"))
	float SwitchMaxAngle = 50.f;

private:
	struct FCandidate
	{
		TWeakObjectPtr<AActor> Actor;
		FknightTargetPoint Point;
	};

	void GatherCandidates(TArray<FCandidate>& OutCandidates) const;
	bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
	bool HasLineOfSight(const FVector& ViewLocation, const FVector& PointLocation, const AActor* Target) const;
	/** 카메라 앞쪽(MaxLockAngle 안)·시야가 트인 지점 중 플레이어와 가장 가까운 지점. 없으면 false. */
	bool FindNearestPointInView(FCandidate& OutCandidate) const;
	void SetLock(const FCandidate& Candidate);
	FVector ComputeTargetCenterLocation(const AActor* Target) const;
	bool SelectBestPartOnActor(AActor* Target, bool bUseHysteresis, FCandidate& OutCandidate) const;
	/**
	 * 잠근 지점이 아직 대상의 락온 지점 목록에 있는지 본다. 빠졌으면(잘려 나간 꼬리 등) InOutLocation(잃은 지점 위치)에서
	 * 가장 가까운 같은 대상의 지점으로 옮기고 InOutLocation을 새 위치로 바꾼다. 남은 지점이 없으면 false.
	 */
	bool KeepLockedPointAvailable(AActor* Target, FVector& InOutLocation);
	void UpdateControlRotation(float DeltaTime) const;
	/** Mirrors the lock onto the owner's ASC as State.LockedOn, which selects the lock-on camera mode. */
	void SetLockedOnTag(bool bLocked) const;
	/** State.LockOn.Blocked(몬스터 어둠 등)가 붙어 있으면 새로 잡지 못하고, 잡고 있던 것도 다음 틱에 풀린다. */
	bool IsLockOnBlocked() const;
	/** State.LockOn.Far를 켜거나 끈다(바뀔 때만). */
	void SetFarTag(bool bFar);
	bool bFarTagged = false;

	TWeakObjectPtr<AActor> LockedActor;
	TWeakObjectPtr<USceneComponent> LockedComponent;
	FName LockedSocket;
	FGameplayTag LockedPartTag;
	float TimeOutOfSight = 0.f;
	FVector SmoothedCameraFocusLocation = FVector::ZeroVector;
	bool bHasSmoothedCameraFocus = false;
	double LastAutoPartSwitchTime = -1.0;
	double ManualPartPinEndTime = -1.0;

	// 시점 입력 전환 상태 (실제 시간 기준).
	FVector2D SwitchMouseSum = FVector2D::ZeroVector;
	double SwitchMouseStartTime = -1.0;
	double SwitchMouseCooldownEndTime = 0.0;
	bool bSwitchStickArmed = true;
	uint64 LastSwitchStickFrame = 0;

	// 락온 키 두 번 누르기 판정 (실제 시간 기준).
	double LastLockOnPressTime = -1.0;
	bool bLastLockOnPressReleased = false;
};
