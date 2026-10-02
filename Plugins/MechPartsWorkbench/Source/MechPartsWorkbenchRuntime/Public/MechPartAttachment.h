#pragma once

#include "CoreMinimal.h"
#include "MechPartAttachment.generated.h"

class USceneComponent;
class USkeletalMeshComponent;

/** How a part mesh follows the mech skeleton. */
UENUM(BlueprintType)
enum class EMechPartAttachMode : uint8
{
	/** Part shares the mech skeleton and follows its pose (armor skinned to the same bones). */
	LeaderPose,
	/** Part is a rigid piece attached to a socket with an offset (shoulder cannon, backpack). */
	Socket
};

/**
 * Where and how a part mesh sits on the mech. Written precisely in the Mech Parts Workbench viewport,
 * read by the game when a part is equipped.
 */
USTRUCT(BlueprintType)
struct MECHPARTSWORKBENCHRUNTIME_API FMechPartAttachment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attachment")
	EMechPartAttachMode AttachMode = EMechPartAttachMode::LeaderPose;

	/** Socket or bone on the mech mesh. Used in Socket mode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attachment", meta=(EditCondition="AttachMode==EMechPartAttachMode::Socket"))
	FName AttachSocket;

	/** Offset from the socket. Used in Socket mode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attachment", meta=(EditCondition="AttachMode==EMechPartAttachMode::Socket"))
	FTransform RelativeTransform = FTransform::Identity;

	/** Attaches PartMesh to MechMesh according to this data. */
	void Apply(USkeletalMeshComponent* PartMesh, USkeletalMeshComponent* MechMesh) const;
};
