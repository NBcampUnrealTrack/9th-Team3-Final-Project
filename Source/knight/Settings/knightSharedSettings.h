#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "knightSharedSettings.generated.h"

/**
 * 기기를 옮겨도 따라가는 개인 설정 (로컬 플레이어 세이브 게임으로 저장하므로 Steam Cloud로 옮겨진다):
 * 카메라 감도와 반전, 카메라 흔들림, 자막, 조작 방식.
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

	/** 볼륨 분류. 같은 이름의 Knight Audio 컨트롤 버스에 적용된다. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Audio", meta=(ClampMin=0, ClampMax=1))
	float MasterVolume = 1.f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Audio", meta=(ClampMin=0, ClampMax=1))
	float MusicVolume = 0.8f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Audio", meta=(ClampMin=0, ClampMax=1))
	float SFXVolume = 1.f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Knight|Settings|Audio", meta=(ClampMin=0, ClampMax=1))
	float VoiceVolume = 1.f;
};
