#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "knightMonsterPushOutComponent.generated.h"

class UPrimitiveComponent;

/**
 * 몬스터 몸(메시 바디, 몸 아래 차단 상자)과 겹친 플레이어를 몸 바깥으로 밀어낸다.
 *
 * 캐릭터 이동은 캡슐만 쓸어서 검사하므로, 몬스터의 메시나 차단 상자가 움직이며 플레이어를 덮치면(앞발 내려찍기, 꼬리, 회전)
 * 플레이어가 몸 안에 끼고, 플레이어가 움직이기 전까지 빠져나오지 못한다. 이 컴포넌트가 매 프레임 겹침을 찾아
 * 몸 중심선(액터 앞뒤 축)에서 멀어지는 쪽으로 PushSpeed만큼 옮긴다. 벽에 막히면 벽을 따라 미끄러진다.
 *
 * 대상: 플레이어가 조종하는 캐릭터만. 검사 대상 몸: 몸 아래 차단 상자(UknightMonsterBodyBlockerComponent), bPushOutOfMesh면 메시 바디도.
 * 검사할 몸이 없는 몬스터(작은 몬스터)에서는 틱을 끈다. 몬스터가 죽으면 멈춘다.
 *
 * 메시 바디는 Pawn을 막게(Block) 두지 않는다: 내려찍는 앞발처럼 위에서 덮치면 캐릭터 이동의 겹침 해소가 플레이어를 아래로 밀어
 * 바닥을 뚫고 떨어진다(2026-10-09 테스트). 대신 이 컴포넌트가 수평으로만 밀어낸다.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightMonsterPushOutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightMonsterPushOutComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** 밀어내는 속도(cm/s). 플레이어 달리기보다 빨라야 몸 안으로 계속 파고들지 못한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Push Out", meta=(ClampMin=0, Units="CentimetersPerSecond"))
	float PushSpeed = 1500.f;

	/**
	 * 몸 중심선의 앞뒤 반 길이(cm). 액터 위치에서 앞뒤로 이만큼이 중심선이고, 플레이어는 중심선의 가장 가까운 점에서 멀어지는 쪽으로 밀린다.
	 * 0이면 액터 위치 한 점에서 바깥으로 민다. 긴 몸(잔열)은 몸통 길이의 절반쯤으로 둔다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Push Out", meta=(ClampMin=0, Units="Centimeters"))
	float BodyAxisHalfLength = 0.f;

	/** 메시 바디(피직스 에셋)와 겹친 플레이어도 밀어낸다. 몸이 이동 캡슐보다 훨씬 큰 몬스터(머리·꼬리가 캡슐 밖으로 나옴)에서 켠다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Push Out")
	bool bPushOutOfMesh = false;

private:
	bool IsOverlappingBody(const class ACharacter* Character) const;
	FVector GetPushDirection(const FVector& Location) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> BodyPrimitives;
};
