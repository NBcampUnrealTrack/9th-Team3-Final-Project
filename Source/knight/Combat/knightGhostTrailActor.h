#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "knightGhostTrailActor.generated.h"

class ACharacter;
class UGameplayAbility;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * 돌진 잔상 설정. 기본값은 락온 돌진(UknightGameplayAbility_BoostLunge)과 같다:
 * 0.06초마다 하나, 0.28초 동안 보이고, 처음 투명도 0.65, 한손 스탠스는 파랑·양손은 빨강, 50cm 넘게 움직일 때만.
 * 점프 공격(PlungeAttack의 대상 쪽 도약)과 대쉬공격(MeleeAttack 돌진)이 쓴다(AknightGhostTrailActor::StartGhostTrailForAbility).
 */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightGhostTrailSettings
{
	GENERATED_BODY()

	/** 잔상에 덮어쓸 머티리얼. 비우면 M_Knight_GhostTrail. */
	UPROPERTY(EditDefaultsOnly, Category="Ghost Trail")
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditDefaultsOnly, Category="Ghost Trail", meta=(ClampMin=0.02, Units="s"))
	float SpawnInterval = 0.06f;

	UPROPERTY(EditDefaultsOnly, Category="Ghost Trail", meta=(ClampMin=0.05, Units="s"))
	float Lifetime = 0.28f;

	UPROPERTY(EditDefaultsOnly, Category="Ghost Trail", meta=(ClampMin=0.05, ClampMax=1.0))
	float InitialOpacity = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category="Ghost Trail")
	FLinearColor OneHandedColor = FLinearColor(0.15f, 1.1f, 3.2f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category="Ghost Trail")
	FLinearColor TwoHandedColor = FLinearColor(3.2f, 0.22f, 0.08f, 1.f);

	/**
	 * 몬스터와 붙어 있을 때는 잔상을 남기지 않는다: 돌진이 멈출 지점(대상 표면에서 돌진 정지 거리 150~170cm 앞)까지
	 * 남은 거리가 이 값 이하면 이미 기본 공격이 닿는 거리로 보고 잔상을 남기지 않는다. 돌진하다 이 안으로 들어와도 멈춘다. 바뀔 수 있는 값.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Ghost Trail", meta=(ClampMin=0, Units="cm"))
	float InRangeDistance = 100.f;
};

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

	/**
	 * 지금부터 Duration초 동안 Settings.SpawnInterval마다 어빌리티 아바타의 잔상을 남긴다(색은 그때의 스탠스).
	 * 어빌리티가 끝나거나, 돌진이 멈출 지점(StopLocation)까지 남은 수평 거리가 Settings.InRangeDistance 이하가 되거나,
	 * ShouldStop이 true를 돌려주면(예: 휘두르기 시작) 멈춘다. 처음부터 그 거리 안이면 아무것도 남기지 않는다.
	 */
	static void StartGhostTrailForAbility(UGameplayAbility* Ability, const FknightGhostTrailSettings& Settings, float Duration,
		const FVector& StopLocation, TFunction<bool()> ShouldStop = nullptr);

private:
	void CaptureCharacterMeshes(const ACharacter& Character, UMaterialInterface* BaseMaterial, const FLinearColor& Color, float InLifetime, float InInitialOpacity);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GhostMID;

	float Lifetime = 0.26f;
	float ElapsedTime = 0.f;
	float InitialOpacity = 0.65f;
};
