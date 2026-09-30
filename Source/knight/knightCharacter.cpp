// Copyright Epic Games, Inc. All Rights Reserved.

#include "knightCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayEffect.h"
#include "Character/knightMovementComponent.h"
#include "Input/knightInputBufferComponent.h"
#include "Input/knightInputComponent.h"
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
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	AbilitySystemComponent = CreateDefaultSubobject<UknightAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// Attribute sets created as subobjects of the ASC's owner are registered by the ASC automatically.
	CoreAttributeSet = CreateDefaultSubobject<UknightCoreAttributeSet>(TEXT("CoreAttributeSet"));
	MechAttributeSet = CreateDefaultSubobject<UknightAttributeSet>(TEXT("MechAttributeSet"));

	InputBuffer = CreateDefaultSubobject<UknightInputBufferComponent>(TEXT("InputBuffer"));

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
	CoreAttributeSet->OnPoiseBroken.AddUObject(this, &ThisClass::HandlePoiseBroken);
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
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_Look_Mouse, ETriggerEvent::Triggered, this, &ThisClass::Look, false);

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
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AknightCharacter::Look);

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
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

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
	if (GetController() != nullptr)
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

void AknightCharacter::HandlePoiseBroken(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec,
	float DamageMagnitude, float OldValue, float NewValue)
{
	if (IsDead())
	{
		return;
	}
	// A stagger ability triggered by Event.PoiseBroken plays the reaction and restores Poise.
	FGameplayEventData Payload;
	Payload.EventTag = knightGameplayTags::Event_PoiseBroken;
	Payload.Instigator = DamageInstigator;
	Payload.Target = this;
	Payload.EventMagnitude = DamageMagnitude;
	if (DamageEffectSpec)
	{
		Payload.ContextHandle = DamageEffectSpec->GetEffectContext();
	}
	AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);
}
