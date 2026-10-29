#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KnightCheatOverlayWidget.generated.h"

class UEditableTextBox;
class UTextBlock;

/** Small runtime command entry; it does not use the Unreal developer console. */
UCLASS()
class KNIGHT_API UKnightCheatOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FString GetCommandText() const;
	void SetCommandText(const FText& Text);
	void FocusInput();
	void ShowFeedback(const FText& Message);
	void ClearFeedback();

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> CommandInput;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> FeedbackText;
};
