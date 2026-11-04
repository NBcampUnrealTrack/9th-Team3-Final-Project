#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "knightAttackWarningSubsystem.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;

/** 공격 하나의 예고 표시. 몬스터 공격 어빌리티(UknightGameplayAbility_MonsterAttack)의 Warning 항목이다. */
USTRUCT(BlueprintType)
struct KNIGHT_API FknightAttackWarningSettings
{
	GENERATED_BODY()

	/** 가드·패링으로 받을 수 없는 공격에 켠다. 예비 동작부터 첫 판정까지 HUD 경고 마커와 부위 섬광·불씨를 띄운다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Warning")
	bool bEnabled = false;

	/** 공격에 쓰는 부위의 소켓(앞발, 머리, 가슴). 마커와 불씨가 이 소켓을 따라간다. 없으면 메시 중심. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Warning", meta=(EditCondition="bEnabled"))
	FName Socket;

	/** 소켓 위치에 더하는 월드 오프셋(cm). 소켓이 몸 안쪽에 있어 마커가 몸에 묻힐 때 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Warning", meta=(EditCondition="bEnabled"))
	FVector Offset = FVector::ZeroVector;

	/** 불씨가 퍼지는 크기 배율(1 = 앞발 하나 정도). 몸 전체로 치는 공격은 키운다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Warning", meta=(EditCondition="bEnabled", ClampMin=0.1, ClampMax=5))
	float EmberScale = 1.f;
};

/** 지금 떠 있는 예고 하나. HUD가 매 프레임 읽는다. */
USTRUCT()
struct KNIGHT_API FknightActiveAttackWarning
{
	GENERATED_BODY()

	int32 Id = 0;
	TWeakObjectPtr<USceneComponent> Component;
	TWeakObjectPtr<AActor> Owner;
	FName Socket;
	FVector Offset = FVector::ZeroVector;
	/** 시작한 뒤 흐른 몬스터 시간(히트스톱 동안 멈춘다). */
	float Elapsed = 0.f;
	/** 시작부터 첫 판정까지(몬스터 시간). 다 차면 예고가 저절로 끝난다. */
	float Duration = 0.f;
	TWeakObjectPtr<UNiagaraComponent> Embers;

	float GetRemaining() const { return Duration - Elapsed; }
	FVector GetLocation() const;
};

/** 예고 연출 에셋. Project Settings > Knight Attack Warning. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Knight Attack Warning"))
class KNIGHT_API UknightAttackWarningConfig : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 예고가 시작될 때 부위에서 한 번 번쩍이는 섬광(스스로 끝남). 절대 스케일 1로 소켓에 붙는다. */
	UPROPERTY(EditAnywhere, Config, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> GlintEffect;

	/** 예고 동안 부위 주변에 옅게 피어오르는 붉은 불씨(계속 생성). 판정 순간 끈다. */
	UPROPERTY(EditAnywhere, Config, Category="Effects")
	TSoftObjectPtr<UNiagaraSystem> EmberEffect;
};

/**
 * 가드·패링 불가 공격의 예고(#198)를 모아 둔다. 몬스터 공격 어빌리티가 Start로 시작하고, 판정 시각이 되면 저절로 끝난다
 * (어빌리티가 먼저 끝나면 Stop). 시작할 때 부위에 섬광과 불씨를 붙이고, HUD(knightUI의 공격 경고 마커)는 GetWarnings로
 * 남은 시간과 위치를 읽어 마커를 그린다.
 */
UCLASS()
class KNIGHT_API UknightAttackWarningSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Duration은 첫 판정까지의 몬스터 시간(초). 0 이하면 시작하지 않는다. 반환값은 Stop에 넘길 번호(0이면 시작 안 함). */
	int32 Start(USceneComponent* Component, const FknightAttackWarningSettings& Settings, float Duration);
	void Stop(int32 Id);

	TConstArrayView<FknightActiveAttackWarning> GetWarnings() const { return Warnings; }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	void Remove(int32 Index);

	TArray<FknightActiveAttackWarning> Warnings;
	int32 NextId = 1;
};
