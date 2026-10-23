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
 * (2026-10-13) 원본 메카 몸의 부위. 파츠가 어느 부위를 대신하는지(FMechPartAttachment::ReplaceRegions) 고를 때 쓴다.
 * 여러 개를 함께 고를 수 있다(비트 마스크). 실제로 숨기려면 원본을 부위별 메시로 나눠 둬야 한다(UMechBodyRegionSet).
 */
UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class EMechBodyRegion : uint8
{
	None = 0 UMETA(Hidden),
	Head = 1 << 0,
	Torso = 1 << 1,
	ArmLeft = 1 << 2,
	ArmRight = 1 << 3,
	LegLeft = 1 << 4,
	LegRight = 1 << 5,
};
ENUM_CLASS_FLAGS(EMechBodyRegion)

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

	/**
	 * Offset from the socket. Used in Socket mode.
	 * (2026-10-23) 위치·회전·크기를 따로 저장한다. 예전 FTransform은 회전을 쿼터니언으로 저장해서, 디테일 패널에서 각도 하나를
	 * 바꾸면 같은 회전의 다른 각도 조합으로 다시 계산돼 나머지 축이 같이 바뀌었다(예: Y 120 → X 180, Y 60, Z 180).
	 * FRotator는 입력한 각도를 그대로 들고 있다(엔진 씬 컴포넌트의 RelativeRotation과 같은 방식).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attachment", meta=(EditCondition="AttachMode==EMechPartAttachMode::Socket"))
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attachment", meta=(EditCondition="AttachMode==EMechPartAttachMode::Socket"))
	FRotator RelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attachment", meta=(EditCondition="AttachMode==EMechPartAttachMode::Socket"))
	FVector RelativeScale = FVector::OneVector;

	/**
	 * (2026-10-23까지의 저장 형식) 예전 에셋을 불러오면 PostSerialize가 위의 위치·회전·크기로 옮기고 Identity로 비운다.
	 * 편집하지 않는다. 이름을 그대로 둬야 예전 에셋의 저장 값이 여기로 읽힌다.
	 */
	UPROPERTY()
	FTransform RelativeTransform = FTransform::Identity;

	/** 위치·회전·크기를 합친 소켓 기준 변환. */
	FTransform GetRelativeTransform() const { return FTransform(RelativeRotation, RelativeLocation, RelativeScale); }

	/** 예전 RelativeTransform 값을 새 형식으로 옮긴다. */
	void PostSerialize(const FArchive& Ar);

	/**
	 * (2026-10-13) 이 파츠를 끼는 동안 숨길 원본 메카 부위. 비우면 원본 위에 겹쳐 입는다(원본 유지).
	 * 예: 투구가 머리를 통째로 대신하면 Head, 잔열 풀암 L이면 ArmLeft.
	 * 원본을 부위별 메시로 나눈 메카에서만 보인다 (워크벤치 목록 우클릭 '원본을 부위별로 나누기',
	 * 게임은 나이트 파츠 컴포넌트의 Body Regions에 그 세트를 넣어야 함).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attachment", meta=(Bitmask, BitmaskEnum="/Script/MechPartsWorkbenchRuntime.EMechBodyRegion"))
	int32 ReplaceRegions = 0;

	/**
	 * Attaches PartMesh to MechMesh according to this data.
	 * (2026-10-10) PartMesh는 스켈레탈·스태틱 메시 컴포넌트 둘 다 받는다. 스태틱 메시는 뼈대가 없어서
	 * AttachMode가 LeaderPose여도 Socket 방식(AttachSocket + RelativeTransform)으로 붙인다.
	 */
	void Apply(UMeshComponent* PartMesh, USkeletalMeshComponent* MechMesh) const;
};

template<>
struct TStructOpsTypeTraits<FMechPartAttachment> : public TStructOpsTypeTraitsBase2<FMechPartAttachment>
{
	enum { WithPostSerialize = true };
};
