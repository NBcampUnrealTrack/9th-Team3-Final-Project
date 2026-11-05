#include "HUD/knightHUDMessageLog.h"

#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Foundation/knightUISubsystem.h"
#include "knightAudioSubsystem.h"
#include "Monster/knightMonsterDefinition.h"
#include "Settings/knightSettingsSubsystem.h"
#include "Sound/SoundBase.h"
#include "ViewModels/knightVM_PlayerStatus.h"

void UknightHUDMessageLog::NativeConstruct()
{
	Super::NativeConstruct();
	if (Box_Lines)
	{
		Box_Lines->ClearChildren();
	}
	if (UknightAudioSubsystem* const Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightAudioSubsystem>() : nullptr)
	{
		VoiceHandle = Audio->OnVoiceStarted.AddUObject(this, &ThisClass::HandleVoiceStarted);
	}
	const ULocalPlayer* const LocalPlayer = GetOwningLocalPlayer();
	const UknightUISubsystem* const UI = LocalPlayer ? LocalPlayer->GetSubsystem<UknightUISubsystem>() : nullptr;
	if (UknightVM_PlayerStatus* const VM = UI ? UI->GetPlayerStatusViewModel() : nullptr)
	{
		ViewModel = VM;
		PartHandle = VM->OnPlayerPartBroken.AddUObject(this, &ThisClass::HandlePartBroken);
	}
}

void UknightHUDMessageLog::NativeDestruct()
{
	if (UknightAudioSubsystem* const Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightAudioSubsystem>() : nullptr)
	{
		Audio->OnVoiceStarted.Remove(VoiceHandle);
	}
	if (UknightVM_PlayerStatus* const VM = ViewModel.Get())
	{
		VM->OnPlayerPartBroken.Remove(PartHandle);
	}
	VoiceHandle.Reset();
	PartHandle.Reset();
	ViewModel.Reset();
	if (Ticker.IsValid())
	{
		FTSTicker::RemoveTicker(Ticker);
		Ticker.Reset();
	}
	Lines.Reset();
	Super::NativeDestruct();
}

void UknightHUDMessageLog::HandleVoiceStarted(USoundBase* Voice)
{
	ULocalPlayer* const LocalPlayer = GetOwningLocalPlayer();
	UknightSettingsSubsystem* const Settings = LocalPlayer ? LocalPlayer->GetSubsystem<UknightSettingsSubsystem>() : nullptr;
	if (!Voice || !Messages || (Settings && !Settings->AreSubtitlesEnabled()))
	{
		return;
	}
	const FSoftObjectPath Path(Voice);
	if (const FknightVoiceSubtitle* const Subtitle = Messages->VoiceSubtitles.FindByPredicate(
		[&Path](const FknightVoiceSubtitle& Each) { return Each.Voice.ToSoftObjectPath() == Path; }))
	{
		AddLine(Subtitle->Line, false);
	}
}

void UknightHUDMessageLog::HandlePartBroken(EknightBodyPart Part)
{
	const int32 Index = static_cast<int32>(Part);
	if (Messages && Messages->PartBrokenLines.IsValidIndex(Index))
	{
		AddLine(Messages->PartBrokenLines[Index], true);
	}
}

void UknightHUDMessageLog::AddMonsterReadoutLine(EknightMonsterReadout Type, const UknightMonsterDefinition* Definition)
{
	const FknightMonsterReadoutLine* const Entry = Messages ? Messages->MonsterReadoutLines.Find(Type) : nullptr;
	if (!Entry)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (const double* const Last = ReadoutLineTimes.Find(Type); Last && Now - *Last < Entry->Cooldown)
	{
		return;
	}
	const FknightMonsterReadoutText* const Override = Definition ? Definition->ReadoutTexts.Find(Type) : nullptr;
	const FText& Line = Override && !Override->AssistantLine.IsEmpty() ? Override->AssistantLine : Entry->Line;
	if (Line.IsEmpty())
	{
		return;
	}
	ReadoutLineTimes.Add(Type, Now);
	AddLine(Line, false);
}

void UknightHUDMessageLog::AddLine(const FText& Text, bool bSystem)
{
	if (!Box_Lines || Text.IsEmpty())
	{
		return;
	}
	const FText Speaker = Messages ? (bSystem ? Messages->SystemSpeaker : Messages->VoiceSpeaker) : FText::GetEmpty();
	UCommonTextBlock* const Label = WidgetTree->ConstructWidget<UCommonTextBlock>();
	if (LineStyle)
	{
		Label->SetStyle(LineStyle);
	}
	Label->SetText(Speaker.IsEmpty() ? Text : FText::Format(NSLOCTEXT("knightHUD", "MessageLine", "{0} {1}"), Speaker, Text));
	Label->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UVerticalBoxSlot* const LineSlot = Box_Lines->AddChildToVerticalBox(Label))
	{
		LineSlot->SetHorizontalAlignment(HAlign_Center);
	}
	// After adding: the text block applies its style color again when its Slate widget is built.
	Label->SetColorAndOpacity(bSystem ? SystemColor : VoiceColor);

	// The box is bottom aligned, so the new line pushes the others up by its height at once. Start everything that much
	// lower and slide it back up, which reads as the feed scrolling.
	Label->ForceLayoutPrepass();
	const float Height = FMath::Max(Label->GetDesiredSize().Y, 1.f);
	SlideOffset += Height;
	SlideSpeed = SlideOffset / SlideTime;

	FLine& Line = Lines.AddDefaulted_GetRef();
	Line.Text = Label;
	int32 Showing = 0;
	for (int32 Index = Lines.Num() - 1; Index >= 0; --Index)
	{
		if (Lines[Index].FadeLeft < 0.f && ++Showing > MaxLines)
		{
			StartFade(Lines[Index]);
		}
	}
	if (!Ticker.IsValid())
	{
		Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime) { return Tick(DeltaTime); }));
	}
	Tick(0.f);
}

void UknightHUDMessageLog::StartFade(FLine& Line)
{
	if (Line.FadeLeft < 0.f)
	{
		Line.FadeLeft = FadeTime;
	}
}

bool UknightHUDMessageLog::Tick(float DeltaTime)
{
	DeltaTime = FMath::Min(DeltaTime, 0.05f);
	SlideOffset = FMath::Max(0.f, SlideOffset - SlideSpeed * DeltaTime);
	for (int32 Index = Lines.Num() - 1; Index >= 0; --Index)
	{
		FLine& Line = Lines[Index];
		UCommonTextBlock* const Label = Line.Text.Get();
		if (!Label)
		{
			Lines.RemoveAt(Index);
			continue;
		}
		Line.Age += DeltaTime;
		if (Line.Age >= LineTime)
		{
			StartFade(Line);
		}
		float Opacity = FMath::Clamp(Line.Age / SlideTime, 0.f, 1.f);
		if (Line.FadeLeft >= 0.f)
		{
			Line.FadeLeft -= DeltaTime;
			if (Line.FadeLeft <= 0.f)
			{
				Label->RemoveFromParent();
				Lines.RemoveAt(Index);
				continue;
			}
			Opacity = FMath::Min(Opacity, Line.FadeLeft / FadeTime);
		}
		Label->SetRenderOpacity(Opacity);
		Label->SetRenderTranslation(FVector2D(0.f, SlideOffset));
	}
	if (Lines.IsEmpty())
	{
		SlideOffset = 0.f;
		Ticker.Reset();
		return false;
	}
	return true;
}
