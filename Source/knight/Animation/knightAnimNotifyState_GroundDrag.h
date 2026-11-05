#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "knightAnimNotifyState_GroundDrag.generated.h"

class UMaterialInterface;
class UNiagaraSystem;

/**
 * 구간 동안 발이 땅에 끌린 자국과 흙먼지를 남긴다. 대쉬공격처럼 부스터에 밀려 발이 미끄러지는 동작용 연출이다.
 *
 * 자국을 그을 점: ActorTrailOffsets가 있으면 캐릭터 몸 아래(액터 위치에서 옆으로 그 거리만큼 띄운 곳, 줄마다 하나),
 * 비어 있으면 FootSockets의 발 소켓. 매 틱 점마다 아래로 바닥(WorldStatic)을 찾아 바닥에 붙어 있으면(발은 ContactHeight 안,
 * 몸 아래는 캡슐 바닥에서 ContactHeight 안) 땅에 닿은 것으로 본다. 발 소켓은 디딘 발이 미끄러질 때만 자국이 생겨 끊기기 쉽다.
 * 닿은 점이 지난 자국 끝에서 MarkSpacing 이상 움직이면 그 사이를 잇는 길쭉한 데칼(MarkMaterial)을 바닥에 놓는다.
 * 데칼은 MarkLifeTime초 동안 그대로 있다가 MarkFadeTime초 동안 흐려지고 사라진다(재질에 Decal Lifetime Opacity가 있어야 흐려진다).
 * 닿은 채 DustSpacing만큼 움직일 때마다 흙먼지(DustEffect)를 한 번 뿌린다. 몸에 붙이지 않으므로 지나간 자리에 남는다.
 * 숫자는 모두 바뀔 수 있는 값.
 */
UCLASS(meta=(DisplayName="Knight Ground Drag"))
class KNIGHT_API UknightAnimNotifyState_GroundDrag : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 비어 있지 않으면 발 대신 캐릭터 몸 아래에 줄마다 자국을 긋는다. 값은 액터 기준 옆으로 띄운 거리(cm, +가 오른쪽). */
	UPROPERTY(EditAnywhere, Category="Ground Drag")
	TArray<float> ActorTrailOffsets;

	/** ActorTrailOffsets가 비었을 때 자국을 남길 발 소켓(뼈 이름도 된다). */
	UPROPERTY(EditAnywhere, Category="Ground Drag")
	TArray<FName> FootSockets = { TEXT("foot_l"), TEXT("foot_r") };

	/** 바닥에서 이 높이 안이면 땅에 닿은 것으로 본다. 발목 뼈는 서 있어도 바닥에서 10cm쯤 위다. 몸 아래 줄은 캡슐 바닥 기준. */
	UPROPERTY(EditAnywhere, Category="Ground Drag", meta=(ClampMin=0, Units="cm"))
	float ContactHeight = 25.f;

	/** 끌린 자국 데칼 재질. 비우면 자국 없이 흙먼지만 뿌린다. */
	UPROPERTY(EditAnywhere, Category="Ground Drag|Mark")
	TObjectPtr<UMaterialInterface> MarkMaterial;

	UPROPERTY(EditAnywhere, Category="Ground Drag|Mark", meta=(ClampMin=1, Units="cm"))
	float MarkWidth = 18.f;

	/** 발이 이만큼 움직일 때마다 자국 한 조각을 놓는다. 작을수록 매끄럽고 데칼이 많아진다. */
	UPROPERTY(EditAnywhere, Category="Ground Drag|Mark", meta=(ClampMin=1, Units="cm"))
	float MarkSpacing = 8.f;

	/** 한 틱에 이보다 많이 움직였으면(순간이동, 떨어짐) 잇지 않고 그 자리부터 새로 시작한다. */
	UPROPERTY(EditAnywhere, Category="Ground Drag|Mark", meta=(ClampMin=1, Units="cm"))
	float MaxSegmentLength = 80.f;

	/** 조각을 앞뒤로 이만큼 더 늘려 겹친다. 이음매가 끊겨 보이지 않게 한다. */
	UPROPERTY(EditAnywhere, Category="Ground Drag|Mark", meta=(ClampMin=0, Units="cm"))
	float MarkOverlap = 6.f;

	/** 데칼이 바닥 위아래로 비추는 두께(절반). 너무 크면 발이나 가까운 물체에도 묻는다. */
	UPROPERTY(EditAnywhere, Category="Ground Drag|Mark", meta=(ClampMin=1, Units="cm"))
	float MarkDepth = 15.f;

	/** 자국이 그대로 남아 있는 시간. */
	UPROPERTY(EditAnywhere, Category="Ground Drag|Mark", meta=(ClampMin=0, Units="s"))
	float MarkLifeTime = 4.f;

	/** 그 뒤 흐려지며 사라지는 시간. */
	UPROPERTY(EditAnywhere, Category="Ground Drag|Mark", meta=(ClampMin=0, Units="s"))
	float MarkFadeTime = 1.5f;

	/** 끌리는 동안 뿌리는 흙먼지(한 번 터지고 끝나는 이펙트). 비우면 뿌리지 않는다. */
	UPROPERTY(EditAnywhere, Category="Ground Drag|Dust")
	TObjectPtr<UNiagaraSystem> DustEffect;

	UPROPERTY(EditAnywhere, Category="Ground Drag|Dust", meta=(ClampMin=1, Units="cm"))
	float DustSpacing = 50.f;

	UPROPERTY(EditAnywhere, Category="Ground Drag|Dust", meta=(ClampMin=0))
	float DustScale = 0.6f;

private:
	struct FFootTrack
	{
		bool bHasLast = false;
		FVector Last = FVector::ZeroVector;
		float DustDistance = 0.f;
	};

	/** 노티파이 객체는 같은 몽타주를 재생하는 모든 메시가 함께 쓰므로 메시마다 발 자취를 따로 둔다. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<FFootTrack>> Tracks;

	/** 줄 수(ActorTrailOffsets가 있으면 그 수, 없으면 발 소켓 수). */
	int32 GetTrackCount() const;
	/** Index번 줄의 점을 바닥에 대 보고, 닿은 채 움직였으면 자국·흙먼지를 남긴다. */
	void UpdateTrack(USkeletalMeshComponent* MeshComp, int32 Index, FFootTrack& Track) const;
	/** From에서 To까지 바닥에 길쭉한 자국 데칼 한 조각을 놓는다. */
	void SpawnMark(UWorld* World, const FVector& From, const FVector& To) const;
	void RemoveTracks(USkeletalMeshComponent* MeshComp);
};
