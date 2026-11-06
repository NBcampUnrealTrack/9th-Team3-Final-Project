#include "knightMaintenanceEntranceCheckCommandlet.h"
#include "Menu/UI/KnightMainMenuWidget.h"
#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Menu/UI/KnightButtonFillExtension.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Item/knightEquipmentData.h"
#include "Progression/knightProgressionSubsystem.h"
#include "System/knightGameplayTags.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Input/HittestGrid.h"
#include "Slate/WidgetRenderer.h"
#include "ShaderCompiler.h"
#include "TextureCompiler.h"
#include "UObject/UObjectIterator.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "RenderingThread.h"

int32 UknightMaintenanceEntranceCheckCommandlet::CheckExit(APlayerController* Player, const FString& Out)
{
	int32 Checks = 0, Failures = 0;
	auto Check = [&](bool Pass, const TCHAR* What)
	{
		++Checks; if (!Pass) ++Failures;
		UE_LOG(LogTemp, Display, TEXT("MaintenanceExit %s %s"), Pass ? TEXT("PASS") : TEXT("FAIL"), What);
	};
	auto* Progression = Player->GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>();
	const int64 InitialMoney = Progression->GetMoney();
	TArray<FknightItemStack> InitialItems; Progression->GetOwnedItems(InitialItems);
	TArray<FknightItemStack> Weapons; Progression->GetOwnedWeapons(Weapons);
	const UknightPartItemDefinition* Weapon = Weapons.IsEmpty() ? nullptr : Cast<UknightPartItemDefinition>(Weapons[0].Item);
	if (!Weapon) return 1;
	const auto Class = LoadClass<UKnightMainMenuWidget>(nullptr, TEXT("/Game/Knight/UI/Menu/WBP_KnightMainMenu.WBP_KnightMainMenu_C"));
	UKnightMainMenuWidget* Menu = Class ? CreateWidget<UKnightMainMenuWidget>(Player, Class) : nullptr;
	if (!Menu) return 1;
	Menu->AddToRoot(); Menu->ShowMaintenanceSelection(); Menu->SetMaintenanceSubmenuOpen(false);
	UKnightMaintenanceWidget* Widget = Menu->MaintenanceWidget;
	if (!Widget) return 1;
	FWidgetRenderer* Renderer = new FWidgetRenderer(true);
	TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow);
	Window->SetContent(Menu->WidgetTree->RootWidget->TakeWidget());
	FSlateApplication::Get().RegisterVirtualWindow(Window);
	FHittestGrid Grid; UTextureRenderTarget2D* Target = nullptr;
	FIntPoint Size; float Scale = 1.f;
	auto Draw = [&](float Dt = 0.f)
	{
		++GFrameCounter;
		// The harness renders the real tree without the unrelated flow subsystem.
		// Drive the parent's production tick before its canvas paints, as in-game.
		Menu->NativeTick(Menu->GetCachedGeometry(), Dt);
		Renderer->DrawWindow(Target, Grid, Window, Scale, FVector2D(Size), Dt); FlushRenderingCommands();
	};
	auto SetSize = [&](FIntPoint NewSize, float Multiplier = 1.f)
	{
		if (Target) Target->RemoveFromRoot(); Size = NewSize;
		Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size) * Multiplier;
		Window->Resize(FVector2D(Size)); Target = FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, true); Target->AddToRoot(); Draw(); Draw();
	};
	auto ReadPixels = [&]()
	{
		TArray<FColor> Image; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
		Check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Image, Flags), TEXT("actual Slate/RHI pixel readback")); return Image;
	};
	auto Capture = [&](FString Name)
	{
		const auto Pixels = ReadPixels(); TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		Check(FFileHelper::SaveArrayToFile(Png, *(Out / (Name + TEXT(".png")))), TEXT("rendered frame saved"));
	};
	auto Rect = [](UWidget* W)
	{
		const FGeometry& G = W->GetCachedGeometry();
		return FBox2D(G.LocalToAbsolute(FVector2D::ZeroVector), G.LocalToAbsolute(G.GetLocalSize()));
	};
	auto Click = [&](UCommonButtonBase* Button)
	{
		const FVector2D Center = Rect(Button).GetCenter();
		auto Bubble = Grid.GetBubblePath(Center, 0.f, false); FWidgetPath Path(Bubble);
		const FPointerEvent Move(0, Center, Center - FVector2D(1,0), TSet<FKey>(), EKeys::Invalid, 0.f, FModifierKeysState());
		for (const FWidgetAndPointer& Node : Bubble) Node.Widget->OnMouseEnter(Node.Geometry, Move);
		Check(Button->IsHovered(), TEXT("actual Slate pointer enters target button"));
		const FPointerEvent Down(0, Center, Center, TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
		const FPointerEvent Up(0, Center, Center, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
		FSlateApplication::Get().RoutePointerDownEvent(Path, Down); FSlateApplication::Get().RoutePointerUpEvent(Path, Up);
		for (const FWidgetAndPointer& Node : Bubble) Node.Widget->OnMouseLeave(Move);
	};
	auto Key = [&](FKey K, bool Repeat = false) { return FKeyEvent(K, FModifierKeysState(), 0, Repeat, 0, 0); };
	auto Enter = [&](EKnightMaintenanceDestination Destination)
	{
		Menu->ShowMaintenanceSelection(); Menu->OpenMaintenanceDestination(Destination);
		FTextureCompilingManager::Get().FinishAllCompilation(); GShaderCompilingManager->FinishAllCompilation();
		for (TObjectIterator<UTexture2D> It; It; ++It)
			if (It->GetPathName().StartsWith(TEXT("/Game/Knight/"))) { It->SetForceMipLevelsToBeResident(60.f); It->WaitForStreaming(); }
		Draw(); Draw(); Draw(); Draw(.4f); Draw();
		Check(!Widget->IsTransitionActive(), TEXT("existing side entrance still finishes"));
	};
	SetSize(FIntPoint(1920,1080));
	const TArray<UWidget*> HomeChildren = Menu->Panel_Home->GetAllChildren();
	TArray<FWidgetTransform> HomeTransforms; for (UWidget* W : HomeChildren) HomeTransforms.Add(W->GetRenderTransform());
	Check(FMath::IsNearlyEqual(Widget->EntranceSettings.ExitDuration, .28f) && FMath::IsNearlyEqual(Widget->EntranceSettings.HomeRevealDuration, .2f), TEXT("actual asset exit=0.28 and home=0.20 seconds"));
	const TArray<EKnightMaintenanceDestination> Destinations = { EKnightMaintenanceDestination::Crafting, EKnightMaintenanceDestination::Enhancement, EKnightMaintenanceDestination::Sockets };
	for (int32 Mode = 0; Mode < 3; ++Mode)
	{
		Enter(Destinations[Mode]);
		if (Mode == 0) { Widget->SetCategory(2, false); Widget->SelectEquipmentSlot(knightGameplayTags::Equipment_Slot_Weapon); }
		if (Mode == 1) Widget->OpenEnhancementWeapon(Weapon->GetPrimaryAssetId());
		Draw(); Draw();
		const FBox2D Row = Mode == 0 ? Rect(Widget->EquipmentSlotButtons[0]) : FBox2D(ForceInit);
		const FBox2D Action = Mode == 0 ? Rect(Widget->EquipmentReplaceButton) : FBox2D(ForceInit);
		const FWidgetTransform WeaponTransform = Widget->EnhancementWeaponImage->GetRenderTransform();
		const FString Prefix = FString::Printf(TEXT("Exit%d"), Mode);
		Capture(Prefix + TEXT("_00"));
		if (Mode == 0) Click(Menu->Button_Back);
		else Menu->NativeOnPreviewKeyDown(FGeometry(), Key(EKeys::Escape));
		Check(Widget->IsTransitionExiting(), TEXT("on-screen Back / Esc starts true main-menu return"));
		Check(Menu->GetCurrentPage() == EKnightMainMenuPage::Maintenance, TEXT("outgoing page stays alive until exit/reveal finish"));
		const auto ExitOffset0 = Widget->EntranceTransition->GetPanelOffset();
		float PreviousOpacity = Widget->GetRenderOpacity(); FVector2D PreviousOffset = ExitOffset0;
		TArray<double> Steps;
		for (int32 Frame = 1; Frame <= 24; ++Frame)
		{
			if (Frame == 3) { Menu->HandleBackRequested(); Menu->NativeOnPreviewKeyDown(FGeometry(), Key(EKeys::Enter)); Menu->NativeOnPreviewKeyDown(FGeometry(), Key(EKeys::Escape, true)); }
			Draw(1.f / 60.f); Capture(Prefix + FString::Printf(TEXT("_%02d"), Frame));
			if (Frame <= 16)
			{
				const FVector2D Offset = Widget->EntranceTransition->GetPanelOffset();
				Check(Offset.X <= PreviousOffset.X && Offset.Y >= PreviousOffset.Y, TEXT("panels travel monotonically outward"));
				Steps.Add(FMath::Abs(Offset.X - PreviousOffset.X)); PreviousOffset = Offset;
				Check(Widget->GetRenderOpacity() <= PreviousOpacity + .001f, TEXT("opacity decreases from current state without flash"));
				PreviousOpacity = Widget->GetRenderOpacity();
				Check(Widget->GetVisibility() == ESlateVisibility::HitTestInvisible, TEXT("outgoing maintenance controls cannot receive clicks"));
				if (Mode == 0 && Frame < 10)
				{
					const auto MovedRow = Rect(Widget->EquipmentSlotButtons[0]), MovedAction = Rect(Widget->EquipmentReplaceButton);
					Check((MovedAction.Min - MovedRow.Min).Equals(Action.Min - Row.Min, 2.f)
						&& MovedRow.GetSize().Equals(Row.GetSize(), 1.f), TEXT("external actions travel with full-width row without scale/layout change"));
				}
				if (Mode == 1) Check(Widget->EnhancementWeaponImage->GetRenderTransform() == WeaponTransform, TEXT("central weapon authored angle position scale unchanged"));
			}
			if (Frame == 12)
			{
				UWidget* Overlay = Menu->Button_Maintenance->GetParent();
				while (Overlay && Overlay->GetParent() != Menu->WidgetTree->RootWidget) Overlay = Overlay->GetParent();
				Check(Overlay && Overlay->GetRenderOpacity() > 0.f && Overlay->GetRenderOpacity() < 1.f
					&& Widget->GetRenderOpacity() > 0.f && Widget->GetRenderOpacity() < .15f, TEXT("main menu fades in while outgoing text is already faint"));
				Check(Overlay && Overlay->GetVisibility() == ESlateVisibility::HitTestInvisible, TEXT("revealing home content cannot receive early input"));
			}
			if (Frame == 17) Check(Widget->GetRenderOpacity() == 0.f && Widget->IsTransitionExiting(), TEXT("maintenance is fully transparent and alive before cleanup"));
		}
		Check(Steps.Num() > 14 && Steps[0] < Steps[5] && Steps[14] < Steps[8], TEXT("EaseInOutCubic gently accelerates and decelerates"));
		Check(Menu->GetCurrentPage() == EKnightMainMenuPage::Home && !Widget->IsTransitionActive(), TEXT("one completed return reaches main menu"));
		Check(!Menu->MaintenanceSubmenu->IsVisible() && !Menu->Button_Back->IsVisible(), TEXT("no automatic submenu reopen or orphan footer"));
		Check(Menu->Button_Maintenance->HasAnyUserFocus() || Menu->Button_Maintenance->HasFocusedDescendants(), TEXT("focus restored to maintenance entry"));
		for (int32 I = 0; I < HomeChildren.Num(); ++I)
			Check(HomeChildren[I]->GetParent() == Menu->Panel_Home && HomeChildren[I]->GetRenderTransform() == HomeTransforms[I], TEXT("original home elements and transforms restored"));
		Check(!Widget->GetWidgetFromName(TEXT("MaintenanceScanLeft"))->IsVisible() && Widget->EntranceTransition->GetRetainerCount() == 0, TEXT("no residual orange layer or temporary render targets"));
		// Record the restored focus fill through its settled state as well as the
		// handoff frame; this catches late flashes rather than freezing mid-fill.
		for (int32 Frame = 25; Frame <= 42; ++Frame)
		{
			Draw(1.f / 60.f); Capture(Prefix + FString::Printf(TEXT("_%02d"), Frame));
		}
		const TArray<FColor> FinalPixels = ReadPixels();
		Check(!FinalPixels.ContainsByPredicate([](const FColor& P) { return int32(P.R) > int32(P.G) + 12 && int32(P.G) > int32(P.B) + 12; }), TEXT("settled rendered main menu contains no amber remnants"));
		Menu->NativeOnPreviewKeyDown(FGeometry(), Key(EKeys::Enter, true));
		Menu->NativeOnPreviewKeyDown(FGeometry(), Key(EKeys::Escape, true));
		Check(Menu->GetCurrentPage() == EKnightMainMenuPage::Home && !Menu->MaintenanceSubmenu->IsVisible(), TEXT("held return/confirm input cannot activate destination"));
		Menu->NativeOnKeyUp(FGeometry(), Key(EKeys::Enter)); Menu->NativeOnKeyUp(FGeometry(), Key(EKeys::Escape));
		Draw(); Click(Menu->Button_Maintenance); Draw();
		Check(Menu->MaintenanceSubmenu->IsVisible(), TEXT("real Slate click works after cleanup"));
	}
	// Nested returns are decided before any exit or camera-return request.
	Enter(EKnightMaintenanceDestination::Crafting);
	Widget->OpenCraftResult(Weapon);
	Menu->HandleBackRequested();
	Check(!Widget->IsCraftResultOpen() && !Widget->IsTransitionActive() && Menu->GetCurrentPage() == EKnightMainMenuPage::Maintenance, TEXT("craft result Back closes only dialog; ownership preserved"));
	Widget->SetCategory(2, false); Widget->SelectEquipmentSlot(knightGameplayTags::Equipment_Slot_Weapon); Draw(); Draw();
	Widget->OpenEquipmentPicker(); Menu->HandleBackRequested();
	Check(!Widget->bEquipmentPickerOpen && Widget->SelectedEquipmentSlot == knightGameplayTags::Equipment_Slot_Weapon
		&& !Widget->IsTransitionActive(), TEXT("replacement cancel returns to same equipment slot"));
	Widget->RequestNearbyWeaponEnhancement();
	Check(Widget->GetDestination() == EKnightMaintenanceDestination::Enhancement && Menu->bReturnToWeaponEquipment, TEXT("existing equipped weapon shortcut route used"));
	Menu->HandleBackRequested(); Draw(); Draw();
	Check(Widget->GetDestination() == EKnightMaintenanceDestination::Crafting && Widget->SelectedEquipmentSlot == knightGameplayTags::Equipment_Slot_Weapon
		&& Widget->EquipmentEnhanceButton->IsVisible() && !Widget->IsTransitionActive(), TEXT("shortcut Back restores selected weapon equipment without exit"));
	for (const FIntPoint Resolution : { FIntPoint(1280,720), FIntPoint(2560,1440) })
	{
		Menu->ShowMaintenanceSelection(); SetSize(Resolution, Resolution.X == 1280 ? 1.25f : 1.f);
		Menu->OpenMaintenanceDestination(EKnightMaintenanceDestination::Crafting); Draw(); Draw(); Draw(); Draw(.14f);
		const FVector2D Before = Widget->EntranceTransition->GetPanelOffset(); const float BeforeAlpha = Widget->GetRenderOpacity();
		Menu->NativeOnPreviewKeyDown(FGeometry(), Key(EKeys::Escape));
		Check(Widget->EntranceTransition->GetPanelOffset() == Before && Widget->GetRenderOpacity() == BeforeAlpha, TEXT("Back during entry preserves current position and opacity"));
		if (Resolution.X == 1280) Capture(TEXT("Interrupted_00"));
		for (int32 F = 0; F < 24; ++F)
		{
			Draw(1.f / 60.f);
			if (Resolution.X == 1280) Capture(FString::Printf(TEXT("Interrupted_%02d"), F + 1));
			if (F == 7)
			{
				UWidget* Overlay = Menu->Button_Maintenance->GetParent();
				while (Overlay && Overlay->GetParent() != Menu->WidgetTree->RootWidget) Overlay = Overlay->GetParent();
				Check(Overlay && Overlay->GetRenderOpacity() > 0.f, TEXT("interrupted entry reveals Home early without a blank UI interval"));
			}
		}
		Check(Menu->GetCurrentPage() == EKnightMainMenuPage::Home && !Widget->IsTransitionActive(), TEXT("interrupted entry completes exit at alternate DPI"));
		Capture(FString::Printf(TEXT("Interrupted_%d_Final"), Resolution.X));
		Menu->NativeOnKeyUp(FGeometry(), Key(EKeys::Escape));
		Enter(EKnightMaintenanceDestination::Crafting);
		Check(Widget->GetRenderOpacity() == 1.f, TEXT("reentry restores original entrance and opacity"));
	}
	// Early interruption before the geometry preparation frame must never flash full UI.
	Menu->ShowMaintenanceSelection(); Menu->OpenMaintenanceDestination(EKnightMaintenanceDestination::Crafting);
	Menu->HandleBackRequested(); Draw(.10f);
	Check(Widget->GetRenderOpacity() == 0.f, TEXT("pre-layout interruption stays invisible")); Draw(.30f); Draw();
	Check(Menu->GetCurrentPage() == EKnightMainMenuPage::Home, TEXT("pre-layout interruption restores main menu"));
	Check(Progression->GetMoney() == InitialMoney, TEXT("exit and nested navigation consume no currency"));
	for (const FknightItemStack& Item : InitialItems) Check(Progression->GetItemCount(Item.Item) == Item.Count, TEXT("owned item quantities unchanged"));
	Check(Menu->Button_Maintenance->GetExtension<UKnightButtonFillExtension>()->IsEnabled(), TEXT("existing main-menu fill remains enabled"));
	Menu->ShowPage(EKnightMainMenuPage::Home); Menu->RemoveFromRoot();
	FSlateApplication::Get().UnregisterVirtualWindow(Window); BeginCleanup(Renderer); FlushRenderingCommands(); Target->RemoveFromRoot();
	UE_LOG(LogTemp, Display, TEXT("MaintenanceExit COMPLETE checks=%d failures=%d output=%s"), Checks, Failures, *Out);
	return Failures ? 1 : 0;
}
