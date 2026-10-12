#pragma once

#include "CoreMinimal.h"
#include "MechPartAttachment.generated.h"

class UMeshComponent;
class USceneComponent;
class USkeletalMeshComponent;

/** How a part mesh follows the mech skeleton. */
UENUM(BlueprintType)
enum class EMechPartAttachMode : uint8
{
	/** Part shares the mech skeleton and follows its pose (armor skinned to the same bones). 스켈레탈 메시 파츠만. */
	LeaderPose,
	/** Part is a rigid piece attached to a socket with an offset (shoulder cannon, backpack). 스태틱 메시 파츠는 항상 이 방식. */
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

	/**
	 * Attaches PartMesh to MechMesh according to this data.
	 * (2026-10-10) PartMesh는 스켈레탈·스태틱 메시 컴포넌트 둘 다 받는다. 스태틱 메시는 뼈대가 없어서
	 * AttachMode가 LeaderPose여도 Socket 방식(AttachSocket + RelativeTransform)으로 붙인다.
	 */
	void Apply(UMeshComponent* PartMesh, USkeletalMeshComponent* MechMesh) const;
};
