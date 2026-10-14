#include "MechPartAttachment.h"

#include "Components/SkeletalMeshComponent.h"

void FMechPartAttachment::Apply(UMeshComponent* PartMesh, USkeletalMeshComponent* MechMesh) const
{
	if (!PartMesh || !MechMesh)
	{
		return;
	}
	USkeletalMeshComponent* const SkeletalPart = Cast<USkeletalMeshComponent>(PartMesh);
	// 붙일 때 메카의 크기(액터 배율)를 이어받는다. 메카를 키우면 파츠도 같이 커진다.
	if (AttachMode == EMechPartAttachMode::LeaderPose && SkeletalPart)
	{
		PartMesh->AttachToComponent(MechMesh, FAttachmentTransformRules::SnapToTargetIncludingScale);
		SkeletalPart->SetLeaderPoseComponent(MechMesh);
		return;
	}
	// 소켓 방식. 스태틱 메시는 뼈대가 없어 LeaderPose를 쓸 수 없으므로 모드와 상관없이 여기로 온다
	// (파츠 정의의 데이터 검증이 'Socket으로 바꾸라'고 경고한다).
	if (SkeletalPart)
	{
		SkeletalPart->SetLeaderPoseComponent(nullptr);
	}
	PartMesh->AttachToComponent(MechMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, AttachSocket);
	PartMesh->SetRelativeTransform(RelativeTransform);
}
