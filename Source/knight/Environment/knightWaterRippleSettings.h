#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "knightWaterRippleSettings.generated.h"

class UMaterialInterface;
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
	float NormalScale = 3.f;

	/** 물 머티리얼로 보내는 높이(수면 출렁임) 배율. 0이면 노멀만 흔든다. 얕은 물에서 크면 수면이 바닥 아래로 꺼진다. */
	UPROPERTY(EditAnywhere, Config, Category="Simulation", meta=(ClampMin=0))
	float HeightScale = 0.5f;

	/** 계산 높이 상한(위아래). 같은 자리를 계속 누르면 감쇠보다 빨리 쌓여 수면이 꺼지므로 자른다. */
	UPROPERTY(EditAnywhere, Config, Category="Simulation", meta=(ClampMin=1, Units="cm"))
	float MaxHeight = 15.f;

	/** 이 이름이 들어간 뼈가 물속에서 움직이면 물결을 만든다(대소문자 무시). */
	UPROPERTY(EditAnywhere, Config, Category="Sources")
	TArray<FString> BoneKeywords;

	/** 캐릭터마다 쓰는 뼈 수 상한. */
	UPROPERTY(EditAnywhere, Config, Category="Sources", meta=(ClampMin=0, ClampMax=8))
	int32 MaxBonesPerCharacter = 4;

	/** 뼈 물결 반지름 = 캡슐 반지름 x 이 값. 몸통 물결은 캡슐 반지름 그대로다. */
	UPROPERTY(EditAnywhere, Config, Category="Sources", meta=(ClampMin=0.05))
	float BoneRadiusScale = 0.35f;

	/** 수면보다 이만큼(캡슐 반지름 비율) 위에 있는 뼈까지 물에 닿은 것으로 본다. 큰 몬스터는 발목 뼈가 바닥보다 높다. */
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
	float StrengthPerSecond = 40.f;

	/** 물 밖이거나 계산 범위 밖인 캐릭터가 수면을 다시 찾는 간격. */
	UPROPERTY(EditAnywhere, Config, Category="Performance", meta=(ClampMin=0.02, Units="s"))
	float WaterQueryInterval = 0.1f;

	/** 물 머티리얼을 아직 못 찾았을 때(물이 없는 레벨) 다시 확인하는 간격. 그동안은 아무것도 계산하지 않는다. */
	UPROPERTY(EditAnywhere, Config, Category="Performance", meta=(ClampMin=0.1, Units="s"))
	float InactiveCheckInterval = 1.f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
