#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "knightUISubsystem.generated.h"

class APawn;
class APlayerController;
class UCommonActivatableWidget;
class UknightPilotVoice;
class UknightPrimaryGameLayout;
class UknightVM_BossStatus;
class UknightVM_PlayerStatus;
class UknightVM_Raid;

/**
 * Owns the local player's gameplay UI: the primary layout (one per world) and the HUD view models.
 *
 * View models live for the whole session and are registered in the MVVM global view model collection as
 * "PlayerStatus", "BossStatus" and "Raid", so MVVM bindings can use the Global View Model Collection creation type.
 * The current HUD widgets read them through the Get*ViewModel getters instead. They are rebound when a new world's
 * HUD starts and when the possessed pawn changes.
 */
UCLASS()
class KNIGHTUI_API UknightUISubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Creates the layout for this world on first call and binds the view models to the controller's pawn. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI")
	UknightPrimaryGameLayout* GetOrCreateLayout(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category="Knight|UI")
	UCommonActivatableWidget* PushWidgetToLayer(UPARAM(meta=(Categories="UI.Layer")) FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass);

	UFUNCTION(BlueprintPure, Category="Knight|UI")
	UknightPrimaryGameLayout* GetLayout() const { return Layout.Get(); }

	UknightVM_PlayerStatus* GetPlayerStatusViewModel() const { return PlayerStatusVM; }
	UknightVM_BossStatus* GetBossStatusViewModel() const { return BossStatusVM; }
	UknightVM_Raid* GetRaidViewModel() const { return RaidVM; }

private:
	void RegisterViewModels();
	void UnregisterViewModels();
	void BindToController(APlayerController* PlayerController);
	void BindPawn(APawn* Pawn);
	void RefreshViewModels();

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UPROPERTY(Transient)
	TObjectPtr<UknightVM_PlayerStatus> PlayerStatusVM;

	UPROPERTY(Transient)
	TObjectPtr<UknightVM_BossStatus> BossStatusVM;

	UPROPERTY(Transient)
	TObjectPtr<UknightVM_Raid> RaidVM;

	UPROPERTY(Transient)
	TObjectPtr<UknightPilotVoice> PilotVoice;

	TWeakObjectPtr<UknightPrimaryGameLayout> Layout;
	TWeakObjectPtr<APlayerController> BoundController;
	FTimerHandle RefreshTimer;
};
