#include "Foundation/knightUISubsystem.h"

#include "AbilitySystemGlobals.h"
#include "Combat/knightTargetLockComponent.h"
#include "CommonActivatableWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Foundation/knightPrimaryGameLayout.h"
#include "Foundation/knightUISettings.h"
#include "GameFramework/PlayerController.h"
#include "MVVMGameSubsystem.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Raid/knightRaidGameMode.h"
#include "Raid/knightRaidGameState.h"
#include "TimerManager.h"
#include "Types/MVVMViewModelCollection.h"
#include "ViewModels/knightVM_BossStatus.h"
#include "Foundation/knightPilotVoice.h"
#include "ViewModels/knightVM_PlayerStatus.h"
#include "ViewModels/knightVM_Raid.h"
#include "knightUI.h"

namespace knightUIViewModelNames
{
	static const FName PlayerStatus(TEXT("PlayerStatus"));
	static const FName BossStatus(TEXT("BossStatus"));
	static const FName Raid(TEXT("Raid"));

	static FMVVMViewModelContext MakeContext(TSubclassOf<UMVVMViewModelBase> ViewModelClass, FName Name)
	{
		FMVVMViewModelContext Context;
		Context.ContextClass = ViewModelClass;
		Context.ContextName = Name;
		return Context;
	}
}

void UknightUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// UMVVMGameSubsystem is a game instance subsystem, created before any local player subsystem.
	Super::Initialize(Collection);

	PlayerStatusVM = NewObject<UknightVM_PlayerStatus>(this);
	BossStatusVM = NewObject<UknightVM_BossStatus>(this);
	RaidVM = NewObject<UknightVM_Raid>(this);
	PilotVoice = NewObject<UknightPilotVoice>(this);
	PilotVoice->Preload();
	RegisterViewModels();
}

void UknightUISubsystem::Deinitialize()
{
	if (APlayerController* const PC = BoundController.Get())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
		PC->GetWorldTimerManager().ClearTimer(RefreshTimer);
	}
	PlayerStatusVM->Unbind();
	BossStatusVM->Unbind();
	RaidVM->Unbind();
	PilotVoice->Unbind();
	UnregisterViewModels();
	Super::Deinitialize();
}

void UknightUISubsystem::RegisterViewModels()
{
	const UGameInstance* const GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr;
	UMVVMGameSubsystem* const MVVM = GameInstance ? GameInstance->GetSubsystem<UMVVMGameSubsystem>() : nullptr;
	UMVVMViewModelCollectionObject* const ViewModels = MVVM ? MVVM->GetViewModelCollection() : nullptr;
	if (!ViewModels)
	{
		UE_LOG(LogknightUI, Error, TEXT("MVVM global view model collection unavailable; HUD bindings will be empty."));
		return;
	}
	ViewModels->AddViewModelInstance(knightUIViewModelNames::MakeContext(UknightVM_PlayerStatus::StaticClass(), knightUIViewModelNames::PlayerStatus), PlayerStatusVM);
	ViewModels->AddViewModelInstance(knightUIViewModelNames::MakeContext(UknightVM_BossStatus::StaticClass(), knightUIViewModelNames::BossStatus), BossStatusVM);
	ViewModels->AddViewModelInstance(knightUIViewModelNames::MakeContext(UknightVM_Raid::StaticClass(), knightUIViewModelNames::Raid), RaidVM);
}

void UknightUISubsystem::UnregisterViewModels()
{
	const UGameInstance* const GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr;
	UMVVMGameSubsystem* const MVVM = GameInstance ? GameInstance->GetSubsystem<UMVVMGameSubsystem>() : nullptr;
	if (UMVVMViewModelCollectionObject* const ViewModels = MVVM ? MVVM->GetViewModelCollection() : nullptr)
	{
		ViewModels->RemoveViewModel(knightUIViewModelNames::MakeContext(UknightVM_PlayerStatus::StaticClass(), knightUIViewModelNames::PlayerStatus));
		ViewModels->RemoveViewModel(knightUIViewModelNames::MakeContext(UknightVM_BossStatus::StaticClass(), knightUIViewModelNames::BossStatus));
		ViewModels->RemoveViewModel(knightUIViewModelNames::MakeContext(UknightVM_Raid::StaticClass(), knightUIViewModelNames::Raid));
	}
}

UknightPrimaryGameLayout* UknightUISubsystem::GetOrCreateLayout(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return nullptr;
	}
	UknightPrimaryGameLayout* Existing = Layout.Get();
	if (Existing && Existing->GetWorld() == PlayerController->GetWorld())
	{
		BindToController(PlayerController);
		return Existing;
	}

	const UknightUISettings* const Settings = GetDefault<UknightUISettings>();
	UClass* const LayoutClass = Settings->PrimaryGameLayoutClass.LoadSynchronous();
	if (!LayoutClass)
	{
		UE_LOG(LogknightUI, Error, TEXT("Knight UI settings have no PrimaryGameLayoutClass."));
		return nullptr;
	}
	UknightPrimaryGameLayout* const NewLayout = CreateWidget<UknightPrimaryGameLayout>(PlayerController, LayoutClass);
	if (!NewLayout)
	{
		return nullptr;
	}
	NewLayout->AddToPlayerScreen(1000);
	Layout = NewLayout;
	BindToController(PlayerController);
	return NewLayout;
}

UCommonActivatableWidget* UknightUISubsystem::PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	UknightPrimaryGameLayout* const CurrentLayout = Layout.Get();
	return CurrentLayout ? CurrentLayout->PushWidgetToLayer(LayerTag, WidgetClass) : nullptr;
}

void UknightUISubsystem::BindToController(APlayerController* PlayerController)
{
	if (BoundController.Get() != PlayerController)
	{
		if (APlayerController* const OldController = BoundController.Get())
		{
			OldController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
			OldController->GetWorldTimerManager().ClearTimer(RefreshTimer);
		}
		BoundController = PlayerController;
		PlayerController->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}

	BindPawn(PlayerController->GetPawn());
	RaidVM->Bind(PlayerController->GetWorld()->GetGameState<AknightRaidGameState>());
	PilotVoice->BindRaid(PlayerController->GetWorld()->GetGameState<AknightRaidGameState>());
	BossStatusVM->Unbind();

	const float Interval = GetDefault<UknightUISettings>()->ViewModelRefreshInterval;
	PlayerController->GetWorldTimerManager().SetTimer(RefreshTimer, this, &ThisClass::RefreshViewModels, Interval, true);
}

void UknightUISubsystem::BindPawn(APawn* Pawn)
{
	PlayerStatusVM->Bind(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn),
		Pawn ? Pawn->FindComponentByClass<UknightTargetLockComponent>() : nullptr);
	PilotVoice->BindPawn(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn));
}

void UknightUISubsystem::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindPawn(NewPawn);
}

void UknightUISubsystem::RefreshViewModels()
{
	RaidVM->RefreshTimer();
	PilotVoice->Refresh();

	// Raid targets are registered after actors begin play, so the boss binds late, on the first refresh that sees it.
	if (!BossStatusVM->HasMonster())
	{
		const APlayerController* const PC = BoundController.Get();
		const AknightRaidGameMode* const RaidMode = PC && PC->GetWorld() ? PC->GetWorld()->GetAuthGameMode<AknightRaidGameMode>() : nullptr;
		if (RaidMode)
		{
			for (const TWeakObjectPtr<AknightMonsterCharacterBase>& Monster : RaidMode->GetTargetMonsters())
			{
				if (Monster.IsValid())
				{
					BossStatusVM->Bind(Monster.Get());
					break;
				}
			}
		}
	}
}
