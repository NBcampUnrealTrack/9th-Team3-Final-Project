#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "KnightSaveSlotCard.generated.h"

class UTextBlock;

UENUM(BlueprintType)
enum class EKnightSaveSlotState : uint8
{
	Empty,
	Ready,
	Unreadable
};

/** What one save card shows. Built by the title menu from the existing save files; nothing here is written back. */
USTRUCT(BlueprintType)
struct KNIGHT_API FKnightSaveSlotView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Knight|Save")
	int32 SlotNumber = 1;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Save")
	EKnightSaveSlotState State = EKnightSaveSlotState::Empty;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Save")
	float PlayTimeSeconds = 0.f;

	/** File time of the save, so "last played" needs no extra field in the save. Invalid when unknown. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Save")
	FDateTime LastPlayed = FDateTime::MinValue();

	UPROPERTY(BlueprintReadOnly, Category="Knight|Save")
	int32 ClearedStages = 0;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Save")
	int32 TotalClears = 0;

	/** Last written slot: the one "게임 시작" continues. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Save")
	bool bActive = false;
};

/**
 * One card on the save-select page. Layout lives in WBP_KnightSaveSlotCard; widgets are found by name and all optional.
 * The card takes focus when hovered, so mouse and keyboard always highlight the same card.
 */
UCLASS(Abstract, Blueprintable)
class KNIGHT_API UKnightSaveSlotCard : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	void SetSlotView(const FKnightSaveSlotView& InView);
	const FKnightSaveSlotView& GetSlotView() const { return View; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void HandleFocusReceived() override;
	virtual void HandleFocusLost() override;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Save", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_SlotName;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Save", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_SlotCaption;

	/** Shown for a slot with data. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Save", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Filled;

	/** Shown for an empty or unreadable slot; Txt_EmptyTitle / Txt_EmptyBody explain which. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Save", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Empty;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Save", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_EmptyTitle;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Save", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_EmptyBody;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Save", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_PlayTime;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Save", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_LastPlayed;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Save", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_ClearedStages;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Save", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_TotalClears;

	/** "최근 플레이" tag for the active slot. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Save", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Active;

	/** Orange frame while hovered or focused. */
	UPROPERTY(BlueprintReadOnly, Category="Knight|Save", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Img_FocusFrame;

private:
	void Refresh();
	void RefreshFocusVisual();

	FKnightSaveSlotView View;
	bool bFocused = false;
};
