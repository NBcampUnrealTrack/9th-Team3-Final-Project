// 원본 메카를 부위별 메시로 나눈 묶음 (2026-10-13)
//
// 만드는 법: Mech Parts Workbench 목록에서 아무 파츠나 우클릭 → '원본을 부위별로 나누기'.
//   미리보기 메카 메시(예: Knight)를 머리·몸통·팔 L/R·다리 L/R 스켈레탈 메시로 나눠, 원본 메시 옆 BodyRegions 폴더에
//   SK_<원본>_<부위>와 이 세트(DA_<원본>_BodyRegions)로 저장한다. 다시 누르면 덮어쓴다(원본 메시는 바꾸지 않음).
// 쓰는 곳:
//   - 게임: 나이트 파츠 컴포넌트(UknightPartsComponent)의 Body Regions에 이 세트를 넣는다.
//   - 워크벤치 미리보기: 같은 원본 메시용 세트를 자동으로 찾는다.
// 동작: 원본 본체는 뼈 계산·충돌만 하고 그리지 않으며, 부위 메시들이 같은 자세로 대신 그려진다.
//   파츠 Attachment의 Replace Regions에 든 부위는 그 파츠를 끼는 동안 숨는다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MechPartAttachment.h"
#include "MechBodyRegionSet.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;

/** 부위 하나의 메시. */
USTRUCT(BlueprintType)
struct MECHPARTSWORKBENCHRUNTIME_API FMechBodyRegionMesh
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body Region")
	EMechBodyRegion Region = EMechBodyRegion::Torso;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body Region")
	TSoftObjectPtr<USkeletalMesh> Mesh;
};

UCLASS(BlueprintType)
class MECHPARTSWORKBENCHRUNTIME_API UMechBodyRegionSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 나눈 원본 메시. 이 메시를 쓰는 메카에만 적용한다(다른 메시면 쓰지 않음). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body Region")
	TSoftObjectPtr<USkeletalMesh> SourceMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body Region")
	TArray<FMechBodyRegionMesh> Regions;

	/** RegionComponent를 Mech에 붙여 같은 자세를 따라가게 한다(LeaderPose, 충돌 없음). 컴포넌트 만들기·등록은 부르는 쪽이 한다. */
	static void SetupRegionComponent(USkeletalMeshComponent* RegionComponent, USkeletalMesh* Mesh, USkeletalMeshComponent* Mech);

	/**
	 * 원본 메카 본체를 그리지 않는다(bHidden) / 다시 그린다. 숨겨도 뼈 계산은 계속한다(부위 메시·파츠가 그 뼈를 따라감).
	 * 충돌·피격 판정은 그대로 원본 본체가 한다. 붙어 있는 파츠·부위 메시에는 영향이 없다.
	 */
	static void SetMechBodyHidden(USkeletalMeshComponent* Mech, bool bHidden);
};
