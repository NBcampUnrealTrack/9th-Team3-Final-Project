// Opt-in engine-render capture. No desktop input and no normal profile writes.
#if WITH_EDITOR
#include "Codex/KnightCodexWidget.h"
#include "Menu/UI/KnightMainMenuWidget.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "Quest/KnightQuestData.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
struct FCodexCapture : TSharedFromThis<FCodexCapture>
{
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UKnightMainMenuWidget> Menu;
	TWeakObjectPtr<UKnightCodexWidget> Codex;
	FString Directory;
	double Next = 0, Deadline = 0;
	int32 Step = 0, Failures = 0;
	TFunction<void()> AfterCapture;
	void Check(bool Passed, const TCHAR* What)
	{
		Failures += Passed ? 0 : 1;
		UE_LOG(LogTemp, Display, TEXT("CodexCapture %s %s"), Passed ? TEXT("PASS") : TEXT("FAIL"), What);
	}
	UWidget* Widget(const TCHAR* Name) const { return Codex.IsValid() ? Codex->GetWidgetFromName(Name) : nullptr; }
	FString Text(const TCHAR* Name) const
	{
		const UTextBlock* Label = Cast<UTextBlock>(Widget(Name));
		return Label ? Label->GetText().ToString() : FString();
	}
	FString DropText(const TCHAR* Name) const
	{
		FString Result;
		const UPanelWidget* Drops = Cast<UPanelWidget>(Widget(TEXT("Box_Drops")));
		if (!Drops) return Result;
		for (UWidget* Child : Drops->GetAllChildren())
		{
			const UUserWidget* Row = Cast<UUserWidget>(Child);
			const UTextBlock* Label = Row ? Cast<UTextBlock>(Row->GetWidgetFromName(Name)) : nullptr;
			if (Label) { Result += Label->GetText().ToString(); Result += TEXT("\n"); }
		}
		return Result;
	}
	bool HasGroupedDifficulties() const
	{
		const UPanelWidget* Drops = Cast<UPanelWidget>(Widget(TEXT("Box_Drops")));
		if (!Drops) return false;
		for (UWidget* Child : Drops->GetAllChildren())
		{
			const UUserWidget* Row = Cast<UUserWidget>(Child);
			const UTextBlock* Details = Row ? Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_Details"))) : nullptr;
			if (Details && Details->GetText().ToString().Contains(TEXT("보통 · 완료"))
				&& Details->GetText().ToString().Contains(TEXT("어려움 · 완료"))) return true;
		}
		return false;
	}
	void Click(const TCHAR* Name) { if (UButton* Button = Cast<UButton>(Widget(Name))) Button->OnClicked.Broadcast(); }
	void Entry(const TCHAR* ListName, int32 Index)
	{
		UPanelWidget* List = Cast<UPanelWidget>(Widget(ListName));
		UPanelWidget* Row = List ? Cast<UPanelWidget>(List->GetChildAt(Index)) : nullptr;
		UButton* Button = Row ? Cast<UButton>(Row->GetChildAt(0)) : nullptr;
		Check(Button != nullptr, TEXT("record button exists"));
		if (Button) Button->OnClicked.Broadcast();
	}
	void Capture(const TCHAR* Name)
	{
		Check(Menu.IsValid() && Menu->GetCurrentPage() == EKnightMainMenuPage::Codex,
			TEXT("capture is the visible Codex page"));
		if (FParse::Param(FCommandLine::Get(), TEXT("KnightSkipCodexCaptures"))) return;
		FVector2D Pixels; World->GetGameViewport()->GetViewportSize(Pixels);
		UE_LOG(LogTemp, Display, TEXT("CodexCapture IMAGE %s viewport=%.0fx%.0f title=%s"), Name, Pixels.X, Pixels.Y, *Text(TEXT("Text_WorldTitle")));
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory, FString(Name)+TEXT(".png")), true, false);
	}
	bool Tick(float)
	{
		const double Now = FPlatformTime::Seconds();
		if (!World.IsValid() || Now > Deadline)
		{
			UE_LOG(LogTemp, Error, TEXT("CodexCapture TIMEOUT step=%d"), Step);
			FPlatformMisc::RequestExit(false); return false;
		}
		if (Now < Next) return true;
		if (AfterCapture)
		{
			if (FScreenshotRequest::IsScreenshotRequested()) return true;
			AfterCapture(); AfterCapture = nullptr; Next = Now + 2.; return true;
		}
		if (Step == 0)
		{
			TArray<UUserWidget*> Widgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(), Widgets, UKnightMainMenuWidget::StaticClass(), false);
			if (Widgets.IsEmpty()) return true;
			Menu = Cast<UKnightMainMenuWidget>(Widgets[0]);
			UknightSaveSubsystem* Save = World->GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
			if (!Save || !Save->SelectFixedSlot(1)) return true;
			if (!Menu->ShowPage(EKnightMainMenuPage::Codex)) return true;
			Widgets.Reset();
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(), Widgets, UKnightCodexWidget::StaticClass(), false);
			if (Widgets.IsEmpty()) return true;
			Codex = Cast<UKnightCodexWidget>(Widgets[0]);
			World->GetFirstPlayerController()->ConsoleCommand(TEXT("r.SetRes 1920x1080w"), false);
			Check(Cast<UPanelWidget>(Widget(TEXT("Box_WorldEntries")))->GetChildrenCount() == 12, TEXT("12 reader records; source notes excluded"));
			Check(Cast<UPanelWidget>(Widget(TEXT("Box_MechEntries")))->GetChildrenCount() == 4, TEXT("4 mech records"));
			Next = Now + 6.; ++Step; return true;
		}
		switch (Step)
		{
		case 1: Capture(TEXT("01_world")); AfterCapture = [this] { Entry(TEXT("Box_WorldEntries"), 2); }; break;
		case 2:
			Check(Text(TEXT("Text_WorldTitle")) == TEXT("변화한 지구"), TEXT("entry click updates stationary title"));
			Capture(TEXT("02_world_record"));
			AfterCapture = [this] {
			Cast<UScrollBox>(Widget(TEXT("Scroll_WorldEntries")))->ScrollToEnd();
			Cast<UScrollBox>(Widget(TEXT("Scroll_World")))->ScrollToEnd(); }; break;
		case 3:
			UE_LOG(LogTemp, Display, TEXT("CodexCapture SCROLL list=%.1f/%.1f body=%.1f/%.1f"),
				Cast<UScrollBox>(Widget(TEXT("Scroll_WorldEntries")))->GetScrollOffset(), Cast<UScrollBox>(Widget(TEXT("Scroll_WorldEntries")))->GetScrollOffsetOfEnd(),
				Cast<UScrollBox>(Widget(TEXT("Scroll_World")))->GetScrollOffset(), Cast<UScrollBox>(Widget(TEXT("Scroll_World")))->GetScrollOffsetOfEnd());
			Check(Cast<UScrollBox>(Widget(TEXT("Scroll_WorldEntries")))->GetScrollOffset() > 0, TEXT("long entry list scrolls"));
			Check(FMath::IsNearlyEqual(Cast<UScrollBox>(Widget(TEXT("Scroll_World")))->GetScrollOffset(),
				Cast<UScrollBox>(Widget(TEXT("Scroll_World")))->GetScrollOffsetOfEnd(), 1.f), TEXT("reader reaches final paragraph"));
			Capture(TEXT("03_world_scroll")); AfterCapture = [this] { Click(TEXT("Button_Mech")); Entry(TEXT("Box_MechEntries"), 3); }; break;
		case 4:
			Check(Cast<UWidgetSwitcher>(Widget(TEXT("Switcher_Content")))->GetActiveWidgetIndex() == 1, TEXT("mech category selected"));
			Capture(TEXT("04_mech")); AfterCapture = [this] { Click(TEXT("Button_Monster")); }; break;
		case 5:
			Check(Text(TEXT("Text_BossDescription")) == TEXT("관련 퀘스트 완료 시 공개됩니다."), TEXT("locked details concealed"));
			Check(Cast<UPanelWidget>(Widget(TEXT("Box_Drops")))->GetChildrenCount() == 0, TEXT("locked drops concealed"));
			Capture(TEXT("05_monster_locked")); break;
		case 6:
			Check(World->GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>()->RecordQuestCompleted(TEXT("Quest_Janyeol_Normal")), TEXT("isolated quest completion")); break;
		case 7:
			Check(Text(TEXT("Text_BossDescription")) != TEXT("관련 퀘스트 완료 시 공개됩니다."), TEXT("open codex refreshes on progression event"));
			Check(Cast<UPanelWidget>(Widget(TEXT("Box_Drops")))->GetChildrenCount() == 8,
				TEXT("Janyeol quest drops display while raid rewards are empty"));
			Check(HasGroupedDifficulties() && DropText(TEXT("Text_Details")).Contains(TEXT("부위 파괴: Head")),
				TEXT("repeated Janyeol items retain difficulty and part sources"));
			Capture(TEXT("06_monster_unlocked"));
			AfterCapture = [this] { Cast<UScrollBox>(Widget(TEXT("Scroll_BossDetail")))->ScrollToEnd(); }; break;
		case 8:
			Capture(TEXT("07_monster_drops"));
			AfterCapture = [this] {
			Codex->SelectBoss(TEXT("Janhyang"));
			Check(Text(TEXT("Text_BossDescription")) == TEXT("관련 퀘스트 완료 시 공개됩니다."), TEXT("only completed boss unlocks"));
			Codex->SelectBoss(TEXT("Janyeol"));
			World->GetFirstPlayerController()->ConsoleCommand(TEXT("r.SetRes 1280x720w"), false); }; break;
		case 9: Capture(TEXT("08_monster_720")); AfterCapture = [this] { Click(TEXT("Button_World")); Entry(TEXT("Box_WorldEntries"), 2); }; break;
		case 10: Capture(TEXT("09_world_720")); AfterCapture = [this] { Click(TEXT("Button_Mech")); Entry(TEXT("Box_MechEntries"), 3); }; break;
		case 11:
			Capture(TEXT("10_mech_720"));
			AfterCapture = [this] {
			Check(Menu->RequestBack(), TEXT("shared Back handler accepted"));
			Check(Menu->GetCurrentPage() == EKnightMainMenuPage::Home, TEXT("Back restores menu home")); }; break;
		case 12:
			Check(Menu->ShowPage(EKnightMainMenuPage::Codex), TEXT("codex reopens"));
			Check(Cast<UWidgetSwitcher>(Widget(TEXT("Switcher_Content")))->GetActiveWidgetIndex() == 0, TEXT("reentry starts world"));
			Click(TEXT("Button_Close"));
			Check(Menu->GetCurrentPage() == EKnightMainMenuPage::Home, TEXT("close button delegate restores home")); break;
		case 13:
		{
			Check(Menu->SelectQuest(TEXT("Quest_Janhyang_Hard")), TEXT("different quest selected on stage page"));
			// Wait for the existing operation-map entrance before opening another page.
			AfterCapture = [this] {
			Check(Menu->ShowPage(EKnightMainMenuPage::Codex), TEXT("Codex returns after quest navigation settles"));
			Codex->SelectCategory(2); Codex->SelectBoss(TEXT("Janyeol"));
			const FString JanyeolNames = DropText(TEXT("Text_Name"));
			Check(Cast<UPanelWidget>(Widget(TEXT("Box_Drops")))->GetChildrenCount() == 8,
				TEXT("selected Janhyang quest does not replace Janyeol drops"));
			Check(World->GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>()->RecordQuestCompleted(TEXT("Quest_0002")),
				TEXT("Janhyang Codex unlocked in isolated profile"));
			Codex->SelectBoss(TEXT("Janhyang"));
			Check(Cast<UPanelWidget>(Widget(TEXT("Box_Drops")))->GetChildrenCount() == 8
				&& DropText(TEXT("Text_Name")) != JanyeolNames,
				TEXT("Janhyang normal and hard quest drops display independently"));
			Check(HasGroupedDifficulties() && DropText(TEXT("Text_Details")).Contains(TEXT("부위 파괴: Chest"))
				&& DropText(TEXT("Text_Details")).Contains(TEXT("부위 파괴: Tail")),
				TEXT("Janhyang repeated items retain chest/tail and difficulty sources"));
			Cast<UScrollBox>(Widget(TEXT("Scroll_BossDetail")))->ScrollToEnd();
			};
			break;
		}
		case 14: Capture(TEXT("11_janhyang_quest_drops_720")); break;
		case 15:
			World->GetFirstPlayerController()->ConsoleCommand(TEXT("r.SetRes 2560x1080w"), false);
			Cast<UScrollBox>(Widget(TEXT("Scroll_BossDetail")))->ScrollToStart(); break;
		case 16: Capture(TEXT("12_monster_ultrawide")); AfterCapture = [this] { Click(TEXT("Button_World")); }; break;
		case 17: Capture(TEXT("13_world_ultrawide")); AfterCapture = [this] { Click(TEXT("Button_Mech")); }; break;
		case 18: Capture(TEXT("14_mech_ultrawide")); break;
		case 19: Codex->RequestClose(); break;
		default:
			UE_LOG(LogTemp, Display, TEXT("CodexCapture COMPLETE failures=%d; delegate/API checks, not physical input"), Failures);
			FPlatformMisc::RequestExit(false); return false;
		}
		++Step; Next = Now + 2.; return true;
	}
};

void StartCodexCapture(const TArray<FString>& Args, UWorld* World)
{
	FString Namespace;
	if (!World || Args.Num() != 1 || !FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace)
		|| !Namespace.StartsWith(TEXT("CodexArchive")))
	{
		UE_LOG(LogTemp, Error, TEXT("CodexCapture requires output directory and an isolated CodexArchive save namespace.")); return;
	}
	TSharedRef<FCodexCapture> Run = MakeShared<FCodexCapture>();
	Run->World = World; Run->Directory = FPaths::ConvertRelativePathToFull(Args[0]);
	Run->Deadline = FPlatformTime::Seconds()+120.;
	IFileManager::Get().MakeDirectory(*Run->Directory, true);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }));
}
FAutoConsoleCommandWithWorldAndArgs CodexCaptureCommand(TEXT("knight.CodexCapture"),
	TEXT("Engine-only archive captures/checks; requires isolated CodexArchive namespace. No desktop input."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartCodexCapture), ECVF_Cheat);
}
#endif
