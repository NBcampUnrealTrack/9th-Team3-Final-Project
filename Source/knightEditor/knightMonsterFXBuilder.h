#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightMonsterFXBuilder.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;

/**
 * 몬스터 연출용 경량 Niagara 시스템을 만든다. 엔진의 FountainLightweight 템플릿을 복제해 모듈 값을 채우므로 같은 경로로
 * 다시 부르면 값만 갱신된다. 경량 이미터는 항상 로컬 공간이라 위치·속도는 붙는 컴포넌트의 스케일을 받고, 스프라이트 크기는 받지 않는다.
 */
UCLASS()
class KNIGHTEDITOR_API UknightMonsterFXBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Bounded continuous environment dust: fine grains + soft wisps. User parameters are driven by AKnightSandstorm. */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildSandstormDustSystem(const FString& PackagePath, UMaterialInterface* Material);

	/**
	 * 입에서 로컬 +X로 뻗는 불줄기. Reach(cm)는 불꽃이 닿는 거리, OwnerScale은 붙을 메시의 월드 스케일(잔열 8).
	 * 스프라이트 크기는 cm 그대로 쓴다. 이미터 네 개(뒤에서 앞 순서로 Smoke, Fire, Core, Embers)를 만들고, 머티리얼이 없는
	 * 레이어는 끈다. Fire는 불줄기 몸통, Core는 입 앞의 흰빛 심지, Smoke는 불줄기 끝에서 피어올라 쏘기가 끝난 뒤에도 남는
	 * 짙은 회갈색 연기, Embers는 흩날리는 불씨다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildBreathSystem(const FString& PackagePath, UMaterialInterface* FireMaterial, UMaterialInterface* CoreMaterial,
		UMaterialInterface* SmokeMaterial, UMaterialInterface* EmberMaterial, float Reach = 3000.f, float OwnerScale = 1.f);

	/**
	 * 브레스 예고: 입 안에서 달아오르는 빛, 콧김처럼 새어 나와 오르는 연기, 입가에서 튀는 작은 불씨. 브레스와 같은 머리 소켓
	 * 위치·방향(로컬 +X가 입 앞)에 붙여 준비 동작 동안 재생한다. OwnerScale은 붙을 메시의 월드 스케일.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildBreathChargeSystem(const FString& PackagePath, UMaterialInterface* GlowMaterial, UMaterialInterface* SmokeMaterial,
		UMaterialInterface* EmberMaterial, float OwnerScale = 1.f);

	/**
	 * 브레스가 바닥에 닿는 띠(로컬 +X가 앞, Length x Width cm)에서 짧게 솟는 불길과, 바닥에 투영해 5초쯤 남는 빛나는 잔열 자국.
	 * 몸에 붙이지 않고 바닥에 스폰하는 일회성 이펙트다(Knight Spawn Ground FX 노티파이). ActiveTime(초) 동안 생성하고,
	 * 남은 자국이 식어 사라지면 스스로 끝나므로 몬스터가 움직여도 자국은 제자리에 남는다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildBreathGroundSystem(const FString& PackagePath, UMaterialInterface* FlameMaterial, UMaterialInterface* ScorchMaterial,
		float Length = 1200.f, float Width = 600.f, float ActiveTime = 1.6f);

	/**
	 * 바닥에서 솟구치는 불기둥 한 번과, 그 자리에 5초쯤 남는 빛나는 잔열 자국(데칼). 월드에 스폰하는 일회성이라 cm 그대로다.
	 * Height(cm)는 불길이 오르는 높이, Radius(cm)는 불기둥 밑동과 자국이 퍼지는 반지름. 머티리얼이 없는 레이어는 끈다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildFireEruptionSystem(const FString& PackagePath, UMaterialInterface* FireMaterial, UMaterialInterface* CoreMaterial,
		UMaterialInterface* SmokeMaterial, UMaterialInterface* EmberMaterial, UMaterialInterface* ScorchMaterial, float Height = 1000.f, float Radius = 150.f);

	/**
	 * 불기둥이 솟기 직전 그 자리 바닥에서 새어 나오는 예고: 바닥에 낮게 깔려 깜빡이는 열빛, 가늘게 오르는 연기, 위로 튀는 불씨(계속 생성).
	 * 바닥에 스케일 1로 스폰하고 타격 순간 끈다. Radius(cm)는 새어 나오는 바닥 범위의 반지름.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildFireWarningSystem(const FString& PackagePath, UMaterialInterface* GlowMaterial, UMaterialInterface* SmokeMaterial,
		UMaterialInterface* EmberMaterial, float Radius = 120.f);

	/** 불기둥 예고의 박자 한 번: 바닥에서 짧게 번쩍이는 열빛과 위로 튀어 오르는 불씨(월드에 스폰, 스스로 끝남). */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildFireWarningBeatSystem(const FString& PackagePath, UMaterialInterface* GlowMaterial, UMaterialInterface* EmberMaterial,
		float Radius = 120.f);

	/**
	 * 화상 입은 캐릭터 몸에 붙는 작은 불길, 연기, 불씨(계속 생성). 캡슐처럼 스케일 1이고 위가 +Z인 컴포넌트 중심에 붙이며,
	 * BodyExtent(cm, 반 크기) 상자 안에서 생긴다. FlameScale은 불길·연기 크기와 생성량 배율이다(가시처럼 가는 물체는 작게).
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildBurnSystem(const FString& PackagePath, UMaterialInterface* FireMaterial, UMaterialInterface* SmokeMaterial,
		UMaterialInterface* EmberMaterial, FVector BodyExtent = FVector(60.f, 60.f, 120.f), float FlameScale = 1.f);

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

	/**
	 * 날아가는 음파(잔향 초음파): 로컬 +X로 볼록한 파면 메시 하나와, 파면 테두리에서 뒤로 벗겨져 흐르는 가는 바람결, 바람결을
	 * 따라 흩날리는 회색 연기 결. 투사체처럼 스케일 1인 컴포넌트에 붙여 함께 날아가므로 cm 그대로다.
	 * WaveRadius(cm)는 파면 테두리 반지름, Tint는 바람결·연기 색 배율(공명해방은 푸르게).
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildSonicWaveSystem(const FString& PackagePath, UStaticMesh* WaveMesh, UMaterialInterface* WaveMaterial,
		UMaterialInterface* StreakMaterial, UMaterialInterface* WispMaterial, float WaveRadius = 180.f, FLinearColor Tint = FLinearColor::White);

	/**
	 * 음파가 부서지는 한 번(잔향 초음파 착탄). 로컬 +X가 음파가 날아온 방향이고, 월드에 스폰하는 일회성이라 cm 그대로다.
	 * 레이어: 빠르게 퍼지며 사라지는 파면 링(RingMaterial, RingGrow배까지 커짐), 바깥으로 튀는 휜 파편 줄기(ShardCount개),
	 * 뒤이어 사방으로 터지는 회색 돌풍(GustCount개), 맞은 순간의 짧은 섬광. 머티리얼이 없거나 개수가 0인 레이어는 끈다.
	 * bSideways면 파편과 돌풍이 위아래 대신 좌우로만 갈라진다(가드에 막혀 옆으로 흘러감).
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildSonicImpactSystem(const FString& PackagePath, UStaticMesh* WaveMesh, UMaterialInterface* RingMaterial,
		UMaterialInterface* ShardMaterial, UMaterialInterface* GustMaterial, UMaterialInterface* FlashMaterial, float Radius = 180.f,
		int32 ShardCount = 14, int32 GustCount = 10, float RingGrow = 2.6f, bool bSideways = false);

	/**
	 * 초음파 브레스 예고(잔향): 가슴에서 맥박처럼 밝아지는 옅은 빛과, 주위 Radius(cm)에서 가슴으로 빨려 드는 바람결·연기 결.
	 * 가슴 소켓에 스케일 1(절대 스케일)로 붙인다. Tint는 바람결·연기·빛의 색 배율(공명해방은 푸르게).
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildSonicChargeSystem(const FString& PackagePath, UMaterialInterface* GlowMaterial, UMaterialInterface* StreakMaterial,
		UMaterialInterface* WispMaterial, float Radius = 600.f, FLinearColor Tint = FLinearColor::White);

	/**
	 * 초음파 브레스 예고 동안 가슴 아래 바닥에서 위(로컬 +Z)로 Height(cm)까지 빨려 오르는 먼지와 바람결, 바닥에서 잘게 튀는 알갱이.
	 * bWater면 먼지 대신 물안개, 알갱이 대신 물방울이다. 바닥(또는 수면)에 스케일 1로 붙이고, 실제 가슴 높이에 맞춰 Z로 늘인다.
	 * Radius(cm)는 빨려 드는 바닥 범위의 반지름, Tint는 먼지·물안개 색.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildSonicInflowSystem(const FString& PackagePath, UMaterialInterface* StreakMaterial, UMaterialInterface* WispMaterial,
		float Radius = 500.f, float Height = 1000.f, bool bWater = false, FLinearColor Tint = FLinearColor::White);

	/**
	 * 초음파 브레스 기둥을 따라 아래(로컬 -Z)로 쏟아지는 바람결과 연기 결. 위 끝(가슴)에 붙이고, 로컬 Length(cm) 아래가 바닥이 되도록
	 * 실제 기둥 높이에 맞춰 컴포넌트를 Z로 늘인다(가로 크기는 그대로). Radius(cm)는 기둥 아래 끝 반지름.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildSonicPillarSystem(const FString& PackagePath, UMaterialInterface* StreakMaterial, UMaterialInterface* WispMaterial,
		float Radius = 400.f, float Length = 1000.f, FLinearColor Tint = FLinearColor::White);

	/**
	 * 초음파 브레스 기둥이 바닥에 닿는 자리에서 바닥을 따라 바깥으로 퍼지는 바람결과 연기 결(계속 생성). 기둥 아래 끝에 스케일 1로 붙인다.
	 * bTrail이면 대신 지나간 자리에 남겨 두는 일회성 연기 한 번(월드에 스폰, 스스로 끝남)을 만든다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildSonicGroundSystem(const FString& PackagePath, UMaterialInterface* StreakMaterial, UMaterialInterface* WispMaterial,
		float Radius = 400.f, bool bTrail = false, FLinearColor Tint = FLinearColor::White);

	/**
	 * 초음파 브레스가 수면을 가르는 자리: 양옆(로컬 ±Y)으로 갈라져 솟았다 떨어지는 물보라 막과, 낮게 퍼지는 물안개(계속 생성).
	 * 수면 위 닿는 자리에 스케일 1로 붙이고, 로컬 +X가 지나가는 방향이다. Width(cm)는 갈라지는 폭의 절반.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildSonicWaterSpraySystem(const FString& PackagePath, UMaterialInterface* SprayMaterial, UMaterialInterface* MistMaterial,
		float Width = 400.f, FLinearColor Tint = FLinearColor::White);

	/**
	 * 물 위 회오리(용오름)의 입자: 밑동에서 바깥으로 튀었다 떨어지는 물보라 막과 물방울, 낮게 퍼지는 물안개, 기둥 안을 감아 오르는 물안개,
	 * 꼭대기에서 흩어지는 물안개, 밑동에서 세차게 치솟는 물줄기, 기둥 겉에서 내던져지는 물방울(계속 생성). 밑동(수면)에 스케일 1로 붙이고, 컴포넌트를 Z축으로 돌리면 입자가 함께 돈다.
	 * Radius(cm)는 판정 반지름, Height(cm)는 기둥 높이.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildTornadoSystem(const FString& PackagePath, UMaterialInterface* SprayMaterial, UMaterialInterface* MistMaterial,
		float Radius = 300.f, float Height = 1200.f, FLinearColor Tint = FLinearColor::White);

	/** 회오리가 만들어지는 동안 밑동 수면: 도는 물보라 고리와 물방울, 바깥에서 빨려 드는 물안개(계속 생성). 수면에 스케일 1로 붙인다. */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildTornadoFormSystem(const FString& PackagePath, UMaterialInterface* SprayMaterial, UMaterialInterface* MistMaterial,
		float Radius = 300.f, FLinearColor Tint = FLinearColor::White);

	/**
	 * 회오리 물 한 번(월드에 스폰, 스스로 끝남). bCollapse가 아니면 다 만들어지는 순간 밑동에서 크게 솟구치는 물,
	 * bCollapse면 사라질 때 기둥 높이에서 쏟아져 내리는 물. 밑동(수면)에 스폰한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildTornadoBurstSystem(const FString& PackagePath, UMaterialInterface* SprayMaterial, UMaterialInterface* MistMaterial,
		float Radius = 300.f, float Height = 1200.f, bool bCollapse = false, FLinearColor Tint = FLinearColor::White);

	/** 위로 솟구치는 불꽃 한 번(가시 발사). 월드에 스폰하는 용도라 cm 그대로다. */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildFireBurstSystem(const FString& PackagePath, UMaterialInterface* Material, float Height = 800.f);

	/** 바닥을 따라 사방으로 퍼지는 짧은 줄기 한 번(착탄 충격파). 월드에 스폰하는 용도라 cm 그대로다. */
	UFUNCTION(BlueprintCallable, Category="Knight|FX")
	static UNiagaraSystem* BuildShockwaveSystem(const FString& PackagePath, UMaterialInterface* Material, float Radius = 500.f);
};
