#include "Animation/knightAnimNotify_AttachWeapon.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Item/knightPartsComponent.h"

void UknightAnimNotify_AttachWeapon::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* const Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!Owner || !MeshComp->DoesSocketExist(Socket))
	{
		return;
	}

	TArray<UMeshComponent*> Meshes;
	Owner->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* const Mesh : Meshes)
	{
		if (Mesh != MeshComp && Mesh->ComponentHasTag(WeaponComponentTag))
		{
			// (2026-10-26) 손 소켓이면 무기 파츠의 워크벤치 위치·회전을 다시 적용한다(UknightPartsComponent::AttachWeaponToSocket).
			UknightPartsComponent::AttachWeaponToSocket(Mesh, MeshComp, Socket);
			return;
		}
	}
}

FString UknightAnimNotify_AttachWeapon::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Attach Weapon (%s)"), *Socket.ToString());
}
