#include "Foundation/knightPrimaryGameLayout.h"

#include "CommonActivatableWidget.h"
#include "knightUI.h"

void UknightPrimaryGameLayout::RegisterLayer(FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* LayerWidget)
{
	if (IsDesignTime() || !LayerTag.IsValid() || !LayerWidget)
	{
		return;
	}
	if (Layers.Contains(LayerTag))
	{
		UE_LOG(LogknightUI, Warning, TEXT("%s: layer %s registered twice; keeping the latest."), *GetName(), *LayerTag.ToString());
	}
	Layers.Add(LayerTag, LayerWidget);
}

UCommonActivatableWidget* UknightPrimaryGameLayout::K2_PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	UCommonActivatableWidget* const Widget = PushWidgetToLayer(LayerTag, WidgetClass);
	if (!Widget)
	{
		UE_LOG(LogknightUI, Warning, TEXT("%s: could not push %s to layer %s."), *GetName(), *GetNameSafe(WidgetClass), *LayerTag.ToString());
	}
	return Widget;
}

void UknightPrimaryGameLayout::RemoveWidget(UCommonActivatableWidget* Widget)
{
	if (!Widget)
	{
		return;
	}
	for (const TPair<FGameplayTag, TObjectPtr<UCommonActivatableWidgetContainerBase>>& Layer : Layers)
	{
		if (Layer.Value)
		{
			Layer.Value->RemoveWidget(*Widget);
		}
	}
}

UCommonActivatableWidgetContainerBase* UknightPrimaryGameLayout::GetLayer(FGameplayTag LayerTag) const
{
	const TObjectPtr<UCommonActivatableWidgetContainerBase>* const Found = Layers.Find(LayerTag);
	return Found ? Found->Get() : nullptr;
}
