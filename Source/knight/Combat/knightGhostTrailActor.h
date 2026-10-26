#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "knightGhostTrailActor.generated.h"

class ACharacter;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * (2026-10-26) 퀵스텝(BoostDash) 및 돌진 베기(BoostLunge) 시 남기는 메카 잔상(Afterimage) 액터.
 * 캐릭터의 현재 보이는 모든 스켈레탈 메시(부위별 메시 포함)와 스태틱 메시(장착 파츠·무기)의 포즈 및 트랜스폼을
 * 복제한 뒤, 발광 프레넬 홀로그램 머티리얼(M_Knight_GhostTrail)을 입혀 서서히 페이드아웃하며 소멸한다.
 */
UCLASS(NotPlaceable, Transient)
class KNIGHT_API AknightGhostTrailActor : public AActor
{
	GENERATED_BODY()

public:
	AknightGhostTrailActor();

	virtual void Tick(float DeltaSeconds) override;

	/** 캐릭터의 현재 포즈 스냅샷을 찍어 잔상 액터를 생성한다. */
	static AknightGhostTrailActor* SpawnGhostFromCharacter(
		const ACharacter* Character,
		UMaterialInterface* GhostMaterial = nullptr,
		const FLinearColor& Color = FLinearColor(0.15f, 1.1f, 3.2f, 1.f),
		float InLifetime = 0.26f,
		float InInitialOpacity = 0.65f);

private:
	void CaptureCharacterMeshes(const ACharacter& Character, UMaterialInterface* BaseMaterial, const FLinearColor& Color, float InLifetime, float InInitialOpacity);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GhostMID;

	float Lifetime = 0.26f;
	float ElapsedTime = 0.f;
	float InitialOpacity = 0.65f;
};
