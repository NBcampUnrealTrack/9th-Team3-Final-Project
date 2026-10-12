#include "Animation/knightAnimNotify_MeleeArea.h"

#include "Combat/knightMeleeTraceComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UknightAnimNotify_MeleeArea::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	// 에디터 미리보기 액터에는 판정 컴포넌트가 없어 아무것도 하지 않는다.
	const AActor* const Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UknightMeleeTraceComponent* const Trace = Owner ? Owner->FindComponentByClass<UknightMeleeTraceComponent>() : nullptr)
	{
		Trace->HitArea(Owner->GetActorTransform().TransformPosition(Offset), Radius);
	}
}

FString UknightAnimNotify_MeleeArea::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Melee Area (r%.0f)"), Radius);
}
