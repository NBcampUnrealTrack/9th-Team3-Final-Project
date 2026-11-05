#include "knightEquipmentActionCheckCommandlet.h"
#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Menu/UI/KnightMaintenanceEntryWidget.h"
#include "Menu/UI/KnightMaintenancePresentation.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "CommonGameViewportClient.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Item/knightEquipmentData.h"
#include "Item/knightEquipmentStats.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveSubsystem.h"
#include "Progression/knightSaveGame.h"
#include "System/knightGameplayTags.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "RenderingThread.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBoxSlot.h"
#include "Input/HittestGrid.h"
#include "Components/PanelWidget.h"
#include "Blueprint/WidgetNavigation.h"
#include "Engine/UserInterfaceSettings.h"

int32 UknightEquipmentActionCheckCommandlet::Main(const FString& Params)
{
	FString Namespace, Out, UserDir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace) || Namespace.IsEmpty()
		|| !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || UserDir.IsEmpty()
		|| !FParse::Value(*Params, TEXT("Out="), Out)
		|| !FParse::Param(FCommandLine::Get(), TEXT("AllowCommandletRendering"))) return 1;
	IFileManager::Get().MakeDirectory(*Out, true);
	if (!FSlateApplication::IsInitialized())
	{
		FSlateApplication::Create();
		FSlateApplication::Get().InitializeRenderer(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
	}
	int32 Failures = 0;
	auto Check = [&Failures](bool bPass, const TCHAR* What)
	{
		if (!bPass) ++Failures;
		UE_LOG(LogTemp, Display, TEXT("EquipmentAction %s %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), What);
	};
	UGameInstance* GI = NewObject<UGameInstance>(GEngine); GI->AddToRoot(); GI->InitializeStandalone();
	UWorld* World = GI->GetWorld();
	// Match this fixture's actual viewport; do not alter the project's engine configuration.
	TGuardValue<TSubclassOf<UGameViewportClient>> ViewportClass(GEngine->GameViewportClientClass, UCommonGameViewportClient::StaticClass());
	UGameViewportClient* Viewport = NewObject<UCommonGameViewportClient>(GEngine);
	GI->GetWorldContext()->GameViewport = Viewport;
	Viewport->Init(*GI->GetWorldContext(), GI, false); // No OS window or interactive viewport.
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	GI->AddLocalPlayer(LocalPlayer, FPlatformUserId::CreateFromInternalId(0));
	APlayerController* Player = World->SpawnActor<APlayerController>();
	World->AddController(Player); Player->SetPlayer(LocalPlayer);
	auto* Saves = GI->GetSubsystem<UknightSaveSubsystem>();
	auto* Progression = GI->GetSubsystem<UknightProgressionSubsystem>();
	if (!Saves || !Progression) return 1;
	auto DrainSave = [&]()
	{
		const double Deadline = FPlatformTime::Seconds() + 15.;
		while (!Saves->CanStartAtomicTransaction() && FPlatformTime::Seconds() < Deadline)
		{
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			FTSTicker::GetCoreTicker().Tick(.01f); FPlatformProcess::Sleep(.01f);
		}
		Check(Saves->CanStartAtomicTransaction(), TEXT("normal async save completed"));
	};
	const FString RunId = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12);
	Check(Saves->SelectSlot(TEXT("EquipmentAction") + RunId), TEXT("fresh isolated profile"));
	const auto* Catalog = UknightEquipmentSettings::LoadCatalog();
	if (!Catalog) return 1;
	const TArray<FGameplayTag> Tags = { knightGameplayTags::Equipment_Slot_Weapon, knightGameplayTags::Equipment_Slot_Head,
		knightGameplayTags::Equipment_Slot_Torso, knightGameplayTags::Equipment_Slot_Arms, knightGameplayTags::Equipment_Slot_Legs };
	TArray<const UknightPartItemDefinition*> Candidates;
	const auto InitialLoadout = knightEquipmentStats::ReadLoadout(Progression, Catalog);
	for (const FGameplayTag Tag : Tags)
	{
		const auto* Recipe = Catalog->Recipes.FindByPredicate([&](const auto& Entry)
		{
			return Entry && Entry->Result && Entry->Result->SlotTag == Tag
				&& Entry->GetName().Contains(TEXT("Janyeol")) && Entry->Result != InitialLoadout.FindRef(Tag);
		});
		if (!Recipe) return 1;
		Candidates.Add((*Recipe)->Result);
		Progression->AddItem((*Recipe)->Result, 1);
		// Give every initially empty armor slot a different real owned item so
		// replacement tests verify old-item retention, not only first-time equipping.
		if (!InitialLoadout.FindRef(Tag))
		{
			const auto* PreviousRecipe = Catalog->Recipes.FindByPredicate([&](const auto& Entry)
			{
				return Entry && Entry->Result && Entry->Result->SlotTag == Tag
					&& Entry->GetName().Contains(TEXT("Janhyang")) && Entry->Result != (*Recipe)->Result;
			});
			if (!PreviousRecipe) return 1;
			Progression->AddItem((*PreviousRecipe)->Result, 1);
			Check(Progression->SetEquippedPart((*PreviousRecipe)->Result), TEXT("seed distinct owned armor for replacement fixture"));
		}
	}
	DrainSave();
	UKnightMaintenanceWidget* Widget = CreateWidget<UKnightMaintenanceWidget>(Player, UKnightMaintenanceWidget::GetSharedWidgetClass());
	if (!Widget) return 1;
	Widget->AddToRoot(); Widget->Open(EKnightMaintenanceDestination::Crafting); Widget->SetCategory(2, false);
	Check(Widget->EquipmentSlotButtons.Num() == 5 && Widget->List_Items->GetChildrenCount() == 5, TEXT("five original full-width rows without action columns"));
	if (Widget->EquipmentSlotButtons.Num() != 5) return 1;
	FWidgetRenderer* Renderer = new FWidgetRenderer(true);
	const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	const TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow);
	Window->SetContent(SlateWidget);
	FHittestGrid HitTestGrid;
	UTextureRenderTarget2D* Target = nullptr;
	FIntPoint Size; float Scale = 1.f;
	auto Draw = [&]()
	{
		Renderer->DrawWindow(Target, HitTestGrid, Window, Scale, FVector2D(Size), 0.f); FlushRenderingCommands();
	};
	auto SetRenderSize = [&](FIntPoint InSize, float ScaleMultiplier)
	{
		if (Target) Target->RemoveFromRoot();
		Size = InSize; Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size) * ScaleMultiplier;
		Window->Resize(FVector2D(Size));
		Target = FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, true); Target->AddToRoot(); Draw(); Draw();
		UE_LOG(LogTemp, Display, TEXT("EquipmentAction RENDER %dx%d scale=%f"), Size.X, Size.Y, Scale);
	};
	auto Capture = [&](const FString& Name)
	{
		GShaderCompilingManager->FinishAllCompilation(); Draw(); Draw();
		TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
		Check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags), TEXT("real rendered capture"));
		TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		Check(FFileHelper::SaveArrayToFile(Png, *(Out / (Name + TEXT(".png")))), TEXT("capture written"));
	};
	auto Rect = [](UWidget* Item)
	{
		const FGeometry& G = Item->GetCachedGeometry();
		const FVector2D A = G.LocalToAbsolute(FVector2D::ZeroVector), B = G.LocalToAbsolute(G.GetLocalSize());
		return FSlateRect(A.X, A.Y, B.X, B.Y);
	};
	auto HitsAction = [&](FVector2D Point)
	{
		return HitTestGrid.GetBubblePath(Point, 0.f, false).ContainsByPredicate([&](const FWidgetAndPointer& Entry)
		{
			return Entry.Widget == Widget->EquipmentReplaceButton->GetCachedWidget();
		});
	};
	auto HitsEnhance = [&](FVector2D Point)
	{
		return HitTestGrid.GetBubblePath(Point, 0.f, false).ContainsByPredicate([&](const FWidgetAndPointer& Entry)
		{
			return Entry.Widget == Widget->EquipmentEnhanceButton->GetCachedWidget();
		});
	};
	auto Center = [](FSlateRect Bounds) { return FVector2D((Bounds.Left + Bounds.Right) * .5f, (Bounds.Top + Bounds.Bottom) * .5f); };
	auto ValidateLayout = [&](int32 Selected)
	{
		Draw(); Draw();
		const FSlateRect ButtonRect = Rect(Widget->EquipmentReplaceButton), SlotRect = Rect(Widget->EquipmentSlotButtons[Selected]);
		const FSlateRect ListRect = Rect(Widget->List_Items);
		const FSlateRect SurfaceRect = Rect(Widget->GetWidgetFromName(TEXT("CraftingSurfaceLeft")));
		const float PanelRail = SurfaceRect.Right - (SurfaceRect.Right - SurfaceRect.Left) * KnightMaintenancePresentation::PanelArtworkInset;
		Check(FMath::IsNearlyEqual(ButtonRect.Left - PanelRail, 12.f * Scale, 1.f), TEXT("action outside entire artwork frame with consistent DPI-scaled gap"));
		Check(FMath::IsNearlyEqual(Center(ButtonRect).Y, Center(SlotRect).Y, 1.f), TEXT("action vertically centered on selected row"));
		Check(ButtonRect.Right < Rect(Widget->GetWidgetFromName(TEXT("Frame_Panel_Right"))).Left
			&& ButtonRect.Top >= 0 && ButtonRect.Bottom <= Size.Y, TEXT("action inside central preview and viewport"));
		Check(ButtonRect.Top >= SlotRect.Top - 1 && ButtonRect.Bottom <= SlotRect.Bottom + 1, TEXT("action stays in selected row without vertical overlap"));
		Check(Widget->EquipmentReplaceButton->GetParent() == Widget->WidgetTree->RootWidget
			&& Widget->EquipmentReplaceButton->IsVisible(), TEXT("single action is a root canvas sibling outside list clipping"));
		Check(CastChecked<UCanvasPanelSlot>(Widget->EquipmentReplaceButton->Slot)->GetZOrder()
			< CastChecked<UCanvasPanelSlot>(Widget->Panel_Confirm->Slot)->GetZOrder(), TEXT("existing modal is above action"));
		Check(HitsAction(Center(ButtonRect)), TEXT("rendered external button has matching hit-test area"));
		Check(!HitsAction(FVector2D(ButtonRect.Left - 3.f * Scale, Center(ButtonRect).Y))
			&& !HitsAction(FVector2D(ButtonRect.Right + 3.f * Scale, Center(ButtonRect).Y))
			&& !HitsAction(FVector2D(Center(ButtonRect).X, ButtonRect.Top - 3.f * Scale))
			&& !HitsAction(FVector2D(Center(ButtonRect).X, ButtonRect.Bottom + 3.f * Scale)), TEXT("action does not intercept surrounding preview or gap"));
		for (UCommonButtonBase* Row : Widget->EquipmentSlotButtons)
		{
			const FSlateRect Bounds = Rect(Row);
			Check(Row->GetParent() == Widget->List_Items && Cast<UScrollBoxSlot>(Row->Slot)
				&& FMath::IsNearlyEqual(Bounds.Left, ListRect.Left, 1.f)
				&& FMath::IsNearlyEqual(Bounds.Right, ListRect.Right - 10.f * Scale, 1.f), TEXT("every selected/unselected row fills original list width and padding"));
		}
		Check(Widget->EquipmentSlotButtons[Selected]->Navigation->Right.Widget == Widget->EquipmentReplaceButton
			&& Widget->EquipmentReplaceButton->Navigation->Left.Widget == Widget->EquipmentSlotButtons[Selected], TEXT("explicit slot/action navigation in both directions"));
		Check(Selected == 0 && Widget->EquipmentEnhanceWeaponId.IsValid()
			? Widget->EquipmentReplaceButton->Navigation->Next.Widget == Widget->EquipmentEnhanceButton
			: Selected + 1 < Tags.Num()
			? Widget->EquipmentReplaceButton->Navigation->Next.Widget == Widget->EquipmentSlotButtons[Selected + 1]
			: Widget->EquipmentReplaceButton->Navigation->Next.Rule == EUINavigationRule::Escape,
			TEXT("Tab advances beyond action without trapping final row"));
		const bool bEnhanceExpected = Selected == 0
			&& knightEquipmentStats::ReadLoadout(Progression, Catalog).FindRef(Tags[0]) != nullptr;
		Check(Widget->EquipmentEnhanceButton->IsVisible() == bEnhanceExpected, TEXT("enhancement only for populated selected weapon"));
		if (bEnhanceExpected)
		{
			const FSlateRect EnhanceRect = Rect(Widget->EquipmentEnhanceButton);
			Check(FMath::IsNearlyEqual(EnhanceRect.Left, ButtonRect.Left, 1.f)
				&& FMath::IsNearlyEqual(EnhanceRect.Right, ButtonRect.Right, 1.f)
				&& FMath::IsNearlyEqual(EnhanceRect.Top - ButtonRect.Bottom, 8.f * Scale, 1.f),
				TEXT("enhance directly beneath replace with matched width and scaled gap"));
			Check(EnhanceRect.Left > PanelRail && EnhanceRect.Right < Rect(Widget->GetWidgetFromName(TEXT("Frame_Panel_Right"))).Left
				&& EnhanceRect.Bottom < Size.Y && HitsEnhance(Center(EnhanceRect)),
				TEXT("external enhance is unclipped and hit-testable"));
			Check(!HitsEnhance(FVector2D(EnhanceRect.Left - 3.f * Scale, Center(EnhanceRect).Y)),
				TEXT("enhance does not intercept surrounding preview"));
			Check(Widget->EquipmentReplaceButton->Navigation->Down.Widget == Widget->EquipmentEnhanceButton
				&& Widget->EquipmentEnhanceButton->Navigation->Up.Widget == Widget->EquipmentReplaceButton
				&& Widget->EquipmentEnhanceButton->Navigation->Left.Widget == Widget->EquipmentSlotButtons[0],
				TEXT("keyboard/controller path connects weapon row and both actions"));
		}
		for (int32 Other = 0; Other < Tags.Num(); ++Other)
			if (Other != Selected)
			{
				const FSlateRect OtherRect = Rect(Widget->EquipmentSlotButtons[Other]);
				Check(ButtonRect.Bottom <= OtherRect.Top || ButtonRect.Top >= OtherRect.Bottom, TEXT("no neighboring row covered"));
			}
	};
	for (const FIntPoint Resolution : { FIntPoint(1280,720), FIntPoint(1920,1080), FIntPoint(2560,1440), FIntPoint(3440,1440) })
	{
		SetRenderSize(Resolution, 1.f);
		TArray<FSlateRect> Before; for (UCommonButtonBase* Button : Widget->EquipmentSlotButtons) Before.Add(Rect(Button));
		for (int32 Index = 0; Index < Tags.Num(); ++Index)
		{
			if (Widget->SelectedEquipmentSlot == Tags[Index]) Widget->SelectEquipmentSlot(Tags[Index]);
			Widget->EquipmentSlotButtons[Index]->OnClicked().Broadcast();
			Check(Widget->EquipmentReplaceTarget == Tags[Index] && Widget->SelectedEquipmentSlot == Tags[Index], TEXT("selection retargets one contextual action"));
			Widget->EquipmentSlotButtons[Index]->OnUnhovered().Broadcast(); Widget->EquipmentReplaceButton->OnHovered().Broadcast();
			Check(Widget->EquipmentReplaceButton->IsVisible() && Widget->SelectedEquipmentSlot == Tags[Index], TEXT("slot-to-action hover events do not dismiss or clear selection"));
			ValidateLayout(Index);
			for (int32 Row = 0; Row < Tags.Num(); ++Row)
			{
				const auto After = Rect(Widget->EquipmentSlotButtons[Row]);
				Check(FMath::IsNearlyEqual(After.Top, Before[Row].Top, 1.f) && FMath::IsNearlyEqual(After.Bottom, Before[Row].Bottom, 1.f), TEXT("selection does not move or resize slot rows vertically"));
			}
			if (Resolution == FIntPoint(1920,1080) || Index == 0)
				Capture(FString::Printf(TEXT("Slot%d_%dx%d"), Index, Resolution.X, Resolution.Y));
		}
	}
	SetRenderSize(FIntPoint(1920,1080), 1.25f); Widget->SelectEquipmentSlot(Tags[0]); ValidateLayout(0); Capture(TEXT("Weapon_UI125"));
	SetRenderSize(FIntPoint(1920,1080), 1.f);
	for (int32 Index = 0; Index < Tags.Num(); ++Index)
	{
		if (Widget->SelectedEquipmentSlot != Tags[Index]) Widget->SelectEquipmentSlot(Tags[Index]);
		const auto Before = knightEquipmentStats::ReadLoadout(Progression, Catalog);
		Check(Widget->Btn_Execute->GetVisibility() == ESlateVisibility::Collapsed && Widget->Btn_Unequip->IsVisible(), TEXT("distant replace removed while Unequip preserved"));
		Widget->EquipmentReplaceButton->OnClicked().Broadcast();
		Check(Widget->bEquipmentPickerOpen && Widget->SelectedEquipmentSlot == Tags[Index], TEXT("nearby action opens existing picker for correct slot"));
		Draw(); Draw();
		Check(!Widget->EquipmentReplaceButton->IsVisible() && !HitsAction(Center(Rect(Widget->EquipmentReplaceButton))), TEXT("modal hides external action and removes its input target"));
		Check(!Widget->EquipmentEnhanceButton->IsVisible(), TEXT("enhancement shortcut hidden behind replacement modal"));
		for (const UknightPartItemDefinition* Part : Widget->EquipmentPickerParts) Check(Part->SlotTag == Tags[Index] && Progression->GetItemCount(Part) > 0, TEXT("existing picker compatibility and ownership filtering"));
		Widget->EquipmentSlotButtons[(Index + 1) % 5]->OnClicked().Broadcast();
		Check(Widget->SelectedEquipmentSlot == Tags[Index], TEXT("open picker prevents stale slot retarget"));
		Widget->Btn_ConfirmCancel->OnClicked().Broadcast();
		Check(!Widget->bEquipmentPickerOpen && Widget->SelectedEquipmentSlot == Tags[Index]
			&& Widget->EquipmentReplaceButton->GetIsEnabled() && Before.OrderIndependentCompareEqual(knightEquipmentStats::ReadLoadout(Progression, Catalog)), TEXT("cancel returns to same slot without changing equipment"));
		Widget->EquipmentReplaceButton->OnClicked().Broadcast();
		const int32 Pick = Widget->EquipmentPickerParts.IndexOfByKey(Candidates[Index]);
		Check(Pick != INDEX_NONE, TEXT("owned replacement offered")); if (Pick == INDEX_NONE) continue;
		Widget->EquipmentPickerButtons[Pick]->OnClicked().Broadcast(); Widget->Btn_ConfirmOk->OnClicked().Broadcast();
		Check(!Widget->bEquipmentPickerOpen && Widget->SelectedEquipmentSlot == Tags[Index]
			&& Progression->GetEquippedPart(Tags[Index]) == Candidates[Index], TEXT("replacement succeeds and preserves selected slot"));
		Check(Before.FindRef(Tags[Index]) && Before.FindRef(Tags[Index]) != Candidates[Index]
			&& Progression->GetItemCount(Before.FindRef(Tags[Index])) > 0, TEXT("previous distinct equipment remains owned"));
		Check(Widget->Txt_PartName->GetText().EqualTo(Candidates[Index]->DisplayName)
			&& Widget->Img_Selected->GetBrush().GetResourceObject() == Candidates[Index]->Icon.LoadSynchronous(), TEXT("existing detail name and icon refreshed"));
		DrainSave();
		Widget->UnequipSelection(); DrainSave();
		Check(Cast<UTextBlock>(Widget->EquipmentReplaceButton->GetWidgetFromName(TEXT("Txt_Label")))->GetText().ToString() == TEXT("장착"), TEXT("empty slot exposes Equip"));
		ValidateLayout(Index);
		if (Index == 0) Capture(TEXT("EmptyWeapon"));
		Widget->EquipmentReplaceButton->OnClicked().Broadcast();
		const int32 EmptyPick = Widget->EquipmentPickerParts.IndexOfByKey(Candidates[Index]);
		Check(EmptyPick != INDEX_NONE, TEXT("empty slot uses same compatible picker"));
		if (EmptyPick != INDEX_NONE) { Widget->EquipmentPickerButtons[EmptyPick]->OnClicked().Broadcast(); Widget->Btn_ConfirmOk->OnClicked().Broadcast(); }
		Check(Progression->GetEquippedPart(Tags[Index]) == Candidates[Index], TEXT("empty slot equipped through existing API")); DrainSave();
	}
	Capture(TEXT("ReplacedFinal"));
	Widget->SelectEquipmentSlot(Tags[0]);
	Draw(); Draw();
	const UknightWeaponItemDefinition* ShortcutWeapon = Cast<UknightWeaponItemDefinition>(Progression->GetEquippedPart(Tags[0]));
	const int64 MoneyBeforeShortcut = Progression->GetMoney();
	const int32 LevelBeforeShortcut = Progression->GetUpgradeLevel(ShortcutWeapon);
	const int32 OwnedBeforeShortcut = Progression->GetItemCount(ShortcutWeapon);
	int32 ShortcutEntries = 0;
	Widget->OnEnhancementShortcutRequested.BindLambda([&](FPrimaryAssetId WeaponId)
	{
		++ShortcutEntries;
		Widget->Open(EKnightMaintenanceDestination::Enhancement);
		Check(Widget->OpenEnhancementWeapon(WeaponId), TEXT("existing enhancement page accepts equipped weapon"));
	});
	Widget->EquipmentEnhanceButton->OnClicked().Broadcast();
	Widget->EquipmentEnhanceButton->OnClicked().Broadcast();
	bool bTargetComplete = false;
	Check(ShortcutEntries == 1 && Widget->GetDestination() == EKnightMaintenanceDestination::Enhancement
		&& Widget->GetEnhancementTarget(bTargetComplete) == ShortcutWeapon && bTargetComplete,
		TEXT("shortcut opens correct equipped weapon once despite repeated input"));
	FKnightWeaponUpgradeQuote ShortcutQuote;
	Progression->GetWeaponUpgradeQuote(ShortcutWeapon, ShortcutQuote);
	Check(Widget->UpgradeHeading->GetText().EqualTo(ShortcutWeapon->DisplayName)
		&& Widget->EnhancementWeaponImage->GetBrush().GetResourceObject() == ShortcutWeapon->Icon.LoadSynchronous()
		&& Widget->EnhancementLevels->GetText().ToString().Contains(FString::FromInt(ShortcutQuote.CurrentLevel))
		&& !Widget->UpgradeMoney->GetText().IsEmpty() && !Widget->UpgradeChance->GetText().IsEmpty(),
		TEXT("existing enhancement detail renders equipped identity, level, cost and chance"));
	Check(Progression->GetMoney() == MoneyBeforeShortcut && Progression->GetUpgradeLevel(ShortcutWeapon) == LevelBeforeShortcut
		&& Progression->GetItemCount(ShortcutWeapon) == OwnedBeforeShortcut,
		TEXT("inspection alone changes no money, level or inventory"));
	Check(!Widget->IsEnhancementNavigationBlocked(), TEXT("idle enhancement screen permits Back"));
	// Exercise one real isolated upgrade transaction, then the same return path.
	if (ShortcutQuote.MoneyCost > 0 && ShortcutQuote.TargetLevel > ShortcutQuote.CurrentLevel)
	{
		Check(Progression->AddMoney(ShortcutQuote.MoneyCost), TEXT("isolated upgrade money seeded"));
		if (ShortcutQuote.GateMaterial && ShortcutQuote.GateMaterialNeeded > ShortcutQuote.GateMaterialOwned)
			Progression->AddItem(ShortcutQuote.GateMaterial, ShortcutQuote.GateMaterialNeeded - ShortcutQuote.GateMaterialOwned);
		DrainSave();
		FKnightWeaponUpgradeQuote ReadyQuote; Progression->GetWeaponUpgradeQuote(ShortcutWeapon, ReadyQuote);
		Check(ReadyQuote.Availability == EKnightWeaponUpgradeAvailability::Ready, TEXT("isolated enhancement attempt accepted"));
		if (ReadyQuote.Availability == EKnightWeaponUpgradeAvailability::Ready)
		{
			Progression->SetDebugNextWeaponUpgradeOutcome(1);
			FKnightWeaponUpgradeReceipt Receipt;
			Check(Progression->TryUpgradeWeapon(ShortcutWeapon, Receipt) == EKnightWeaponUpgradeOutcome::Success,
				TEXT("one actual upgrade transaction succeeds in isolated profile"));
			DrainSave();
			Check(Progression->GetUpgradeLevel(ShortcutWeapon) == LevelBeforeShortcut + 1
				&& Widget->GetEnhancementTarget(bTargetComplete) == ShortcutWeapon,
				TEXT("enhancement result retains selected weapon and advances its level"));
		}
	}
	Widget->Open(EKnightMaintenanceDestination::Crafting);
	Widget->RestoreWeaponEquipmentSelection(); Draw(); Draw();
	Check(Widget->SelectedEquipmentSlot == Tags[0] && Widget->EquipmentReplaceButton->IsVisible()
		&& Widget->EquipmentEnhanceButton->IsVisible(), TEXT("return restores selected weapon and both external actions"));
	Check(Widget->Txt_PartName->GetText().EqualTo(ShortcutWeapon->DisplayName)
		&& Widget->Img_Selected->GetBrush().GetResourceObject() == ShortcutWeapon->Icon.LoadSynchronous(),
		TEXT("return refreshes equipped weapon details after enhancement"));
	Widget->SetCategory(0, false); Draw(); Draw();
	Check(!Widget->EquipmentReplaceButton->IsVisible() && !Widget->EquipmentEnhanceButton->IsVisible(), TEXT("leaving equipment view hides both external actions"));
	Widget->SetCategory(2, false); Draw(); Draw();
	for (int32 Index = 0; Index < 25; ++Index) Widget->SelectEquipmentSlot(Tags[Index % 5]);
	Check(Widget->EquipmentReplaceTarget == Widget->SelectedEquipmentSlot, TEXT("rapid selection never retains previous target"));
	Widget->Close(); Widget->EquipmentReplaceButton->OnClicked().Broadcast();
	Check(!Widget->EquipmentReplaceTarget.IsValid() && !Widget->bEquipmentPickerOpen, TEXT("screen exit invalidates stale action"));
	Widget->Open(EKnightMaintenanceDestination::Crafting); Widget->SetCategory(2, false);
	Check(!Widget->SelectedEquipmentSlot.IsValid() && !Widget->EquipmentReplaceButton->IsVisible(), TEXT("reentry has no old selection or action"));
	Widget->SelectEquipmentSlot(Tags[0]);
	Check(Saves->SelectSlot(TEXT("EquipmentOther") + RunId), TEXT("second isolated profile"));
	Widget->EquipmentReplaceButton->OnClicked().Broadcast();
	Check(!Widget->EquipmentReplaceTarget.IsValid() && !Widget->bEquipmentPickerOpen, TEXT("profile switch invalidates action"));
	Widget->Close(); Widget->RemoveFromRoot();
	BeginCleanup(Renderer); FlushRenderingCommands(); if (Target) Target->RemoveFromRoot();
	GI->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GI->RemoveFromRoot();
	UE_LOG(LogTemp, Display, TEXT("EquipmentAction COMPLETE failures=%d output=%s"), Failures, *Out);
	return Failures ? 1 : 0;
}
