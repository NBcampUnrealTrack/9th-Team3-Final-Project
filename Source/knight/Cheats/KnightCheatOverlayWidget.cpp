#include "Cheats/KnightCheatOverlayWidget.h"

#include "Cheats/KnightCheatSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"

void UKnightCheatOverlayWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	UCanvasPanel* const Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	UBorder* const Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.025f, 0.055f, 0.09f, 0.96f));
	Panel->SetPadding(FMargin(14.f));
	UCanvasPanelSlot* const PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.f));
	PanelSlot->SetPosition(FVector2D(0.f, 32.f));
	PanelSlot->SetSize(FVector2D(540.f, 116.f));

	UVerticalBox* const Body = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Body);
	UTextBlock* const Prompt = WidgetTree->ConstructWidget<UTextBlock>();
	Prompt->SetText(INVTEXT("치트 명령  ·  Enter 실행  ·  Esc 닫기"));
	Prompt->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Body->AddChildToVerticalBox(Prompt)->SetPadding(FMargin(0.f, 0.f, 0.f, 7.f));
	CommandInput = WidgetTree->ConstructWidget<UEditableTextBox>();
	CommandInput->SetHintText(INVTEXT("명령 입력"));
	CommandInput->SetClearKeyboardFocusOnCommit(false);
	Body->AddChildToVerticalBox(CommandInput)->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
	FeedbackText = WidgetTree->ConstructWidget<UTextBlock>();
	FeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.9f, 0.75f, 1.f)));
	FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
	Body->AddChildToVerticalBox(FeedbackText);
}

FReply UKnightCheatOverlayWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (UKnightCheatSubsystem* const Cheats = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKnightCheatSubsystem>() : nullptr)
	{
		if (Cheats->HandleOverlayKey(Event)) return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FString UKnightCheatOverlayWidget::GetCommandText() const
{
	return CommandInput ? CommandInput->GetText().ToString() : FString();
}

void UKnightCheatOverlayWidget::SetCommandText(const FText& Text)
{
	if (CommandInput) CommandInput->SetText(Text);
}

void UKnightCheatOverlayWidget::FocusInput()
{
	if (CommandInput) CommandInput->SetKeyboardFocus();
}

void UKnightCheatOverlayWidget::ShowFeedback(const FText& Message)
{
	if (!FeedbackText) return;
	FeedbackText->SetText(Message);
	FeedbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UKnightCheatOverlayWidget::ClearFeedback()
{
	if (FeedbackText) FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
}
