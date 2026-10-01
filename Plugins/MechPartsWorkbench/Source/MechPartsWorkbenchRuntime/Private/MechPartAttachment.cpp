#include "MechPartAttachment.h"

#include "Components/SkeletalMeshComponent.h"

void FMechPartAttachment::Apply(USkeletalMeshComponent* PartMesh, USkeletalMeshComponent* MechMesh) const
{
	if (!PartMesh || !MechMesh)
	{
		return;
	}
	if (AttachMode == EMechPartAttachMode::LeaderPose)
	{
		PartMesh->AttachToComponent(MechMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		PartMesh->SetLeaderPoseComponent(MechMesh);
		return;
	}
	PartMesh->SetLeaderPoseComponent(nullptr);
	PartMesh->AttachToComponent(MechMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
	PartMesh->SetRelativeTransform(RelativeTransform);
}
