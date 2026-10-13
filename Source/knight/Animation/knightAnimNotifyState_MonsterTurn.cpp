#include "Animation/knightAnimNotifyState_MonsterTurn.h"

#include "Components/SkeletalMeshComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterMovementComponent.h"
#include "Monster/knightMonsterTargetingComponent.h"

namespace knightMonsterTurnNotify
{
	// 허용 각도에 들어와도 구간이 끝날 때까지 계속 따라가므로 작게 둔다.
	constexpr float Tolerance = 1.f;
}

void UknightAnimNotifyState_MonsterTurn::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	// 에디터 미리보기 액터는 몬스터가 아니므로 아무것도 하지 않는다.
	const AknightMonsterCharacterBase* const Monster = MeshComp ? Cast<AknightMonsterCharacterBase>(MeshComp->GetOwner()) : nullptr;
	UknightMonsterMovementComponent* const Movement = Monster ? Monster->GetMonsterMovementComponent() : nullptr;
	const UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	if (!Movement || !Target)
	{
		return;
	}
	const FVector ToTarget = (Target->GetActorLocation() - Monster->GetActorLocation()).GetSafeNormal2D();
	const FVector Facing = Monster->GetActorLocation() + ToTarget.RotateAngleAxis(YawOffset, FVector::UpVector) * 100.f;
	Movement->TurnTowards(Facing, FrameDeltaTime, TurnRate, knightMonsterTurnNotify::Tolerance);
}

FString UknightAnimNotifyState_MonsterTurn::GetNotifyName_Implementation() const
{
	return FMath::IsNearlyZero(YawOffset) ? FString(TEXT("Monster Turn")) : FString::Printf(TEXT("Monster Turn (%+.0f)"), YawOffset);
}
