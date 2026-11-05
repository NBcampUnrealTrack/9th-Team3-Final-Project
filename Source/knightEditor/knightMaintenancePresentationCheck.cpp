#include "knightEquipmentActionCheckCommandlet.h"
#include "Menu/UI/KnightMainMenuWidget.h"
#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Menu/UI/KnightButtonFillExtension.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "Item/knightEquipmentData.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/HittestGrid.h"
#include "Layout/WidgetPath.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "UObject/UnrealType.h"

int32 UknightEquipmentActionCheckCommandlet::CheckPresentation(APlayerController* Player, const FString& Out)
{
	int32 Failures = 0;
	auto Check = [&Failures](bool bPass, const TCHAR* What)
	{
		if (!bPass) ++Failures;
		UE_LOG(LogTemp, Display, TEXT("MaintenancePresentation %s %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), What);
	};
	auto* Saves = Player->GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	auto* Progression = Player->GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	const auto Class = LoadClass<UKnightMainMenuWidget>(nullptr, TEXT("/Game/Knight/UI/Menu/WBP_KnightMainMenu.WBP_KnightMainMenu_C"));
	UKnightMainMenuWidget* Menu = Class ? CreateWidget<UKnightMainMenuWidget>(Player, Class) : nullptr;
	if (!Menu || !Saves || !Progression) return 1;
	Menu->AddToRoot(); Menu->ShowMaintenanceSelection();
	if (!Menu->MaintenanceSubmenu || !Menu->MaintenanceWidget) return 1;
	FWidgetRenderer* Renderer = new FWidgetRenderer(true);
	TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow);
	FSlateApplication::Get().RegisterVirtualWindow(Window);
	FHittestGrid Grid;
	UTextureRenderTarget2D* Target = nullptr;
	FIntPoint Size; float Scale = 1.f;
	auto Draw = [&]() { Renderer->DrawWindow(Target, Grid, Window, Scale, FVector2D(Size), 1.f / 60.f); FlushRenderingCommands(); };
	auto SetView = [&](UWidget* View, FIntPoint InSize, float InScale)
	{
		if (Target) Target->RemoveFromRoot();
		Size = InSize; Scale = InScale;
		Window->SetContent(View->TakeWidget()); Window->Resize(FVector2D(Size));
		Target = FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, true); Target->AddToRoot();
		Draw(); Draw();
	};
	auto Capture = [&](const FString& Name)
	{
		GShaderCompilingManager->FinishAllCompilation(); Draw(); Draw();
		TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
		Check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags), TEXT("real rendered capture"));
		TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		Check(FFileHelper::SaveArrayToFile(Png, *(Out / (Name + TEXT(".png")))), TEXT("capture written"));
	};
	SetView(Menu->MaintenanceSubmenu, FIntPoint(520, 424), 2.f);
	const TArray<UCommonButtonBase*> Choices = { Menu->MaintenanceCraftingButton, Menu->MaintenanceEnhancementButton, Menu->MaintenanceSocketsButton };
	const TArray<EKnightMaintenanceDestination> Destinations = { EKnightMaintenanceDestination::Crafting, EKnightMaintenanceDestination::Enhancement, EKnightMaintenanceDestination::Sockets };
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		UCommonButtonBase* Button = Choices[Index];
		if (!Button) { Check(false, TEXT("submenu choice exists")); continue; }
		auto CheckFrame = [&]()
		{
			Draw(); Draw();
			const UWidget* Outline = Button->GetWidgetFromName(TEXT("MaintenanceFocusOutline"));
			Check(Outline && !Outline->IsVisible(), TEXT("submenu inset focus layer hidden"));
			FSlateBrush Brush; Button->GetStyleCDO()->GetNormalBaseBrush(Brush);
			Check(Brush.DrawAs != ESlateBrushDrawType::NoDrawType && Brush.TintColor.GetSpecifiedColor().A > 0.f,
				TEXT("authored outer-frame/background brush retained"));
			Check(Button->GetIsFocusable() && Button->GetIsEnabled(), TEXT("submenu remains focusable and enabled"));
			const auto* Fill = Button->GetExtension<UKnightButtonFillExtension>();
			Check(!Fill || !Fill->IsEnabled(), TEXT("submenu remains outside animated fill scope"));
		};
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
		CheckFrame(); Capture(FString::Printf(TEXT("Submenu%d_Normal"), Index));
		const auto& Geometry = Button->GetCachedGeometry();
		const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f);
		const FPointerEvent Move(0, Center, Center - FVector2D(1,0), TSet<FKey>(), EKeys::Invalid, 0.f, FModifierKeysState());
		auto HoverPath = Grid.GetBubblePath(Center, 0.f, false);
		for (const FWidgetAndPointer& Node : HoverPath) Node.Widget->OnMouseEnter(Node.Geometry, Move);
		Check(Button->IsHovered(), TEXT("actual Slate hover state"));
		CheckFrame(); Capture(FString::Printf(TEXT("Submenu%d_Hovered"), Index));
		FWidgetPath Path(HoverPath);
		const FPointerEvent Down(0, Center, Center, TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
		FSlateApplication::Get().RoutePointerDownEvent(Path, Down);
		Check(Button->IsPressed(), TEXT("actual Slate pressed state"));
		CheckFrame(); Capture(FString::Printf(TEXT("Submenu%d_Pressed"), Index));
		const FPointerEvent Up(0, Center, Center, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
		FSlateApplication::Get().RoutePointerUpEvent(Path, Up);
		Check(Menu->GetCurrentPage() == EKnightMainMenuPage::Maintenance && Menu->MaintenanceWidget->GetDestination() == Destinations[Index], TEXT("click opens original destination"));
		Menu->ShowMaintenanceSelection(); Draw(); Draw();
		for (const FWidgetAndPointer& Node : HoverPath) Node.Widget->OnMouseLeave(Move);
		CheckFrame(); Capture(FString::Printf(TEXT("Submenu%d_PostClick"), Index));
		Button->SetFocus(); Draw(); Draw();
		Check(Button->HasAnyUserFocus() || Button->HasFocusedDescendants(), TEXT("actual keyboard focus retained"));
		CheckFrame(); Capture(FString::Printf(TEXT("Submenu%d_Focused"), Index));
		const auto* Label = Cast<UTextBlock>(Button->GetWidgetFromName(TEXT("Txt_Label")));
		Check(Label && Label->GetColorAndOpacity().GetSpecifiedColor().R > .8f, TEXT("focus remains visible through text brightness"));
	}
	// The main-menu entry still owns its original fill extension.
	Check(Menu->Button_Maintenance->GetExtension<UKnightButtonFillExtension>()
		&& Menu->Button_Maintenance->GetExtension<UKnightButtonFillExtension>()->IsEnabled(), TEXT("main-menu fill remains enabled"));
	UKnightMaintenanceWidget* Widget = Menu->MaintenanceWidget;
	Menu->OpenMaintenanceDestination(EKnightMaintenanceDestination::Enhancement);
	SetView(Widget, FIntPoint(1920,1080), 1.f);
	// Full-screen entry now intentionally blocks transactions until its final layout.
	// Exercise the original upgrade path only once the player can activate it.
	for (int32 Frame = 0; Frame < 60 && Widget->IsTransitionActive(); ++Frame) Draw();
	Check(!Widget->IsTransitionActive(), TEXT("entrance completes before upgrade transaction check"));
	auto CheckLabel = [&]()
	{
		const auto* Label = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("EnhancementChanceLabel")));
		Check(Label && Label->GetText().ToString() == TEXT("성공률"), TEXT("runtime localized label is exactly 성공률"));
		const FByteProperty* Justification = FindFProperty<FByteProperty>(UTextBlock::StaticClass(), TEXT("Justification"));
		Check(Label && Justification && Justification->GetPropertyValue_InContainer(Label) == ETextJustify::Center && Label->GetFont().Size == 17,
			TEXT("original label alignment and typography retained"));
	};
	TArray<FknightItemStack> InitiallyOwned;
	Progression->GetOwnedWeapons(InitiallyOwned);
	if (!InitiallyOwned.IsEmpty() && InitiallyOwned[0].Item)
		Widget->SelectEnhancementWeapon(InitiallyOwned[0].Item->GetPrimaryAssetId());
	CheckLabel();
	const auto* Catalog = UknightEquipmentSettings::LoadCatalog();
	const UknightWeaponItemDefinition* Weapon = nullptr;
	for (const UknightCraftRecipeDefinition* Recipe : Catalog->Recipes)
		if (Recipe && Recipe->Result && Recipe->GetName().Contains(TEXT("Janyeol")) && Recipe->Result->IsA<UknightWeaponItemDefinition>())
		{ Weapon = Cast<UknightWeaponItemDefinition>(Recipe->Result); break; }
	if (Weapon)
	{
		Check(!InitiallyOwned.IsEmpty() && InitiallyOwned[0].Item != Weapon, TEXT("weapon-switch fixture starts from a different selected weapon"));
		Progression->AddItem(Weapon, 1); Widget->SelectEnhancementWeapon(Weapon->GetPrimaryAssetId()); CheckLabel();
		FKnightWeaponUpgradeQuote Quote; Progression->GetWeaponUpgradeQuote(Weapon, Quote);
		Progression->AddMoney(Quote.MoneyCost);
		if (Quote.GateMaterial && Quote.GateMaterialNeeded > Quote.GateMaterialOwned)
			Progression->AddItem(Quote.GateMaterial, Quote.GateMaterialNeeded - Quote.GateMaterialOwned);
		const double Deadline = FPlatformTime::Seconds() + 15.;
		while (!Saves->CanStartAtomicTransaction() && FPlatformTime::Seconds() < Deadline)
		{ FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread); FTSTicker::GetCoreTicker().Tick(.01f); FPlatformProcess::Sleep(.01f); }
		Capture(TEXT("Enhancement_ChangedWeapon"));
		Progression->SetDebugNextWeaponUpgradeOutcome(1);
		Widget->RequestWeaponUpgrade();
		Check(Progression->GetUpgradeLevel(Weapon) == Quote.CurrentLevel + 1, TEXT("one existing enhancement transaction completed"));
		const double SaveDeadline = FPlatformTime::Seconds() + 15.;
		while (!Saves->CanStartAtomicTransaction() && FPlatformTime::Seconds() < SaveDeadline)
		{ FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread); FTSTicker::GetCoreTicker().Tick(.01f); FPlatformProcess::Sleep(.01f); }
		for (int32 Frame = 0; Frame < 240; ++Frame) Widget->TickEnhancementBars(1.f / 60.f);
		Widget->RefreshEnhancementDetails(); CheckLabel();
		Check(!Widget->IsEnhancementNavigationBlocked(), TEXT("label checked after presentation cleanup"));
		Capture(TEXT("Enhancement_AfterResult"));
	}
	else Check(false, TEXT("real weapon fixture exists"));
	Widget->Close(); Menu->RemoveFromRoot();
	FSlateApplication::Get().UnregisterVirtualWindow(Window);
	BeginCleanup(Renderer); FlushRenderingCommands(); Target->RemoveFromRoot();
	UE_LOG(LogTemp, Display, TEXT("MaintenancePresentation COMPLETE failures=%d output=%s"), Failures, *Out);
	return Failures ? 1 : 0;
}
