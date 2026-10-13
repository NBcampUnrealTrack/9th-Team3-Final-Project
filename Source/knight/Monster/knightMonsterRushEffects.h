#pragma once

#include "CoreMinimal.h"
#include "knightMonsterRushEffects.generated.h"

class ACharacter;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * 돌진처럼 빠르게 달려드는 공격의 연출. 몸에 바람 줄기(WindEffect)를 붙이고, 땅에 닿아 있는 동안 몸 뒤에
 * 흙먼지(TrailDust)를 일정 간격으로 남긴다. 같은 달리기 애니메이션이라도 공격으로 달려드는 것임을 보여 준다.
 * 어빌리티가 Start, Tick, Stop을 부른다.
 */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightMonsterRushEffects
{
	GENERATED_BODY()

	/** 캡슐(스케일 1)에 붙는 바람 줄기. */
	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TObjectPtr<UNiagaraSystem> WindEffect;

	/** 바람 줄기 위치(액터 기준, X가 정면). */
	UPROPERTY(EditDefaultsOnly, Category="Effects")
	FVector WindOffset = FVector::ZeroVector;

	/** 몸 뒤 바닥에 남기는 흙먼지. 붙이지 않고 월드에 스폰한다. */
	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TObjectPtr<UNiagaraSystem> TrailDust;

	/** 흙먼지 위치(액터 기준, X가 정면). 높이는 캡슐 바닥이다. */
	UPROPERTY(EditDefaultsOnly, Category="Effects")
	FVector TrailDustOffset = FVector(-300.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category="Effects", meta=(ClampMin=0.05, Units="s"))
	float TrailDustInterval = 0.2f;

	void Start(ACharacter& Character);
	/** bGrounded가 false면(공중) 흙먼지를 남기지 않는다. */
	void Tick(ACharacter& Character, float DeltaTime, bool bGrounded);
	/** 바람 줄기를 끈다. 남은 줄기는 수명까지 흘러가고 사라진다. */
	void Stop();

	bool IsActive() const { return bActive; }

private:
	TWeakObjectPtr<UNiagaraComponent> Wind;
	float DustTimer = 0.f;
	bool bActive = false;
};
