#include "KnightGameFlowSubsystem.h"

#include "KnightFlowSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"
#include "knight.h"

#define LOCTEXT_NAMESPACE "KnightGameFlow"

void UKnightGameFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	if (GEngine)
	{
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}
}

void UKnightGameFlowSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	if (GEngine && TravelFailureHandle.IsValid())
	{
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	ResetPendingTravel();
	RegisteredWorld.Reset();
	Super::Deinitialize();
}

bool UKnightGameFlowSubsystem::RequestOpenMainMenu()
{
	return RequestOpenMenu(EKnightFlowState::MainMenu);
}

bool UKnightGameFlowSubsystem::RequestOpenTitle()
{
	return RequestOpenMenu(EKnightFlowState::Title);
}

bool UKnightGameFlowSubsystem::RequestOpenMenu(EKnightFlowState Destination)
{
	if (bTravelPending)
	{
		return RejectRequest(Destination, LOCTEXT("TravelAlreadyPending", "A menu level transition is already in progress."));
	}
	if (CurrentFlowState == Destination)
	{
		return RejectRequest(Destination, LOCTEXT("AlreadyAtDestination", "This menu level is already active."));
	}

	UGameInstance* const GameInstance = GetGameInstance();
	UWorld* const World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!GEngine || !World || !World->IsGameWorld() || World->GetGameInstance() != GameInstance)
	{
		return RejectRequest(Destination, LOCTEXT("NoGameWorld", "No active game world is available for menu travel."));
	}

	const UKnightFlowSettings* const Settings = GetDefault<UKnightFlowSettings>();
	const TSoftObjectPtr<UWorld>& TargetMap = Destination == EKnightFlowState::Title ? Settings->TitleMap : Settings->MainMenuMap;
	if (TargetMap.IsNull())
	{
		return RejectRequest(Destination, LOCTEXT("MapNotConfigured", "The destination menu map is not configured."));
	}

	// A valid soft map reference need not already be loaded into memory.
	const FSoftObjectPath MapPath = TargetMap.ToSoftObjectPath();
	const FString PackageName = MapPath.GetLongPackageName();
	if (!FPackageName::IsValidLongPackageName(PackageName, true)
		|| MapPath.GetAssetName() != FPackageName::GetShortName(PackageName)
		|| !FPackageName::DoesPackageExist(PackageName))
	{
		return RejectRequest(Destination, FText::Format(
			LOCTEXT("InvalidMap", "The destination menu map is invalid or missing: {0}"),
			FText::FromString(MapPath.ToString())));
	}

	PendingDestination = Destination;
	PendingMapPackage = PackageName;
	PendingSourceWorld = World;
	SetTravelPending(true);
	UGameplayStatics::OpenLevel(World, FName(*PackageName));
	return true;
}

bool UKnightGameFlowSubsystem::RejectRequest(EKnightFlowState Destination, const FText& Reason)
{
	UE_LOG(Logknight, Warning, TEXT("Menu travel to %d rejected: %s"), static_cast<int32>(Destination), *Reason.ToString());
	OnTravelRejected.Broadcast(Destination, Reason);
	return false;
}

void UKnightGameFlowSubsystem::RegisterEnteredMenu(EKnightFlowState MenuState, UWorld* EnteredWorld)
{
	if (!EnteredWorld || !EnteredWorld->IsGameWorld()
		|| EnteredWorld->GetGameInstance() != GetGameInstance()
		|| GetGameInstance()->GetWorld() != EnteredWorld)
	{
		return;
	}
	if (MenuState != EKnightFlowState::Title && MenuState != EKnightFlowState::MainMenu)
	{
		UE_LOG(Logknight, Error, TEXT("A menu GameMode attempted to register invalid flow state %d."), static_cast<int32>(MenuState));
		return;
	}

	const bool bWasPending = bTravelPending;
	const EKnightFlowState RequestedState = PendingDestination;
	const bool bMatchesRequest = !bWasPending
		|| (MenuState == PendingDestination
			&& UWorld::RemovePIEPrefix(EnteredWorld->GetOutermost()->GetName()) == PendingMapPackage);

	RegisteredWorld = EnteredWorld;
	ResetPendingTravel();
	SetCurrentFlowState(MenuState);
	if (bWasPending && !bMatchesRequest)
	{
		const FText Reason = LOCTEXT("UnexpectedMenu", "The loaded level did not register the requested menu type or map.");
		UE_LOG(Logknight, Error, TEXT("Menu travel entered an unexpected menu level."));
		OnTravelFailed.Broadcast(RequestedState, Reason);
	}
}

void UKnightGameFlowSubsystem::HandleTravelFailure(UWorld* FailedWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (!bTravelPending || !FailedWorld || FailedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	if (FailedWorld != PendingSourceWorld.Get() && FailedWorld != GetGameInstance()->GetWorld())
	{
		return;
	}

	const EKnightFlowState RequestedState = PendingDestination;
	ResetPendingTravel();
	if (RegisteredWorld.Get() != GetGameInstance()->GetWorld())
	{
		RegisteredWorld.Reset();
		SetCurrentFlowState(EKnightFlowState::Uninitialized);
	}
	const FText Reason = ErrorString.IsEmpty()
		? LOCTEXT("EngineTravelFailure", "The engine could not open the destination menu level.")
		: FText::FromString(ErrorString);
	UE_LOG(Logknight, Error, TEXT("Menu travel failed (%d): %s"), static_cast<int32>(FailureType), *Reason.ToString());
	OnTravelFailed.Broadcast(RequestedState, Reason);
}

void UKnightGameFlowSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || !LoadedWorld->IsGameWorld()
		|| LoadedWorld->GetGameInstance() != GetGameInstance()
		|| GetGameInstance()->GetWorld() != LoadedWorld
		|| RegisteredWorld.Get() == LoadedWorld)
	{
		return;
	}

	// InitGame has already run. A level without our menu GameMode cannot confirm a menu request.
	const bool bWasPending = bTravelPending;
	const EKnightFlowState RequestedState = PendingDestination;
	ResetPendingTravel();
	RegisteredWorld.Reset();
	SetCurrentFlowState(EKnightFlowState::Uninitialized);
	if (bWasPending)
	{
		const FText Reason = LOCTEXT("MissingMenuMode", "The loaded level did not initialize a Knight menu GameMode.");
		UE_LOG(Logknight, Error, TEXT("Menu travel loaded a level without a valid Knight menu GameMode."));
		OnTravelFailed.Broadcast(RequestedState, Reason);
	}
}

void UKnightGameFlowSubsystem::SetCurrentFlowState(EKnightFlowState NewState)
{
	if (CurrentFlowState != NewState)
	{
		const EKnightFlowState PreviousState = CurrentFlowState;
		CurrentFlowState = NewState;
		OnFlowStateChanged.Broadcast(PreviousState, NewState);
	}
}

void UKnightGameFlowSubsystem::SetTravelPending(bool bNewPending)
{
	if (bTravelPending != bNewPending)
	{
		bTravelPending = bNewPending;
		OnTravelPendingChanged.Broadcast(bTravelPending);
	}
}

void UKnightGameFlowSubsystem::ResetPendingTravel()
{
	PendingDestination = EKnightFlowState::Uninitialized;
	PendingMapPackage.Reset();
	PendingSourceWorld.Reset();
	SetTravelPending(false);
}

#undef LOCTEXT_NAMESPACE
