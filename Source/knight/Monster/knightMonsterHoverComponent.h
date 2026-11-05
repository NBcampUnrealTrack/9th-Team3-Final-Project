#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "knightMonsterHoverComponent.generated.h"

class USkeletalMeshComponent;

/**
 * 늘 공중에 떠 있게 만든 애니메이션(날짐승)을 바닥 가까이 내려 보이게 한다. 메시를 최대 HoverDrop만큼 내리되, 발 뼈가
 * 디딘 높이(FootGroundHeight) 아래로 들어가지 않을 만큼만 내린다. 그래서 떠 있는 동안은 발끝이 바닥 바로 위에 오고,
 * 쓰러지거나 내려앉는 애니메이션(발이 원래 바닥에 닿아 있음)은 내리지 않은 원래 높이로 돌아간다.
 *
 * 애니메이션이 끝난 뒤(TG_PostUpdateWork) 그 프레임의 포즈로 메시 높이만 바꾼다. 캡슐과 이동은 그대로다.
 * 공중 턴(SetAirborne)에는 내리지 않고 AirLift만큼 더 띄운다. 바뀌는 높이는 BlendTime 동안 이어서 옮긴다.
 *
 * BodyBones가 있으면 몸 뼈가 바닥 아래로 들어가지 않게 메시를 올린다.
 *
 * HoverDrop·AirLift가 0이고 BodyBones가 비어 있으면 틱을 끈다(대부분의 몬스터).
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightMonsterHoverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightMonsterHoverComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 공중 높이(내리지 않고 AirLift만큼 더 띄움)와 지상 높이 사이를 BlendTime 동안 옮긴다. 0이면 바로 바꾼다. */
	void SetAirborne(bool bInAirborne, float BlendTime);

	/** 지상(0)과 공중(1) 사이 높이 Target으로 지금 높이에서 BlendTime 동안 옮긴다(지쳐 조금씩 내려앉기). */
	void SetAirAlpha(float Target, float BlendTime);

	/**
	 * 죽을 때: 몸의 가장 낮은 뼈(BodyBones, 없으면 FootBones, 둘 다 없으면 모든 뼈)를 지금 높이에서 중력(GravityAccel, cm/s², 양수)으로
	 * 떨어뜨린다. 나는 자세에서 바닥 사망 애니메이션으로 섞이면 몸이 몇 분의 1초 만에 바닥으로 내려오는데, 그동안 메시를 받쳐
	 * 떨어지는 속도를 중력에 맞춘다. 애니메이션의 자세가 이 높이보다 높아지면 끝난다. 이미 바닥이면 아무것도 하지 않는다.
	 */
	void FallToGround(float GravityAccel);

	/** FallToGround로 떨어지는 중인지, 그리고 시작할 때 계산한 바닥까지의 시간(게임 초). */
	bool IsFallingToGround() const { return bFalling; }
	float GetFallDuration() const { return FallDuration; }

	/** 0 = 지상 높이, 1 = 공중 높이. */
	float GetAirAlpha() const { return AirAlpha; }

	/**
	 * 지난 틱에 메시 높이를 바꾼 양(cm). 애님 노드는 이 컴포넌트가 높이를 바꾸기 전에 포즈를 정하므로, 이 값으로 이번 프레임 높이를 미리 잡는다.
	 * 다운에서 일어날 때처럼 메시가 프레임마다 40cm씩 내려가면 그만큼 날개·꼬리가 바닥 아래로 들어간다.
	 */
	float GetLastHeightDelta() const { return LastHeightDelta; }

protected:
	/** 메시를 내리는 최대 높이(cm). 떠 있는 대기 자세에서 발이 바닥 바로 위에 오는 값. 0이면 쓰지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hover", meta=(ClampMin=0, Units="Centimeters"))
	float HoverDrop = 0.f;

	/** 바닥 아래로 들어가면 안 되는 발 뼈. 가장 낮은 발을 기준으로 내린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hover")
	TArray<FName> FootBones;

	/** 발로 디뎠을 때 발 뼈의 바닥 위 높이(cm, 메시 배율 적용). 뼈는 발 속에 있어서 바닥보다 조금 위다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hover", meta=(ClampMin=0, Units="Centimeters"))
	float FootGroundHeight = 0.f;

	/**
	 * 바닥(캡슐 바닥) 위 BodyClearance 아래로 들어가면 안 되는 몸 뼈(골반·가슴·머리·날개 어깨 등). 들어가는 프레임에는 그만큼 메시를 올린다.
	 * 떠 있게 만든 애니메이션이 쓰러질 때 발은 바닥 위인데 몸이 바닥 아래로 처박히는 것을 막는다(잔향 사망).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hover")
	TArray<FName> BodyBones;

	/** 몸 뼈의 바닥 위 최소 높이(cm, 메시 배율 적용). 뼈는 몸 속에 있으니 몸 두께만큼 띄운다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hover", meta=(ClampMin=0, Units="Centimeters"))
	float BodyClearance = 0.f;

	/** 공중 턴에 애니메이션 원래 높이보다 더 띄우는 높이(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hover", meta=(ClampMin=0, Units="Centimeters"))
	float AirLift = 0.f;

	/**
	 * 공중 높이로 옮기는 동안과 공중에서 AirCeilingBone이 바닥(캡슐 바닥) 위 AirCeiling보다 올라가지 않게 한다. 0이면 끈다.
	 * 이륙 애니메이션이 스스로 몸을 크게 들어 올리면 공중 높이와 겹쳐 치솟았다가 떨어지는 것을 막는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hover", meta=(ClampMin=0, Units="Centimeters"))
	float AirCeiling = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hover", meta=(EditCondition="AirCeiling > 0"))
	FName AirCeilingBone = TEXT("pelvis");

private:
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	float BaseMeshHeight = 0.f;
	/** 0 = 지상 높이, 1 = 공중 높이. */
	float AirAlpha = 0.f;
	float AirAlphaTarget = 0.f;
	float AirAlphaSpeed = 0.f;

	/** 메시 원점 기준 가장 낮은 뼈의 높이(cm). FallToGround가 쓴다. */
	float GetLowestFallBone(const USkeletalMeshComponent& MeshComponent, float MeshOriginZ) const;

	/** FallToGround 중: 시작할 때 가장 낮은 뼈의 높이(캡슐 기준), 지난 시간, 중력. */
	bool bFalling = false;
	float FallStartLowest = 0.f;
	float FallElapsed = 0.f;
	float FallGravity = 0.f;
	float FallDuration = 0.f;
	float LastHeightDelta = 0.f;
};
