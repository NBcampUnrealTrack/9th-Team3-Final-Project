#include "Animation/knightAnimNotify_MonsterSound.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"

void UknightAnimNotify_MonsterSound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	const UAbilitySystemComponent* const ASC = MeshComp ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner()) : nullptr;
	if (ASC)
	{
		if (!RequiredTags.IsEmpty() && !ASC->HasAnyMatchingGameplayTags(RequiredTags))
		{
			return;
		}
		if (ASC->HasAnyMatchingGameplayTags(BlockedTags))
		{
			return;
		}
	}
	Super::Notify(MeshComp, Animation, EventReference);
}
