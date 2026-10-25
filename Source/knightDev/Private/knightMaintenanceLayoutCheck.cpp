// Opt-in real UMG rendering / delegate checks, without OS input or combat.
#if WITH_EDITOR
#include "Menu/UI/KnightMainMenuWidget.h"
#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Menu/UI/KnightInventoryWidget.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Item/knightItemDefinitions.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
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
struct FMaintenanceLayoutCheck
{
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UKnightMainMenuWidget> Menu;
	TWeakObjectPtr<UKnightMaintenanceWidget> Screen;
	FString Directory;
	double Next = 0, Deadline = 0;
	int32 Step = 0, Failures = 0;
	FVector HomeCamera;
	void Check(bool Passed, const TCHAR* Label)
	{
		Failures += Passed ? 0 : 1;
		UE_LOG(LogTemp, Display, TEXT("MaintenanceLayout %s %s"), Passed ? TEXT("PASS") : TEXT("FAIL"), Label);
	}
	void Click(UUserWidget* Owner, const TCHAR* Name)
	{
		UCommonButtonBase* Button = Owner ? Cast<UCommonButtonBase>(Owner->GetWidgetFromName(Name)) : nullptr;
		Check(Button != nullptr, Name);
		if (Button)
			if (UFunction* Function = Button->FindFunction(TEXT("HandleButtonClicked"))) Button->ProcessEvent(Function, nullptr);
	}
	void Open(const TCHAR* Choice)
	{
		Menu->ShowPage(EKnightMainMenuPage::Maintenance);
		Check(Menu->GetCurrentPage() == EKnightMainMenuPage::Home
			&& Menu->GetWidgetFromName(TEXT("Panel_MaintenanceSubmenu"))->GetVisibility() == ESlateVisibility::Visible,
			TEXT("real viewport Home submenu is expanded"));
		Click(Menu.Get(), Choice);
		if (UCommonButtonBase* Action = Cast<UCommonButtonBase>(Menu->GetWidgetFromName(Choice)))
			Check(!Action->GetSelected(), TEXT("visited submenu action does not retain selection"));
		UWidgetSwitcher* Host = Cast<UWidgetSwitcher>(Menu->GetWidgetFromName(TEXT("Switcher_MaintenanceRuntime")));
		Screen = Host ? Cast<UKnightMaintenanceWidget>(Host->GetActiveWidget()) : nullptr;
		Check(Screen.IsValid(), TEXT("existing destination widget active"));
	}
	void Capture(const TCHAR* Name)
	{
		FVector2D Pixels; World->GetGameViewport()->GetViewportSize(Pixels);
		UE_LOG(LogTemp, Display, TEXT("MaintenanceLayout IMAGE %s viewport=%.0fx%.0f"), Name, Pixels.X, Pixels.Y);
		if (Screen.IsValid() && Menu->GetCurrentPage() == EKnightMainMenuPage::Maintenance)
		{
			const FGeometry Left = Screen->GetWidgetFromName(TEXT("Frame_Panel_Left"))->GetCachedGeometry();
			const FGeometry Right = Screen->GetWidgetFromName(TEXT("Frame_Panel_Right"))->GetCachedGeometry();
			const FVector2D L = Left.LocalToAbsolute(FVector2D(Left.GetLocalSize().X, 0));
			const FVector2D R = Right.LocalToAbsolute(FVector2D::ZeroVector);
			if (Screen->GetDestination() == EKnightMaintenanceDestination::Crafting)
			{
				Check(Left.GetLocalSize().X > 500 && Right.GetLocalSize().X > 500, TEXT("readable responsive Crafting side panels"));
				Check(R.X - L.X > Pixels.X * .35f, TEXT("Crafting leaves live robot center open"));
				Check(Screen->GetWidgetFromName(TEXT("MaintenanceBackdrop"))->GetVisibility() == ESlateVisibility::Collapsed,
					TEXT("uniform backdrop is absent from Crafting"));
				const UImage* const Shade = Cast<UImage>(Screen->GetWidgetFromName(TEXT("CraftingEdgeShade")));
				Check(Shade && Shade->GetVisibility() == ESlateVisibility::HitTestInvisible
					&& Shade->GetCachedGeometry().GetLocalSize().Equals(Screen->GetCachedGeometry().GetLocalSize(), 1.f),
					TEXT("single gradient covers the entire Maintenance viewport without taking input"));
				Check(Screen->GetWidgetFromName(TEXT("Panel_Confirm"))->GetVisibility() == ESlateVisibility::Collapsed,
					TEXT("inactive dialog dim does not overlap the gradient"));
				const UBorder* const LeftFrame = Cast<UBorder>(Screen->GetWidgetFromName(TEXT("Frame_Panel_Left")));
				const UBorder* const RightFrame = Cast<UBorder>(Screen->GetWidgetFromName(TEXT("Frame_Panel_Right")));
				Check(LeftFrame && RightFrame && LeftFrame->Background.DrawAs == ESlateBrushDrawType::NoDrawType
					&& RightFrame->Background.DrawAs == ESlateBrushDrawType::NoDrawType,
					TEXT("Crafting layout containers draw no rectangular frames"));
				const UUserWidget* const Back = Cast<UUserWidget>(Menu->GetWidgetFromName(TEXT("Button_Back")));
				const UWidget* const Footer = Back ? Back->GetWidgetFromName(TEXT("MaintenanceBackFooter")) : nullptr;
				const UWidget* const OldLabel = Back ? Back->GetWidgetFromName(TEXT("Txt_Label")) : nullptr;
				Check(Footer && Footer->IsVisible() && OldLabel && OldLabel->GetVisibility() == ESlateVisibility::Collapsed,
					TEXT("Maintenance Back uses compact footer content on existing bound button"));
			}
			else Check(R.X >= L.X && R.X - L.X < 30, TEXT("other destinations retain their layout"));
		}
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory, FString(Name) + TEXT(".png")), true, false);
	}
	bool Tick(float)
	{
		const double Now = FPlatformTime::Seconds();
		if (!World.IsValid() || Now > Deadline)
		{
			UE_LOG(LogTemp, Error, TEXT("MaintenanceLayout TIMEOUT step=%d"), Step);
			FPlatformMisc::RequestExit(false); return false;
		}
		if (Now < Next || FScreenshotRequest::IsScreenshotRequested()) return true;
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC) return true;
		// Changing test profiles is correctly rejected during a periodic async save.
		// Wait across frames instead of misreporting a fixture setup failure.
		if (Step >= 24)
			if (UknightSaveSubsystem* Saves = World->GetGameInstance()->GetSubsystem<UknightSaveSubsystem>())
				if (Saves->IsSaveInFlight()) return true;
		if (Step == 0)
		{
			TArray<UUserWidget*> Widgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(), Widgets, UKnightMainMenuWidget::StaticClass(), false);
			if (Widgets.IsEmpty()) return true;
			Menu = Cast<UKnightMainMenuWidget>(Widgets[0]);
			UknightSaveSubsystem* Saves = World->GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
			if (!Saves || !Saves->SelectSlot(TEXT("LayoutFixture"))) return true;
			UknightSaveGame* Save = Saves->GetSaveGame();
			Save->Money = 10000; Save->UnlockedSharedSocketSlots = 2;
			for (const TCHAR* Path : {
				TEXT("/Game/Knight/Data/Equipment/Janyeol/DA_Equipment_Janyeol_Weapon"),
				TEXT("/Game/Knight/Items/Materials/Janyeol/DA_Material_Janyeol_Hide"),
				TEXT("/Game/Knight/Items/Materials/Janyeol/DA_Material_Janyeol_ReinforcedBone"),
				TEXT("/Game/Knight/Items/Materials/Janyeol/DA_Material_Janyeol_Essence"),
				TEXT("/Game/Knight/Items/Sockets/DA_Socket_RepairExpansion"),
				TEXT("/Game/Knight/Items/Sockets/DA_Socket_AttackEfficiency") })
				if (const UknightItemDefinition* Item = LoadObject<UknightItemDefinition>(nullptr, Path))
					Save->ItemCounts.Add(Item->GetPrimaryAssetId(), Item->IsA<UknightMaterialItemDefinition>() ? 12 : 1);
			// Transient fixture only; production saves and content defaults are never modified.
			FRotator Rotation; PC->GetPlayerViewPoint(HomeCamera, Rotation);
			Menu->ShowPage(EKnightMainMenuPage::Maintenance);
			PC->ConsoleCommand(TEXT("r.SetRes 1920x1080w"), false);
		}
		else switch (Step)
		{
		case 1:
		{
			FVector Location; FRotator Rotation; PC->GetPlayerViewPoint(Location, Rotation);
			Check(Location.Equals(HomeCamera, 1.f), TEXT("submenu keeps Home camera"));
			Capture(TEXT("01_home_submenu")); break;
		}
		case 2:
		{
			Open(TEXT("MaintenanceChoiceCrafting"));
			UScrollBox* List = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("List_Items")));
			if (UCommonButtonBase* Row = List ? Cast<UCommonButtonBase>(List->GetChildAt(1)) : nullptr) Row->OnClicked().Broadcast();
			break;
		}
		case 3: Capture(TEXT("02_crafting")); break;
		case 4:
			Menu->RequestBack();
			Open(TEXT("MaintenanceChoiceEnhancement"));
			Click(Screen.Get(), TEXT("Phase7_EnhancementTarget")); break;
		case 5:
		{
			UScrollBox* List = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("RuntimeEquipmentPickerList")));
			if (UCommonButtonBase* Row = List ? Cast<UCommonButtonBase>(List->GetChildAt(0)) : nullptr) Row->OnClicked().Broadcast();
			Click(Screen.Get(), TEXT("Btn_ConfirmOk")); break;
		}
		case 6: Capture(TEXT("03_enhancement")); break;
		case 7:
			Menu->RequestBack(); Open(TEXT("MaintenanceChoiceSockets"));
			Click(Screen.Get(), TEXT("Phase8_SocketSlot0")); break;
		case 8: Capture(TEXT("04_sockets")); break;
		case 9: PC->ConsoleCommand(TEXT("r.SetRes 1280x720w"), false); break;
		case 10: Capture(TEXT("05_sockets_720")); break;
		case 11: Menu->RequestBack(); Open(TEXT("MaintenanceChoiceCrafting")); break;
		case 12: Capture(TEXT("06_crafting_720")); break;
		case 13: Menu->RequestBack(); Open(TEXT("MaintenanceChoiceEnhancement")); break;
		case 14: Capture(TEXT("07_enhancement_720")); break;
		case 15: Menu->RequestBack(); break;
		case 16: Capture(TEXT("08_home_submenu_720")); break;
		// Existing isolated delegate / data checks. These are not physical UI-input tests.
		case 17:
			// Keep the submenu visible until the asynchronous screenshot has completed.
			Menu->RequestBack();
			Open(TEXT("MaintenanceChoiceCrafting")); Click(Screen.Get(), TEXT("Btn_Weapon")); break;
		case 18: Capture(TEXT("09_equipment_720")); break;
		case 19:
		{
			Click(Screen.Get(), TEXT("Btn_Armor"));
			UScrollBox* List = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("List_Items")));
			if (UKnightInventoryEntryWidget* Row = List ? Cast<UKnightInventoryEntryWidget>(List->GetChildAt(0)) : nullptr)
				if (Row->GetButton()) Row->GetButton()->OnClicked.Broadcast();
			break;
		}
		case 20: Capture(TEXT("10_materials_720")); break;
		case 21:
			PC->ConsoleCommand(TEXT("r.SetRes 1920x1080w"), false);
			Click(Screen.Get(), TEXT("Btn_Craft")); break;
		case 22:
		{
			UScrollBox* List = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("List_Items")));
			if (UCommonButtonBase* Row = List ? Cast<UCommonButtonBase>(List->GetChildAt(1)) : nullptr)
				if (UFunction* ClickFunction = Row->FindFunction(TEXT("HandleButtonClicked"))) Row->ProcessEvent(ClickFunction, nullptr);
			break;
		}
		case 23: Capture(TEXT("11_crafting_1920_final")); break;
		case 24: PC->ConsoleCommand(TEXT("knight.MaintenanceTest IntegrationCases Navigation"), false); break;
		case 25: PC->ConsoleCommand(TEXT("knight.MaintenanceTest MaterialWidgetCases"), false); break;
		case 26: PC->ConsoleCommand(TEXT("knight.MaintenanceTest UpgradeCases Widget"), false); break;
		case 27: PC->ConsoleCommand(TEXT("knight.MaintenanceTest SocketCases Widget"), false); break;
		case 28: PC->ConsoleCommand(TEXT("knight.MaintenanceTest SocketCases Overflow"), false); break;
		case 29: PC->ConsoleCommand(TEXT("knight.MaintenanceTest IntegrationCases EquipmentEquip"), false); break;
		default:
			UE_LOG(LogTemp, Display, TEXT("MaintenanceLayout COMPLETE failures=%d; noninteractive render and delegate checks"), Failures);
			FPlatformMisc::RequestExit(false); return false;
		}
		++Step; Next = Now + 3.; return true;
	}
};
void StartMaintenanceLayoutCheck(const TArray<FString>& Args, UWorld* World)
{
	FString Namespace;
	if (!World || Args.Num() != 1 || !FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace)
		|| !Namespace.StartsWith(TEXT("MaintenanceLayout")))
	{
		UE_LOG(LogTemp, Error, TEXT("MaintenanceLayout requires output directory and isolated MaintenanceLayout save namespace.")); return;
	}
	TSharedRef<FMaintenanceLayoutCheck> Run = MakeShared<FMaintenanceLayoutCheck>();
	Run->World = World; Run->Directory = FPaths::ConvertRelativePathToFull(Args[0]);
	Run->Deadline = FPlatformTime::Seconds() + 180.;
	IFileManager::Get().MakeDirectory(*Run->Directory, true);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }));
}
FAutoConsoleCommandWithWorldAndArgs LayoutCommand(TEXT("knight.MaintenanceLayoutCheck"),
	TEXT("Isolated maintenance render/delegate checks. No desktop input."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartMaintenanceLayoutCheck), ECVF_Cheat);
}
#endif
