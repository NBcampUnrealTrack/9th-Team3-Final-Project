#include "Settings/knightGameUserSettings.h"

#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "knight.h"

namespace
{
	void SetConsoleInt(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* const Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Variable->Set(Value, ECVF_SetByGameSetting);
		}
	}

	bool IsGraphicsStateSaved(int32 Serial, bool bPending)
	{
		GConfig->Flush(false, GGameUserSettingsIni);
		const FString FilePath = FConfigCacheIni::GetDestIniFilename(*GGameUserSettingsIni, nullptr, *FPaths::GeneratedConfigDir());
		FString Contents;
		const bool bLoaded = FFileHelper::LoadFileToString(Contents, *FilePath);
		const bool bSerialSaved = Contents.Contains(FString::Printf(TEXT("PendingGraphicsSaveSerial=%d"), Serial));
		const bool bPendingSaved = Contents.Contains(bPending ? TEXT("bHasPendingGraphics=True") : TEXT("bHasPendingGraphics=False"));
		if (!bLoaded || !bSerialSaved || !bPendingSaved)
		{
			UE_LOG(Logknight, Warning, TEXT("Graphics save verification failed: file=%s loaded=%d serial=%d pending=%d"),
				*FilePath, bLoaded, bSerialSaved, bPendingSaved);
		}
		return bLoaded && bSerialSaved && bPendingSaved;
	}
}

bool FKnightGraphicsChoice::operator==(const FKnightGraphicsChoice& Other) const
{
	return WindowMode == Other.WindowMode && Resolution == Other.Resolution
		&& FrameRateLimit == Other.FrameRateLimit && bVSync == Other.bVSync
		&& AntiAliasing == Other.AntiAliasing && Shadows == Other.Shadows
		&& Textures == Other.Textures && Effects == Other.Effects
		&& PostProcess == Other.PostProcess && ViewDistance == Other.ViewDistance
		&& bMotionBlur == Other.bMotionBlur;
}

UknightGameUserSettings* UknightGameUserSettings::Get()
{
	return GEngine ? Cast<UknightGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UknightGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	SetFrameRateLimit(60.f);
	Upscaler = EknightUpscaler::TSR;
	bMotionBlurEnabled = true;
}

void UknightGameUserSettings::GetSupportedResolutions(TArray<FIntPoint>& OutResolutions)
{
	OutResolutions.Reset();
	TArray<FIntPoint> Fullscreen;
	TArray<FIntPoint> Windowed;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Fullscreen);
	UKismetSystemLibrary::GetConvenientWindowedResolutions(Windowed);
	for (const FIntPoint& Size : Fullscreen) if (Size.X > 0 && Size.Y > 0) OutResolutions.AddUnique(Size);
	for (const FIntPoint& Size : Windowed) if (Size.X > 0 && Size.Y > 0) OutResolutions.AddUnique(Size);
	OutResolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
}

bool UknightGameUserSettings::ValidateGraphics(const FKnightGraphicsChoice& Choice, FText& Error)
{
	if (Choice.WindowMode != EWindowMode::Fullscreen && Choice.WindowMode != EWindowMode::Windowed
		&& Choice.WindowMode != EWindowMode::WindowedFullscreen)
	{
		Error = NSLOCTEXT("KnightSettings", "InvalidMode", "화면 모드가 올바르지 않습니다.");
		return false;
	}
	if (Choice.FrameRateLimit != 30 && Choice.FrameRateLimit != 60 && Choice.FrameRateLimit != 90
		&& Choice.FrameRateLimit != 120 && Choice.FrameRateLimit != 144 && Choice.FrameRateLimit != 165
		&& Choice.FrameRateLimit != 240)
	{
		Error = NSLOCTEXT("KnightSettings", "InvalidFPS", "FPS 제한을 확인해 주세요.");
		return false;
	}
	for (int32 Quality : {Choice.AntiAliasing, Choice.Shadows, Choice.Textures, Choice.Effects, Choice.PostProcess, Choice.ViewDistance})
	{
		if (Quality < 0 || Quality > 3)
		{
			Error = NSLOCTEXT("KnightSettings", "InvalidQuality", "그래픽 품질 값이 올바르지 않습니다.");
			return false;
		}
	}
	const UknightGameUserSettings* Settings = Get();
	const FIntPoint Desktop = Settings ? Settings->GetDesktopResolution() : FIntPoint::ZeroValue;
	if (Choice.WindowMode == EWindowMode::WindowedFullscreen && Choice.Resolution != Desktop)
	{
		Error = NSLOCTEXT("KnightSettings", "BorderlessDesktop", "테두리 없는 창모드는 데스크톱 해상도를 사용합니다.");
		return false;
	}
	TArray<FIntPoint> Resolutions;
	GetSupportedResolutions(Resolutions);
	if (Choice.Resolution.X < 640 || Choice.Resolution.Y < 480 || !Resolutions.Contains(Choice.Resolution))
	{
		Error = NSLOCTEXT("KnightSettings", "InvalidResolution", "지원하지 않는 해상도입니다.");
		return false;
	}
	return true;
}

FKnightGraphicsChoice UknightGameUserSettings::GetActiveGraphics() const
{
	FKnightGraphicsChoice Result;
	Result.WindowMode = GetFullscreenMode();
	Result.Resolution = GetScreenResolution();
	Result.FrameRateLimit = GetFrameRateLimit() > 0.f ? FMath::RoundToInt(GetFrameRateLimit()) : 60;
	Result.bVSync = IsVSyncEnabled();
	Result.AntiAliasing = GetAntiAliasingQuality();
	Result.Shadows = GetShadowQuality();
	Result.Textures = GetTextureQuality();
	Result.Effects = GetVisualEffectQuality();
	Result.PostProcess = GetPostProcessingQuality();
	Result.ViewDistance = GetViewDistanceQuality();
	Result.bMotionBlur = bMotionBlurEnabled;
	return Result;
}

FKnightGraphicsChoice UknightGameUserSettings::GetDisplayedGraphics() const
{
	return bHasPendingGraphics ? PendingGraphics : GetActiveGraphics();
}

bool UknightGameUserSettings::QueueGraphicsForNextProcess(const FKnightGraphicsChoice& Choice, FText& Error)
{
	if (!ValidateGraphics(Choice, Error)) return false;
	const FKnightGraphicsChoice Previous = PendingGraphics;
	const bool bPreviouslyPending = bHasPendingGraphics;
	const int32 PreviousSerial = PendingGraphicsSaveSerial;
	PendingGraphics = Choice;
	bHasPendingGraphics = true;
	PendingGraphicsSaveSerial = PendingGraphicsSaveSerial == MAX_int32 ? 1 : PendingGraphicsSaveSerial + 1;
	SaveSettings();
	if (!IsGraphicsStateSaved(PendingGraphicsSaveSerial, true))
	{
		PendingGraphics = Previous;
		bHasPendingGraphics = bPreviouslyPending;
		PendingGraphicsSaveSerial = PreviousSerial;
		Error = NSLOCTEXT("KnightSettings", "GraphicsSaveFailed", "그래픽 설정을 저장하지 못했습니다. 저장 위치를 확인해 주세요.");
		return false;
	}
	return true;
}

bool UknightGameUserSettings::ApplyPendingGraphicsAtStartup()
{
	if (!bHasPendingGraphics)
	{
		if (!bKnightGraphicsInitialized && GetFrameRateLimit() <= 0.f)
		{
			SetFrameRateLimit(60.f);
			ApplySettings(false);
		}
		bKnightGraphicsInitialized = true;
		SaveSettings();
		return IsGraphicsStateSaved(PendingGraphicsSaveSerial, false);
	}
	FText Error;
	if (!ValidateGraphics(PendingGraphics, Error))
	{
		UE_LOG(Logknight, Warning, TEXT("Pending graphics validation failed: %s"), *Error.ToString());
		return false;
	}
	const FKnightGraphicsChoice Desired = PendingGraphics;
	SetFullscreenMode(Desired.WindowMode);
	SetScreenResolution(Desired.Resolution);
	SetFrameRateLimit(Desired.FrameRateLimit);
	SetVSyncEnabled(Desired.bVSync);
	SetAntiAliasingQuality(Desired.AntiAliasing);
	SetShadowQuality(Desired.Shadows);
	SetTextureQuality(Desired.Textures);
	SetVisualEffectQuality(Desired.Effects);
	SetPostProcessingQuality(Desired.PostProcess);
	SetViewDistanceQuality(Desired.ViewDistance);
	bMotionBlurEnabled = Desired.bMotionBlur;
	ApplySettings(false);
	const FKnightGraphicsChoice Applied = GetActiveGraphics();
	if (Applied != Desired)
	{
		UE_LOG(Logknight, Warning, TEXT("Pending graphics could not be applied at startup."));
		return false;
	}
	bHasPendingGraphics = false;
	bKnightGraphicsInitialized = true;
	SaveSettings();
	if (!IsGraphicsStateSaved(PendingGraphicsSaveSerial, false))
	{
		bHasPendingGraphics = true;
		UE_LOG(Logknight, Warning, TEXT("Applied graphics could not be confirmed on disk."));
		return false;
	}
	bGraphicsAppliedNotice = true;
	return true;
}

bool UknightGameUserSettings::ConsumeGraphicsAppliedNotice()
{
	const bool bResult = bGraphicsAppliedNotice;
	bGraphicsAppliedNotice = false;
	return bResult;
}

void UknightGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();

	// r.AntiAliasingMethod: 0 none, 2 TAA, 4 TSR.
	switch (Upscaler)
	{
	case EknightUpscaler::TSR:
		SetConsoleInt(TEXT("r.AntiAliasingMethod"), 4);
		break;
	case EknightUpscaler::TAA:
		SetConsoleInt(TEXT("r.AntiAliasingMethod"), 2);
		break;
	default:
		SetConsoleInt(TEXT("r.AntiAliasingMethod"), 0);
		break;
	}
	SetConsoleInt(TEXT("r.MotionBlurQuality"), bMotionBlurEnabled ? 4 : 0);
}
