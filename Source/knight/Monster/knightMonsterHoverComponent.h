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
 * HoverDrop이 0이면 틱을 끈다(대부분의 몬스터).
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightMonsterHoverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightMonsterHoverComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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

private:
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	float BaseMeshHeight = 0.f;
};
