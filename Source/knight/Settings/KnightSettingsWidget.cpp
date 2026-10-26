#include "Settings/KnightSettingsWidget.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputCoreTypes.h"
#include "knight.h"
#include "knightAudioSubsystem.h"
#include "Settings/knightSettingsSubsystem.h"
#include "Settings/knightSharedSettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#define LOCTEXT_NAMESPACE "KnightSettingsScreen"

const FString UKnightSettingsWidget::OnText(TEXT("켜기"));
const FString UKnightSettingsWidget::OffText(TEXT("끄기"));

namespace
{
	const TCHAR* const SettingsWidgetPath = TEXT("/Game/Knight/UI/Screens/Settings/WBP_KnightSettings.WBP_KnightSettings_C");

	FString OnOff(bool bValue) { return bValue ? UKnightSettingsWidget::OnText : UKnightSettingsWidget::OffText; }
	FString Number(float Value) { return FString::SanitizeFloat(FMath::RoundToFloat(Value)); }

	int32 QualityFromText(const FString& Text)
	{
		if (Text == TEXT("낮음")) return 0;
		if (Text == TEXT("중간")) return 1;
		if (Text == TEXT("높음")) return 2;
		if (Text == TEXT("매우 높음")) return 3;
		return 4;
	}
	FString QualityText(int32 Quality)
	{
		const TCHAR* Names[] = {TEXT("낮음"), TEXT("중간"), TEXT("높음"), TEXT("매우 높음")};
		return Quality >= 0 && Quality < 4 ? Names[Quality] : TEXT("사용자 지정");
	}
	TArray<FString> QualityOptions(bool bPreset = false)
	{
		TArray<FString> Values = {TEXT("낮음"), TEXT("중간"), TEXT("높음"), TEXT("매우 높음")};
		if (bPreset) Values.Add(TEXT("사용자 지정"));
		return Values;
	}
	FString WindowModeText(EWindowMode::Type Mode)
	{
		return Mode == EWindowMode::Fullscreen ? TEXT("전체 화면") : Mode == EWindowMode::Windowed ? TEXT("창 모드") : TEXT("테두리 없는 창");
	}
	FString DLSSModeText(EknightDLSSMode Mode)
	{
		switch (Mode)
		{
		case EknightDLSSMode::Quality: return TEXT("품질");
		case EknightDLSSMode::Balanced: return TEXT("균형");
		case EknightDLSSMode::Performance: return TEXT("성능");
		default: return TEXT("꺼짐");
		}
	}
	EknightDLSSMode DLSSModeFromText(const FString& Text)
	{
		if (Text == TEXT("품질")) return EknightDLSSMode::Quality;
		if (Text == TEXT("균형")) return EknightDLSSMode::Balanced;
		if (Text == TEXT("성능")) return EknightDLSSMode::Performance;
		return EknightDLSSMode::Off;
	}
	bool IsUniformQuality(const FKnightGraphicsChoice& Graphics)
	{
		const int32 Q = Graphics.AntiAliasing;
		return Graphics.Shadows == Q && Graphics.Textures == Q && Graphics.Effects == Q && Graphics.PostProcess == Q && Graphics.ViewDistance == Q;
	}

	FKnightSettingItem Rotator(FName Id, const FText& Label, const FText& Description, const TArray<FString>& Options)
	{
		FKnightSettingItem Item;
		Item.Id = Id;
		Item.Label = Label;
		Item.Description = Description;
		Item.Type = EKnightSettingControl::Rotator;
		Item.Options = Options;
		return Item;
	}
	FKnightSettingItem Toggle(FName Id, const FText& Label, const FText& Description)
	{
		return Rotator(Id, Label, Description, {UKnightSettingsWidget::OffText, UKnightSettingsWidget::OnText});
	}
	FKnightSettingItem Slider(FName Id, const FText& Label, const FText& Description, float Min, float Max)
	{
		FKnightSettingItem Item;
		Item.Id = Id;
		Item.Label = Label;
		Item.Description = Description;
		Item.Type = EKnightSettingControl::Slider;
		Item.Min = Min;
		Item.Max = Max;
		return Item;
	}
}

TSubclassOf<UKnightSettingsWidget> UKnightSettingsWidget::GetSharedWidgetClass()
{
	return LoadClass<UKnightSettingsWidget>(nullptr, SettingsWidgetPath);
}

FText UKnightSettingsWidget::GetTabLabel(int32 Index)
{
	switch (Index)
	{
	case 0: return LOCTEXT("TabGraphics", "그래픽");
	case 1: return LOCTEXT("TabControls", "조작");
	case 2: return LOCTEXT("TabAudio", "소리");
	default: return LOCTEXT("TabCalibration", "화면 보정");
	}
}

FText UKnightSettingsWidget::GetTabCaption(int32 Index)
{
	switch (Index)
	{
	case 0: return INVTEXT("GRAPHICS");
	case 1: return INVTEXT("CONTROLS");
	case 2: return INVTEXT("AUDIO");
	default: return INVTEXT("CALIBRATION");
	}
}

void UKnightSettingsWidget::Open()
{
	UknightGameUserSettings* const Graphics = UknightGameUserSettings::Get();
	ULocalPlayer* const Player = GetOwningLocalPlayer();
	UknightSettingsSubsystem* const Settings = Player ? Player->GetSubsystem<UknightSettingsSubsystem>() : nullptr;
	UknightSharedSettings* const Stored = Settings ? Settings->GetSharedSettings() : nullptr;
	if (!Graphics || !Stored)
	{
		SetStatus(LOCTEXT("Unavailable", "설정을 불러오지 못했습니다."));
		return;
	}
	Settings->EndDisplayPreview();
	HandleOpened();
	SavedGraphics = Graphics->GetActiveGraphics();
	DraftGraphics = SavedGraphics;
	SavedPersonal = NewObject<UknightSharedSettings>(this);
	DraftPersonal = NewObject<UknightSharedSettings>(this);
	CopyPersonal(SavedPersonal, Stored);
	CopyPersonal(DraftPersonal, Stored);
	SavedKeys.Reset();
	DraftKeys.Reset();
	DefaultKeys.Reset();
	KeyLabels.Reset();
	if (UEnhancedInputLocalPlayerSubsystem* const Input = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		if (UEnhancedInputUserSettings* const UserSettings = Input->GetUserSettings())
		{
			if (const UEnhancedPlayerMappableKeyProfile* const Profile = UserSettings->GetActiveKeyProfile())
			{
				for (const TPair<FName, FKeyMappingRow>& Entry : Profile->GetPlayerMappingRows())
				{
					for (const FPlayerKeyMapping& Mapping : Entry.Value.Mappings)
					{
						if (!Mapping.GetCurrentKey().IsGamepadKey() && !Mapping.GetCurrentKey().IsTouch() && Mapping.GetCurrentKey().IsDigital())
						{
							SavedKeys.Add(Entry.Key, Mapping.GetCurrentKey());
							DraftKeys.Add(Entry.Key, Mapping.GetCurrentKey());
							DefaultKeys.Add(Entry.Key, Mapping.GetDefaultKey());
							KeyLabels.Add(Entry.Key, Mapping.GetDisplayName().IsEmpty() ? FText::FromName(Entry.Key) : Mapping.GetDisplayName());
							break;
						}
					}
				}
			}
		}
	}
	CurrentTab = 0;
	HandleTabChanged();
	SetStatus(FText::GetEmpty());
	ShowUnsavedPrompt(false);
}

void UKnightSettingsWidget::CopyPersonal(UknightSharedSettings* To, const UknightSharedSettings* From) const
{
	if (!To || !From) return;
	To->MouseSensitivity = From->MouseSensitivity;
	To->bMouseAcceleration = From->bMouseAcceleration;
	To->FieldOfViewOffset = From->FieldOfViewOffset;
	To->Brightness = From->Brightness;
	To->Gamma = From->Gamma;
	To->GamepadSensitivity = From->GamepadSensitivity;
	To->bInvertLookY = From->bInvertLookY;
	To->CameraShakeScale = From->CameraShakeScale;
	To->bSubtitlesEnabled = From->bSubtitlesEnabled;
	To->MasterVolume = From->MasterVolume;
	To->MusicVolume = From->MusicVolume;
	To->SFXVolume = From->SFXVolume;
	To->VoiceVolume = From->VoiceVolume;
}

FString UKnightSettingsWidget::GetValue(FName Id, bool bSaved) const
{
	const FKnightGraphicsChoice& G = bSaved ? SavedGraphics : DraftGraphics;
	const UknightSharedSettings* const P = bSaved ? SavedPersonal.Get() : DraftPersonal.Get();
	if (Id == TEXT("WindowMode")) return WindowModeText(G.WindowMode);
	if (Id == TEXT("Display"))
	{
		FKnightDisplayInfo Display;
		return UknightGameUserSettings::ResolveDisplay(G.DisplayID, G.DisplayIndex, Display) ? Display.Label : FString();
	}
	if (Id == TEXT("Resolution")) return FString::Printf(TEXT("%dx%d"), G.Resolution.X, G.Resolution.Y);
	if (Id == TEXT("FPS")) return FString::FromInt(G.FrameRateLimit);
	if (Id == TEXT("VSync")) return OnOff(G.bVSync);
	if (Id == TEXT("DLSS")) return UknightGameUserSettings::IsDLSSModeSupported(G.DLSSMode, G.Resolution)
		? DLSSModeText(G.DLSSMode) : DLSSModeText(EknightDLSSMode::Off);
	if (Id == TEXT("Preset")) return IsUniformQuality(G) ? QualityText(G.AntiAliasing) : QualityText(4);
	if (Id == TEXT("AA")) return QualityText(G.AntiAliasing);
	if (Id == TEXT("Shadows")) return QualityText(G.Shadows);
	if (Id == TEXT("Textures")) return QualityText(G.Textures);
	if (Id == TEXT("Effects")) return QualityText(G.Effects);
	if (Id == TEXT("PostProcess")) return QualityText(G.PostProcess);
	if (Id == TEXT("ViewDistance")) return QualityText(G.ViewDistance);
	if (Id == TEXT("MotionBlur")) return OnOff(G.bMotionBlur);
	if (const TMap<FName, FKey>& Keys = bSaved ? SavedKeys : DraftKeys; Keys.Contains(Id)) return Keys.FindRef(Id).GetFName().ToString();
	if (!P) return FString();
	if (Id == TEXT("MouseSensitivity")) return Number(P->MouseSensitivity * 100.f);
	if (Id == TEXT("MouseAcceleration")) return OnOff(P->bMouseAcceleration);
	if (Id == TEXT("Master")) return Number(P->MasterVolume * 100.f);
	if (Id == TEXT("Music")) return Number(P->MusicVolume * 100.f);
	if (Id == TEXT("SFX")) return Number(P->SFXVolume * 100.f);
	if (Id == TEXT("Voice")) return Number(P->VoiceVolume * 100.f);
	if (Id == TEXT("Subtitles")) return OnOff(P->bSubtitlesEnabled);
	if (Id == TEXT("FOV")) return Number(80.f + P->FieldOfViewOffset);
	if (Id == TEXT("Gamma")) return Number(P->Gamma * 100.f);
	if (Id == TEXT("Brightness")) return Number(P->Brightness * 100.f);
	return FString();
}

bool UKnightSettingsWidget::IsItemDirty(FName Id) const
{
	return Id == TEXT("DLSS") ? DraftGraphics.DLSSMode != SavedGraphics.DLSSMode
		: GetValue(Id, false) != GetValue(Id, true);
}

void UKnightSettingsWidget::GetTabSections(int32 Tab, TArray<FKnightSettingSection>& OutSections) const
{
	OutSections.Reset();
	if (!DraftPersonal) return;

	const auto AddSection = [&OutSections](const FText& Caption) -> FKnightSettingSection& {
		FKnightSettingSection& Section = OutSections.AddDefaulted_GetRef();
		Section.Caption = Caption;
		return Section;
	};

	if (Tab == 0)
	{
		FKnightSettingSection& Display = AddSection(INVTEXT("DISPLAY"));
		Display.Items.Add(Rotator(TEXT("WindowMode"), LOCTEXT("WindowMode", "화면 모드"),
			LOCTEXT("WindowModeDesc", "전체 화면: 화면을 독점해 지연이 가장 적습니다.\n창 모드: 다른 창과 함께 씁니다.\n테두리 없는 창: 데스크톱 해상도로 전환이 빠릅니다."),
			{TEXT("전체 화면"), TEXT("창 모드"), TEXT("테두리 없는 창")}));
		TArray<FKnightDisplayInfo> Displays;
		UknightGameUserSettings::GetConnectedDisplays(Displays);
		TArray<FString> DisplayOptions;
		for (const FKnightDisplayInfo& Monitor : Displays) DisplayOptions.Add(Monitor.Label);
		FKnightSettingItem& MonitorRow = Display.Items.Add_GetRef(Rotator(TEXT("Display"), LOCTEXT("Display", "표시 모니터"),
			LOCTEXT("DisplayDesc", "게임 창을 표시할 모니터입니다. 적용 후 화면이 보이지 않으면 자동으로 이전 모니터로 돌아갑니다."), DisplayOptions));
		MonitorRow.bEnabled = Displays.Num() > 1;
		TArray<FIntPoint> Resolutions;
		UknightGameUserSettings::GetSupportedResolutions(DraftGraphics, Resolutions);
		TArray<FString> ResolutionOptions;
		for (const FIntPoint& Size : Resolutions) ResolutionOptions.Add(FString::Printf(TEXT("%dx%d"), Size.X, Size.Y));
		FKnightSettingItem& Resolution = Display.Items.Add_GetRef(Rotator(TEXT("Resolution"), LOCTEXT("Resolution", "해상도"),
			LOCTEXT("ResolutionDesc", "화면을 그리는 해상도입니다. 테두리 없는 창에서는 데스크톱 해상도를 씁니다."), ResolutionOptions));
		Resolution.bEnabled = DraftGraphics.WindowMode != EWindowMode::WindowedFullscreen;
		Display.Items.Add(Rotator(TEXT("FPS"), LOCTEXT("FPS", "FPS 제한"), LOCTEXT("FPSDesc", "초당 프레임 상한입니다. 모니터 주사율에 맞추면 안정적입니다."),
			{TEXT("30"), TEXT("60"), TEXT("90"), TEXT("120"), TEXT("144"), TEXT("165"), TEXT("240")}));
		Display.Items.Add(Toggle(TEXT("VSync"), INVTEXT("V-Sync"), LOCTEXT("VSyncDesc", "화면 찢어짐을 막습니다. 입력 지연이 조금 늘어납니다.")));

		FKnightSettingSection& Quality = AddSection(INVTEXT("QUALITY"));
		Quality.Items.Add(Rotator(TEXT("Preset"), LOCTEXT("Preset", "전체 품질"), LOCTEXT("PresetDesc", "아래 품질 항목을 한 번에 바꿉니다."), QualityOptions(true)));
		TArray<FString> DLSSOptions = {TEXT("꺼짐")};
		for (const EknightDLSSMode Mode : {EknightDLSSMode::Quality, EknightDLSSMode::Balanced, EknightDLSSMode::Performance})
		{
			if (UknightGameUserSettings::IsDLSSModeSupported(Mode, DraftGraphics.Resolution))
			{
				DLSSOptions.Add(DLSSModeText(Mode));
			}
		}
		FKnightSettingItem& DLSS = Quality.Items.Add_GetRef(Rotator(TEXT("DLSS"), INVTEXT("NVIDIA DLSS"),
			DLSSOptions.Num() > 1 ? LOCTEXT("DLSSDesc", "Super Resolution입니다. 적용하면 렌더 해상도와 동적 해상도를 DLSS가 제어합니다. 끄면 이전 설정으로 돌아갑니다.")
				: LOCTEXT("DLSSUnavailable", "현재 환경에서 지원하지 않습니다."), DLSSOptions));
		DLSS.bEnabled = DLSSOptions.Num() > 1;
		FKnightSettingItem& AA = Quality.Items.Add_GetRef(Rotator(TEXT("AA"), LOCTEXT("AA", "안티앨리어싱"),
			DraftGraphics.DLSSMode != EknightDLSSMode::Off && UknightGameUserSettings::IsDLSSModeSupported(DraftGraphics.DLSSMode, DraftGraphics.Resolution)
				? LOCTEXT("AADLSSDesc", "DLSS 사용 중에는 DLSS가 화면의 계단 현상을 처리합니다. 끄면 이전 품질 설정이 복원됩니다.")
				: LOCTEXT("AADesc", "물체 가장자리의 계단 현상을 줄입니다."), QualityOptions()));
		AA.bEnabled = DraftGraphics.DLSSMode == EknightDLSSMode::Off
			|| !UknightGameUserSettings::IsDLSSModeSupported(DraftGraphics.DLSSMode, DraftGraphics.Resolution);
		Quality.Items.Add(Rotator(TEXT("Shadows"), LOCTEXT("Shadows", "그림자"), LOCTEXT("ShadowsDesc", "그림자의 선명도와 거리입니다. 성능 영향이 큽니다."), QualityOptions()));
		Quality.Items.Add(Rotator(TEXT("Textures"), LOCTEXT("Textures", "텍스처"), LOCTEXT("TexturesDesc", "표면 이미지의 해상도입니다. 그래픽 메모리를 씁니다."), QualityOptions()));
		Quality.Items.Add(Rotator(TEXT("Effects"), LOCTEXT("Effects", "이펙트"), FText::GetEmpty(), QualityOptions()));
		Quality.Items.Add(Rotator(TEXT("PostProcess"), LOCTEXT("Post", "후처리"), LOCTEXT("PostDesc", "블룸, 피사계 심도 같은 화면 효과의 품질입니다."), QualityOptions()));
		Quality.Items.Add(Rotator(TEXT("ViewDistance"), LOCTEXT("View", "시야 거리"), FText::GetEmpty(), QualityOptions()));
		Quality.Items.Add(Toggle(TEXT("MotionBlur"), LOCTEXT("Motion", "모션 블러"), LOCTEXT("MotionDesc", "빠르게 움직일 때 화면을 흐리게 합니다.")));
		Display.Note = FText::Format(LOCTEXT("DisplayNote", "화면 모드·표시 모니터·해상도는 적용 후 {0}초 안에 유지를 고르지 않으면 이전 값으로 돌아갑니다."),
			FText::AsNumber(FMath::RoundToInt(DisplayConfirmSeconds)));
	}
	else if (Tab == 1)
	{
		FKnightSettingSection& Mouse = AddSection(INVTEXT("MOUSE"));
		Mouse.Items.Add(Slider(TEXT("MouseSensitivity"), LOCTEXT("Sensitivity", "마우스 감도"), LOCTEXT("SensitivityDesc", "시점이 도는 속도입니다. 100이 기본값입니다."), 10.f, 500.f));
		Mouse.Items.Add(Toggle(TEXT("MouseAcceleration"), LOCTEXT("Acceleration", "마우스 가속"), LOCTEXT("AccelerationDesc", "마우스를 빠르게 움직일수록 시점이 더 많이 돕니다.")));

		FKnightSettingSection& Keys = AddSection(INVTEXT("KEY BINDINGS"));
		TArray<FName> Names;
		DraftKeys.GetKeys(Names);
		Names.Sort(FNameLexicalLess());
		for (const FName& Name : Names)
		{
			FKnightSettingItem Item;
			Item.Id = Name;
			Item.Label = KeyLabels.FindRef(Name);
			Item.Description = LOCTEXT("KeyDesc", "Enter 또는 클릭 후 새 키를 누르세요. Esc를 누르면 취소합니다.");
			Item.Type = EKnightSettingControl::Key;
			Keys.Items.Add(Item);
		}
		Keys.Note = Names.IsEmpty()
			? LOCTEXT("NoMappings", "변경 가능한 입력이 없습니다. Input Mapping Context의 Player Mappable 설정을 확인해 주세요.")
			: LOCTEXT("ReservedEscape", "Esc는 메뉴 열기와 닫기에 예약되어 있습니다.");
	}
	else if (Tab == 2)
	{
		FKnightSettingSection& Volume = AddSection(INVTEXT("VOLUME"));
		Volume.Items.Add(Slider(TEXT("Master"), LOCTEXT("Master", "마스터"), FText::GetEmpty(), 0.f, 100.f));
		Volume.Items.Add(Slider(TEXT("Music"), INVTEXT("BGM"), FText::GetEmpty(), 0.f, 100.f));
		Volume.Items.Add(Slider(TEXT("SFX"), LOCTEXT("SFX", "효과음"), FText::GetEmpty(), 0.f, 100.f));
		Volume.Items.Add(Slider(TEXT("Voice"), LOCTEXT("Voice", "음성"), FText::GetEmpty(), 0.f, 100.f));
		Volume.Note = LOCTEXT("VolumeNote", "조절하는 동안 바로 들립니다. 적용을 눌러야 저장됩니다.");

		FKnightSettingSection& Subtitle = AddSection(INVTEXT("SUBTITLE"));
		Subtitle.Items.Add(Toggle(TEXT("Subtitles"), LOCTEXT("Subtitles", "자막"), LOCTEXT("SubtitlesDesc", "AI 음성 대사를 화면 아래 대화창에 글자로 표시합니다. 부위 파손 알림은 항상 표시됩니다.")));
	}
	else
	{
		FKnightSettingSection& View = AddSection(INVTEXT("VIEW"));
		View.Items.Add(Slider(TEXT("FOV"), LOCTEXT("FOV", "시야각"), LOCTEXT("FOVDesc", "카메라가 보여 주는 범위입니다. 넓을수록 주변이 잘 보이지만 멀미가 날 수 있습니다."), 60.f, 110.f));

		FKnightSettingSection& Light = AddSection(INVTEXT("BRIGHTNESS"));
		Light.Items.Add(Slider(TEXT("Gamma"), LOCTEXT("Gamma", "감마"), LOCTEXT("GammaDesc", "게임 장면의 중간 밝기를 조절합니다. 100이 기본값입니다."), 80.f, 120.f));
		Light.Items.Add(Slider(TEXT("Brightness"), LOCTEXT("Brightness", "밝기"), LOCTEXT("BrightnessDesc", "기존 장면에 밝기 보정을 적용합니다. 조명과 자동 노출 설정은 유지됩니다."), 80.f, 120.f));
		Light.Note = LOCTEXT("CalibrationNote", "장면에 즉시 미리 적용됩니다. 적용하면 저장하고, 되돌리면 이전 값으로 복원합니다. UI와 타이틀 영상은 보정 대상이 아닙니다.");
	}

	for (FKnightSettingSection& Section : OutSections)
	{
		for (FKnightSettingItem& Item : Section.Items)
		{
			Item.Value = GetValue(Item.Id, false);
			Item.bDirty = IsItemDirty(Item.Id);
		}
	}
}

int32 UKnightSettingsWidget::GetDirtyCount() const
{
	// Every id GetValue knows except the preset, which only mirrors the quality rows. Listed rather than built from
	// GetTabSections, which queries the display modes and runs on every slider step.
	static const FName Ids[] = {
		TEXT("WindowMode"), TEXT("Display"), TEXT("Resolution"), TEXT("FPS"), TEXT("VSync"), TEXT("DLSS"), TEXT("AA"), TEXT("Shadows"), TEXT("Textures"),
		TEXT("Effects"), TEXT("PostProcess"), TEXT("ViewDistance"), TEXT("MotionBlur"), TEXT("MouseSensitivity"),
		TEXT("MouseAcceleration"), TEXT("Master"), TEXT("Music"), TEXT("SFX"), TEXT("Voice"), TEXT("Subtitles"),
		TEXT("FOV"), TEXT("Gamma"), TEXT("Brightness") };
	int32 Count = 0;
	for (const FName Id : Ids)
	{
		Count += IsItemDirty(Id) ? 1 : 0;
	}
	for (const TPair<FName, FKey>& Entry : DraftKeys)
	{
		Count += SavedKeys.FindRef(Entry.Key) != Entry.Value ? 1 : 0;
	}
	return Count;
}

bool UKnightSettingsWidget::IsPersonalDirty() const
{
	if (!SavedPersonal || !DraftPersonal) return false;
	const UknightSharedSettings& A = *SavedPersonal;
	const UknightSharedSettings& B = *DraftPersonal;
	return A.MouseSensitivity != B.MouseSensitivity || A.bMouseAcceleration != B.bMouseAcceleration
		|| A.FieldOfViewOffset != B.FieldOfViewOffset || A.Brightness != B.Brightness || A.Gamma != B.Gamma
		|| A.bSubtitlesEnabled != B.bSubtitlesEnabled || A.MasterVolume != B.MasterVolume
		|| A.MusicVolume != B.MusicVolume || A.SFXVolume != B.SFXVolume || A.VoiceVolume != B.VoiceVolume;
}

bool UKnightSettingsWidget::IsKeyDirty() const
{
	for (const TPair<FName, FKey>& Entry : DraftKeys)
		if (SavedKeys.FindRef(Entry.Key) != Entry.Value) return true;
	return false;
}

bool UKnightSettingsWidget::IsDirty() const { return DraftGraphics != SavedGraphics || IsPersonalDirty() || IsKeyDirty(); }

void UKnightSettingsWidget::SetStatus(const FText& Text) { HandleStatusChanged(Text); }
void UKnightSettingsWidget::FocusSettings() { HandleFocusRequested(); }

void UKnightSettingsWidget::SetTab(int32 Index)
{
	const int32 NewTab = (Index % TabCount + TabCount) % TabCount;
	if (NewTab == CurrentTab) return;
	CurrentTab = NewTab;
	HandleTabChanged();
}

void UKnightSettingsWidget::ApplyPreset(int32 Quality)
{
	DraftGraphics.AntiAliasing = Quality;
	DraftGraphics.Shadows = Quality;
	DraftGraphics.Textures = Quality;
	DraftGraphics.Effects = Quality;
	DraftGraphics.PostProcess = Quality;
	DraftGraphics.ViewDistance = Quality;
}

void UKnightSettingsWidget::PreviewAudio(const UknightSharedSettings* Settings) const
{
	const UGameInstance* const GameInstance = GetGameInstance();
	UknightAudioSubsystem* const Audio = GameInstance ? GameInstance->GetSubsystem<UknightAudioSubsystem>() : nullptr;
	if (!Audio || !Settings) return;
	Audio->SetCategoryVolume(TEXT("Master"), Settings->MasterVolume);
	Audio->SetCategoryVolume(TEXT("Music"), Settings->MusicVolume);
	Audio->SetCategoryVolume(TEXT("SFX"), Settings->SFXVolume);
	Audio->SetCategoryVolume(TEXT("Voice"), Settings->VoiceVolume);
}

void UKnightSettingsWidget::PreviewDisplay(const UknightSharedSettings* Settings) const
{
	const ULocalPlayer* const Player = GetOwningLocalPlayer();
	UknightSettingsSubsystem* const Subsystem = Player ? Player->GetSubsystem<UknightSettingsSubsystem>() : nullptr;
	if (Subsystem && Settings)
	{
		Subsystem->PreviewDisplaySettings(Settings->FieldOfViewOffset, Settings->Brightness, Settings->Gamma);
	}
}

void UKnightSettingsWidget::EndDisplayPreview() const
{
	const ULocalPlayer* const Player = GetOwningLocalPlayer();
	if (UknightSettingsSubsystem* const Subsystem = Player ? Player->GetSubsystem<UknightSettingsSubsystem>() : nullptr)
	{
		Subsystem->EndDisplayPreview();
	}
}

void UKnightSettingsWidget::OptionEdited(FName Id, const FString& Value)
{
	if (!DraftPersonal) return;
	if (bDisplayConfirmShown) { HandleValuesChanged(); return; }
	const bool bOn = Value == OnText;
	if (Id == TEXT("WindowMode"))
	{
		DraftGraphics.WindowMode = Value == TEXT("전체 화면") ? EWindowMode::Fullscreen
			: Value == TEXT("창 모드") ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen;
	}
	else if (Id == TEXT("Display"))
	{
		TArray<FKnightDisplayInfo> Displays;
		UknightGameUserSettings::GetConnectedDisplays(Displays);
		if (const FKnightDisplayInfo* Selected = Displays.FindByPredicate([&Value](const FKnightDisplayInfo& Display) { return Display.Label == Value; }))
		{
			DraftGraphics.DisplayID = Selected->ID;
			DraftGraphics.DisplayIndex = Selected->Index;
		}
	}
	if (Id == TEXT("WindowMode") || Id == TEXT("Display"))
	{
		FKnightDisplayInfo Display;
		if (UknightGameUserSettings::ResolveDisplay(DraftGraphics.DisplayID, DraftGraphics.DisplayIndex, Display))
		{
			if (DraftGraphics.WindowMode == EWindowMode::WindowedFullscreen) DraftGraphics.Resolution = Display.DesktopResolution;
			else
			{
				TArray<FIntPoint> Sizes;
				UknightGameUserSettings::GetSupportedResolutions(DraftGraphics, Sizes);
				if (!Sizes.Contains(DraftGraphics.Resolution) && !Sizes.IsEmpty())
					DraftGraphics.Resolution = Sizes.Contains(Display.DesktopResolution) ? Display.DesktopResolution : Sizes.Last();
			}
		}
	}
	else if (Id == TEXT("Resolution"))
	{
		TArray<FString> Parts;
		Value.ParseIntoArray(Parts, TEXT("x"));
		if (Parts.Num() == 2) DraftGraphics.Resolution = FIntPoint(FCString::Atoi(*Parts[0]), FCString::Atoi(*Parts[1]));
	}
	else if (Id == TEXT("FPS")) DraftGraphics.FrameRateLimit = FCString::Atoi(*Value);
	else if (Id == TEXT("VSync")) DraftGraphics.bVSync = bOn;
	else if (Id == TEXT("DLSS")) DraftGraphics.DLSSMode = DLSSModeFromText(Value);
	else if (Id == TEXT("Preset")) { const int32 Q = QualityFromText(Value); if (Q < 4) ApplyPreset(Q); }
	else if (Id == TEXT("AA")) DraftGraphics.AntiAliasing = QualityFromText(Value);
	else if (Id == TEXT("Shadows")) DraftGraphics.Shadows = QualityFromText(Value);
	else if (Id == TEXT("Textures")) DraftGraphics.Textures = QualityFromText(Value);
	else if (Id == TEXT("Effects")) DraftGraphics.Effects = QualityFromText(Value);
	else if (Id == TEXT("PostProcess")) DraftGraphics.PostProcess = QualityFromText(Value);
	else if (Id == TEXT("ViewDistance")) DraftGraphics.ViewDistance = QualityFromText(Value);
	else if (Id == TEXT("MotionBlur")) DraftGraphics.bMotionBlur = bOn;
	else if (Id == TEXT("MouseSensitivity")) DraftPersonal->MouseSensitivity = FMath::Clamp(FCString::Atof(*Value) / 100.f, 0.1f, 5.f);
	else if (Id == TEXT("MouseAcceleration")) DraftPersonal->bMouseAcceleration = bOn;
	else if (Id == TEXT("Master")) DraftPersonal->MasterVolume = FMath::Clamp(FCString::Atof(*Value) / 100.f, 0.f, 1.f);
	else if (Id == TEXT("Music")) DraftPersonal->MusicVolume = FMath::Clamp(FCString::Atof(*Value) / 100.f, 0.f, 1.f);
	else if (Id == TEXT("SFX")) DraftPersonal->SFXVolume = FMath::Clamp(FCString::Atof(*Value) / 100.f, 0.f, 1.f);
	else if (Id == TEXT("Voice")) DraftPersonal->VoiceVolume = FMath::Clamp(FCString::Atof(*Value) / 100.f, 0.f, 1.f);
	else if (Id == TEXT("Subtitles")) DraftPersonal->bSubtitlesEnabled = bOn;
	else if (Id == TEXT("FOV")) DraftPersonal->FieldOfViewOffset = FMath::Clamp(FCString::Atof(*Value) - 80.f, -20.f, 30.f);
	else if (Id == TEXT("Gamma")) DraftPersonal->Gamma = FMath::Clamp(FCString::Atof(*Value) / 100.f, 0.8f, 1.2f);
	else if (Id == TEXT("Brightness")) DraftPersonal->Brightness = FMath::Clamp(FCString::Atof(*Value) / 100.f, 0.8f, 1.2f);
	else if (DraftKeys.Contains(Id))
	{
		const FKey NewKey(*Value);
		if (!NewKey.IsValid() || NewKey == EKeys::Escape || NewKey.IsGamepadKey() || NewKey.IsTouch() || !NewKey.IsDigital())
		{
			SetStatus(LOCTEXT("InvalidKey", "이 키는 지정할 수 없습니다. Esc는 예약되어 있습니다."));
			HandleValuesChanged();
			return;
		}
		for (const TPair<FName, FKey>& Other : DraftKeys)
		{
			if (Other.Key != Id && Other.Value == NewKey)
			{
				SetStatus(FText::Format(LOCTEXT("KeyConflict", "{0}에 이미 지정된 키입니다."), KeyLabels.FindRef(Other.Key)));
				HandleValuesChanged();
				return;
			}
		}
		DraftKeys.Add(Id, NewKey);
	}
	if (Id == TEXT("Master") || Id == TEXT("Music") || Id == TEXT("SFX") || Id == TEXT("Voice"))
	{
		PreviewAudio(DraftPersonal);
	}
	if (Id == TEXT("FOV") || Id == TEXT("Gamma") || Id == TEXT("Brightness"))
	{
		PreviewDisplay(DraftPersonal);
	}
	SetStatus(FText::GetEmpty());
	HandleValuesChanged();
}

void UKnightSettingsWidget::Apply()
{
	if (bDisplayConfirmShown || !IsDirty()) return;
	// A saved mode can become unavailable after a GPU, driver, or resolution change.
	// The disabled row displays Off; keep the value we apply and save in sync with it.
	if (DraftGraphics.DLSSMode != EknightDLSSMode::Off
		&& !UknightGameUserSettings::IsDLSSModeSupported(DraftGraphics.DLSSMode, DraftGraphics.Resolution))
	{
		DraftGraphics.DLSSMode = EknightDLSSMode::Off;
	}
	UknightGameUserSettings* const Graphics = UknightGameUserSettings::Get();
	ULocalPlayer* const Player = GetOwningLocalPlayer();
	UknightSettingsSubsystem* const Settings = Player ? Player->GetSubsystem<UknightSettingsSubsystem>() : nullptr;
	UknightSharedSettings* const Stored = Settings ? Settings->GetSharedSettings() : nullptr;
	if (!Graphics || !Stored) { SetStatus(LOCTEXT("LoadFailed", "설정 저장소를 사용할 수 없습니다.")); return; }
	FText Error;
	if (DraftGraphics != SavedGraphics && !UknightGameUserSettings::ValidateGraphics(DraftGraphics, Error))
	{ SetStatus(Error); return; }
	if (IsPersonalDirty())
	{
		CopyPersonal(Stored, DraftPersonal);
		if (!Settings->SaveSharedSettings())
		{
			CopyPersonal(Stored, SavedPersonal);
			SetStatus(LOCTEXT("SaveFailed", "설정을 저장하지 못했습니다. 변경 값을 확인해 주세요."));
			return;
		}
		CopyPersonal(SavedPersonal, DraftPersonal);
		Settings->EndDisplayPreview();
	}
	if (IsKeyDirty())
	{
		UEnhancedInputLocalPlayerSubsystem* const Input = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		UEnhancedInputUserSettings* const Keys = Input ? Input->GetUserSettings() : nullptr;
		if (!Keys) { SetStatus(LOCTEXT("KeyStoreMissing", "키 설정 저장소를 사용할 수 없습니다.")); return; }
		for (const TPair<FName, FKey>& Entry : DraftKeys)
		{
			if (SavedKeys.FindRef(Entry.Key) == Entry.Value) continue;
			FMapPlayerKeyArgs Args;
			Args.MappingName = Entry.Key;
			Args.NewKey = Entry.Value;
			Args.Slot = EPlayerMappableKeySlot::First;
			FGameplayTagContainer Failure;
			Keys->MapPlayerKey(Args, Failure);
			if (!Failure.IsEmpty())
			{
				SetStatus(LOCTEXT("KeySaveFailed", "키를 변경하지 못했습니다. 입력 에셋의 매핑을 확인해 주세요."));
				return;
			}
		}
		Keys->ApplySettings();
		Keys->SaveSettings();
		SavedKeys = DraftKeys;
	}
	if (DraftGraphics != SavedGraphics)
	{
		const FKnightGraphicsChoice Previous = SavedGraphics;
		const bool bDisplayChanged = DraftGraphics.WindowMode != Previous.WindowMode || DraftGraphics.Resolution != Previous.Resolution
			|| DraftGraphics.DisplayID != Previous.DisplayID || DraftGraphics.DisplayIndex != Previous.DisplayIndex;
		if (!Graphics->ApplyGraphicsNow(DraftGraphics, bDisplayChanged ? &Previous : nullptr, Error)) { SetStatus(Error); return; }
		SavedGraphics = DraftGraphics;
		if (bDisplayChanged)
		{
			HandleValuesChanged();
			StartDisplayConfirm(Previous);
			return;
		}
	}
	SetStatus(LOCTEXT("Saved", "설정을 저장했습니다."));
	HandleValuesChanged();
}

void UKnightSettingsWidget::StartDisplayConfirm(const FKnightGraphicsChoice& Previous)
{
	DisplayRevertGraphics = Previous;
	DisplayConfirmDeadline = FPlatformTime::Seconds() + DisplayConfirmSeconds;
	DisplaySecondsShown = -1;
	bDisplayConfirmShown = true;
	// Core ticker: keeps counting while the raid is paused behind the settings screen.
	DisplayTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickDisplayConfirm), 0.1f);
	UpdateDisplayCountdown();
	HandleDisplayConfirm(true);
}

void UKnightSettingsWidget::StopDisplayConfirm()
{
	FTSTicker::RemoveTicker(DisplayTicker);
	DisplayTicker.Reset();
	bDisplayConfirmShown = false;
}

bool UKnightSettingsWidget::TickDisplayConfirm(float DeltaTime)
{
	if (FPlatformTime::Seconds() >= DisplayConfirmDeadline)
	{
		RevertDisplay();
		return false;
	}
	UpdateDisplayCountdown();
	return true;
}

void UKnightSettingsWidget::UpdateDisplayCountdown()
{
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(DisplayConfirmDeadline - FPlatformTime::Seconds()));
	if (Seconds == DisplaySecondsShown) return;
	DisplaySecondsShown = Seconds;
	SetStatus(FText::Format(LOCTEXT("DisplayCountdown", "이 화면 설정을 유지할까요? {0}초 후 이전 설정으로 돌아갑니다."), FText::AsNumber(Seconds)));
}

void UKnightSettingsWidget::KeepDisplay()
{
	if (!bDisplayConfirmShown) return;
	StopDisplayConfirm();
	if (UknightGameUserSettings* const Graphics = UknightGameUserSettings::Get()) Graphics->ConfirmGraphics();
	HandleDisplayConfirm(false);
	SetStatus(LOCTEXT("DisplayKept", "화면 설정을 저장했습니다."));
	const bool bClose = bCloseAfterDisplayConfirm && !IsDirty();
	bCloseAfterDisplayConfirm = false;
	if (bClose) CloseNow();
	else HandleFocusRequested();
}

void UKnightSettingsWidget::RevertDisplay()
{
	if (bDisplayConfirmShown) RestoreDisplay(true);
}

void UKnightSettingsWidget::RestoreDisplay(bool bRefreshScreen)
{
	StopDisplayConfirm();
	bCloseAfterDisplayConfirm = false;
	FText Error;
	UknightGameUserSettings* const Graphics = UknightGameUserSettings::Get();
	const bool bRestored = Graphics && Graphics->ApplyGraphicsNow(DisplayRevertGraphics, nullptr, Error, true);
	if (!bRestored)
	{
		// The pending revert stays saved, so the next launch still restores the previous display.
		UE_LOG(Logknight, Warning, TEXT("Display revert failed: %s"), *Error.ToString());
	}
	SavedGraphics = DisplayRevertGraphics;
	DraftGraphics = SavedGraphics;
	if (!bRefreshScreen) return;
	HandleDisplayConfirm(false);
	HandleTabChanged();
	SetStatus(bRestored ? LOCTEXT("DisplayReverted", "이전 화면 설정으로 되돌렸습니다.") : Error);
	HandleFocusRequested();
}

void UKnightSettingsWidget::Revert()
{
	if (bDisplayConfirmShown) return;
	DraftGraphics = SavedGraphics;
	CopyPersonal(DraftPersonal, SavedPersonal);
	DraftKeys = SavedKeys;
	PreviewAudio(SavedPersonal);
	EndDisplayPreview();
	HandleTabChanged();
	ShowUnsavedPrompt(false);
	SetStatus(LOCTEXT("Discarded", "편집한 내용을 버렸습니다."));
}

void UKnightSettingsWidget::RestoreDefaults()
{
	if (bDisplayConfirmShown) return;
	DraftGraphics = FKnightGraphicsChoice();
	FKnightDisplayInfo Primary;
	if (UknightGameUserSettings::ResolveDisplay(FString(), INDEX_NONE, Primary))
	{
		DraftGraphics.DisplayID = Primary.ID;
		DraftGraphics.DisplayIndex = Primary.Index;
		DraftGraphics.Resolution = Primary.DesktopResolution;
	}
	UknightSharedSettings* const Defaults = NewObject<UknightSharedSettings>(this);
	CopyPersonal(DraftPersonal, Defaults);
	DraftKeys = DefaultKeys;
	PreviewAudio(DraftPersonal);
	PreviewDisplay(DraftPersonal);
	HandleTabChanged();
	SetStatus(LOCTEXT("DefaultsStaged", "기본값을 불러왔습니다. 적용을 누르면 저장됩니다."));
}

void UKnightSettingsWidget::RequestClose()
{
	if (bDisplayConfirmShown) return;
	if (IsDirty())
	{
		ShowUnsavedPrompt(true);
		SetStatus(LOCTEXT("Unsaved", "적용하지 않은 변경 사항이 있습니다."));
	}
	else CloseNow();
}

void UKnightSettingsWidget::ShowUnsavedPrompt(bool bShow)
{
	bUnsavedPromptShown = bShow;
	HandleUnsavedPrompt(bShow);
}

void UKnightSettingsWidget::CloseNow()
{
	ShowUnsavedPrompt(false);
	EndDisplayPreview();
	OnClosed.Broadcast();
}

void UKnightSettingsWidget::ConfirmApply()
{
	ShowUnsavedPrompt(false);
	Apply();
	if (bDisplayConfirmShown) bCloseAfterDisplayConfirm = true;
	else if (!IsDirty()) CloseNow();
	else ShowUnsavedPrompt(true);
}
void UKnightSettingsWidget::ConfirmDiscard() { Revert(); CloseNow(); }
void UKnightSettingsWidget::ConfirmContinue() { ShowUnsavedPrompt(false); SetStatus(FText::GetEmpty()); HandleFocusRequested(); }

FReply UKnightSettingsWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		if (bDisplayConfirmShown) { RevertDisplay(); return FReply::Handled(); }
		if (IsCapturingKey()) return FReply::Unhandled();
		// A second Esc on the unsaved-changes prompt means "keep editing", not "close anyway".
		if (bUnsavedPromptShown) ConfirmContinue();
		else if (!HandleBackRequested()) RequestClose();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UKnightSettingsWidget::NativeDestruct()
{
	// Leaving the map mid-countdown counts as not keeping the display.
	if (bDisplayConfirmShown) RestoreDisplay(false);
	EndDisplayPreview();
	if (SavedPersonal) PreviewAudio(SavedPersonal);
	Super::NativeDestruct();
}

#undef LOCTEXT_NAMESPACE
