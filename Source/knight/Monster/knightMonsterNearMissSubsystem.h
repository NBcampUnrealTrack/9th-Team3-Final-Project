#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "knightMonsterNearMissSubsystem.generated.h"

class AknightMonsterCharacterBase;

/**
 * 몬스터 공격이 플레이어 바로 옆을 스쳤지만 맞지 않았을 때 스친 자리에서 묵직한 바람 소리를 낸다(아슬아슬하게 피했다는 신호).
 * 소리와 거리·간격은 몬스터 정의(NearMiss*)에서 읽는다. 소리가 비어 있으면 내지 않는다.
 *
 * 회피 무적으로 흘린 공격(Evaded)은 확실히 비켜 간 것이라 바로 낸다. 판정 겉면이 가까이 지나간 것은 다음 틱에 맞을 수도 있어
 * PendingTime 동안 기다렸다가 그 사이 맞거나 막지 않았을 때만 낸다. 같은 플레이어에게는 NearMissInterval보다 자주 내지 않는다.
 */
UCLASS()
class KNIGHT_API UknightMonsterNearMissSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void ReportNearMiss(const AknightMonsterCharacterBase& Monster, AActor& Target, const FVector& Location, bool bCertain);

	/** 맞았거나 막았다: 기다리던 바람 소리를 지운다. */
	void ReportContact(const AActor& Target);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FPending
	{
		TWeakObjectPtr<const AknightMonsterCharacterBase> Monster;
		FVector Location = FVector::ZeroVector;
		double PlayTime = 0.0;
	};

	void Play(const AknightMonsterCharacterBase& Monster, const AActor& Target, const FVector& Location);
	void FlushPending();

	TMap<TWeakObjectPtr<AActor>, FPending> Pending;
	TMap<TWeakObjectPtr<AActor>, double> LastPlayTime;
	FTimerHandle FlushTimer;
};
