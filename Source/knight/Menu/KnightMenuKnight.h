#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "KnightMenuKnight.generated.h"

class UAnimSequenceBase;
class UknightPartsComponent;

/**
 * (2026-10-26) 메인메뉴·정비 화면에 세워 두는 나이트.
 *
 * 저장된 장착(진행 데이터, 저장에 없는 칸은 기본 장착)대로 파츠를 입고, 정비 화면에서 장비를 바꾸면
 * (UknightProgressionSubsystem::OnProgressionChanged) 바뀐 칸만 바로 다시 입는다(UknightPartsComponent::SyncLoadoutWithSave).
 * 조작·능력·이동은 없고 IdleAnimation만 반복한다. 능력 시스템이 없으니 파츠 능력치(GE)는 적용되지 않는다(보이기만).
 *
 * 사용법: BP_MenuKnight(이 클래스의 자식)를 L_MainMenu에 하나 배치하고 정비 카메라(AKnightMaintenanceCameraRig)의 FocusActor로 지정한다.
 *   BP에서 메시(BP_Knight와 같은 Knight_v2)와 Parts 컴포넌트의 Default Loadout Asset·Body Regions(BP_Knight와 같은 에셋)를 넣는다.
 */
UCLASS()
class KNIGHT_API AKnightMenuKnight : public ACharacter
{
	GENERATED_BODY()

public:
	AKnightMenuKnight(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Knight|Menu")
	TObjectPtr<UknightPartsComponent> Parts;

	/** 대기 애니메이션. 비우면 기본 자세(T자)로 선다. BP_MenuKnight: 한손 대기 KN5_1H-B_Idle_Seq(칼을 허리 아래로 내린 자세, 2026-10-26 사용자 결정). */
	UPROPERTY(EditAnywhere, Category="Knight|Menu")
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	/**
	 * (2026-10-26 사용자 요청 '자연스러운 자세에서 멈춰 있게') 0 이상이면 IdleAnimation의 이 시각 자세로 멈춰 선다.
	 * 음수면 IdleAnimation을 반복 재생한다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditAnywhere, Category="Knight|Menu", meta=(Units="s"))
	float FrozenPoseTime = 0.f;

	/** 무기를 붙일 몸 소켓. (2026-10-26 사용자 결정) 등(weapon_back). 비우면 무기 파츠의 붙일 소켓(손) 그대로 둔다. */
	UPROPERTY(EditAnywhere, Category="Knight|Menu")
	FName WeaponSocket = TEXT("weapon_back");

	/** 무기 메시를 찾는 컴포넌트 태그(무기 파츠 정의의 MeshComponentTags). */
	UPROPERTY(EditAnywhere, Category="Knight|Menu")
	FName WeaponComponentTag = TEXT("Weapon");

private:
	UFUNCTION()
	void HandleProgressionChanged();

	/** 무기 메시를 WeaponSocket으로 옮긴다. 장비를 다시 입으면 무기가 파츠 소켓(손)에 새로 붙으므로 그때마다 부른다. */
	void AttachWeaponToMenuSocket();
};
