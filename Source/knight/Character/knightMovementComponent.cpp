#include "Character/knightMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "System/knightGameplayTags.h"

UknightMovementComponent::UknightMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Ground: heavy start and stop, slow turning.
	MaxWalkSpeed = 600.f;
	MinAnalogWalkSpeed = 20.f;
	MaxAcceleration = 1200.f;
	BrakingDecelerationWalking = 1400.f;
	GroundFriction = 6.f;
	RotationRate = FRotator(0.f, 360.f, 0.f);
	bOrientRotationToMovement = true;

	// Air: falls fast, little steering.
	GravityScale = 1.6f;
	JumpZVelocity = 600.f;
	AirControl = 0.2f;
	BrakingDecelerationFalling = 800.f;

	// Boost flight.
	MaxFlySpeed = 1200.f;
	BrakingDecelerationFlying = 900.f;
}

bool UknightMovementComponent::StartFlight()
{
	if (!UpdatedComponent || IsInFlight())
	{
		return false;
	}
	SetMovementMode(MOVE_Flying);
	return IsInFlight();
}

void UknightMovementComponent::StopFlight()
{
	if (IsInFlight())
	{
		SetMovementMode(MOVE_Falling);
	}
}

void UknightMovementComponent::AddFlightVerticalInput(float Scale)
{
	if (IsInFlight() && PawnOwner && !FMath::IsNearlyZero(Scale))
	{
		PawnOwner->AddMovementInput(FVector::UpVector, Scale);
	}
}

void UknightMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	SyncMovementTags();
}

void UknightMovementComponent::SyncMovementTags() const
{
	UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!ASC)
	{
		return;
	}
	ASC->SetLooseGameplayTagCount(knightGameplayTags::State_Flying, IsInFlight() ? 1 : 0);
	ASC->SetLooseGameplayTagCount(knightGameplayTags::State_Airborne, (IsInFlight() || IsFalling()) ? 1 : 0);
}
