#include "Input/knightInputBufferComponent.h"

#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "Engine/World.h"
#include "System/knightGameplayTags.h"

UknightInputBufferComponent::UknightInputBufferComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UknightInputBufferComponent::RecordInput(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid() || !GetWorld())
	{
		return;
	}
	BufferedTag = InputTag;
	BufferedTime = GetWorld()->GetTimeSeconds();
	bFreshPress = true;
}

void UknightInputBufferComponent::ProcessBuffer(UknightAbilitySystemComponent* ASC)
{
	if (!BufferedTag.IsValid())
	{
		return;
	}
	if (!ASC || IsExpired() || ASC->HasMatchingGameplayTag(knightGameplayTags::State_InputBlocked))
	{
		ClearBuffer();
		return;
	}
	if (bFreshPress)
	{
		bFreshPress = false;
		// The normal input path already handled this press.
		if (ASC->WasInputTagActivatedThisFrame(BufferedTag))
		{
			ClearBuffer();
			return;
		}
	}
	if (ASC->TryActivateAbilityByInputTag(BufferedTag))
	{
		ClearBuffer();
	}
}

bool UknightInputBufferComponent::ConsumeBufferedInput(FGameplayTag InputTag)
{
	if (!HasBufferedInput(InputTag))
	{
		return false;
	}
	ClearBuffer();
	return true;
}

bool UknightInputBufferComponent::HasBufferedInput(FGameplayTag InputTag) const
{
	return BufferedTag.IsValid() && BufferedTag == InputTag && !IsExpired();
}

void UknightInputBufferComponent::ClearBuffer()
{
	BufferedTag = FGameplayTag();
	BufferedTime = 0.0;
	bFreshPress = false;
}

bool UknightInputBufferComponent::IsExpired() const
{
	const UWorld* const World = GetWorld();
	return !World || World->GetTimeSeconds() - BufferedTime > BufferDuration;
}
