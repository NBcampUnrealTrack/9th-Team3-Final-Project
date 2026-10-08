#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "knightPrimaryGameLayout.generated.h"

class UCommonActivatableWidget;

/**
 * Root widget of the gameplay UI: one activatable widget stack per layer (UI.Layer.Game, GameMenu, Menu, Modal).
 * Stacks named Layer_Game, Layer_GameMenu, Layer_Menu and Layer_Modal in the Blueprint subclass register themselves;
 * other containers can still be registered with RegisterLayer.
 * Widgets are pushed by layer tag, never added to the viewport directly.
 *
 * Replaces Lyra's CommonGame PrimaryGameLayout, which this project does not include.
 */
UCLASS(Abstract, Blueprintable)
class KNIGHTUI_API UknightPrimaryGameLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Knight|UI")
	void RegisterLayer(UPARAM(meta=(Categories="UI.Layer")) FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* LayerWidget);

	/** Pushes a widget onto a layer's stack and returns it (already activated when it is the top). */
	template<typename WidgetT = UCommonActivatableWidget>
	WidgetT* PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass)
	{
		UCommonActivatableWidgetContainerBase* const Layer = GetLayer(LayerTag);
		return Layer && WidgetClass ? Layer->AddWidget<WidgetT>(WidgetClass) : nullptr;
	}

	UFUNCTION(BlueprintCallable, Category="Knight|UI", meta=(DisplayName="Push Widget To Layer"))
	UCommonActivatableWidget* K2_PushWidgetToLayer(UPARAM(meta=(Categories="UI.Layer")) FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass);

	/** Removes the widget from whichever layer holds it. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI")
	void RemoveWidget(UCommonActivatableWidget* Widget);

	UFUNCTION(BlueprintPure, Category="Knight|UI")
	UCommonActivatableWidgetContainerBase* GetLayer(FGameplayTag LayerTag) const;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, Category="Knight|UI", meta=(BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetContainerBase> Layer_Game;

	UPROPERTY(BlueprintReadOnly, Category="Knight|UI", meta=(BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetContainerBase> Layer_GameMenu;

	UPROPERTY(BlueprintReadOnly, Category="Knight|UI", meta=(BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetContainerBase> Layer_Menu;

	UPROPERTY(BlueprintReadOnly, Category="Knight|UI", meta=(BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetContainerBase> Layer_Modal;

private:
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidgetContainerBase>> Layers;
};
