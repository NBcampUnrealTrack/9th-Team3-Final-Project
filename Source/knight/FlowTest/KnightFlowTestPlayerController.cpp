#include "KnightFlowTestPlayerController.h"

#include "KnightFlowTestGameMode.h"
#include "KnightFlowTestReturnWidget.h"
#include "GameFlow/KnightGameFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Dev/KnightCoreInterfaceProbe.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Raid/knightRaidGameMode.h"
#include "Raid/knightRaidGameState.h"
#include "Raid/knightRaidSubsystem.h"
#include "knight.h"

AKnightFlowTestPlayerController::AKnightFlowTestPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
}

void AKnightFlowTestPlayerController::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	if (!IsLocalController() || ReturnWidget)
	{
		return;
	}
	if (GetWorld() && GetWorld()->GetAuthGameMode<AknightRaidGameMode>())
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
		return;
	}
	const AKnightFlowTestGameMode* const TestMode = GetWorld() ? Cast<AKnightFlowTestGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
	UKnightGameFlowSubsystem* const Flow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKnightGameFlowSubsystem>() : nullptr;
	if (!TestMode || !Flow || Flow->GetCurrentFlowState() != EKnightFlowState::Gameplay)
	{
		UE_LOG(Logknight, Error, TEXT("Flow test UI requires a registered Gameplay world and KnightFlowTestGameMode."));
		return;
	}
	UClass* const WidgetClass = TestMode->GetTestWidgetClass();
	if (!WidgetClass)
	{
		UE_LOG(Logknight, Warning, TEXT("%s has no TestWidgetClass yet; no return UI was created."), *TestMode->GetClass()->GetName());
		return;
	}
	ReturnWidget = CreateWidget<UKnightFlowTestReturnWidget>(this, WidgetClass);
	if (!ReturnWidget)
	{
		UE_LOG(Logknight, Error, TEXT("Failed to create flow test return widget %s."), *WidgetClass->GetName());
		return;
	}
	ReturnWidget->AddToViewport();
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(ReturnWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	ReturnWidget->FocusReturnButton();
#endif
}

void AKnightFlowTestPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ReturnWidget)
	{
		ReturnWidget->RemoveFromParent();
		ReturnWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AKnightFlowTestPlayerController::KnightCoreSelectTestSlot(const FString& SlotId)
{
	UknightSaveSubsystem* const Save = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UE_LOG(Logknight, Log, TEXT("KnightCoreSelectTestSlot %s: %s"), *SlotId,
		Save && Save->SelectSlot(SlotId) ? TEXT("loaded") : TEXT("failed"));
}

void AKnightFlowTestPlayerController::KnightCoreSuccess()
{
	if (AknightRaidGameMode* const Raid = GetWorld()->GetAuthGameMode<AknightRaidGameMode>())
	{
		Raid->DebugFinishRaid(true);
	}
}

void AKnightFlowTestPlayerController::KnightCoreFailure()
{
	if (AknightRaidGameMode* const Raid = GetWorld()->GetAuthGameMode<AknightRaidGameMode>())
	{
		Raid->DebugFinishRaid(false);
	}
}

void AKnightFlowTestPlayerController::KnightCoreStatus()
{
	const UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	const UknightSaveGame* const Save = Saves ? Saves->GetSaveGame() : nullptr;
	const UknightRaidSubsystem* const Session = GetGameInstance()->GetSubsystem<UknightRaidSubsystem>();
	const AknightRaidGameState* const State = GetWorld()->GetGameState<AknightRaidGameState>();
	FknightRaidResult Last;
	const bool bHasResult = Session && Session->GetLastResult(Last);
	const int32 MaterialCount = Save ? Save->ItemCounts.FindRef(FPrimaryAssetId(TEXT("Item"), TEXT("DA_CoreTestMaterial"))) : -1;
	UE_LOG(Logknight, Log, TEXT("KnightCoreStatus slot=%s loadFailed=%d phase=%d run=%s result=%d success=%d materials=%d appliedRuns=%d"),
		Saves ? *Saves->GetActiveSlotId() : TEXT("None"), Saves && Saves->DidLoadFail(),
		State ? static_cast<int32>(State->GetRaidPhase()) : -1,
		Session ? *Session->GetCurrentRunId().ToString() : TEXT("None"), bHasResult,
		bHasResult && Last.bSuccess, MaterialCount, Save ? Save->AppliedRaidRuns.Num() : -1);
}

void AKnightFlowTestPlayerController::KnightCoreProbeInterfaces()
{
	for (TActorIterator<AKnightCoreInterfaceProbe> It(GetWorld()); It; ++It)
	{
		AActor* const Probe = *It;
		const bool bCombat = ICombatInterface::Execute_CanReceiveHitBP(Probe, GetPawn());
		const bool bTarget = ITargetableInterface::Execute_CanBeTargeted(Probe);
		const int32 Points = ITargetableInterface::Execute_QueryTargetPoints(Probe).Num();
		const bool bInteract = IInteractableInterface::Execute_CanInteract(Probe, GetPawn());
		UE_LOG(Logknight, Log, TEXT("KnightCoreProbeInterfaces class=%s combat=%d target=%d points=%d interact=%d"),
			*Probe->GetClass()->GetName(), bCombat, bTarget, Points, bInteract);
		return;
	}
	UE_LOG(Logknight, Warning, TEXT("KnightCoreProbeInterfaces: no probe actor in the level."));
}

void AKnightFlowTestPlayerController::KnightCoreSaveQueueTest(const FString& AttemptSwitchSlot)
{
#if WITH_EDITOR
	UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>();
	UknightSaveGame* const Save = Saves ? Saves->GetSaveGame() : nullptr;
	if (!Save || Saves->IsSaveInFlight()) return;
	const FPrimaryAssetId ItemId(TEXT("Item"), TEXT("DA_CoreTestMaterial"));
	Save->ItemCounts.FindOrAdd(ItemId) += 1;
	Saves->RequestSave();
	const bool bSwitchedWhileSaving = Saves->SelectSlot(AttemptSwitchSlot);
	Save->ItemCounts.FindOrAdd(ItemId) += 1;
	Save->ItemUpgradeLevels.Add(ItemId, 3);
	Save->UnlockedStages.Add(FName(TEXT("Core_Raid_Test")));
	Saves->RequestSave();
	UE_LOG(Logknight, Log, TEXT("KnightCoreSaveQueueTest slot=%s switchAccepted=%d busy=%d latestMaterial=%d upgrade=%d unlock=%d"),
		*Saves->GetActiveSlotId(), bSwitchedWhileSaving, Saves->IsSaveInFlight(),
		Save->ItemCounts.FindRef(ItemId), Save->ItemUpgradeLevels.FindRef(ItemId), Save->UnlockedStages.Contains(FName(TEXT("Core_Raid_Test"))));
#endif
}

void AKnightFlowTestPlayerController::KnightCoreSaveOnce()
{
#if WITH_EDITOR
	if (UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>())
	{
		Saves->RequestSave();
		UE_LOG(Logknight, Log, TEXT("KnightCoreSaveOnce slot=%s busy=%d"), *Saves->GetActiveSlotId(), Saves->IsSaveInFlight());
	}
#endif
}

void AKnightFlowTestPlayerController::KnightCoreFailNextSaves(int32 Count)
{
#if WITH_EDITOR
	if (UknightSaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<UknightSaveSubsystem>())
	{
		Saves->DebugFailNextSaves(Count);
		UE_LOG(Logknight, Log, TEXT("KnightCoreFailNextSaves count=%d"), Count);
	}
#endif
}
