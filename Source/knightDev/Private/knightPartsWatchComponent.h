// 파츠 장착 이벤트 확인용 컴포넌트 (개발 빌드 전용)
//
// 사용법: PIE 중 콘솔에서 knight.Player.PartsWatch 1 → 플레이어 폰에 붙어 OnEquipmentChanged를 순서대로 로그로 남긴다.
//         knight.Player.PartsWatch 0 → 떼어 낸다. 폰과 함께 사라지므로 다시 출격하면 다시 켠다.
// 켤 때는 GetLoadoutSlots + GetEquippedPart로 슬롯별 현재 장착을 한 번 조회해 남긴다
// (이벤트 구독이 출격 시 초기 장착보다 늦은 쪽이 초기 외형을 맞추는 방법 그대로).
// 명령 구현은 knightPartsCheats.cpp.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "knightPartsWatchComponent.generated.h"

class UknightPartItemDefinition;
class UknightPartsComponent;

UCLASS(NotBlueprintable)
class UknightPartsWatchComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 이벤트 구독을 시작하고, 지금 장착 상태를 슬롯별로 한 번 조회해 로그로 남긴다. 이미 보고 있으면 처음부터 다시 센다. */
	void StartWatching(UknightPartsComponent* InParts);

	/** 이벤트 구독을 끊는다. */
	void StopWatching();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 이벤트마다 번호, 프레임, 슬롯, 받은 파츠(해제면 null), 그 순간 GetEquippedPart 결과, 주요 스탯을 남긴다. */
	UFUNCTION()
	void HandleEquipmentChanged(FGameplayTag SlotTag, UknightPartItemDefinition* Part);

	TWeakObjectPtr<UknightPartsComponent> Parts;
	int32 EventCount = 0;
};
