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
	// 회전은 FRotator 그대로 넘긴다(쿼터니언을 거치면 입력한 각도와 다른 조합으로 바뀐다).
	PartMesh->SetRelativeLocationAndRotation(RelativeLocation, RelativeRotation);
	PartMesh->SetRelativeScale3D(RelativeScale);
}

void FMechPartAttachment::PostSerialize(const FArchive& Ar)
{
	// (2026-10-23) 예전 형식(FTransform 하나)으로 저장된 에셋: 값을 위치·회전·크기로 옮기고 비운다.
	// 비워 두므로 다음에 불러올 때(되돌리기 포함)는 다시 옮기지 않는다. 다음에 저장하면 새 형식만 남는다.
	if (Ar.IsLoading() && !RelativeTransform.Equals(FTransform::Identity))
	{
		RelativeLocation = RelativeTransform.GetLocation();
		RelativeRotation = RelativeTransform.Rotator();
		RelativeScale = RelativeTransform.GetScale3D();
		RelativeTransform = FTransform::Identity;
	}
}
