// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"
#include "Interfaces/CombatInterface.h"
#include "AbilitySystem/knightAbilitySet.h"
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
class UknightInputConfig;
class UknightMeleeTraceComponent;
class UknightMovementComponent;
class UknightTargetLockComponent;
class UMotionWarpingComponent;
struct FGameplayEffectSpec;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

class AknightCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FknightCharacterDeathEvent, AknightCharacter*, DeadCharacter);

/**
 * The player's Knight mech.
 *
 * Owns its AbilitySystemComponent (solo game, so no PlayerState indirection) with the Core and mech
 * attribute sets. Abilities come from DefaultAbilitySets on possession; parts and weapons add more later.
 * Input is bound by tag through InputConfig. Without an InputConfig, the template's direct input
 * actions still work, so existing template maps stay playable during the transition.
 */
UCLASS(abstract)
class KNIGHT_API AknightCharacter : public ACharacter, public IAbilitySystemInterface, public ICombatInterface, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UknightTargetLockComponent> TargetLock;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMotionWarpingComponent> MotionWarping;

	UPROPERTY()
	TObjectPtr<UknightCoreAttributeSet> CoreAttributeSet;

	UPROPERTY()
	TObjectPtr<UknightAttributeSet> MechAttributeSet;

protected:

	/** Tag-based input. When set, it replaces the legacy template input actions below. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Input")
	TObjectPtr<const UknightInputConfig> InputConfig;

	/** Granted once on first possession. */
	UPROPERTY(EditDefaultsOnly, Category="Knight|Abilities")
	TArray<TObjectPtr<const UknightAbilitySet>> DefaultAbilitySets;

	UPROPERTY(EditDefaultsOnly, Category="Knight|Abilities")
	TObjectPtr<UknightAbilityTagRelationshipMapping> TagRelationshipMapping;

	/** Legacy template input. Used only when InputConfig is empty. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Legacy template input. Used only when InputConfig is empty. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Legacy template input. Used only when InputConfig is empty. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Legacy template input. Used only when InputConfig is empty. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:

	AknightCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	//~ ICombatInterface
	virtual bool CanReceiveHit(const AActor* Attacker) const override;
	virtual bool IsDead() const override;

	//~ IGenericTeamAgentInterface
	virtual FGenericTeamId GetGenericTeamId() const override;

	UknightAbilitySystemComponent* GetKnightAbilitySystemComponent() const { return AbilitySystemComponent; }
	UknightInputBufferComponent* GetInputBuffer() const { return InputBuffer; }
	UknightTargetLockComponent* GetTargetLock() const { return TargetLock; }
	UknightMovementComponent* GetKnightMovementComponent() const;

	/** Fires once when Health reaches zero, after State.Dead is applied and Event.Death is sent. */
	UPROPERTY(BlueprintAssignable, Category="Knight|Health")
	FknightCharacterDeathEvent OnDeathStarted;

protected:

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Mouse look: same as Look but uses the mouse sensitivity setting. */
	void LookMouse(const FInputActionValue& Value);

	void ApplyLookInput(FVector2D LookAxisVector, bool bFromMouse);

	void MoveVertical(const FInputActionValue& Value);

	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

	void Input_LockOn();
	void Input_LockOnSwitchLeft();
	void Input_LockOnSwitchRight();

	void InitializeAbilitySystem();

	/** Poise breaks and ordinary hits are turned into reaction events by UknightHitReactionComponent. */
	void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec,
		float DamageMagnitude, float OldValue, float NewValue);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:
	UPROPERTY()
	FknightAbilitySet_GrantedHandles DefaultAbilityHandles;

	bool bAbilitySystemInitialized = false;
	bool bIsDying = false;
};
