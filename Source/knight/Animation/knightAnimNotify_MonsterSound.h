#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "GameplayTagContainer.h"
#include "knightAnimNotify_MonsterSound.generated.h"

/**
 * 소리 재생(Play Sound)에 몬스터 태그 조건을 붙인 것. 여러 어빌리티가 같은 몽타주를 쓸 때 한쪽에서만 소리를 낸다
 * (잔향 포효 몽타주: 첫 만남 포효는 반응 어빌리티 중에만, 활강·회오리 소환에서는 다른 울음).
 * 조건은 메시 주인의 어빌리티 시스템 태그로 본다. 어빌리티 시스템이 없으면 조건 없이 재생한다.
 */
UCLASS(meta=(DisplayName="Knight Monster Sound"))
class KNIGHT_API UknightAnimNotify_MonsterSound : public UAnimNotify_PlaySound
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	/** 이 태그가 하나라도 있어야 재생한다. 비우면 조건 없음. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AnimNotify")
	FGameplayTagContainer RequiredTags;

	/** 이 태그가 하나라도 있으면 재생하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AnimNotify")
	FGameplayTagContainer BlockedTags;
};
