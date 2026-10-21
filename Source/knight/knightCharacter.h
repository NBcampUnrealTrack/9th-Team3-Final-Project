// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"
#include "Interfaces/CombatInterface.h"
#include "AbilitySystem/knightAbilitySet.h"
#include "GameplayEffectTypes.h"
#include "Logging/LogMacros.h"
#include "knightCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UknightAbilitySystemComponent;
class UknightAbilityTagRelationshipMapping;
class UknightAttributeSet;
class UknightCoreAttributeSet;
class UknightInputBufferComponent;
class UknightHitReactionComponent;
class UknightHitStopComponent;
class UknightGroggyFeedbackComponent;
class UknightInputConfig;
class UknightMeleeTraceComponent;
class UknightMovementComponent;
class UknightTargetLockComponent;
class UMotionWarpingComponent;
class UknightDamageVisualComponent;
class UknightPartsComponent;
class UknightPartHealthComponent;
class UGameplayEffect;
struct FGameplayEffectSpec;
struct FGameplayEventData;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

class AknightCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FknightCharacterDeathEvent, AknightCharacter*, DeadCharacter);

/**
 * 플레이어의 나이트 메카.
 *
 * AbilitySystemComponent를 직접 가진다(싱글 게임이라 PlayerState를 거치지 않음). Core·메카 속성 세트가 붙는다.
 * 어빌리티는 빙의할 때 DefaultAbilitySets에서 받고, 이후 파츠와 무기가 더 추가한다.
 * 입력은 InputConfig를 통해 태그로 연결한다. InputConfig가 없으면 템플릿의 직접 입력 액션이 그대로 동작해서,
 * 기존 템플릿 맵도 전환 기간 동안 플레이할 수 있다.
 */
UCLASS(abstract)
class KNIGHT_API AknightCharacter : public ACharacter, public IAbilitySystemInterface, public ICombatInterface, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

	/** 캐릭터 뒤에 카메라를 두는 카메라 붐 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** 따라오는 카메라 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UknightAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UknightInputBufferComponent> InputBuffer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UknightMeleeTraceComponent> MeleeTrace;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UknightHitStopComponent> HitStop;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UknightHitReactionComponent> HitReaction;

	/** (2026-10-14) 내가 상대의 그로기를 일으키면 큰 히트스탑·이펙트(GCN_Hit_Groggy). 값은 이 컴포넌트에서 조절. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UknightGroggyFeedbackComponent> GroggyFeedback;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UknightTargetLockComponent> TargetLock;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMotionWarpingComponent> MotionWarping;

	/** 메카 파츠와 무기: 메시와, 파츠가 주는 스탯·어빌리티. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UknightPartsComponent> Parts;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UknightDamageVisualComponent> DamageVisual;

	/** 부위 체력: 맞은 부위 판정, 부위 피해 분배, 파괴·복구, 최대 체력 = 부위 최대 체력의 합. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UknightPartHealthComponent> PartHealth;

	UPROPERTY()
	TObjectPtr<UknightCoreAttributeSet> CoreAttributeSet;

	UPROPERTY()
	TObjectPtr<UknightAttributeSet> MechAttributeSet;

protected:

	/** 태그 기반 입력. 지정하면 아래 템플릿 입력 액션 대신 쓰인다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Input")
	TObjectPtr<const UknightInputConfig> InputConfig;

	/** 처음 빙의할 때 한 번 부여한다. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Abilities")
	TArray<TObjectPtr<const UknightAbilitySet>> DefaultAbilitySets;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Abilities")
	TObjectPtr<UknightAbilityTagRelationshipMapping> TagRelationshipMapping;

	/**
	 * (2026-10-16 사용자 결정) 퍼펙트 가드 1번에 차는 수리 키트 게이지(메카 속성 RepairKitGauge, 100이면 키트 +1).
	 * 기본 20 = 퍼펙트 가드 5번에 키트 1개. 키트가 최대 개수(MaxRepairKitCount)면 차지 않는다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|RepairKit", meta=(ClampMin=0))
	float RepairKitGaugePerPerfectGuard = 20.f;

	/** This socket bonus is a removable GE; kit count is never replenished by a loadout refresh. */
	UPROPERTY(Transient) TObjectPtr<UGameplayEffect> SocketRepairCapacityEffect;
	FActiveGameplayEffectHandle SocketRepairCapacityHandle;
	int32 AppliedSocketRepairBonus = INDEX_NONE;
	UFUNCTION() void HandleSocketProgressionChanged();

	/**
	 * (2026-10-20 사용자 결정) 기본 메카 스탯 중 속성 세트 기본값과 다른 것. 파츠를 입히기 전에 기본값(Base)으로 넣는다.
	 * 그로기 자연 회복: 안 맞은 지 ASC의 PoiseRecoveryDelay(BP_Knight 3초)가 지나면 초당 이만큼 오른다. 바뀔 수 있는 값.
	 * (속성 세트 기본값은 0이고 몬스터도 같은 세트를 써서, 플레이어만 여기서 넣는다.)
	 * (2026-10-21) 10 → 5, 대기 2 → 3초: 회복이 빨라 가드·피격으로 쌓인 그로기가 금방 비어 기절이 거의 안 났다.
	 * (2026-10-21) 5 → 2: 체력 150 판에서도 7분 동안 기절 0번(맞는 간격이 평균 14초라 그 사이 다 찼다). 100이 차는 데 50초.
	 * (2026-10-21) 2 → 1: main 병합 뒤 기본 메카 판에서도 기절 0번. 100이 차는 데 100초.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|BaseStats", meta=(ClampMin=0))
	float BasePoiseRecoveryRate = 1.f;

	/**
	 * (2026-10-20 사용자 결정) 기본 가드 효율. 0.8이면 막을 때 피해 0.36배·그로기 0.6배(1.0이면 0.2배·0.5배).
	 * 막으면 사실상 피해가 없던 것을 낮췄다. 파츠로 올릴 여지를 둔다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|BaseStats", meta=(ClampMin=0))
	float BaseGuardEfficiency = 0.8f;

	/**
	 * (2026-10-21 사용자 결정) 치명타 피해 배율 1.25 → 1.5. 치명타 확률은 파츠(머리)가 준다. 바뀔 수 있는 값.
	 * (속성 세트 기본값 1.25는 몬스터도 같이 쓰므로 그대로 두고, 플레이어만 여기서 넣는다.)
	 */
	UPROPERTY(EditDefaultsOnly, Category="Knight|BaseStats", meta=(ClampMin=1))
	float BaseCriticalDamageMultiplier = 1.5f;

	/** 템플릿 입력. InputConfig가 비어 있을 때만 쓴다. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** 템플릿 입력. InputConfig가 비어 있을 때만 쓴다. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** 템플릿 입력. InputConfig가 비어 있을 때만 쓴다. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** 템플릿 입력. InputConfig가 비어 있을 때만 쓴다. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:

	AknightCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	//~ ICombatInterface
	virtual bool CanReceiveHit(const AActor* Attacker) const override;
	virtual bool IsDead() const override;
	/** 맞은 위치 → 나이트 부위 태그(Knight.Part.*). 부위 체력 컴포넌트가 판정한다. */
	virtual FGameplayTag ResolveHitPart(const FHitResult& Hit) const override;
	/** 파괴된 부위 재피격 크리티컬과 머리 파괴 피해 증가. 부위 체력 컴포넌트가 계산한다. */
	virtual float GetPartDamageMultiplier(const FGameplayTag& PartTag) const override;

	//~ IGenericTeamAgentInterface
	virtual FGenericTeamId GetGenericTeamId() const override;

	UknightAbilitySystemComponent* GetKnightAbilitySystemComponent() const { return AbilitySystemComponent; }
	UknightInputBufferComponent* GetInputBuffer() const { return InputBuffer; }
	UknightTargetLockComponent* GetTargetLock() const { return TargetLock; }
	UknightPartHealthComponent* GetPartHealth() const { return PartHealth; }
	UknightMovementComponent* GetKnightMovementComponent() const;
	/** Recompute the equipped socket capacity without restoring spent repair kits. */
	void RefreshSocketEffects();

	/** Health가 0이 되면 한 번 발생한다. State.Dead가 붙고 Event.Death가 보내진 뒤다. */
	UPROPERTY(BlueprintAssignable, Category="Knight|Health")
	FknightCharacterDeathEvent OnDeathStarted;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;

	/** 입력 액션 연결을 초기화한다 */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 이동 입력 */
	void Move(const FInputActionValue& Value);

	/** 시점 입력 */
	void Look(const FInputActionValue& Value);

	/** 마우스 시점: Look과 같지만 마우스 감도 설정을 쓴다. */
	void LookMouse(const FInputActionValue& Value);

	void ApplyLookInput(FVector2D LookAxisVector, bool bFromMouse);

	void MoveVertical(const FInputActionValue& Value);

	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

	void Input_LockOn();
	void Input_LockOnSwitchLeft();
	void Input_LockOnSwitchRight();

	/**
	 * 달리기 입력(InputTag.Run, 기본 LeftShift). 누르고 있는 동안만 달린다.
	 * (2026-10-08 달리기와 퀵스텝 키 분리: Shift 달리기, Ctrl 퀵스텝, Space는 점프 예정.
	 *  전에는 Shift를 누르고 있으면 달리고 두 번 탭하면 퀵스텝이었다. 같은 날 달리기 토글 설정은 빼기로 함)
	 */
	void Input_RunPressed();
	void Input_RunReleased();

	/** 달리기를 켜고 끈다: 무브먼트 속도 + State.Sprinting(켜져 있는 동안 공격 키가 1타 대신 달리기 공격). */
	void SetRunning(bool bRun);

	void InitializeAbilitySystem();

	/** Poise 붕괴와 일반 피격은 UknightHitReactionComponent가 반응 이벤트로 바꾼다. */
	void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec,
		float DamageMagnitude, float OldValue, float NewValue);

	/** 퍼펙트 가드 성공(Event.ParrySucceeded): 키트가 최대보다 적으면 수리 키트 게이지를 채우고, 꽉 차면 키트 +1. (2026-10-16) */
	void HandlePerfectGuard(const FGameplayEventData* Payload);

public:

	/** 조작이나 UI에서 오는 이동 입력을 처리한다 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** 조작이나 UI에서 오는 시점 입력을 처리한다 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** 조작이나 UI에서 오는 점프 누름을 처리한다 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** 조작이나 UI에서 오는 점프 뗌을 처리한다 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** CameraBoom 하위 오브젝트를 돌려준다 **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** FollowCamera 하위 오브젝트를 돌려준다 **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:
	UPROPERTY()
	FknightAbilitySet_GrantedHandles DefaultAbilityHandles;

	bool bAbilitySystemInitialized = false;
	bool bIsDying = false;
};
