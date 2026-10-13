#include "MechBodyRegionSet.h"

#include "Components/SkeletalMeshComponent.h"

void UMechBodyRegionSet::SetupRegionComponent(USkeletalMeshComponent* RegionComponent, USkeletalMesh* Mesh, USkeletalMeshComponent* Mech)
{
	if (!RegionComponent || !Mech)
	{
		return;
	}
	RegionComponent->SetSkeletalMeshAsset(Mesh);
	RegionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RegionComponent->AttachToComponent(Mech, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	RegionComponent->SetLeaderPoseComponent(Mech);
}

void UMechBodyRegionSet::SetMechBodyHidden(USkeletalMeshComponent* Mech, bool bHidden)
{
	if (!Mech)
	{
		return;
	}
	// 안 보이는 메시는 기본 설정이면 뼈 갱신을 건너뛰어, 따라가는 부위 메시·파츠가 멈춘다. 숨길 때는 항상 갱신하게 한다.
	if (bHidden)
	{
		Mech->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	Mech->SetVisibility(!bHidden, /*bPropagateToChildren=*/false);
}
