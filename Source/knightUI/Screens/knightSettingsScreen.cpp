#include "Screens/knightSettingsScreen.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/knightSettingRow.h"
#include "Menu/UI/KnightButtonFillExtension.h"

#define LOCTEXT_NAMESPACE "knightUI"

UknightSettingsTab::UknightSettingsTab(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HoverScale = 1.f;
	PressedScale = 1.f;
	bSelectable = true;
}

void UknightSettingsTab::SetTab(const FText& Caption, const FText& Label)
{
	SetButtonText(Label);
	if (Txt_Caption)
	{
		Txt_Caption->SetText(Caption);
	}
}

void UknightSettingsTab::SetTabSelected(bool bInSelected)
{
	if (Img_Selected)
	{
		Img_Selected->SetVisibility(bInSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	SetIsSelected(bInSelected, false);
}

void UknightSettingsTab::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	OnTabFocused.ExecuteIfBound();
}

FReply UknightSettingsTab::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Right goes into the rows; left has nowhere to go. Both caught before Slate turns them into navigation.
	if (InKeyEvent.GetKey() == EKeys::Right)
	{
		OnTabEntered.ExecuteIfBound();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::Left)
	{
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UknightSettingsSectionHeader::SetCaption(const FText& Caption)
{
	if (Txt_Caption)
	{
		Txt_Caption->SetText(Caption);
	}
}

void UknightSettingsScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	CalibrationBackdrop = Cast<UImage>(WidgetTree->FindWidget(TEXT("Img_Backdrop")));
	if (CalibrationBackdrop) OriginalBackdropTint = CalibrationBackdrop->GetColorAndOpacity();

	for (int32 Index = 0; Index < TabCount; ++Index)
	{
		if (UknightSettingsTab* const Tab = GetTabButton(Index))
		{
			UKnightButtonFillExtension::SetEnabled(Tab, true);
			Tab->SetTab(GetTabCaption(Index), GetTabLabel(Index));
			Tab->OnClicked().AddWeakLambda(this, [this, Index] { SetTab(Index); FocusLevel(true); });
			Tab->OnTabFocused.BindWeakLambda(this, [this, Index] { HandleTabFocused(Index); });
			Tab->OnTabEntered.BindWeakLambda(this, [this] { FocusLevel(true); });
		}
	}
	for (UknightButtonBase* Button : { Btn_Apply.Get(), Btn_Revert.Get(), Btn_Defaults.Get(),
		Btn_PromptApply.Get(), Btn_PromptDiscard.Get(), Btn_PromptContinue.Get() })
		UKnightButtonFillExtension::SetEnabled(Button, true);
	Btn_Apply->OnClicked().AddUObject(this, &ThisClass::Apply);
	Btn_Revert->OnClicked().AddUObject(this, &ThisClass::Revert);
	if (Btn_Defaults) Btn_Defaults->OnClicked().AddUObject(this, &ThisClass::RestoreDefaults);
	if (Btn_PromptApply)
	{
		PromptApplyText = Btn_PromptApply->GetButtonText();
		Btn_PromptApply->OnClicked().AddWeakLambda(this, [this] { IsDisplayConfirmShown() ? KeepDisplay() : ConfirmApply(); });
	}
	if (Btn_PromptDiscard)
	{
		PromptDiscardText = Btn_PromptDiscard->GetButtonText();
		Btn_PromptDiscard->OnClicked().AddWeakLambda(this, [this] { IsDisplayConfirmShown() ? RevertDisplay() : ConfirmDiscard(); });
	}
	if (Btn_PromptContinue) Btn_PromptContinue->OnClicked().AddUObject(this, &ThisClass::ConfirmContinue);
	HandleUnsavedPrompt(false);
}

UknightSettingsTab* UknightSettingsScreen::GetTabButton(int32 Index) const
{
	switch (Index)
	{
	case 0: return Tab_0;
	case 1: return Tab_1;
	case 2: return Tab_2;
	case 3: return Tab_3;
	default: return nullptr;
	}
}

bool UknightSettingsScreen::IsCapturingKey() const
{
	return Rows.ContainsByPredicate([](const UknightSettingRow* Row) { return Row && Row->IsCapturingKey(); });
}

void UknightSettingsScreen::HandleTabChanged()
{
	if (CalibrationBackdrop)
	{
		FLinearColor Tint = OriginalBackdropTint;
		// Only the background becomes translucent; row text/controls keep full opacity.
		if (GetCurrentTab() == 3) Tint.A = FMath::Min(Tint.A, 0.35f);
		CalibrationBackdrop->SetColorAndOpacity(Tint);
	}
	const bool bHadFocus = HasFocusedDescendants();
	Box_Rows->ClearChildren();
	Rows.Reset();

	TArray<FKnightSettingSection> Sections;
	GetTabSections(GetCurrentTab(), Sections);
	bool bFirstSection = true;
	for (const FKnightSettingSection& Section : Sections)
	{
		if (SectionHeaderClass)
		{
			UknightSettingsSectionHeader* const Header = CreateWidget<UknightSettingsSectionHeader>(this, SectionHeaderClass);
			Header->SetCaption(Section.Caption);
			if (UVerticalBoxSlot* const HeaderSlot = Cast<UVerticalBoxSlot>(Box_Rows->AddChild(Header)); HeaderSlot && !bFirstSection)
			{
				HeaderSlot->SetPadding(FMargin(0.f, 24.f, 0.f, 0.f));
			}
		}
		bFirstSection = false;

		for (const FKnightSettingItem& Item : Section.Items)
		{
			const TSubclassOf<UknightSettingRow> RowClass = Item.Type == EKnightSettingControl::Slider ? SliderRowClass
				: Item.Type == EKnightSettingControl::Key ? KeyRowClass : RotatorRowClass;
			if (!RowClass)
			{
				continue;
			}
			UknightSettingRow* const Row = CreateWidget<UknightSettingRow>(this, RowClass);
			UKnightButtonFillExtension::SetEnabled(Row, true);
			Row->Setup(Item);
			Row->OnEdited.BindUObject(this, &ThisClass::HandleRowEdited);
			Row->OnFocused.BindUObject(this, &ThisClass::HandleRowFocused);
			// Left / right belong to the value; up / down stop at the ends instead of leaving the list.
			Row->SetNavigationRuleBase(EUINavigation::Left, EUINavigationRule::Stop);
			Row->SetNavigationRuleBase(EUINavigation::Right, EUINavigationRule::Stop);
			Box_Rows->AddChild(Row);
			Rows.Add(Row);
		}

		if (!Section.Note.IsEmpty())
		{
			UCommonTextBlock* const Note = WidgetTree->ConstructWidget<UCommonTextBlock>();
			if (NoteTextStyle) Note->SetStyle(NoteTextStyle);
			Note->SetAutoWrapText(true);
			Note->SetText(Section.Note);
			if (UVerticalBoxSlot* const NoteSlot = Cast<UVerticalBoxSlot>(Box_Rows->AddChild(Note)))
			{
				NoteSlot->SetPadding(FMargin(12.f, 8.f, 0.f, 0.f));
			}
		}
	}

	for (int32 Index = 0; Index < TabCount; ++Index)
	{
		if (UknightSettingsTab* const Tab = GetTabButton(Index))
		{
			Tab->SetTabSelected(Index == GetCurrentTab());
		}
	}
	if (Scroll_Rows)
	{
		Scroll_Rows->ScrollToStart();
	}
	const TArray<TObjectPtr<UknightSettingRow>> Enabled = Rows.FilterByPredicate([](const UknightSettingRow* Row) { return Row && Row->GetIsEnabled(); });
	if (Enabled.Num() > 0)
	{
		Enabled[0]->SetNavigationRuleBase(EUINavigation::Up, EUINavigationRule::Stop);
		Enabled.Last()->SetNavigationRuleBase(EUINavigation::Down, EUINavigationRule::Stop);
		HandleRowFocused(Enabled[0], false);
	}
	if (bHadFocus && bRowsLevel)
	{
		FocusLevel(true);
	}
	RefreshFooter();
}

void UknightSettingsScreen::HandleValuesChanged()
{
	TArray<FKnightSettingSection> Sections;
	GetTabSections(GetCurrentTab(), Sections);
	for (const FKnightSettingSection& Section : Sections)
	{
		for (const FKnightSettingItem& Item : Section.Items)
		{
			if (TObjectPtr<UknightSettingRow>* const Row = Rows.FindByPredicate([&Item](const UknightSettingRow* Candidate) { return Candidate && Candidate->GetItem().Id == Item.Id; }))
			{
				(*Row)->Refresh(Item);
			}
		}
	}
	RefreshFooter();
}

void UknightSettingsScreen::RefreshFooter()
{
	const int32 DirtyCount = GetDirtyCount();
	// Applying or reverting from the keyboard disables the button under the focus; hand the focus on first.
	const bool bLosingFocus = DirtyCount == 0 && ((Btn_Apply->HasFocusedDescendants()) || (Btn_Revert->HasFocusedDescendants()));
	Btn_Apply->SetIsEnabled(DirtyCount > 0);
	Btn_Revert->SetIsEnabled(DirtyCount > 0);
	if (bLosingFocus && !FocusFooter())
	{
		FocusLevel(false);
	}
	if (Txt_DirtyCount)
	{
		Txt_DirtyCount->SetText(FText::Format(LOCTEXT("SettingsDirtyCount", "●  적용하지 않은 변경 {0}"), FText::AsNumber(DirtyCount)));
		Txt_DirtyCount->SetVisibility(DirtyCount > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UknightSettingsScreen::HandleStatusChanged(const FText& Status)
{
	if (Txt_Status)
	{
		Txt_Status->SetText(Status);
	}
}

void UknightSettingsScreen::HandleUnsavedPrompt(bool bShow)
{
	if (Btn_PromptApply) Btn_PromptApply->SetButtonText(PromptApplyText);
	if (Btn_PromptDiscard) Btn_PromptDiscard->SetButtonText(PromptDiscardText);
	if (Btn_PromptContinue) Btn_PromptContinue->SetVisibility(ESlateVisibility::Visible);
	ShowPrompt(bShow, Btn_PromptApply);
}

void UknightSettingsScreen::HandleDisplayConfirm(bool bShow)
{
	if (!bShow)
	{
		HandleUnsavedPrompt(false);
		return;
	}
	if (Btn_PromptApply) Btn_PromptApply->SetButtonText(LOCTEXT("SettingsDisplayKeep", "유지"));
	if (Btn_PromptDiscard) Btn_PromptDiscard->SetButtonText(LOCTEXT("SettingsDisplayRevert", "되돌리기"));
	if (Btn_PromptContinue) Btn_PromptContinue->SetVisibility(ESlateVisibility::Collapsed);
	// Focus starts on revert: a stray Enter on a screen the player cannot read must not keep it.
	ShowPrompt(true, Btn_PromptDiscard);
}

void UknightSettingsScreen::ShowPrompt(bool bShow, UknightButtonBase* FocusTarget)
{
	for (UknightButtonBase* Button : { Btn_Apply.Get(), Btn_Revert.Get(), Btn_Defaults.Get(),
		Btn_PromptApply.Get(), Btn_PromptDiscard.Get(), Btn_PromptContinue.Get() })
		UKnightButtonFillExtension::Reset(Button);
	if (Panel_Prompt)
	{
		Panel_Prompt->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Panel_Footer)
	{
		Panel_Footer->SetVisibility(bShow ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (bShow && FocusTarget)
	{
		FocusTarget->SetFocus();
	}
}

void UknightSettingsScreen::HandleOpened()
{
	SetRowsLevel(false);
}

void UknightSettingsScreen::HandleFocusRequested()
{
	FocusLevel(bRowsLevel);
}

bool UknightSettingsScreen::HandleBackRequested()
{
	if (!bRowsLevel && !IsFooterFocused())
	{
		return false;
	}
	FocusLevel(false);
	return true;
}

bool UknightSettingsScreen::IsFooterFocused() const
{
	for (const UknightButtonBase* const Button : { Btn_Defaults.Get(), Btn_Revert.Get(), Btn_Apply.Get() })
	{
		if (Button && Button->HasFocusedDescendants())
		{
			return true;
		}
	}
	return false;
}

bool UknightSettingsScreen::FocusFooter()
{
	for (UknightButtonBase* const Button : { Btn_Defaults.Get(), Btn_Revert.Get(), Btn_Apply.Get() })
	{
		if (Button && Button->GetIsEnabled() && Button->IsVisible())
		{
			SetRowsLevel(false);
			Button->SetFocus();
			return true;
		}
	}
	return false;
}

void UknightSettingsScreen::FocusLevel(bool bRows)
{
	if (bRows)
	{
		for (UknightSettingRow* const Row : Rows)
		{
			if (Row && Row->GetIsEnabled())
			{
				SetRowsLevel(true);
				Row->SetFocus();
				return;
			}
		}
	}
	SetRowsLevel(false);
	if (UknightSettingsTab* const Tab = GetTabButton(GetCurrentTab()))
	{
		Tab->SetFocus();
	}
	else
	{
		SetKeyboardFocus();
	}
}

void UknightSettingsScreen::SetRowsLevel(bool bRows)
{
	bRowsLevel = bRows;
	if (Panel_HintsTabs)
	{
		Panel_HintsTabs->SetVisibility(bRows ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Panel_HintsRows)
	{
		Panel_HintsRows->SetVisibility(bRows ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UknightSettingsScreen::HandleTabFocused(int32 Index)
{
	// The rows follow the highlighted category, so the player sees what is inside before entering it.
	SetRowsLevel(false);
	SetTab(Index);
}

void UknightSettingsScreen::HandleRowFocused(UknightSettingRow* Row, bool bKeyboardFocus)
{
	if (!Row)
	{
		return;
	}
	if (Txt_DetailTitle) Txt_DetailTitle->SetText(Row->GetItem().Label);
	if (Txt_DetailBody) Txt_DetailBody->SetText(Row->GetItem().Description);
	if (bKeyboardFocus)
	{
		SetRowsLevel(true);
		if (Scroll_Rows)
		{
			Scroll_Rows->ScrollWidgetIntoView(Row, true, EDescendantScrollDestination::IntoView);
		}
	}
}

void UknightSettingsScreen::HandleRowEdited(FName Id, const FString& Value)
{
	OptionEdited(Id, Value);
}

void UknightSettingsScreen::SwitchTab(int32 Direction)
{
	SetTab(GetCurrentTab() + Direction);
	FocusLevel(bRowsLevel);
	if (TabSound)
	{
		UGameplayStatics::PlaySound2D(this, TabSound);
	}
}

FReply UknightSettingsScreen::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (!bRowsLevel && !IsCapturingKey())
	{
		// Down past the last category goes to the footer buttons; up from the footer comes back.
		UknightSettingsTab* const LastTab = GetTabButton(TabCount - 1);
		if (Key == EKeys::Down && LastTab && LastTab->HasFocusedDescendants() && FocusFooter())
		{
			return FReply::Handled();
		}
		if (Key == EKeys::Up && IsFooterFocused())
		{
			FocusLevel(false);
			return FReply::Handled();
		}
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UknightSettingsScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (IsDisplayConfirmShown())
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	if (Key == EKeys::Q || Key == EKeys::E)
	{
		SwitchTab(Key == EKeys::E ? 1 : -1);
		return FReply::Handled();
	}
	if (Key == EKeys::R)
	{
		RestoreDefaults();
		return FReply::Handled();
	}
	if (Key == EKeys::F)
	{
		Apply();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GknightUISettingsTabCommand(
	TEXT("knight.UI.SettingsTab"),
	TEXT("Switches every open settings screen in this world to the given category (0-3)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 Tab = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		for (TObjectIterator<UknightSettingsScreen> It; It; ++It)
		{
			if (It->GetWorld() == World && It->GetCachedWidget().IsValid())
			{
				It->SetTab(Tab);
			}
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
