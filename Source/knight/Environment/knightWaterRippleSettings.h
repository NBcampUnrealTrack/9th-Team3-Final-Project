#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "knightWaterRippleSettings.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;
class UTextureRenderTarget2D;

/**
 * 얕은 물 파동(UknightWaterRippleSubsystem) 설정. Project Settings > Knight Water Ripple.
 *
 * 값은 매 프레임 다시 읽으므로 PIE 중에 바꿔도 바로 반영된다.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Knight Water Ripple"))
class KNIGHT_API UknightWaterRippleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UknightWaterRippleSettings();

	/**
	 * 물 머티리얼이 읽는 결과 텍스처(RGB = 노멀 X, 노멀 Y, 높이 cm). 맵의 물 머티리얼이 NormalAndHeight로
	 * 이 텍스처를 쓰고 있을 때만 시뮬레이션이 켜진다.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Assets")
	TSoftObjectPtr<UTextureRenderTarget2D> OutputTarget;

	/** 파동 한 단계를 계산하는 머티리얼. */
	UPROPERTY(EditAnywhere, Config, Category="Assets")
	TSoftObjectPtr<UMaterialInterface> StepMaterial;

	/** 높이에서 노멀을 만드는 머티리얼. */
	UPROPERTY(EditAnywhere, Config, Category="Assets")
	TSoftObjectPtr<UMaterialInterface> NormalMaterial;

	/** knight.WaterRipple.ForceMaterial 1일 때 게임 중에만 모든 물에 씌우는 머티리얼. 맵을 고치지 않고 확인할 때 쓴다. */
	UPROPERTY(EditAnywhere, Config, Category="Assets")
	TSoftObjectPtr<UMaterialInterface> PreviewWaterMaterial;

	/** 플레이어를 중심으로 계산하는 정사각형 한 변. 크면 넓게 보이지만 물결이 거칠어진다. */
	UPROPERTY(EditAnywhere, Config, Category="Simulation", meta=(ClampMin=1000, Units="cm"))
	float SimulationSize = 8000.f;

	/** 1초에 계산하는 횟수. 프레임과 무관하게 같은 속도로 퍼지게 고정 간격으로 돈다. */
	UPROPERTY(EditAnywhere, Config, Category="Simulation", meta=(ClampMin=15, ClampMax=120))
	float StepsPerSecond = 60.f;

	/** 파동이 퍼지는 빠르기(칸/단계의 제곱). 0.5를 넘으면 계산이 터진다. */
	UPROPERTY(EditAnywhere, Config, Category="Simulation", meta=(ClampMin=0.01, ClampMax=0.5))
	float WaveSpeed = 0.25f;

	/** 단계마다 남는 비율. 낮을수록 물결이 빨리 잦아든다. */
	UPROPERTY(EditAnywhere, Config, Category="Simulation", meta=(ClampMin=0.8, ClampMax=1))
	float Damping = 0.985f;

	/** 물 머티리얼로 보내는 노멀 기울기 배율. */
	UPROPERTY(EditAnywhere, Config, Category="Simulation", meta=(ClampMin=0))
	float NormalScale = 1.5f;

	/** 물 머티리얼로 보내는 높이(수면 출렁임) 배율. 0이면 노멀만 흔든다. 얕은 물에서 크면 수면이 바닥 아래로 꺼진다. */
	UPROPERTY(EditAnywhere, Config, Category="Simulation", meta=(ClampMin=0))
	float HeightScale = 0.5f;

	/** 계산 높이 상한(위아래). 같은 자리를 계속 누르면 감쇠보다 빨리 쌓여 수면이 꺼지므로 자른다. */
	UPROPERTY(EditAnywhere, Config, Category="Simulation", meta=(ClampMin=1, Units="cm"))
	float MaxHeight = 15.f;

	/** 바디 물결 반지름 = 피직스 바디 가로 반폭 x 이 값. 캡슐 물결은 캡슐 반지름 그대로다. */
	UPROPERTY(EditAnywhere, Config, Category="Sources", meta=(ClampMin=0.05))
	float BodyRadiusScale = 1.f;

	/** 캡슐 바닥이 수면보다 이만큼(캡슐 반지름 비율) 위까지는 물에 있는 것으로 보고 바디를 검사한다. 점프 중 착지 첨벙을 놓치지 않게 여유를 둔다. */
	UPROPERTY(EditAnywhere, Config, Category="Sources", meta=(ClampMin=0))
	float SurfaceMarginScale = 0.5f;

	/** 이보다 느리게 움직이면 물결을 만들지 않는다. 실제 문턱 = MinSpeed + 캡슐 반지름 x MinSpeedPerRadius. */
	UPROPERTY(EditAnywhere, Config, Category="Sources", meta=(ClampMin=0, Units="cm/s"))
	float MinSpeed = 30.f;

	/** 큰 캐릭터는 숨쉬기 같은 대기 동작에도 뼈가 빠르게 움직이므로, 몸 크기에 비례해 문턱을 올린다(1/초). */
	UPROPERTY(EditAnywhere, Config, Category="Sources", meta=(ClampMin=0))
	float MinSpeedPerRadius = 1.f;

	/** 이 속도일 때 1초 동안 누르는 깊이. 속도 비례이고 2배에서 멈춘다. */
	UPROPERTY(EditAnywhere, Config, Category="Sources", meta=(ClampMin=0, Units="cm/s"))
	float ReferenceSpeed = 500.f;

	UPROPERTY(EditAnywhere, Config, Category="Sources", meta=(ClampMin=0, Units="cm"))
	float StrengthPerSecond = 25.f;

	/** 피직스 바디(발, 몸통 등)가 물에 부딪히거나 쓸고 지나갈 때 수면에 터뜨리는 이펙트. 비우면 첨벙을 끈다. */
	UPROPERTY(EditAnywhere, Config, Category="Splash")
	TSoftObjectPtr<UNiagaraSystem> SplashSystem;

	/**
	 * 최종 배율이 SplashHeavyScale 이상이고 바디 반폭이 SplashHeavyMinBodySize 이상일 때(큰 몬스터가 세게 디딤) SplashSystem 대신 쓰는 이펙트.
	 * 내려찍기 노티파이의 WaterSplash가 비어 있을 때도 쓴다.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Splash")
	TSoftObjectPtr<UNiagaraSystem> SplashHeavySystem;

	UPROPERTY(EditAnywhere, Config, Category="Splash", meta=(ClampMin=0))
	float SplashHeavyScale = 1.5f;

	/**
	 * 반폭이 이보다 작은 바디(기사 발, 다리, 팔, 몸통)는 아무리 세게 부딪혀도 SplashSystem만 쓴다. 내려찍기 노티파이도 같다(기사 구르기).
	 * 사람 크기 캐릭터의 점프 착지와 구르기가 몬스터 물보라처럼 커지지 않게 한다. 기사 바디 반폭은 5~21, 몬스터는 대부분 30 이상.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Splash", meta=(ClampMin=0, Units="cm"))
	float SplashHeavyMinBodySize = 25.f;

	/**
	 * SplashSystem(가벼운 물보라)의 최종 배율 상한. 기사가 걷고 뛸 때 0.3~0.9이고, 점프 착지(내려오는 속도 600 이상)와
	 * 구르기 등 닿기(노티파이 포함)는 2~3까지 올라가므로 이 값에서 멈춘다.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Splash", meta=(ClampMin=0.1))
	float SplashLightMaxScale = 1.f;

	/** 첨벙 소리. 비우면 소리 없이 이펙트만 낸다. */
	UPROPERTY(EditAnywhere, Config, Category="Splash")
	TSoftObjectPtr<USoundBase> SplashSound;

	/** 바디가 이보다 빠르게 내려오다 멈추면 물에 부딪힌 것으로 본다(디딤, 착지, 구르기, 쓰러짐). 제자리 대기 동작의 작은 흔들림을 거른다. */
	UPROPERTY(EditAnywhere, Config, Category="Splash", meta=(ClampMin=0, Units="cm/s"))
	float SplashMinDownSpeed = 100.f;

	/** 같은 바디가 다시 첨벙하기까지 최소 간격. */
	UPROPERTY(EditAnywhere, Config, Category="Splash", meta=(ClampMin=0, Units="s"))
	float SplashCooldown = 0.25f;

	/** 반폭이 SplashReferenceBodySize인 바디가 SplashReferenceDownSpeed로 부딪혔을 때의 이펙트 배율. 큰 바디(몬스터 발, 몸통)는 크기에 비례해 커진다. */
	UPROPERTY(EditAnywhere, Config, Category="Splash", meta=(ClampMin=0))
	float SplashScale = 0.6f;

	/** 이 속도로 내려와 디디면 SplashScale 그대로. 더 세게 디디면(뛰기, 착지) 그만큼 커진다. 기사가 뛸 때 약 300. */
	UPROPERTY(EditAnywhere, Config, Category="Splash", meta=(ClampMin=1, Units="cm/s"))
	float SplashReferenceDownSpeed = 300.f;

	/** 디딘 세기에 따른 배율 범위(최소, 최대). */
	UPROPERTY(EditAnywhere, Config, Category="Splash")
	FVector2D SplashSpeedScaleRange = FVector2D(0.5, 2.5);

	/** 기준 바디 가로 반폭(기사 발 바디). */
	UPROPERTY(EditAnywhere, Config, Category="Splash", meta=(ClampMin=1, Units="cm"))
	float SplashReferenceBodySize = 12.f;

	/** 물에 잠긴 바디가 이보다 빠르게 옆으로 움직이면 물보라를 남긴다(꼬리 휘두르기, 돌진, 구르기). 실제 문턱 = SplashWakeSpeed + 캡슐 반지름 x SplashWakeSpeedPerRadius. */
	UPROPERTY(EditAnywhere, Config, Category="Splash", meta=(ClampMin=0, Units="cm/s"))
	float SplashWakeSpeed = 600.f;

	/** 큰 몬스터는 팔다리가 원래 빠르게 움직이므로 몸 크기에 비례해 물보라 문턱을 올린다(1/초). */
	UPROPERTY(EditAnywhere, Config, Category="Splash", meta=(ClampMin=0))
	float SplashWakeSpeedPerRadius = 2.f;

	/** 같은 바디가 물보라를 남기는 간격. */
	UPROPERTY(EditAnywhere, Config, Category="Splash", meta=(ClampMin=0.02, Units="s"))
	float SplashWakeInterval = 0.25f;

	/** 같은 캐릭터의 다른 바디가 이 시간 안에 가까이서(바디 반폭 x 3) 첨벙했으면 하나로 본다. 발과 종아리가 같이 디딜 때 겹치지 않게 한다. */
	UPROPERTY(EditAnywhere, Config, Category="Splash", meta=(ClampMin=0, Units="s"))
	float SplashMergeTime = 0.2f;

	/** 바디 크기와 세기를 곱한 배율의 상한. */
	UPROPERTY(EditAnywhere, Config, Category="Splash", meta=(ClampMin=0.1))
	float SplashMaxScale = 12.f;

	/** 첨벙 소리 크기. 큰 캐릭터도 같은 값을 쓴다. */
	UPROPERTY(EditAnywhere, Config, Category="Splash", meta=(ClampMin=0))
	float SplashVolume = 1.f;

	/** 물 밖이거나 계산 범위 밖인 캐릭터가 수면을 다시 찾는 간격. */
	UPROPERTY(EditAnywhere, Config, Category="Performance", meta=(ClampMin=0.02, Units="s"))
	float WaterQueryInterval = 0.1f;

	/** 물 머티리얼을 아직 못 찾았을 때(물이 없는 레벨) 다시 확인하는 간격. 그동안은 아무것도 계산하지 않는다. */
	UPROPERTY(EditAnywhere, Config, Category="Performance", meta=(ClampMin=0.1, Units="s"))
	float InactiveCheckInterval = 1.f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
