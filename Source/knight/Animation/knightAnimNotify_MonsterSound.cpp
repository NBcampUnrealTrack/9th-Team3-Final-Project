#include "Animation/knightAnimNotify_MonsterSound.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sound/SoundBase.h"
#include "knight.h"

void UknightAnimNotify_MonsterSound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	const UAbilitySystemComponent* const ASC = MeshComp ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner()) : nullptr;
	if (ASC)
	{
		if (!RequiredTags.IsEmpty() && !ASC->HasAnyMatchingGameplayTags(RequiredTags))
		{
			UE_LOG(LogknightAbility, Verbose, TEXT("%s: skipped %s on %s (missing RequiredTags)."), *GetNameSafe(MeshComp->GetOwner()), *GetNameSafe(Sound), *GetNameSafe(Animation));
			return;
		}
		if (ASC->HasAnyMatchingGameplayTags(BlockedTags))
		{
			return;
		}
	}
	UE_LOG(LogknightAbility, Verbose, TEXT("%s: played %s on %s."), *GetNameSafe(MeshComp ? MeshComp->GetOwner() : nullptr), *GetNameSafe(Sound), *GetNameSafe(Animation));
	Super::Notify(MeshComp, Animation, EventReference);
}
