#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightMonsterFXBuilder.generated.h"

class UMaterialInterface;
class UNiagaraSystem;

/**
 * 몬스터 연출용 경량 Niagara 시스템을 만든다. 엔진의 FountainLightweight 템플릿을 복제해 모듈 값을 채우므로 같은 경로로
 * 다시 부르면 값만 갱신된다. 경량 이미터는 항상 로컬 공간이라 위치·속도는 붙는 컴포넌트의 스케일을 받고, 스프라이트 크기는 받지 않는다.
 */
UCLASS()
class KNIGHTEDITOR_API UknightMonsterFXBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 입에서 로컬 +X로 뻗는 불줄기. Reach(cm)는 불꽃이 닿는 거리, OwnerScale은 붙을 메시의 월드 스케일(잔열 12).
	 * 스프라이트 크기는 cm 그대로 쓴다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildBreathSystem(const FString& PackagePath, UMaterialInterface* Material, float Reach = 3000.f, float OwnerScale = 1.f);

	/** 바닥에서 옆으로 퍼지며 피어오르는 먼지 한 번(땅울림). 붙이지 않고 월드에 스폰하는 용도라 cm 그대로다. */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildGroundDustSystem(const FString& PackagePath, UMaterialInterface* Material, float Radius = 900.f);

	/**
	 * 몸 주변(BoxExtent 상자 겉면, 로컬 BoxCenter 중심)에서 로컬 -X로 흘러가는 가늘고 긴 바람 줄기. 돌진처럼 빠르게 달릴 때 붙여 둔다.
	 * 캡슐처럼 스케일 1인 컴포넌트에 붙이는 용도라 cm 그대로다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildWindStreakSystem(const FString& PackagePath, UMaterialInterface* Material, FVector BoxCenter, FVector BoxExtent,
		float Speed = 3500.f, float StreakLength = 450.f);

	/** 위로 솟구치는 불꽃 한 번(가시 발사). 월드에 스폰하는 용도라 cm 그대로다. */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildFireBurstSystem(const FString& PackagePath, UMaterialInterface* Material, float Height = 800.f);

	/** 바닥을 따라 사방으로 퍼지는 짧은 줄기 한 번(착탄 충격파). 월드에 스폰하는 용도라 cm 그대로다. */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildShockwaveSystem(const FString& PackagePath, UMaterialInterface* Material, float Radius = 500.f);
};
