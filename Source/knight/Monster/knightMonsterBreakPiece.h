#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "knightMonsterBreakPiece.generated.h"

class UMaterialInstanceDynamic;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * 부위 파괴로 떨어져 나간 조각(잘린 꼬리). 잘린 자리에서 살짝 튕겨 나가 물리로 떨어지고, 절단면 발광(Heat)이 식은 뒤
 * LifeTime이 지나면 디더(Fade)로 사라진다. 바닥과 벽에만 부딪히고 플레이어와 카메라는 통과한다.
 *
 * UknightMonsterPartComponent가 부위 정의의 BreakPieceMesh로 만든다. 머티리얼 파라미터: Heat 1 = 막 잘림, 0 = 식음,
 * Fade 0 = 보임, 1 = 사라짐.
 */
UCLASS(Blueprintable)
class KNIGHT_API AknightMonsterBreakPiece : public AActor
{
	GENERATED_BODY()

public:
	AknightMonsterBreakPiece();

	/** Mesh로 조각을 만들고 Velocity로 날린다. 스폰 직후 한 번 부른다. LandSound는 처음 땅에 닿을 때 한 번 낸다. */
	void Launch(UStaticMesh* Mesh, const FVector& Velocity, const FVector& OutwardDirection, USoundBase* InLandSound = nullptr);

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** 바닥에 남아 있는 시간(초). 이 시간이 지나면 FadeTime 동안 사라진다. */
	UPROPERTY(EditDefaultsOnly, Category="Break Piece", meta=(ClampMin=0, Units="s"))
	float LifeTime = 10.f;

	UPROPERTY(EditDefaultsOnly, Category="Break Piece", meta=(ClampMin=0.01, Units="s"))
	float FadeTime = 2.f;

	/** 절단면 발광이 다 식는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category="Break Piece", meta=(ClampMin=0.01, Units="s"))
	float HeatCoolTime = 5.f;

	/** 잘린 순간 몸 바깥쪽(잘린 부위 방향)으로 더하는 속도. 몬스터의 이동 속도에 더해진다. */
	UPROPERTY(EditDefaultsOnly, Category="Break Piece", meta=(ClampMin=0, Units="cm/s"))
	float LaunchSpeed = 400.f;

	UPROPERTY(EditDefaultsOnly, Category="Break Piece", meta=(ClampMin=0, Units="cm/s"))
	float LaunchUpSpeed = 300.f;

	/** 떨어지면서 도는 속도의 최댓값(도/초). 축은 무작위. */
	UPROPERTY(EditDefaultsOnly, Category="Break Piece", meta=(ClampMin=0, Units="deg/s"))
	float MaxSpinSpeed = 60.f;

	UPROPERTY(EditDefaultsOnly, Category="Break Piece")
	FName HeatParameter = TEXT("Heat");

	UPROPERTY(EditDefaultsOnly, Category="Break Piece")
	FName FadeParameter = TEXT("Fade");

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LandSound;

	float Age = 0.f;

	void SetMaterialParameter(FName Parameter, float Value);

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};
