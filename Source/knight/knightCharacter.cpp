// Copyright Epic Games, Inc. All Rights Reserved.

#include "knightCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Camera/knightCameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayEffect.h"
#include "Character/knightMovementComponent.h"
#include "Combat/knightHitReactionComponent.h"
#include "Combat/knightHitStopComponent.h"
#include "Combat/knightMeleeTraceComponent.h"
#include "Combat/knightTargetLockComponent.h"
#include "MotionWarpingComponent.h"
#include "Input/knightInputBufferComponent.h"
#include "Input/knightInputComponent.h"
#include "Settings/knightSettingsSubsystem.h"
#include "Settings/knightSharedSettings.h"
#include "System/knightGameplayTags.h"
#include "System/knightTeams.h"
#include "knightGameMode.h"
#include "knight.h"

AknightCharacter::AknightCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UknightMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Movement tuning lives in UknightMovementComponent; override per mech in the Blueprint.

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	// Mode-driven camera. Until DefaultCameraMode is set it behaves like the template camera on the boom.
	FollowCamera = CreateDefaultSubobject<UknightCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	AbilitySystemComponent = CreateDefaultSubobject<UknightAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// Attribute sets created as subobjects of the ASC's owner are registered by the ASC automatically.
	CoreAttributeSet = CreateDefaultSubobject<UknightCoreAttributeSet>(TEXT("CoreAttributeSet"));
	MechAttributeSet = CreateDefaultSubobject<UknightAttributeSet>(TEXT("MechAttributeSet"));

	InputBuffer = CreateDefaultSubobject<UknightInputBufferComponent>(TEXT("InputBuffer"));
	MeleeTrace = CreateDefaultSubobject<UknightMeleeTraceComponent>(TEXT("MeleeTrace"));
	HitStop = CreateDefaultSubobject<UknightHitStopComponent>(TEXT("HitStop"));
	HitReaction = CreateDefaultSubobject<UknightHitReactionComponent>(TEXT("HitReaction"));
	TargetLock = CreateDefaultSubobject<UknightTargetLockComponent>(TEXT("TargetLock"));
	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character)
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

UAbilitySystemComponent* AknightCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UknightMovementComponent* AknightCharacter::GetKnightMovementComponent() const
{
	return Cast<UknightMovementComponent>(GetCharacterMovement());
}

bool AknightCharacter::CanReceiveHit(const AActor* Attacker) const
{
	return !IsDead() && Attacker != this;
}

bool AknightCharacter::IsDead() const
{
	return bIsDying || (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(knightGameplayTags::State_Dead));
}

FGenericTeamId AknightCharacter::GetGenericTeamId() const
{
	return knightTeams::ToTeamId(EknightTeam::Player);
}

void AknightCharacter::BeginPlay()
{
	Super::BeginPlay();

	CoreAttributeSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);
}

void AknightCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystem();
}

void AknightCharacter::InitializeAbilitySystem()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	if (bAbilitySystemInitialized)
	{
		return;
	}
	bAbilitySystemInitialized = true;

	AbilitySystemComponent->SetTagRelationshipMapping(TagRelationshipMapping);
	for (const UknightAbilitySet* const AbilitySet : DefaultAbilitySets)
	{
		if (AbilitySet)
		{
			AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &DefaultAbilityHandles, this);
		}
	}
}

void AknightCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (InputConfig)
	{
		UknightInputComponent* const KnightInput = Cast<UknightInputComponent>(PlayerInputComponent);
		if (!KnightInput)
		{
			UE_LOG(Logknight, Error, TEXT("'%s' has an InputConfig but the input component is not knightInputComponent. Check DefaultInputComponentClass in DefaultInput.ini."), *GetNameSafe(this));
			return;
		}
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Move);
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_Move_Vertical, ETriggerEvent::Triggered, this, &ThisClass::MoveVertical, false);
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ThisClass::Look);
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_Look_Mouse, ETriggerEvent::Triggered, this, &ThisClass::LookMouse, false);
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_LockOn, ETriggerEvent::Started, this, &ThisClass::Input_LockOn, false);
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_LockOn_SwitchLeft, ETriggerEvent::Started, this, &ThisClass::Input_LockOnSwitchLeft, false);
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_LockOn_SwitchRight, ETriggerEvent::Started, this, &ThisClass::Input_LockOnSwitchRight, false);

		TArray<uint32> BindHandles;
		KnightInput->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, BindHandles);
		return;
	}

	// Legacy template bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AknightCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AknightCharacter::LookMouse);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AknightCharacter::Look);
	}
	else
	{
		UE_LOG(Logknight, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AknightCharacter::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	AbilitySystemComponent->AbilityInputTagPressed(InputTag);
	InputBuffer->RecordInput(InputTag);
}

void AknightCharacter::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	AbilitySystemComponent->AbilityInputTagReleased(InputTag);
}

void AknightCharacter::Input_LockOn()
{
	if (!IsDead())
	{
		TargetLock->ToggleLockOn();
	}
}

void AknightCharacter::Input_LockOnSwitchLeft()
{
	TargetLock->SwitchTarget(false);
}

void AknightCharacter::Input_LockOnSwitchRight()
{
	TargetLock->SwitchTarget(true);
}

void AknightCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AknightCharacter::MoveVertical(const FInputActionValue& Value)
{
	if (!IsDead())
	{
		if (UknightMovementComponent* const KnightMovement = GetKnightMovementComponent())
		{
			KnightMovement->AddFlightVerticalInput(Value.Get<float>());
		}
	}
}

void AknightCharacter::Look(const FInputActionValue& Value)
{
	ApplyLookInput(Value.Get<FVector2D>(), false);
}

void AknightCharacter::LookMouse(const FInputActionValue& Value)
{
	ApplyLookInput(Value.Get<FVector2D>(), true);
}

void AknightCharacter::ApplyLookInput(FVector2D LookAxisVector, bool bFromMouse)
{
	// Personal settings: per-device sensitivity and Y inversion.
	const APlayerController* const PC = Cast<APlayerController>(GetController());
	const ULocalPlayer* const LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UknightSettingsSubsystem* const Settings = LocalPlayer ? LocalPlayer->GetSubsystem<UknightSettingsSubsystem>() : nullptr;
	if (const UknightSharedSettings* const Shared = Settings ? Settings->GetSharedSettings() : nullptr)
	{
		LookAxisVector *= bFromMouse ? Shared->MouseSensitivity : Shared->GamepadSensitivity;
		if (Shared->bInvertLookY)
		{
			LookAxisVector.Y = -LookAxisVector.Y;
		}
	}

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AknightCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr && !IsDead())
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AknightCharacter::DoLook(float Yaw, float Pitch)
{
	// While locked on, the lock drives the view; switching points uses the dedicated inputs.
	if (GetController() != nullptr && !TargetLock->IsLockedOn())
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AknightCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AknightCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void AknightCharacter::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec,
	float DamageMagnitude, float OldValue, float NewValue)
{
	if (bIsDying)
	{
		return;
	}
	bIsDying = true;

	AbilitySystemComponent->AddLooseGameplayTag(knightGameplayTags::State_Dead);
	AbilitySystemComponent->AddLooseGameplayTag(knightGameplayTags::State_InputBlocked);
	AbilitySystemComponent->ClearAbilityInput();
	InputBuffer->ClearBuffer();
	TargetLock->ClearLockOn();
	GetCharacterMovement()->StopMovementImmediately();

	// A death ability triggered by Event.Death plays the presentation. Everything else is cancelled.
	FGameplayEventData Payload;
	Payload.EventTag = knightGameplayTags::Event_Death;
	Payload.Instigator = DamageInstigator;
	Payload.Target = this;
	Payload.EventMagnitude = DamageMagnitude;
	if (DamageEffectSpec)
	{
		Payload.ContextHandle = DamageEffectSpec->GetEffectContext();
	}
	AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);

	const FGameplayTagContainer KeepAbilities(knightGameplayTags::Ability_Death);
	AbilitySystemComponent->CancelAbilities(nullptr, &KeepAbilities);

	UE_LOG(Logknight, Log, TEXT("%s was destroyed by %s."), *GetNameSafe(this), *GetNameSafe(DamageInstigator));
	OnDeathStarted.Broadcast(this);

	if (AknightGameMode* const GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AknightGameMode>() : nullptr)
	{
		GameMode->HandlePlayerDeath(GetController());
	}
}
