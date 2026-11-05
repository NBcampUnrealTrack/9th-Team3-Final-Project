#include "knightCraftResultCheckCommandlet.h"
#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Menu/UI/KnightMaintenanceEntryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
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

int32 UknightCraftResultCheckCommandlet::Main(const FString& Params)
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
		UE_LOG(LogTemp, Display, TEXT("CraftResult %s %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), What);
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
	const auto* Weapon = LoadObject<UknightCraftRecipeDefinition>(nullptr, TEXT("/Game/Knight/Data/Equipment/Janyeol/Recipes/DA_Recipe_Equipment_Janyeol_Weapon"));
	const auto* Armor = LoadObject<UknightCraftRecipeDefinition>(nullptr, TEXT("/Game/Knight/Data/Equipment/Janyeol/Recipes/DA_Recipe_Equipment_Janyeol_HeadB"));
	if (!Weapon || !Weapon->Result || !Armor || !Armor->Result) return 1;
	UKnightMaintenanceWidget* Widget = CreateWidget<UKnightMaintenanceWidget>(Player, UKnightMaintenanceWidget::GetSharedWidgetClass());
	Check(Widget && Widget->CraftResultEquip && Widget->CraftResultBack, TEXT("actual maintenance WBP and result buttons constructed"));
	if (!Widget || !Widget->CraftResultEquip || !Widget->CraftResultBack) return 1;
	Widget->AddToRoot();
	const FVector2D RenderSize(1920.f, 1080.f);
	FWidgetRenderer* Renderer = new FWidgetRenderer(true);
	auto* Target = FWidgetRenderer::CreateTargetFor(RenderSize, TF_Bilinear, true); Target->AddToRoot();
	const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	auto Draw = [&]() { Renderer->DrawWidget(Target, SlateWidget, RenderSize, 0.f); FlushRenderingCommands(); };
	auto Capture = [&](const TCHAR* Name)
	{
		if (UTexture2D* Texture = Cast<UTexture2D>(Widget->CraftResultIcon->GetBrush().GetResourceObject()))
		{ Texture->SetForceMipLevelsToBeResident(30.f); Texture->WaitForStreaming(); }
		GShaderCompilingManager->FinishAllCompilation(); Draw(); Draw();
		TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
		Check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags), TEXT("real RHI capture readback"));
		TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(1920, 1080, Pixels, Png);
		Check(FFileHelper::SaveArrayToFile(Png, *(Out / (FString(Name) + TEXT(".png")))), TEXT("render capture written"));
	};
	auto Prepare = [&](const TCHAR* Case, const UknightCraftRecipeDefinition* Recipe, bool bFund)
	{
		if (Saves->HasSelectedSlot()) DrainSave();
		Check(Saves->SelectSlot(FString(Case) + RunId), TEXT("fresh isolated profile selected"));
		UknightSaveGame* Save = Saves->GetSaveGame();
		// Fixture prerequisite only, never a production save or asset edit.
		if (!Recipe->RequiredClearedStageId.IsNone()) Save->StageRecords.FindOrAdd(Recipe->RequiredClearedStageId).ClearCount = 1;
		Progression->AddMoney(1234);
		if (bFund) for (const auto& Cost : Recipe->Costs) Progression->AddItem(Cost.Item, Cost.Count + 10);
		DrainSave(); Widget->Open(EKnightMaintenanceDestination::Crafting); Widget->SetSlot(Recipe->Result->SlotTag, false);
		for (UKnightMaintenanceEntryWidget* Entry : Widget->Entries) if (Entry->GetRecipe() == Recipe) Widget->SelectEntry(Entry);
		Check(Widget->SelectedRecipe == Recipe, TEXT("actual catalog recipe selected"));
		Draw();
	};
	auto Craft = [&](const UknightCraftRecipeDefinition* Recipe)
	{
		const auto Counts = Saves->GetSaveGame()->ItemCounts; const int64 Money = Progression->GetMoney();
		const float Scroll = Widget->List_Items->GetScrollOffset();
		const auto Rows = Widget->Entries;
		const auto BeforeEquipped = Saves->GetSaveGame()->EquippedItems;
		Widget->RequestExecute();
		Check(Widget->Panel_Confirm->IsVisible(), TEXT("normal pre-craft confirmation shown"));
		Widget->Btn_ConfirmOk->OnClicked().Broadcast();
		Widget->Btn_ConfirmOk->OnClicked().Broadcast(); Widget->RequestExecute(); Widget->ExecuteSelection();
		Check(Widget->IsCraftResultOpen() && Widget->CraftedPart == Recipe->Result, TEXT("one result holds exact crafted definition"));
		Check(Progression->GetItemCount(Recipe->Result) == 1, TEXT("repeated craft inputs create exactly one item"));
		TMap<FPrimaryAssetId, int32> Costs;
		for (const auto& Cost : Recipe->Costs) Costs.FindOrAdd(Cost.Item->GetPrimaryAssetId()) += Cost.Count;
		for (const auto& Cost : Costs) Check(Saves->GetSaveGame()->ItemCounts.FindRef(Cost.Key) == Counts.FindRef(Cost.Key) - Cost.Value, TEXT("material cost charged exactly once"));
		Check(Money == Progression->GetMoney(), TEXT("existing recipe has no currency charge; money unchanged"));
		Check(BeforeEquipped.OrderIndependentCompareEqual(Saves->GetSaveGame()->EquippedItems), TEXT("craft does not auto equip"));
		Check(Widget->Entries == Rows && Widget->SelectedRecipe == Recipe && Widget->List_Items->GetScrollOffset() == Scroll,
			TEXT("recipe rows selection and scroll preserved"));
		const auto CurrentSlot = Widget->Slot; const int32 Tab = Widget->InfoTab;
		Widget->SetSlot(knightGameplayTags::Equipment_Slot_Legs, true); Widget->SetCategory(2, true); Widget->CycleInfoTab(1);
		Widget->HandleCraftResultKey(FKeyEvent(EKeys::Q, FModifierKeysState(), 0, false, 0, 0));
		bool bDisabledAncestor = false;
		for (UWidget* Ancestor = Widget->Btn_Execute; Ancestor; Ancestor = Ancestor->GetParent())
			bDisabledAncestor |= !Ancestor->GetIsEnabled();
		Check(Widget->Slot == CurrentSlot && Widget->Category == 0 && Widget->InfoTab == Tab && bDisabledAncestor,
			TEXT("modal blocks underlying actions and navigation"));
		Widget->HandleCraftResultKey(FKeyEvent(EKeys::Enter, FModifierKeysState(), 0, true, 0, 0));
		Check(Widget->IsCraftResultOpen(), TEXT("held confirm key cannot activate result buttons"));
		DrainSave();
	};
	for (const auto* Recipe : { Weapon, Armor })
	{
		Prepare(Recipe == Weapon ? TEXT("Weapon") : TEXT("Armor"), Recipe, true);
		const auto* Previous = knightEquipmentStats::ReadLoadout(Progression, UknightEquipmentSettings::LoadCatalog()).FindRef(Recipe->Result->SlotTag);
		Craft(Recipe); Capture(Recipe == Weapon ? TEXT("WeaponResult") : TEXT("ArmorResult"));
		const auto Counts = Saves->GetSaveGame()->ItemCounts; const int64 Money = Progression->GetMoney();
		Widget->CraftResultEquip->OnClicked().Broadcast(); Widget->CraftResultEquip->OnClicked().Broadcast();
		Check(!Widget->IsCraftResultOpen() && Progression->GetEquippedPart(Recipe->Result->SlotTag) == Recipe->Result, TEXT("equip exact crafted item and close result"));
		Check(Progression->GetItemCount(Recipe->Result) == 1 && (!Previous || Progression->GetItemCount(Previous) >= 1), TEXT("crafted and replaced equipment both owned"));
		for (const auto& Cost : Recipe->Costs) Check(Saves->GetSaveGame()->ItemCounts.FindRef(Cost.Item->GetPrimaryAssetId()) == Counts.FindRef(Cost.Item->GetPrimaryAssetId()), TEXT("equip does not charge crafting again"));
		Check(Progression->GetMoney() == Money && Widget->SelectedRecipe == Recipe && Widget->Category == 0, TEXT("stays crafting without charging currency"));
		for (UKnightMaintenanceEntryWidget* Entry : Widget->Entries) if (Entry->GetRecipe() == Recipe)
			Check(Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("Txt_State")))->GetText().ToString() == TEXT("장착 중"), TEXT("equipped row indicator refreshed"));
		DrainSave();
		const FString SavedName = TEXT("KnightProgress_Test_") + Namespace + TEXT("_") + Saves->GetActiveSlotId();
		auto* Disk = Cast<UknightSaveGame>(UGameplayStatics::LoadGameFromSlot(SavedName, 0));
		Check(Disk && Disk->ItemCounts.FindRef(Recipe->Result->GetPrimaryAssetId()) == 1
			&& Disk->EquippedItems.FindRef(Recipe->Result->SlotTag) == Recipe->Result->GetPrimaryAssetId(), TEXT("normal save reload preserves crafted and equipped item"));
	}
	for (const bool bEscape : { false, true })
	{
		Prepare(bEscape ? TEXT("Escape") : TEXT("Back"), Weapon, true); Craft(Weapon);
		const auto Counts = Saves->GetSaveGame()->ItemCounts; const auto Equipped = Saves->GetSaveGame()->EquippedItems;
		if (bEscape) Widget->HandleCraftResultKey(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
		else Widget->CraftResultBack->OnClicked().Broadcast();
		Check(!Widget->IsCraftResultOpen() && Counts.OrderIndependentCompareEqual(Saves->GetSaveGame()->ItemCounts)
			&& Equipped.OrderIndependentCompareEqual(Saves->GetSaveGame()->EquippedItems), TEXT("Back/Esc dismiss without equip refund or duplicate grant"));
		Check(Widget->Btn_Execute->GetIsEnabled(), TEXT("underlying enabled state restored"));
		Widget->Close(); Widget->Open(EKnightMaintenanceDestination::Crafting);
		Check(!Widget->IsCraftResultOpen(), TEXT("reopening has no stale dialog"));
	}
	Prepare(TEXT("Reject"), Weapon, false);
	const auto RejectCounts = Saves->GetSaveGame()->ItemCounts;
	Widget->RequestExecute(); Widget->ExecuteSelection(); Widget->ExecuteSelection();
	Check(!Widget->IsCraftResultOpen() && !Widget->Panel_Confirm->IsVisible()
		&& RejectCounts.OrderIndependentCompareEqual(Saves->GetSaveGame()->ItemCounts), TEXT("insufficient materials: no dialog grant or cost"));
	Prepare(TEXT("EquipReject"), Weapon, true); Craft(Weapon);
	const FString OriginalProfile = Widget->CraftedProfileId;
	Widget->CraftedProfileId = TEXT("DifferentProfile"); Widget->EquipCraftResult();
	Check(Widget->IsCraftResultOpen() && !Widget->CraftResultError->GetText().IsEmpty() && Progression->GetItemCount(Weapon->Result) == 1,
		TEXT("equip rejection keeps item and accurate error"));
	Capture(TEXT("EquipError")); Widget->CraftedProfileId = OriginalProfile;
	// A transient overweight definition exercises the actual capacity rejection; no asset is changed.
	auto* Heavy = DuplicateObject<UknightPartItemDefinition>(Weapon->Result, GetTransientPackage(), TEXT("CraftResultHeavyFixture"));
	Heavy->Weight = 1000000.f; Progression->AddItem(Heavy, 1); DrainSave();
	Widget->CraftedPart = Heavy; Widget->EquipCraftResult();
	Check(Widget->IsCraftResultOpen() && Widget->CraftResultError->GetText().ToString().Contains(TEXT("적재 한도"))
		&& Progression->GetItemCount(Heavy) == 1 && Progression->GetItemCount(Weapon->Result) == 1,
		TEXT("capacity rejection preserves ownership and shows real reason"));
	Widget->CraftedPart = Weapon->Result;
	Check(Saves->SelectSlot(TEXT("Switch") + RunId) && !Widget->IsCraftResultOpen(), TEXT("profile switch invalidates result identity"));
	// Current production recipes only produce equipment. A transient invalid-slot recipe verifies the exclusion.
	auto* NonEquip = NewObject<UknightPartItemDefinition>(GetTransientPackage(), TEXT("CraftResultNonEquipmentFixture"));
	auto* NonEquipRecipe = NewObject<UknightCraftRecipeDefinition>(); NonEquipRecipe->Result = NonEquip;
	Widget->SelectedPart = NonEquip; Widget->SelectedRecipe = NonEquipRecipe;
	Widget->ExecuteSelection();
	Check(Progression->GetItemCount(NonEquip) == 1 && !Widget->IsCraftResultOpen(), TEXT("successful non-equippable craft has no equip dialog"));
	DrainSave();
	Widget->Close(); Widget->RemoveFromRoot();
	BeginCleanup(Renderer); FlushRenderingCommands(); Target->RemoveFromRoot();
	GI->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GI->RemoveFromRoot();
	UE_LOG(LogTemp, Display, TEXT("CraftResult COMPLETE failures=%d output=%s"), Failures, *Out);
	return Failures ? 1 : 0;
}
