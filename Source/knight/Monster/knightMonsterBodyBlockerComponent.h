#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "knightMonsterBodyBlockerComponent.generated.h"

/**
 * 큰 몬스터의 몸 아래로 플레이어가 들어가지 못하게 막는 보이지 않는 상자.
 *
 * 이동 캡슐(발 딛는 면적)만으로는 배 밑·앞다리 사이에 플레이어가 들어갈 틈이 생긴다.
 * 이 상자는 뼈가 아니라 캡슐(액터 루트)에 붙여서, 내려찍기처럼 앞발을 들어 올리는 동안에도 땅에 남아 입구를 막는다.
 * 플레이어(Pawn)만 막고 무기·카메라·시야 판정에는 걸리지 않는다(오브젝트 타입 WorldDynamic, Pawn만 Block).
 * 몬스터가 움직이며 플레이어와 겹치면 UknightMonsterPushOutComponent가 바깥으로 밀어낸다. 몬스터가 죽으면 꺼진다.
 * 크기와 위치는 Blueprint 뷰포트에서 맞춘다. 바닥 아래와 몸통 속까지 넉넉히 넣는다: 겹친 플레이어를 캐릭터 이동이 빼낼 때
 * 가장 가까운 면으로 빼내므로, 위·아래 면이 옆면보다 가까우면 플레이어가 바닥 밑이나 상자 위로 빠진다.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightMonsterBodyBlockerComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UknightMonsterBodyBlockerComponent(const FObjectInitializer& ObjectInitializer);
};
