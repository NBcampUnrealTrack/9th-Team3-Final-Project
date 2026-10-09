#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "knightLoadingScreen.generated.h"

class UCommonTextBlock;

/**
 * Loading screen shown by the CommonLoadingScreen plugin (Project Settings > Common Loading Screen) while a level
 * travel is pending, the next map loads, and until the new level is ready (the raid holds it until the raid starts).
 *
 * The plugin adds it to the game viewport and keeps it across the level change, so it must not use the world, the
 * player controller or anything else that belongs to a level. While the map itself loads the game thread is blocked
 * and the dots stop; they move before and after that.
 *
 * Layout lives in the Widget Blueprint designer (WBP_KnightLoadingScreen). Optional: Txt_Tip, Img_Dot_0 .. Img_Dot_2.
 */
UCLASS(Abstract, Blueprintable)
class KNIGHTUI_API UknightLoadingScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	UknightLoadingScreen(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** One is picked at random each time the screen shows. */
	UPROPERTY(EditAnywhere, Category="Knight|Loading", meta=(MultiLine=true))
	TArray<FText> Tips;

	/** Time each dot stays lit. */
	UPROPERTY(EditAnywhere, Category="Knight|Loading", meta=(ClampMin=0.05, Units="s"))
	float DotInterval = 0.35f;

	UPROPERTY(EditAnywhere, Category="Knight|Loading", meta=(ClampMin=0, ClampMax=1))
	float DimDotOpacity = 0.25f;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Loading", meta=(BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Txt_Tip;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Loading", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Img_Dot_0;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Loading", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Img_Dot_1;

	UPROPERTY(BlueprintReadOnly, Category="Knight|Loading", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> Img_Dot_2;

private:
	void ShowDots(int32 LitIndex);

	float DotTime = 0.f;
	int32 LitDot = -1;
};
