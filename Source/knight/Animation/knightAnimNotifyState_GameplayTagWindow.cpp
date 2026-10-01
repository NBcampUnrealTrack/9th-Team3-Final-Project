#include "Animation/knightAnimNotifyState_GameplayTagWindow.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"

void UknightAnimNotifyState_GameplayTagWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!Tag.IsValid() || !MeshComp)
	{
		return;
	}
	if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner()))
	{
		ASC->AddLooseGameplayTag(Tag);
	}
}

void UknightAnimNotifyState_GameplayTagWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (Tag.IsValid() && MeshComp)
	{
		if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner()))
		{
			ASC->RemoveLooseGameplayTag(Tag);
		}
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UknightAnimNotifyState_GameplayTagWindow::GetNotifyName_Implementation() const
{
	return Tag.IsValid() ? Tag.ToString() : TEXT("Gameplay Tag Window (none)");
}
