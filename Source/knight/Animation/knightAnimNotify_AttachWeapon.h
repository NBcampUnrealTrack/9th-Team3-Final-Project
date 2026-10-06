#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "knightAnimNotify_AttachWeapon.generated.h"

/**
 * Moves the weapon mesh to another socket on the character mesh at this frame.
 * Draw: put it on weapon_r where the hand grabs the hilt. Sheathe: put it on weapon_back where it is released.
 *
 * The weapon is found by component tag ("Weapon" by default), the same tag UknightPartsComponent gives the
 * spawned weapon mesh and the melee trace uses. Editor preview actors have no weapon, so this is a no-op there.
 */
UCLASS(meta=(DisplayName="Knight Attach Weapon"))
class KNIGHT_API UknightAnimNotify_AttachWeapon : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** Socket on the character mesh the weapon snaps to. */
	UPROPERTY(EditAnywhere, Category="Weapon")
	FName Socket = TEXT("weapon_r");

	UPROPERTY(EditAnywhere, Category="Weapon")
	FName WeaponComponentTag = TEXT("Weapon");
};
