#include "Settings/knightGameUserSettings.h"

#include "Engine/Engine.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "knight.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

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
		&& DisplayID == Other.DisplayID && DisplayIndex == Other.DisplayIndex
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

void UknightGameUserSettings::GetConnectedDisplays(TArray<FKnightDisplayInfo>& OutDisplays)
{
	OutDisplays.Reset();
	FDisplayMetrics Metrics;
	FDisplayMetrics::RebuildDisplayMetrics(Metrics);
	for (int32 Index = 0; Index < Metrics.MonitorInfo.Num(); ++Index)
	{
		const FMonitorInfo& Monitor = Metrics.MonitorInfo[Index];
		FKnightDisplayInfo& Display = OutDisplays.AddDefaulted_GetRef();
		Display.ID = Monitor.ID;
		Display.Index = Index;
		Display.DesktopResolution = FIntPoint(Monitor.DisplayRect.Right - Monitor.DisplayRect.Left,
			Monitor.DisplayRect.Bottom - Monitor.DisplayRect.Top);
		Display.WorkAreaSize = FIntPoint(Monitor.WorkArea.Right - Monitor.WorkArea.Left,
			Monitor.WorkArea.Bottom - Monitor.WorkArea.Top);
		Display.DPI = Monitor.DPI;
		Display.bPrimary = Monitor.bIsPrimary;
		Display.Label = FString::Printf(TEXT("모니터 %d%s"), Index + 1,
			Monitor.bIsPrimary ? TEXT(" (주 모니터)") : TEXT(""));
		if (!Monitor.FriendlyName.IsEmpty())
		{
			Display.Label += FString::Printf(TEXT(" · %s"), *Monitor.FriendlyName);
		}
	}
}

bool UknightGameUserSettings::ResolveDisplay(const FString& ID, int32 Index, FKnightDisplayInfo& OutDisplay)
{
	TArray<FKnightDisplayInfo> Displays;
	GetConnectedDisplays(Displays);
	if (Displays.IsEmpty()) return false;
	if (!ID.IsEmpty())
	{
		if (Displays.IsValidIndex(Index) && Displays[Index].ID == ID)
		{
			OutDisplay = Displays[Index];
			return true;
		}
		if (const FKnightDisplayInfo* Found = Displays.FindByPredicate([&ID](const FKnightDisplayInfo& Display) { return Display.ID == ID; }))
		{
			OutDisplay = *Found;
			return true;
		}
		// A former index may now name a different screen. Use the primary instead.
	}
	else if (Displays.IsValidIndex(Index))
	{
		OutDisplay = Displays[Index];
		return true;
	}
	const FKnightDisplayInfo* Primary = Displays.FindByPredicate([](const FKnightDisplayInfo& Display) { return Display.bPrimary; });
	OutDisplay = Primary ? *Primary : Displays[0];
	return true;
}

void UknightGameUserSettings::GetSupportedResolutions(const FKnightGraphicsChoice& Choice, TArray<FIntPoint>& OutResolutions)
{
	OutResolutions.Reset();
	FKnightDisplayInfo Display;
	if (!ResolveDisplay(Choice.DisplayID, Choice.DisplayIndex, Display)) return;
	if (Choice.WindowMode == EWindowMode::WindowedFullscreen)
	{
		OutResolutions.Add(Display.DesktopResolution);
		return;
	}
	if (Choice.WindowMode == EWindowMode::Fullscreen)
	{
#if PLATFORM_WINDOWS
		// FMonitorInfo.Name is a hardware label on Windows, not the \\.\DISPLAYn
		// adapter path required by EnumDisplaySettings. Match its stable ID first.
		FString AdapterName;
		for (DWORD AdapterIndex = 0; AdapterName.IsEmpty(); ++AdapterIndex)
		{
			DISPLAY_DEVICE Adapter = {};
			Adapter.cb = sizeof(Adapter);
			if (!EnumDisplayDevices(nullptr, AdapterIndex, &Adapter, 0)) break;
			if (!(Adapter.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) continue;
			for (DWORD MonitorIndex = 0; ; ++MonitorIndex)
			{
				DISPLAY_DEVICE Monitor = {};
				Monitor.cb = sizeof(Monitor);
				if (!EnumDisplayDevices(Adapter.DeviceName, MonitorIndex, &Monitor, 0)) break;
				if ((Monitor.StateFlags & DISPLAY_DEVICE_ACTIVE) && FString(Monitor.DeviceID) == Display.ID)
				{
					AdapterName = Adapter.DeviceName;
					break;
				}
			}
		}
		if (!AdapterName.IsEmpty())
		{
			for (DWORD ModeIndex = 0; ; ++ModeIndex)
			{
				DEVMODE Mode = {};
				Mode.dmSize = sizeof(Mode);
				if (!EnumDisplaySettings(*AdapterName, ModeIndex, &Mode)) break;
				const FIntPoint Size(static_cast<int32>(Mode.dmPelsWidth), static_cast<int32>(Mode.dmPelsHeight));
				if (Size.X >= 640 && Size.Y >= 480) OutResolutions.AddUnique(Size);
			}
		}
#endif
		if (OutResolutions.IsEmpty())
		{
			TArray<FIntPoint> General;
			GetSupportedResolutions(General);
			for (const FIntPoint& Size : General)
			{
				if (Size.X <= Display.DesktopResolution.X && Size.Y <= Display.DesktopResolution.Y)
					OutResolutions.AddUnique(Size);
			}
		}
	}
	else
	{
		TArray<FIntPoint> General;
		GetSupportedResolutions(General);
		for (const FIntPoint& Size : General)
		{
			int32 OuterWidth = Size.X;
			int32 OuterHeight = Size.Y;
#if PLATFORM_WINDOWS
			RECT WindowRect = {0, 0, Size.X, Size.Y};
			if (AdjustWindowRectExForDpi(&WindowRect, WS_OVERLAPPEDWINDOW, false, 0,
				Display.DPI > 0 ? Display.DPI : 96))
			{
				OuterWidth = WindowRect.right - WindowRect.left;
				OuterHeight = WindowRect.bottom - WindowRect.top;
			}
#endif
			if (OuterWidth <= Display.WorkAreaSize.X && OuterHeight <= Display.WorkAreaSize.Y)
				OutResolutions.AddUnique(Size);
		}
	}
	OutResolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
}

bool UknightGameUserSettings::ValidateGraphics(const FKnightGraphicsChoice& Choice, FText& Error)
{
	FKnightDisplayInfo Display;
	if (!ResolveDisplay(Choice.DisplayID, Choice.DisplayIndex, Display))
	{
		Error = NSLOCTEXT("KnightSettings", "NoMonitor", "사용 가능한 모니터가 없습니다.");
		return false;
	}
	if (!Choice.DisplayID.IsEmpty() && Choice.DisplayID != Display.ID)
	{
		Error = NSLOCTEXT("KnightSettings", "DisconnectedMonitor", "선택한 모니터가 연결되어 있지 않습니다.");
		return false;
	}
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
	if (Choice.WindowMode == EWindowMode::WindowedFullscreen && Choice.Resolution != Display.DesktopResolution)
	{
		Error = NSLOCTEXT("KnightSettings", "BorderlessDesktop", "테두리 없는 창모드는 데스크톱 해상도를 사용합니다.");
		return false;
	}
	TArray<FIntPoint> Resolutions;
	GetSupportedResolutions(Choice, Resolutions);
	if (Choice.Resolution.X < 640 || Choice.Resolution.Y < 480 || !Resolutions.Contains(Choice.Resolution))
	{
		Error = NSLOCTEXT("KnightSettings", "InvalidResolution", "지원하지 않는 해상도입니다.");
		return false;
	}
	return true;
}

bool UknightGameUserSettings::IsRestorableGraphics(const FKnightGraphicsChoice& Choice)
{
	const bool bMode = Choice.WindowMode == EWindowMode::Fullscreen || Choice.WindowMode == EWindowMode::Windowed
		|| Choice.WindowMode == EWindowMode::WindowedFullscreen;
	return bMode && Choice.Resolution.X >= 640 && Choice.Resolution.Y >= 480;
}

FKnightGraphicsChoice UknightGameUserSettings::GetActiveGraphics() const
{
	FKnightGraphicsChoice Result;
	Result.WindowMode = GetFullscreenMode();
	Result.Resolution = GetScreenResolution();
	Result.DisplayID = GetDisplayID();
	Result.DisplayIndex = GetDisplayIndex();
	FKnightDisplayInfo Display;
	if (ResolveDisplay(Result.DisplayID, Result.DisplayIndex, Display))
	{
		Result.DisplayID = Display.ID;
		Result.DisplayIndex = Display.Index;
	}
	// Borderless always covers the desktop; the stored size can be stale (engine defaults keep the windowed size).
	if (Result.WindowMode == EWindowMode::WindowedFullscreen)
	{
		if (Display.DesktopResolution.X > 0 && Display.DesktopResolution.Y > 0) Result.Resolution = Display.DesktopResolution;
	}
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

bool UknightGameUserSettings::ApplyGraphicsNow(const FKnightGraphicsChoice& Choice, const FKnightGraphicsChoice* RevertTo, FText& Error, bool bRestoring)
{
	if (bRestoring ? !IsRestorableGraphics(Choice) : !ValidateGraphics(Choice, Error))
	{
		if (bRestoring) Error = NSLOCTEXT("KnightSettings", "RestoreInvalid", "이전 화면 설정을 복구하지 못했습니다.");
		return false;
	}
	if (RevertTo)
	{
		PendingGraphics = *RevertTo;
	}
	bHasPendingGraphics = RevertTo != nullptr;
	bKnightGraphicsInitialized = true;
	PendingGraphicsSaveSerial = PendingGraphicsSaveSerial == MAX_int32 ? 1 : PendingGraphicsSaveSerial + 1;
	SetGraphics(Choice);
	// ApplySettings also saves GameUserSettings.ini.
	ApplySettings(false);
	if (!bHasPendingGraphics)
	{
		ConfirmVideoMode();
		SaveSettings();
	}
	if (!IsGraphicsStateSaved(PendingGraphicsSaveSerial, bHasPendingGraphics))
	{
		Error = NSLOCTEXT("KnightSettings", "GraphicsSaveFailed", "그래픽 설정을 저장하지 못했습니다. 저장 위치를 확인해 주세요.");
		return false;
	}
	return true;
}

void UknightGameUserSettings::ConfirmGraphics()
{
	if (!bHasPendingGraphics) return;
	bHasPendingGraphics = false;
	ConfirmVideoMode();
	SaveSettings();
}

void UknightGameUserSettings::SetGraphics(const FKnightGraphicsChoice& Choice)
{
	SetDisplayProperties(Choice.DisplayID, Choice.DisplayIndex);
	SetFullscreenMode(Choice.WindowMode);
	SetScreenResolution(Choice.Resolution);
	SetFrameRateLimit(Choice.FrameRateLimit);
	SetVSyncEnabled(Choice.bVSync);
	SetAntiAliasingQuality(Choice.AntiAliasing);
	SetShadowQuality(Choice.Shadows);
	SetTextureQuality(Choice.Textures);
	SetVisualEffectQuality(Choice.Effects);
	SetPostProcessingQuality(Choice.PostProcess);
	SetViewDistanceQuality(Choice.ViewDistance);
	bMotionBlurEnabled = Choice.bMotionBlur;
}

bool UknightGameUserSettings::ApplyPendingGraphicsAtStartup()
{
	if (!bHasPendingGraphics)
	{
		RestoreDisconnectedDisplay();
		if (!bKnightGraphicsInitialized && GetFrameRateLimit() <= 0.f)
		{
			SetFrameRateLimit(60.f);
			ApplySettings(false);
		}
		bKnightGraphicsInitialized = true;
		SaveSettings();
		return IsGraphicsStateSaved(PendingGraphicsSaveSerial, false);
	}
	// Pending values are the display the game ran with before an unkept change, so only the loose check applies.
	if (!IsRestorableGraphics(PendingGraphics))
	{
		UE_LOG(Logknight, Warning, TEXT("Pending graphics are not restorable; keeping the current ones."));
		bHasPendingGraphics = false;
		SaveSettings();
		return false;
	}
	FKnightGraphicsChoice Desired = PendingGraphics;
	FKnightDisplayInfo PendingDisplay;
	if (ResolveDisplay(Desired.DisplayID, Desired.DisplayIndex, PendingDisplay))
	{
		Desired.DisplayID = PendingDisplay.ID;
		Desired.DisplayIndex = PendingDisplay.Index;
		if (Desired.WindowMode == EWindowMode::WindowedFullscreen) Desired.Resolution = PendingDisplay.DesktopResolution;
	}
	SetGraphics(Desired);
	ApplySettings(false);
	bNeedsStartupDisplayRefresh = true;
	const FKnightGraphicsChoice Applied = GetActiveGraphics();
	if (Applied != Desired)
	{
		UE_LOG(Logknight, Warning, TEXT("Pending graphics could not be applied at startup."));
		return false;
	}
	bHasPendingGraphics = false;
	bKnightGraphicsInitialized = true;
	ConfirmVideoMode();
	SaveSettings();
	if (!IsGraphicsStateSaved(PendingGraphicsSaveSerial, false))
	{
		bHasPendingGraphics = true;
		UE_LOG(Logknight, Warning, TEXT("Applied graphics could not be confirmed on disk."));
		return false;
	}
	bGraphicsRevertedNotice = true;
	return true;
}

void UknightGameUserSettings::RestoreDisconnectedDisplay()
{
	const FString SavedID = GetDisplayID();
	if (SavedID.IsEmpty()) return;
	FKnightDisplayInfo Display;
	if (!ResolveDisplay(SavedID, GetDisplayIndex(), Display) || Display.ID == SavedID) return;
	UE_LOG(Logknight, Warning, TEXT("Saved display %s is disconnected; using primary display %s."), *SavedID, *Display.ID);
	FKnightGraphicsChoice Choice = GetActiveGraphics();
	Choice.DisplayID = Display.ID;
	Choice.DisplayIndex = Display.Index;
	if (Choice.WindowMode == EWindowMode::WindowedFullscreen) Choice.Resolution = Display.DesktopResolution;
	else
	{
		TArray<FIntPoint> Sizes;
		GetSupportedResolutions(Choice, Sizes);
		if (!Sizes.Contains(Choice.Resolution) && !Sizes.IsEmpty())
			Choice.Resolution = Sizes.Contains(Display.DesktopResolution) ? Display.DesktopResolution : Sizes.Last();
	}
	SetGraphics(Choice);
	ApplySettings(false);
	ConfirmVideoMode();
	SaveSettings();
	bNeedsStartupDisplayRefresh = true;
}

bool UknightGameUserSettings::ConsumeStartupDisplayRefresh()
{
	const bool bRefresh = bNeedsStartupDisplayRefresh;
	bNeedsStartupDisplayRefresh = false;
	return bRefresh;
}

bool UknightGameUserSettings::ConsumeGraphicsRevertedNotice()
{
	const bool bResult = bGraphicsRevertedNotice;
	bGraphicsRevertedNotice = false;
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
