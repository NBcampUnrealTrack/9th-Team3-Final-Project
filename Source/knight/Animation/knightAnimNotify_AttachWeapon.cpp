#include "Animation/knightAnimNotify_AttachWeapon.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

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
			Mesh->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
			return;
		}
	}
}

FString UknightAnimNotify_AttachWeapon::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Attach Weapon (%s)"), *Socket.ToString());
}
