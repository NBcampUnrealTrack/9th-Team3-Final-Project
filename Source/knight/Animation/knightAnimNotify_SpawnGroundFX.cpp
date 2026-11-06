#include "Animation/knightAnimNotify_SpawnGroundFX.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

void UknightAnimNotify_SpawnGroundFX::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* const Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	UWorld* const World = Owner ? Owner->GetWorld() : nullptr;
	if (!Template || !World)
	{
		return;
	}
	// 공격이 고개를 돌려 쓸고 있으면(브레스) 몸이 아니라 고개 방향 앞에 남긴다.
	float AimYaw = 0.f;
	if (const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(Owner))
	{
		Monster->GetHeadAimYaw(AimYaw);
	}
	const FRotator Facing(0.f, Owner->GetActorRotation().Yaw + AimYaw, 0.f);
	const FVector Source = Owner->GetActorLocation() + Facing.RotateVector(FVector(Offset.X, Offset.Y, 0.0));

	// 앞쪽 지형이 높거나 낮을 수 있어 위아래로 넉넉히 찾는다.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(knightSpawnGroundFX), false, Owner);
	if (!World->LineTraceSingleByObjectType(Hit, Source + FVector(0.f, 0.f, MaxGroundDistance), Source - FVector(0.f, 0.f, MaxGroundDistance),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return;
	}
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Template, Hit.ImpactPoint, Facing, FVector(Scale), true, true, ENCPoolMethod::AutoRelease);
}

FString UknightAnimNotify_SpawnGroundFX::GetNotifyName_Implementation() const
{
	return Template ? FString::Printf(TEXT("Ground FX (%s)"), *Template->GetName()) : TEXT("Ground FX");
}
