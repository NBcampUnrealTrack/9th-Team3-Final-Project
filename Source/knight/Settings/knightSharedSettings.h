#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "knightSharedSettings.generated.h"

/**
 * 세이브 슬롯과 분리된 로컬 플레이어 환경설정: 카메라 감도와 반전, 카메라 흔들림,
 * 자막, 조작 방식, 오디오 볼륨. 클라우드 동기화 여부는 배포 환경의 저장 정책에 따른다.
 * 키 배치는 여기에 저장하지 않는다: Enhanced Input 사용자 설정(Project Settings > Enhanced Input >
 * Enable User Settings)을 쓰며, 플레이어별로 키 변경을 저장한다.
 */
UCLASS()
class KNIGHT_API UknightSharedSettings : public ULocalPlayerSaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;
	virtual int32 GetLatestDataVersion() const override { return CurrentVersion; }

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Camera", meta=(ClampMin=0.1, ClampMax=5))
	float MouseSensitivity = 1.f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Camera")
	bool bMouseAcceleration = false;

	/** Offset from each camera mode's designed FOV, so lock-on and cinematics retain their relative view. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Display", meta=(ClampMin=-20, ClampMax=30))
	float FieldOfViewOffset = 0.f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Display", meta=(ClampMin=0.8, ClampMax=1.2))
	float Brightness = 1.f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Display", meta=(ClampMin=0.8, ClampMax=1.2))
	float Gamma = 1.f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Camera", meta=(ClampMin=0.1, ClampMax=5))
	float GamepadSensitivity = 1.f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Camera")
	bool bInvertLookY = false;

	/** 모든 카메라 흔들림에 곱하는 배율 (0이면 흔들림 끔). */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Camera", meta=(ClampMin=0, ClampMax=1))
	float CameraShakeScale = 1.f;

	/**
	 * 달리기 토글. 끄면(기본) 달리기 키를 누르고 있는 동안만 달리고,
	 * 켜면 누를 때마다 달리기를 켜고 끈다. AknightCharacter가 달리기 입력을 받을 때 읽는다.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Controls")
	bool bToggleRun = false;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Accessibility")
	bool bSubtitlesEnabled = true;

	/** Knight Audio의 Sound Class 카테고리에 런타임 Sound Mix로 적용된다. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Audio", meta=(ClampMin=0, ClampMax=1))
	float MasterVolume = 1.f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Audio", meta=(ClampMin=0, ClampMax=1))
	float MusicVolume = 0.8f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Audio", meta=(ClampMin=0, ClampMax=1))
	float SFXVolume = 1.f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Audio", meta=(ClampMin=0, ClampMax=1))
	float VoiceVolume = 1.f;
};
