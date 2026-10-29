#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "KnightCheatSubsystem.generated.h"

class APlayerController;
class IInputProcessor;
class SWidget;
class UKnightCheatOverlayWidget;
struct FKeyEvent;

/** Session-only cheats and a Shipping-capable command overlay. Nothing here is serialized. */
UCLASS()
class KNIGHT_API UKnightCheatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** One call is one submission; false leaves money and inventory unchanged. */
	bool ExecuteCommand(const FString& Command, FText& OutFeedback);
	bool IsPlayerInvulnerable(const AActor* Actor) const;
	bool IsInvulnerabilityEnabled() const { return bInvulnerable; }
	bool IsOverlayOpen() const;

	/** Consumes only overlay controls. Other keys reach the focused text field. */
	bool HandleOverlayKey(const FKeyEvent& Event);
	bool HandleGlobalKeyDown(const FKeyEvent& Event);
	void OpenOverlay();
	void CloseOverlay();

private:
	UFUNCTION() void HandleActiveSlotChanged(const FString& SlotId);
	bool TryRegisterInputProcessor(float DeltaTime);
	bool FocusOverlayNextTick(float DeltaTime);
	bool ClearFeedbackAfterDelay(float DeltaTime);
	void SubmitCurrentText();
	void DisplayFeedback(const FText& Message);

	UPROPERTY(Transient) TObjectPtr<UKnightCheatOverlayWidget> Overlay;
	TSharedPtr<IInputProcessor> InputProcessor;
	TWeakPtr<SWidget> PreviousFocus;
	TWeakObjectPtr<APlayerController> PreviousController;
	FTSTicker::FDelegateHandle RegistrationTicker;
	FTSTicker::FDelegateHandle FocusTicker;
	FTSTicker::FDelegateHandle FeedbackTicker;
	bool bCursorBeforeOverlay = false;
	bool bInvulnerable = false;
	bool bSubmitting = false;
};
