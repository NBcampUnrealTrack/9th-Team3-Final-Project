#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "knightMonsterPushOutComponent.generated.h"

class UCapsuleComponent;
class UPrimitiveComponent;

/**
 * 몬스터 몸(메시 바디, 몸 아래 차단 상자)과 겹친 플레이어를 몸 바깥으로 밀어낸다.
 *
 * 캐릭터 이동은 캡슐만 쓸어서 검사하므로, 몬스터의 메시나 차단 상자가 움직이며 플레이어를 덮치면(앞발 내려찍기, 꼬리, 회전)
 * 플레이어가 몸 안에 끼고, 플레이어가 움직이기 전까지 빠져나오지 못한다. 이 컴포넌트가 매 프레임 겹침을 찾아
 * 몸 중심선(액터 앞뒤 축)에서 멀어지는 쪽으로 PushSpeed만큼 옮긴다. 벽에 막히면 벽을 따라 미끄러진다.
 * 차단 상자·이동 캡슐 윗면에 서 있거나 모서리에 걸친 플레이어(몸 위로 떨어진 경우)도 같은 방향으로 밀어 내려보낸다.
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

	/**
	 * 몬스터에 이 태그 중 하나가 있으면 메시 바디는 ShallowMeshPenetration만큼 파고들 때까지 밀지 않는다(차단 상자는 그대로).
	 * 기본값은 다운(State.HitReact.Knockdown): 쓰러진 자세는 바디가 땅에 넓게 깔려, 다 밀어내면 머리·몸통에 닿기 전에 밀려난다.
	 * 아예 안 밀면 플레이어가 누운 몸 속에 서 있게 되니, 몸에 붙어 때릴 만큼만 들어오게 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Push Out", meta=(EditCondition="bPushOutOfMesh"))
	FGameplayTagContainer ShallowMeshPushOutTags;

	/** ShallowMeshPushOutTags 중에 메시 바디에 파고들어도 되는 깊이(cm). 플레이어 캡슐 반지름보다 깊게는 못 준다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Push Out", meta=(EditCondition="bPushOutOfMesh", ClampMin=0, Units="Centimeters"))
	float ShallowMeshPenetration = 30.f;

	/**
	 * 몬스터에 이 태그 중 하나가 있으면 이동 캡슐이 플레이어(Pawn)를 막지 않는다. 기본값 State.Monster.PassThroughPawns:
	 * 구르기·몸통박치기 어빌리티가 ActivationOwnedTags로 붙인다. 막으면 몸이 플레이어 앞에서 멈춰 끝까지 덮치지 못한다.
	 * 그동안 캡슐 안에 들어온 플레이어는 몸 밖으로 밀어내고, 태그가 빠져도 플레이어가 캡슐 밖으로 나간 뒤에 다시 막는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Push Out")
	FGameplayTagContainer PassThroughPawnTags;

private:
	/** MeshPenetration: 메시 바디에 이만큼 파고든 것까지는 겹침으로 보지 않는다. */
	bool IsOverlappingBody(const class ACharacter* Character, float MeshPenetration) const;
	/** 플레이어 캡슐을 Penetration만큼 줄였을 때도 Primitive와 겹치는지. */
	bool IsOverlappingPrimitive(const class ACharacter* Character, const UPrimitiveComponent* Primitive, float Penetration) const;
	/** 차단 상자나 이동 캡슐 윗면에 서 있거나 모서리에 걸쳐 있는지. 위에서 떨어지면 몸 위에 올라타거나 공중에 걸린다. */
	bool IsStandingOnBody(const class ACharacter* Character) const;
	bool IsShallowMeshPushOut() const;
	bool WantsPassThroughPawns() const;
	void SetCapsulePassThrough(bool bPassThrough);
	FVector GetPushDirection(const FVector& Location) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> BodyPrimitives;

	/** BodyPrimitives 중 메시. ShallowMeshPushOutTags가 걸리면 얕게만 민다. */
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> MeshPrimitive;

	/** 플레이어가 올라설 수 있는 윗면(차단 상자, Pawn을 막는 이동 캡슐). 위에 서 있으면 밀어 내려보낸다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> TopPrimitives;

	UPROPERTY(Transient)
	TObjectPtr<UCapsuleComponent> MovementCapsule;

	/** 캡슐이 지금 플레이어를 통과시키는 중인지와, 통과 전의 Pawn 응답(되돌릴 값). */
	bool bCapsulePassingThrough = false;
	TEnumAsByte<ECollisionResponse> SavedCapsulePawnResponse = ECR_Block;
};
