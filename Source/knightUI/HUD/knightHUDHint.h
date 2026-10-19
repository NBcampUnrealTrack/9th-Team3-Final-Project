#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "knightHUDHint.generated.h"

class UCommonTextBlock;

/**
 * Hint box at the right of the HUD, above the repair kits: a key and what it does ("[F] 퍼펙트 가드"). Hidden until
 * ShowHint; meant for the tutorial. Widgets are found by name: Txt_HintKey and Txt_HintBody.
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightHUDHint : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows the hint. Duration 0 keeps it until HideHint. */
	UFUNCTION(BlueprintCallable, Category="Knight|HUD")
	void ShowHint(const FText& Key, const FText& Body, float Duration = 0.f);

	UFUNCTION(BlueprintCallable, Category="Knight|HUD")
	void HideHint();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_HintKey;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_HintBody;

private:
	FTSTicker::FDelegateHandle HideTicker;
};
