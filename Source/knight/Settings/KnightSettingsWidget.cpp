#include "Settings/KnightSettingsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/InputKeySelector.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputCoreTypes.h"
#include "Settings/knightSettingsSubsystem.h"
#include "Settings/knightSharedSettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#define LOCTEXT_NAMESPACE "KnightSettingsScreen"

namespace
{
	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text)
	{
		UTextBlock* const Widget = Tree->ConstructWidget<UTextBlock>();
		Widget->SetText(Text);
		Widget->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		return Widget;
	}

	UButton* MakeButton(UWidgetTree* Tree, const FText& Text)
	{
		UButton* const Button = Tree->ConstructWidget<UButton>();
		Button->AddChild(MakeText(Tree, Text));
		return Button;
	}

	FString BoolText(bool bValue) { return bValue ? TEXT("true") : TEXT("false"); }
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
}

void UKnightSettingRow::Configure(UKnightSettingsWidget* InOwner, FName InId, const FText& Label,
	EKnightSettingControl Type, const TArray<FString>& Options, const FString& Value, float Min, float Max)
{
	Owner = InOwner;
	Id = InId;
	MinValue = Min;
	MaxValue = Max;
	UHorizontalBox* const Root = WidgetTree->ConstructWidget<UHorizontalBox>();
	WidgetTree->RootWidget = Root;
	UTextBlock* const NameText = MakeText(WidgetTree, Label);
	UHorizontalBoxSlot* const NameSlot = Root->AddChildToHorizontalBox(NameText);
	NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	NameSlot->SetPadding(FMargin(8.f, 10.f));
	if (Type == EKnightSettingControl::Combo)
	{
		Combo = WidgetTree->ConstructWidget<UComboBoxString>();
		for (const FString& Option : Options) Combo->AddOption(Option);
		Root->AddChildToHorizontalBox(Combo)->SetPadding(FMargin(8.f));
		Combo->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleCombo);
	}
	else if (Type == EKnightSettingControl::Slider)
	{
		Slider = WidgetTree->ConstructWidget<USlider>();
		USlider* const Control = Slider;
		USizeBox* const Width = WidgetTree->ConstructWidget<USizeBox>();
		Width->SetWidthOverride(220.f);
		Width->AddChild(Control);
		Root->AddChildToHorizontalBox(Width)->SetPadding(FMargin(8.f));
		ValueText = MakeText(WidgetTree, FText::GetEmpty());
		Root->AddChildToHorizontalBox(ValueText)->SetPadding(FMargin(8.f));
		Slider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSlider);
	}
	else if (Type == EKnightSettingControl::Check)
	{
		Check = WidgetTree->ConstructWidget<UCheckBox>();
		Root->AddChildToHorizontalBox(Check)->SetPadding(FMargin(8.f));
		Check->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::HandleCheck);
	}
	else
	{
		KeySelector = WidgetTree->ConstructWidget<UInputKeySelector>();
		KeySelector->SetAllowGamepadKeys(false);
		KeySelector->SetEscapeKeys({EKeys::Escape});
		Root->AddChildToHorizontalBox(KeySelector)->SetPadding(FMargin(8.f));
		KeySelector->OnKeySelected.AddUniqueDynamic(this, &ThisClass::HandleKey);
	}
	SetValue(Value);
}

void UKnightSettingRow::SetValue(const FString& Value)
{
	bUpdating = true;
	if (Combo) Combo->SetSelectedOption(Value);
	if (Slider)
	{
		const float Number = FCString::Atof(*Value);
		Slider->SetValue(FMath::GetRangePct(MinValue, MaxValue, Number));
		UpdateValueText(Number);
	}
	if (Check) Check->SetIsChecked(Value == TEXT("true"));
	if (KeySelector) KeySelector->SetSelectedKey(FInputChord(FKey(*Value)));
	bUpdating = false;
}

bool UKnightSettingRow::IsCapturingKey() const { return KeySelector && KeySelector->GetIsSelectingKey(); }

void UKnightSettingRow::UpdateValueText(float Value)
{
	if (ValueText) ValueText->SetText(FText::AsNumber(FMath::RoundToInt(Value)));
}

void UKnightSettingRow::Notify(const FString& Value)
{
	if (!bUpdating && Owner.IsValid()) Owner->OptionEdited(Id, Value);
}

void UKnightSettingRow::HandleCombo(FString Selected, ESelectInfo::Type) { Notify(Selected); }
void UKnightSettingRow::HandleSlider(float Value)
{
	const float Actual = FMath::Lerp(MinValue, MaxValue, Value);
	UpdateValueText(Actual);
	Notify(FString::SanitizeFloat(Actual));
}
void UKnightSettingRow::HandleCheck(bool bChecked) { Notify(BoolText(bChecked)); }
void UKnightSettingRow::HandleKey(FInputChord SelectedKey) { Notify(SelectedKey.Key.GetFName().ToString()); }

bool UKnightSettingsWidget::Initialize()
{
	if (!Super::Initialize()) return false;
	SetIsFocusable(true);
	BuildLayout();
	return true;
}

void UKnightSettingsWidget::BuildLayout()
{
	USizeBox* const Frame = WidgetTree->ConstructWidget<USizeBox>();
	Frame->SetWidthOverride(800.f);
	Frame->SetHeightOverride(620.f);
	WidgetTree->RootWidget = Frame;
	UBorder* const Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.055f, 0.98f));
	Frame->AddChild(Backdrop);
	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Backdrop->SetContent(Column);
	Column->AddChildToVerticalBox(MakeText(WidgetTree, LOCTEXT("Header", "설정")))->SetPadding(FMargin(24.f, 14.f));
	UHorizontalBox* const Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(18.f, 4.f));
	UButton* const Graphics = MakeButton(WidgetTree, LOCTEXT("Graphics", "그래픽"));
	UButton* const Controls = MakeButton(WidgetTree, LOCTEXT("Controls", "조작"));
	UButton* const Audio = MakeButton(WidgetTree, LOCTEXT("Audio", "오디오"));
	UButton* const Display = MakeButton(WidgetTree, LOCTEXT("Display", "화면 보정"));
	for (UButton* Button : {Graphics, Controls, Audio, Display})
		Tabs->AddChildToHorizontalBox(Button)->SetPadding(FMargin(8.f));
	Graphics->OnClicked.AddUniqueDynamic(this, &ThisClass::TabGraphics);
	Controls->OnClicked.AddUniqueDynamic(this, &ThisClass::TabControls);
	Audio->OnClicked.AddUniqueDynamic(this, &ThisClass::TabAudio);
	Display->OnClicked.AddUniqueDynamic(this, &ThisClass::TabDisplay);
	UScrollBox* const Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Column->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	RowsBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(RowsBox);
	StatusText = MakeText(WidgetTree, FText::GetEmpty());
	Column->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(24.f, 8.f));
	UHorizontalBox* const Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Footer)->SetPadding(FMargin(18.f, 12.f));
	ApplyButton = MakeButton(WidgetTree, LOCTEXT("Apply", "적용"));
	UButton* const CancelButton = MakeButton(WidgetTree, LOCTEXT("Cancel", "취소"));
	UButton* const DefaultsButton = MakeButton(WidgetTree, LOCTEXT("Defaults", "기본값 복원"));
	UButton* const CloseButton = MakeButton(WidgetTree, LOCTEXT("Close", "닫기"));
	for (UButton* Button : {ApplyButton.Get(), CancelButton, DefaultsButton, CloseButton})
		Footer->AddChildToHorizontalBox(Button)->SetPadding(FMargin(8.f));
	ApplyButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickApply);
	CancelButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickCancel);
	DefaultsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickDefaults);
	CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickClose);
	ConfirmBox = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(ConfirmBox)->SetPadding(FMargin(18.f, 6.f));
	UButton* const ConfirmSave = MakeButton(WidgetTree, LOCTEXT("ConfirmSave", "적용하고 닫기"));
	UButton* const ConfirmDrop = MakeButton(WidgetTree, LOCTEXT("ConfirmDrop", "버리고 닫기"));
	UButton* const ConfirmStay = MakeButton(WidgetTree, LOCTEXT("ConfirmStay", "계속 편집"));
	for (UButton* Button : {ConfirmSave, ConfirmDrop, ConfirmStay})
		ConfirmBox->AddChildToHorizontalBox(Button)->SetPadding(FMargin(8.f));
	ConfirmSave->OnClicked.AddUniqueDynamic(this, &ThisClass::ConfirmApply);
	ConfirmDrop->OnClicked.AddUniqueDynamic(this, &ThisClass::ConfirmDiscard);
	ConfirmStay->OnClicked.AddUniqueDynamic(this, &ThisClass::ConfirmContinue);
	ShowConfirm(false);
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
	SavedGraphics = Graphics->GetDisplayedGraphics();
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
							KeyLabels.Add(Entry.Key, Mapping.GetDisplayName().IsEmpty() ? Entry.Key.ToString() : Mapping.GetDisplayName().ToString());
							break;
						}
					}
				}
			}
		}
	}
	SetTab(0);
	SetStatus(Graphics->HasPendingGraphics()
		? LOCTEXT("Pending", "그래픽 설정은 게임을 다시 시작하면 적용됩니다.") : FText::GetEmpty());
	ShowConfirm(false);
	RefreshApply();
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
	To->bToggleRun = From->bToggleRun;
	To->bSubtitlesEnabled = From->bSubtitlesEnabled;
	To->MasterVolume = From->MasterVolume;
	To->MusicVolume = From->MusicVolume;
	To->SFXVolume = From->SFXVolume;
	To->VoiceVolume = From->VoiceVolume;
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
void UKnightSettingsWidget::RefreshApply() { if (ApplyButton) ApplyButton->SetIsEnabled(IsDirty()); }
void UKnightSettingsWidget::SetStatus(const FText& Text) { if (StatusText) StatusText->SetText(Text); }
void UKnightSettingsWidget::ShowConfirm(bool bShow) { if (ConfirmBox) ConfirmBox->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
void UKnightSettingsWidget::FocusSettings() { SetKeyboardFocus(); }

void UKnightSettingsWidget::AddRow(FName Id, const FText& Label, EKnightSettingControl Type,
	const TArray<FString>& Options, const FString& Value, float Min, float Max)
{
	UKnightSettingRow* const Row = CreateWidget<UKnightSettingRow>(GetOwningPlayer(), UKnightSettingRow::StaticClass());
	if (!Row || !RowsBox) return;
	Row->Configure(this, Id, Label, Type, Options, Value, Min, Max);
	RowsBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(18.f, 2.f));
	Rows.Add(Id, Row);
}
void UKnightSettingsWidget::AddCombo(FName Id, const FText& Label, const TArray<FString>& Options, const FString& Value)
{ AddRow(Id, Label, EKnightSettingControl::Combo, Options, Value); }
void UKnightSettingsWidget::AddSlider(FName Id, const FText& Label, float Value, float Min, float Max)
{ AddRow(Id, Label, EKnightSettingControl::Slider, {}, FString::SanitizeFloat(Value), Min, Max); }
void UKnightSettingsWidget::AddCheck(FName Id, const FText& Label, bool bChecked)
{ AddRow(Id, Label, EKnightSettingControl::Check, {}, BoolText(bChecked)); }
void UKnightSettingsWidget::AddKey(FName Id, const FText& Label, const FKey& Key)
{ AddRow(Id, Label, EKnightSettingControl::Key, {}, Key.GetFName().ToString()); }

void UKnightSettingsWidget::SetTab(int32 Index)
{
	CurrentTab = Index;
	BuildTab();
}

void UKnightSettingsWidget::BuildTab()
{
	if (!RowsBox || !DraftPersonal) return;
	RowsBox->ClearChildren();
	Rows.Reset();
	if (CurrentTab == 0)
	{
		const FString Mode = DraftGraphics.WindowMode == EWindowMode::Fullscreen ? TEXT("전체 화면")
			: DraftGraphics.WindowMode == EWindowMode::Windowed ? TEXT("창모드") : TEXT("테두리 없는 창모드");
		AddCombo(TEXT("WindowMode"), LOCTEXT("WindowMode", "화면 모드"),
			{TEXT("전체 화면"), TEXT("창모드"), TEXT("테두리 없는 창모드")}, Mode);
		TArray<FIntPoint> Resolutions;
		UknightGameUserSettings::GetSupportedResolutions(Resolutions);
		TArray<FString> Values;
		for (const FIntPoint& Size : Resolutions) Values.Add(FString::Printf(TEXT("%dx%d"), Size.X, Size.Y));
		AddCombo(TEXT("Resolution"), LOCTEXT("Resolution", "해상도"), Values,
			FString::Printf(TEXT("%dx%d"), DraftGraphics.Resolution.X, DraftGraphics.Resolution.Y));
		if (DraftGraphics.WindowMode == EWindowMode::WindowedFullscreen)
		{
			if (UKnightSettingRow* const Row = Rows.FindRef(TEXT("Resolution"))) Row->SetIsEnabled(false);
			RowsBox->AddChildToVerticalBox(MakeText(WidgetTree, LOCTEXT("DesktopRes", "테두리 없는 창모드는 데스크톱 해상도를 사용합니다.")));
		}
		AddCombo(TEXT("FPS"), LOCTEXT("FPS", "FPS 제한"),
			{TEXT("30"), TEXT("60"), TEXT("90"), TEXT("120"), TEXT("144"), TEXT("165"), TEXT("240")},
			FString::FromInt(DraftGraphics.FrameRateLimit));
		AddCheck(TEXT("VSync"), LOCTEXT("VSync", "V-Sync"), DraftGraphics.bVSync);
		const int32 Q = DraftGraphics.AntiAliasing;
		const bool bSame = DraftGraphics.Shadows == Q && DraftGraphics.Textures == Q && DraftGraphics.Effects == Q
			&& DraftGraphics.PostProcess == Q && DraftGraphics.ViewDistance == Q;
		AddCombo(TEXT("Preset"), LOCTEXT("Preset", "전체 품질"), QualityOptions(true), bSame ? QualityText(Q) : TEXT("사용자 지정"));
		AddCombo(TEXT("AA"), LOCTEXT("AA", "안티앨리어싱"), QualityOptions(), QualityText(DraftGraphics.AntiAliasing));
		AddCombo(TEXT("Shadows"), LOCTEXT("Shadows", "그림자"), QualityOptions(), QualityText(DraftGraphics.Shadows));
		AddCombo(TEXT("Textures"), LOCTEXT("Textures", "텍스처"), QualityOptions(), QualityText(DraftGraphics.Textures));
		AddCombo(TEXT("Effects"), LOCTEXT("Effects", "이펙트"), QualityOptions(), QualityText(DraftGraphics.Effects));
		AddCombo(TEXT("PostProcess"), LOCTEXT("Post", "후처리"), QualityOptions(), QualityText(DraftGraphics.PostProcess));
		AddCombo(TEXT("ViewDistance"), LOCTEXT("View", "시야 거리"), QualityOptions(), QualityText(DraftGraphics.ViewDistance));
		AddCheck(TEXT("MotionBlur"), LOCTEXT("Motion", "모션 블러"), DraftGraphics.bMotionBlur);
		RowsBox->AddChildToVerticalBox(MakeText(WidgetTree, LOCTEXT("Tessellation", "테셀레이션: 현재 렌더링 구성에서 지원하지 않습니다.")));
	}
	else if (CurrentTab == 1)
	{
		AddSlider(TEXT("MouseSensitivity"), LOCTEXT("Sensitivity", "마우스 감도"), DraftPersonal->MouseSensitivity * 100.f, 10.f, 500.f);
		AddCheck(TEXT("MouseAcceleration"), LOCTEXT("Acceleration", "마우스 시점 가속"), DraftPersonal->bMouseAcceleration);
		TArray<FName> Names;
		DraftKeys.GetKeys(Names);
		Names.Sort(FNameLexicalLess());
		for (const FName& Name : Names)
			AddKey(Name, FText::FromString(KeyLabels.FindRef(Name)), DraftKeys.FindRef(Name));
		if (Names.IsEmpty()) RowsBox->AddChildToVerticalBox(MakeText(WidgetTree,
			LOCTEXT("NoMappings", "변경 가능한 입력이 없습니다. Input Mapping Context의 Player Mappable 설정을 확인해 주세요.")));
		RowsBox->AddChildToVerticalBox(MakeText(WidgetTree, LOCTEXT("ReservedEscape", "Esc는 설정 열기와 닫기에 예약되어 있습니다.")));
	}
	else if (CurrentTab == 2)
	{
		AddSlider(TEXT("Master"), LOCTEXT("Master", "마스터"), DraftPersonal->MasterVolume * 100.f, 0.f, 100.f);
		AddSlider(TEXT("Music"), LOCTEXT("Music", "BGM"), DraftPersonal->MusicVolume * 100.f, 0.f, 100.f);
		AddSlider(TEXT("SFX"), LOCTEXT("SFX", "SFX"), DraftPersonal->SFXVolume * 100.f, 0.f, 100.f);
		AddSlider(TEXT("Voice"), LOCTEXT("Voice", "Voice"), DraftPersonal->VoiceVolume * 100.f, 0.f, 100.f);
		AddCheck(TEXT("Subtitles"), LOCTEXT("Subtitles", "자막"), DraftPersonal->bSubtitlesEnabled);
		UButton* const SubtitleExample = MakeButton(WidgetTree, LOCTEXT("SubtitleExample", "저장된 자막 설정 확인"));
		RowsBox->AddChildToVerticalBox(SubtitleExample)->SetPadding(FMargin(18.f, 8.f));
		SubtitleExample->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickSubtitleExample);
	}
	else
	{
		AddSlider(TEXT("FOV"), LOCTEXT("FOV", "FOV"), 80.f + DraftPersonal->FieldOfViewOffset, 60.f, 110.f);
		AddSlider(TEXT("Gamma"), LOCTEXT("Gamma", "감마"), DraftPersonal->Gamma * 100.f, 80.f, 120.f);
		AddSlider(TEXT("Brightness"), LOCTEXT("Brightness", "밝기"), DraftPersonal->Brightness * 100.f, 80.f, 120.f);
		UHorizontalBox* const Chart = WidgetTree->ConstructWidget<UHorizontalBox>();
		RowsBox->AddChildToVerticalBox(Chart)->SetPadding(FMargin(20.f, 12.f));
		for (int32 Step = 0; Step <= 10; ++Step)
		{
			UBorder* const Block = WidgetTree->ConstructWidget<UBorder>();
			const float Gray = Step / 10.f;
			Block->SetBrushColor(FLinearColor(Gray, Gray, Gray));
			USizeBox* const Size = WidgetTree->ConstructWidget<USizeBox>();
			Size->SetWidthOverride(40.f);
			Size->SetHeightOverride(40.f);
			Size->AddChild(Block);
			Chart->AddChildToHorizontalBox(Size);
		}
		RowsBox->AddChildToVerticalBox(MakeText(WidgetTree, LOCTEXT("Chart", "어두운 칸부터 밝은 칸까지 구별되는지 확인하세요.")));
	}
}

void UKnightSettingsWidget::ApplyPreset(int32 Quality)
{
	DraftGraphics.AntiAliasing = Quality;
	DraftGraphics.Shadows = Quality;
	DraftGraphics.Textures = Quality;
	DraftGraphics.Effects = Quality;
	DraftGraphics.PostProcess = Quality;
	DraftGraphics.ViewDistance = Quality;
	for (const FName Id : {FName(TEXT("AA")), FName(TEXT("Shadows")), FName(TEXT("Textures")),
		FName(TEXT("Effects")), FName(TEXT("PostProcess")), FName(TEXT("ViewDistance"))})
		if (UKnightSettingRow* const Row = Rows.FindRef(Id)) Row->SetValue(QualityText(Quality));
}
void UKnightSettingsWidget::RefreshPresetRows()
{
	const int32 Q = DraftGraphics.AntiAliasing;
	const bool bSame = DraftGraphics.Shadows == Q && DraftGraphics.Textures == Q && DraftGraphics.Effects == Q
		&& DraftGraphics.PostProcess == Q && DraftGraphics.ViewDistance == Q;
	if (UKnightSettingRow* const Row = Rows.FindRef(TEXT("Preset"))) Row->SetValue(bSame ? QualityText(Q) : TEXT("사용자 지정"));
}

void UKnightSettingsWidget::OptionEdited(FName Id, const FString& Value)
{
	if (!DraftPersonal) return;
	const bool bOn = Value == TEXT("true");
	if (Id == TEXT("WindowMode"))
	{
		DraftGraphics.WindowMode = Value == TEXT("전체 화면") ? EWindowMode::Fullscreen
			: Value == TEXT("창모드") ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen;
		if (DraftGraphics.WindowMode == EWindowMode::WindowedFullscreen)
			if (const UknightGameUserSettings* const Settings = UknightGameUserSettings::Get()) DraftGraphics.Resolution = Settings->GetDesktopResolution();
		if (UKnightSettingRow* const Row = Rows.FindRef(TEXT("Resolution")))
		{
			Row->SetIsEnabled(DraftGraphics.WindowMode != EWindowMode::WindowedFullscreen);
			Row->SetValue(FString::Printf(TEXT("%dx%d"), DraftGraphics.Resolution.X, DraftGraphics.Resolution.Y));
		}
	}
	else if (Id == TEXT("Resolution")) {
		TArray<FString> Parts; Value.ParseIntoArray(Parts, TEXT("x"));
		if (Parts.Num() == 2) DraftGraphics.Resolution = FIntPoint(FCString::Atoi(*Parts[0]), FCString::Atoi(*Parts[1])); }
	else if (Id == TEXT("FPS")) DraftGraphics.FrameRateLimit = FCString::Atoi(*Value);
	else if (Id == TEXT("VSync")) DraftGraphics.bVSync = bOn;
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
			Rows.FindRef(Id)->SetValue(DraftKeys.FindRef(Id).GetFName().ToString());
			return;
		}
		for (const TPair<FName, FKey>& Other : DraftKeys)
		{
			if (Other.Key != Id && Other.Value == NewKey)
			{
				SetStatus(FText::Format(LOCTEXT("KeyConflict", "{0}에 이미 지정된 키입니다."), FText::FromName(Other.Key)));
				Rows.FindRef(Id)->SetValue(DraftKeys.FindRef(Id).GetFName().ToString());
				return;
			}
		}
		DraftKeys.Add(Id, NewKey);
	}
	if (Id == TEXT("AA") || Id == TEXT("Shadows") || Id == TEXT("Textures") || Id == TEXT("Effects")
		|| Id == TEXT("PostProcess") || Id == TEXT("ViewDistance")) RefreshPresetRows();
	SetStatus(FText::GetEmpty());
	RefreshApply();
}

void UKnightSettingsWidget::Apply()
{
	if (!IsDirty()) return;
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
		if (!Graphics->QueueGraphicsForNextProcess(DraftGraphics, Error)) { SetStatus(Error); return; }
		SavedGraphics = DraftGraphics;
		SetStatus(LOCTEXT("GraphicsQueued", "그래픽 설정은 게임을 다시 시작하면 적용됩니다."));
	}
	else SetStatus(LOCTEXT("Saved", "설정을 저장했습니다."));
	RefreshApply();
}

void UKnightSettingsWidget::Cancel()
{
	DraftGraphics = SavedGraphics;
	CopyPersonal(DraftPersonal, SavedPersonal);
	DraftKeys = SavedKeys;
	BuildTab();
	RefreshApply();
	ShowConfirm(false);
	SetStatus(LOCTEXT("Discarded", "편집한 내용을 버렸습니다."));
}

void UKnightSettingsWidget::RestoreDefaults()
{
	DraftGraphics = FKnightGraphicsChoice();
	if (const UknightGameUserSettings* const Graphics = UknightGameUserSettings::Get()) DraftGraphics.Resolution = Graphics->GetDesktopResolution();
	UknightSharedSettings* const Defaults = NewObject<UknightSharedSettings>(this);
	CopyPersonal(DraftPersonal, Defaults);
	DraftKeys = DefaultKeys;
	BuildTab();
	RefreshApply();
	SetStatus(LOCTEXT("DefaultsStaged", "기본값을 편집 값으로 불러왔습니다. 적용을 누르면 저장됩니다."));
}

void UKnightSettingsWidget::RequestClose()
{
	if (IsDirty())
	{
		ShowConfirm(true);
		SetStatus(LOCTEXT("Unsaved", "저장하지 않은 변경 사항이 있습니다."));
	}
	else CloseNow();
}
bool UKnightSettingsWidget::IsCapturingKey() const
{
	for (const TPair<FName, TObjectPtr<UKnightSettingRow>>& Entry : Rows)
		if (Entry.Value && Entry.Value->IsCapturingKey()) return true;
	return false;
}
TSubclassOf<UKnightSettingsWidget> UKnightSettingsWidget::GetSharedWidgetClass()
{
	if (UClass* const AssetClass = LoadClass<UKnightSettingsWidget>(nullptr, TEXT("/Game/Knight/UI/WBP_Settings.WBP_Settings_C")))
		return AssetClass;
	return StaticClass();
}
void UKnightSettingsWidget::CloseNow() { OnClosed.Broadcast(); }
FReply UKnightSettingsWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		if (IsCapturingKey()) return FReply::Unhandled();
		RequestClose();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
void UKnightSettingsWidget::TabGraphics() { SetTab(0); }
void UKnightSettingsWidget::TabControls() { SetTab(1); }
void UKnightSettingsWidget::TabAudio() { SetTab(2); }
void UKnightSettingsWidget::TabDisplay() { SetTab(3); }
void UKnightSettingsWidget::ClickApply() { Apply(); }
void UKnightSettingsWidget::ClickCancel() { Cancel(); }
void UKnightSettingsWidget::ClickDefaults() { RestoreDefaults(); }
void UKnightSettingsWidget::ClickClose() { RequestClose(); }
void UKnightSettingsWidget::ConfirmApply() { Apply(); if (!IsDirty()) CloseNow(); }
void UKnightSettingsWidget::ConfirmDiscard() { Cancel(); CloseNow(); }
void UKnightSettingsWidget::ConfirmContinue() { ShowConfirm(false); }
void UKnightSettingsWidget::ClickSubtitleExample()
{
	const ULocalPlayer* const Player = GetOwningLocalPlayer();
	UknightSettingsSubsystem* const Settings = Player ? Player->GetSubsystem<UknightSettingsSubsystem>() : nullptr;
	SetStatus(Settings && Settings->AreSubtitlesEnabled()
		? LOCTEXT("SubtitleOnExample", "[자막 예시] 기사의 여정이 시작됩니다.")
		: LOCTEXT("SubtitleOffExample", "자막이 꺼져 있어 예시를 표시하지 않습니다."));
}

#undef LOCTEXT_NAMESPACE
