#include "knightMaintenanceEntranceCheckCommandlet.h"
#include "Menu/UI/KnightMainMenuWidget.h"
#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Menu/UI/KnightMaintenancePresentation.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonGameViewportClient.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "GameFramework/PlayerController.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "System/knightGameplayTags.h"
#include "knightItemDefinition.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "ShaderCompiler.h"
#include "Engine/Texture2D.h"
#include "TextureCompiler.h"
#include "UObject/UObjectIterator.h"
#include "Slate/WidgetRenderer.h"
#include "Input/HittestGrid.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "RenderingThread.h"

int32 UknightMaintenanceEntranceCheckCommandlet::Main(const FString& Params)
{
	FString Namespace, Out, UserDir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace) || Namespace.IsEmpty() || Namespace.Len() > 32
		|| !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || UserDir.IsEmpty()
		|| !FParse::Value(*Params, TEXT("Out="), Out) || !FParse::Param(FCommandLine::Get(), TEXT("AllowCommandletRendering"))) return 1;
	IFileManager::Get().MakeDirectory(*Out, true);
	if (!FSlateApplication::IsInitialized())
	{
		FSlateApplication::Create();
		FSlateApplication::Get().InitializeRenderer(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
	}
	int32 Failures = 0, Checks = 0;
	auto Check = [&](bool Pass, const TCHAR* What)
	{
		++Checks; if (!Pass) ++Failures;
		UE_LOG(LogTemp, Display, TEXT("MaintenanceEntrance %s %s"), Pass ? TEXT("PASS") : TEXT("FAIL"), What);
	};
	UGameInstance* GI = NewObject<UGameInstance>(GEngine); GI->AddToRoot(); GI->InitializeStandalone();
	UWorld* World = GI->GetWorld();
	TGuardValue<TSubclassOf<UGameViewportClient>> ViewportClass(GEngine->GameViewportClientClass, UCommonGameViewportClient::StaticClass());
	UGameViewportClient* Viewport = NewObject<UCommonGameViewportClient>(GEngine);
	GI->GetWorldContext()->GameViewport = Viewport; Viewport->Init(*GI->GetWorldContext(), GI, false);
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine); GI->AddLocalPlayer(LocalPlayer, FPlatformUserId::CreateFromInternalId(0));
	APlayerController* Player = World->SpawnActor<APlayerController>(); World->AddController(Player); Player->SetPlayer(LocalPlayer);
	auto* Saves = GI->GetSubsystem<UknightSaveSubsystem>(); auto* Progression = GI->GetSubsystem<UknightProgressionSubsystem>();
	if (!Saves || !Progression || !Saves->SelectSlot(TEXT("Entrance") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12))) return 1;
	const int64 InitialMoney = Progression->GetMoney();
	TArray<FknightItemStack> InitialItems; Progression->GetOwnedItems(InitialItems);
	const auto Class = LoadClass<UKnightMainMenuWidget>(nullptr, TEXT("/Game/Knight/UI/Menu/WBP_KnightMainMenu.WBP_KnightMainMenu_C"));
	UKnightMainMenuWidget* Menu = Class ? CreateWidget<UKnightMainMenuWidget>(Player, Class) : nullptr;
	if (!Menu) return 1;
	Menu->AddToRoot(); Menu->ShowMaintenanceSelection();
	UKnightMaintenanceWidget* Widget = Menu->MaintenanceWidget;
	if (!Widget || !Widget->EntranceRevealMaterial || !Widget->EntranceScanMaterial) return 1;
	FWidgetRenderer* Renderer = new FWidgetRenderer(true);
	TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow);
	// Render the real menu tree without constructing an unrelated game-flow fixture.
	Window->SetContent(Menu->WidgetTree->RootWidget->TakeWidget());
	FSlateApplication::Get().RegisterVirtualWindow(Window);
	FHittestGrid Grid; UTextureRenderTarget2D* Target = nullptr;
	FIntPoint Size; float Scale = 1.f;
	auto Draw = [&](float Dt = 0.f)
	{
		++GFrameCounter;
		Renderer->DrawWindow(Target, Grid, Window, Scale, FVector2D(Size), Dt); FlushRenderingCommands();
	};
	auto SetSize = [&](FIntPoint NewSize, float ExtraScale = 1.f)
	{
		if (Target) Target->RemoveFromRoot(); Size = NewSize;
		Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size) * ExtraScale;
		Window->Resize(FVector2D(Size)); Target = FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, true); Target->AddToRoot();
		Draw(); Draw();
	};
	auto Pixels = [&]()
	{
		TArray<FColor> Result; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
		Check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Result, Flags), TEXT("real Slate/RHI pixels read")); return Result;
	};
	auto Capture = [&](const FString& Name)
	{
		const TArray<FColor> Image = Pixels(); TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Image, Png);
		Check(FFileHelper::SaveArrayToFile(Png, *(Out / (Name + TEXT(".png")))), TEXT("render capture saved"));
	};
	auto Rect = [](UWidget* Item)
	{
		const FGeometry& G = Item->GetCachedGeometry();
		return FBox2D(G.LocalToAbsolute(FVector2D::ZeroVector), G.LocalToAbsolute(G.GetLocalSize()));
	};
	SetSize(FIntPoint(1920,1080)); GShaderCompilingManager->FinishAllCompilation(); Draw(); Draw();
	Check(!Widget->IsTransitionActive() && Menu->GetCurrentPage() == EKnightMainMenuPage::Home, TEXT("small submenu has no full-screen transition"));
	const TArray<EKnightMaintenanceDestination> Destinations = { EKnightMaintenanceDestination::Crafting, EKnightMaintenanceDestination::Enhancement, EKnightMaintenanceDestination::Sockets };
	TArray<FknightItemStack> OwnedWeapons; Progression->GetOwnedWeapons(OwnedWeapons);
	const FPrimaryAssetId PreviewWeapon = OwnedWeapons.IsEmpty() ? FPrimaryAssetId() : OwnedWeapons[0].Item->GetPrimaryAssetId();
	for (int32 Index = 0; Index < Destinations.Num(); ++Index)
	{
		Menu->OpenMaintenanceDestination(Destinations[Index]);
		if (Index == 1) Check(Widget->OpenEnhancementWeapon(PreviewWeapon), TEXT("fixture selects real owned weapon without starting enhancement"));
		FTextureCompilingManager::Get().FinishAllCompilation();
		for (TObjectIterator<UTexture2D> It; It; ++It)
			if (It->GetPathName().StartsWith(TEXT("/Game/Knight/"))) { It->SetForceMipLevelsToBeResident(60.f); It->WaitForStreaming(); }
		Check(Widget->IsTransitionActive() && Widget->GetRenderOpacity() == 0.f, TEXT("full destination starts hidden pending arranged geometry"));
		// Preparation only; all subsequent frames advance through the production NativeTick.
		Draw(); Draw(); Draw();
		Check(Widget->EntranceTransition->IsMoving() && Widget->EntranceTransition->GetRetainerCount() >= 4, TEXT("real visible panel roots wrapped for shared movement and reveal"));
		const FVector2D Start = Widget->EntranceTransition->GetPanelOffset();
		Check(Start.X < -200.f && Start.Y > 200.f, TEXT("opposite side-entry offsets exceed small nudge"));
		UImage* WeaponImage = Widget->EnhancementWeaponImage;
		const FWidgetTransform WeaponTransform = WeaponImage ? WeaponImage->GetRenderTransform() : FWidgetTransform();
		const FString Prefix = FString::Printf(TEXT("Destination%d"), Index);
		Capture(Prefix + TEXT("_00"));
		FVector2D Previous = Start; double PreviousStep = TNumericLimits<double>::Max();
		TArray<FColor> CompletionPixels;
		for (int32 Frame = 1; Frame <= 24; ++Frame)
		{
			Draw(1.f / 60.f);
			const FVector2D Offset = Widget->EntranceTransition->GetPanelOffset();
			const double Step = FMath::Abs(Offset.X - Previous.X);
			Check(Offset.X >= Previous.X - .01 && Offset.Y <= Previous.Y + .01, TEXT("both sides move monotonically inward"));
			Check(Step <= PreviousStep + .02, TEXT("entrance decelerates without overshoot"));
			Previous = Offset; PreviousStep = Step;
			if (Frame <= 23) Capture(Prefix + FString::Printf(TEXT("_%02d"), Frame));
			if (Frame == 23) CompletionPixels = Pixels();
			if (WeaponImage && Index == 1) Check(WeaponImage->GetRenderTransform() == WeaponTransform, TEXT("weapon authored transform untouched throughout entry"));
		}
		Draw(); Draw(); Capture(Prefix + TEXT("_24"));
		Check(!Widget->IsTransitionActive() && Widget->EntranceTransition->GetRetainerCount() == 0, TEXT("completed transition removes retainers and stops animation updates"));
		Check(!Widget->GetWidgetFromName(TEXT("MaintenanceScanLeft"))->IsVisible()
			&& !Widget->GetWidgetFromName(TEXT("MaintenanceScanRight"))->IsVisible(), TEXT("core halo and trails absent at rest"));
		Check(Widget->EnhancementBarPhase == UKnightMaintenanceWidget::EEnhancementBarPhase::Idle, TEXT("entrance never starts enhancement particles"));
		const TArray<FColor> After = Pixels();
		int32 CompletionDifference = 0;
		if (After.Num() == CompletionPixels.Num()) for (int32 P = 0; P < After.Num(); ++P)
			if (FMath::Abs(int32(After[P].R) - CompletionPixels[P].R) > 2 || FMath::Abs(int32(After[P].G) - CompletionPixels[P].G) > 2
				|| FMath::Abs(int32(After[P].B) - CompletionPixels[P].B) > 2) ++CompletionDifference;
		UE_LOG(LogTemp, Display, TEXT("MaintenanceEntrance destination=%d completion/settled changed pixels=%d"), Index, CompletionDifference);
		Check(After.Num() == CompletionPixels.Num() && CompletionDifference == 0, TEXT("completion frame including footer has no missing-art or text flash"));
		// Reopen the same destination internally: baseline layout without entrance.
		Menu->OpenMaintenanceDestination(Destinations[Index]);
		if (Index == 1) Widget->OpenEnhancementWeapon(PreviewWeapon);
		Draw(); Draw();
		Check(!Widget->IsTransitionActive(), TEXT("internal destination change does not replay entrance"));
		const TArray<FColor> Baseline = Pixels();
		int32 Different = 0;
		if (After.Num() == Baseline.Num()) for (int32 P = 0; P < After.Num(); ++P)
			if (FMath::Abs(int32(After[P].R) - Baseline[P].R) > 2 || FMath::Abs(int32(After[P].G) - Baseline[P].G) > 2 || FMath::Abs(int32(After[P].B) - Baseline[P].B) > 2) ++Different;
		UE_LOG(LogTemp, Display, TEXT("MaintenanceEntrance destination=%d final/baseline changed pixels=%d"), Index, Different);
		Check(After.Num() == Baseline.Num() && Different < After.Num() / 100, TEXT("final rendered appearance matches no-transition baseline within one percent focus tolerance"));
		Menu->HandleBackRequested(); Check(Widget->IsTransitionExiting(), TEXT("Back starts short return fade"));
		Draw(.06f); Capture(Prefix + TEXT("_ExitHalf")); Draw(.07f); Draw();
		Check(Menu->GetCurrentPage() == EKnightMainMenuPage::Home && !Widget->IsTransitionActive(), TEXT("return finishes on original submenu with input restored"));
		Check(!Menu->Button_Back->IsVisible(), TEXT("maintenance footer stays hidden after returning Home"));
	}
	// Geometry and input at a different DPI, including the external equipment action group.
	for (const FIntPoint Resolution : { FIntPoint(1280,720), FIntPoint(2560,1440) })
	{
		Menu->OpenMaintenanceDestination(EKnightMaintenanceDestination::Crafting); Widget->CancelTransition();
		Widget->SetCategory(2, false); Widget->SelectEquipmentSlot(knightGameplayTags::Equipment_Slot_Weapon);
		SetSize(Resolution, Resolution.X == 1280 ? 1.25f : 1.f); Draw(); Draw();
		const FBox2D RowBefore = Rect(Widget->EquipmentSlotButtons[0]);
		const FBox2D ActionBefore = Rect(Widget->EquipmentReplaceButton);
		const FBox2D EnhanceBefore = Rect(Widget->EquipmentEnhanceButton);
		int32 ObsoleteCallback = 0;
		Widget->BeginEntrance(Menu->Button_Back, FSimpleDelegate::CreateLambda([&]() { ++ObsoleteCallback; }));
		Draw(); Draw(); Draw(); Draw(.18f); Draw();
		const FBox2D Row = Rect(Widget->EquipmentSlotButtons[0]), Action = Rect(Widget->EquipmentReplaceButton);
		Capture(FString::Printf(TEXT("Equipment_%d_Moving"), Resolution.X));
		Check(FMath::IsNearlyEqual((Action.Min - Row.Min).X, (ActionBefore.Min - RowBefore.Min).X, 2.f)
			&& FMath::IsNearlyEqual(Action.GetCenter().Y, Row.GetCenter().Y, 2.f), TEXT("external replace follows selected full-width row at alternate DPI"));
		Check(FMath::IsNearlyEqual(Row.GetSize().X, RowBefore.GetSize().X, 2.f), TEXT("transition never reserves internal action column or resizes row"));
		const auto Hits = Grid.GetBubblePath(Action.GetCenter(), 0.f, false);
		Check(!Hits.ContainsByPredicate([&](const FWidgetAndPointer& Entry) { return Entry.Widget == Widget->EquipmentReplaceButton->GetCachedWidget(); }), TEXT("incoming action cannot receive unintended click"));
		Check(Widget->HandleConfirmKey(), TEXT("keyboard confirmation consumed during entry"));
		Menu->NativeOnPreviewKeyDown(FGeometry(), FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
		Draw(.13f); Draw();
		Check(ObsoleteCallback == 0 && Menu->GetCurrentPage() == EKnightMainMenuPage::Home && !Widget->IsTransitionActive(), TEXT("Esc cancels entrance callback and cleanly returns without obsolete refocus"));
		Check(!Menu->Button_Back->IsVisible(), TEXT("interrupted return does not resurrect maintenance footer"));
		Menu->OpenMaintenanceDestination(EKnightMaintenanceDestination::Crafting); Draw(); Draw(); Draw(); Draw(.4f); Draw();
		Check(!Widget->IsTransitionActive() && Widget->GetRenderOpacity() == 1.f, TEXT("immediate reentry has no stale opacity or input lock"));
		Widget->SetCategory(2, false); Widget->SelectEquipmentSlot(knightGameplayTags::Equipment_Slot_Weapon); Draw(); Draw();
		Capture(FString::Printf(TEXT("Equipment_%d_Final"), Resolution.X));
		Menu->ShowMaintenanceSelection(); Draw(); Draw();
	}
	SetSize(FIntPoint(1920,1080));
	// Equal elapsed time at three frame rates uses the same production curve.
	TArray<FVector2D> Offsets;
	for (int32 FPS : { 30, 60, 120 })
	{
		Menu->OpenMaintenanceDestination(EKnightMaintenanceDestination::Crafting); Draw(); Draw(); Draw();
		for (int32 F = 0; F < FPS / 5; ++F) Draw(1.f / FPS);
		Offsets.Add(Widget->EntranceTransition->GetPanelOffset());
		Menu->ShowMaintenanceSelection(); Draw(); Draw();
	}
	Check(Offsets[0].Equals(Offsets[1], .1f) && Offsets[1].Equals(Offsets[2], .1f), TEXT("30/60/120 FPS same position at 0.20 seconds"));
	Check(Progression->GetMoney() == InitialMoney, TEXT("navigation and blocked confirm spend no money"));
	TArray<FknightItemStack> FinalItems; Progression->GetOwnedItems(FinalItems);
	Check(FinalItems.Num() == InitialItems.Num(), TEXT("navigation creates no inventory entries"));
	for (const FknightItemStack& Item : InitialItems) Check(Progression->GetItemCount(Item.Item) == Item.Count, TEXT("navigation leaves owned quantities unchanged"));
	Menu->ShowMaintenanceSelection(); Menu->RemoveFromRoot();
	FSlateApplication::Get().UnregisterVirtualWindow(Window); BeginCleanup(Renderer); FlushRenderingCommands(); if (Target) Target->RemoveFromRoot();
	GI->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GI->RemoveFromRoot();
	UE_LOG(LogTemp, Display, TEXT("MaintenanceEntrance COMPLETE checks=%d failures=%d output=%s"), Checks, Failures, *Out);
	return Failures ? 1 : 0;
}
