#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Foundation/knightActivatableWidget.h"
#include "knightConfirmModal.generated.h"

class APlayerController;
class UCommonTextBlock;
class UknightButtonBase;
class USoundBase;

UENUM(BlueprintType)
enum class EknightConfirmResult : uint8
{
	Confirmed,
	Cancelled
};

DECLARE_DELEGATE_OneParam(FknightConfirmResultDelegate, EknightConfirmResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FknightConfirmResultEvent, EknightConfirmResult, Result);

/** What the confirm modal shows. Empty button texts keep the defaults (확인 / 취소). */
USTRUCT(BlueprintType)
struct FknightConfirmRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knight|Confirm")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knight|Confirm", meta=(MultiLine=true))
	FText Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knight|Confirm")
	FText ConfirmText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knight|Confirm")
	FText CancelText;
};

/**
 * Shared confirmation dialog for actions that cannot be undone (abandon raid, overwrite save). Lives on UI.Layer.Modal.
 *
 * Call UknightConfirmModal::Show from C++, or the "Show Confirm Modal" async node from Blueprint. The result arrives
 * exactly once. Focus starts on Cancel so a stray Enter never confirms, and Esc cancels.
 *
 * Layout lives in the Widget Blueprint designer (WBP_KnightConfirmModal). Required names: Btn_Confirm, Btn_Cancel
 * (UknightButtonBase). Optional: Txt_Title, Txt_Message, Panel (scaled by the intro).
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightConfirmModal : public UknightActivatableWidget
{
	GENERATED_BODY()

public:
	UknightConfirmModal(const FObjectInitializer& ObjectInitializer);

	/**
	 * Pushes the modal for this player, creating the UI layout first on maps that have none (menus).
	 * Returns null, without calling OnResult, when the modal cannot be shown.
	 */
	static UknightConfirmModal* Show(APlayerController* PlayerController, const FknightConfirmRequest& Request, FknightConfirmResultDelegate OnResult = FknightConfirmResultDelegate());

	UFUNCTION(BlueprintCallable, Category="Knight|Confirm")
	void Setup(const FknightConfirmRequest& Request);

	UFUNCTION(BlueprintCallable, Category="Knight|Confirm")
	void Confirm();

	UFUNCTION(BlueprintCallable, Category="Knight|Confirm")
	void Cancel();

	UPROPERTY(BlueprintAssignable, Category="Knight|Confirm")
	FknightConfirmResultEvent OnResult;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Played when Esc cancels. The buttons play their own sounds from their style. */
	UPROPERTY(EditAnywhere, Category="Knight|Confirm")
	TObjectPtr<USoundBase> CancelSound;

	UPROPERTY(EditAnywhere, Category="Knight|Confirm|Motion", meta=(ClampMin=0, Units="s"))
	float IntroDuration = 0.12f;

	/** Panel scale at the start of the intro. */
	UPROPERTY(EditAnywhere, Category="Knight|Confirm|Motion", meta=(ClampMin=0.5, ClampMax=1))
	float IntroStartScale = 0.96f;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Confirm", meta=(BindWidget))
	TObjectPtr<UknightButtonBase> Btn_Confirm;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Confirm", meta=(BindWidget))
	TObjectPtr<UknightButtonBase> Btn_Cancel;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Confirm", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Title;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Confirm", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Message;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Confirm", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Panel;

private:
	void Finish(EknightConfirmResult Result);
	bool TickIntro(float DeltaTime);
	void ApplyIntro(float Alpha);
	void StopIntro();

	FknightConfirmResultDelegate NativeOnResult;
	FText DefaultConfirmText;
	FText DefaultCancelText;
	float IntroTime = 0.f;
	bool bFinished = false;
	FTSTicker::FDelegateHandle IntroTicker;
};
