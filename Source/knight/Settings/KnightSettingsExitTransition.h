#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Containers/Ticker.h"

class UKnightSettingsWidget;
class UWidgetSwitcher;
class FKnightSettingsExitInput;

/** UI-only, real-time exit. The settings model decides whether closing is allowed. */
class KNIGHT_API FKnightSettingsExitTransition : public TSharedFromThis<FKnightSettingsExitTransition>
{
public:
	explicit FKnightSettingsExitTransition(UKnightSettingsWidget* InOwner);
	~FKnightSettingsExitTransition();
	void Start();
	void Cancel();
	void Advance(float DeltaTime);
	int32 Paint(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer) const;
	float GetElapsed() const { return Elapsed; }

private:
	friend class UknightSettingsCheckCommandlet;
	struct FTrack
	{
		TWeakObjectPtr<UWidget> Widget;
		FWidgetTransform Transform;
		float Opacity = 1.f;
		float Direction = 0.f;
		bool bChrome = false;
	};
	void Restore(bool bCompleted);
	TWeakObjectPtr<UKnightSettingsWidget> Owner;
	TArray<FTrack> Tracks;
	TWeakObjectPtr<UWidget> ReturnPage;
	TWeakObjectPtr<UWidgetSwitcher> Switcher;
	TWeakObjectPtr<UWidget> PreviousPage;
	ESlateVisibility PageVisibility = ESlateVisibility::Visible;
	ESlateVisibility ScreenVisibility = ESlateVisibility::SelfHitTestInvisible;
	float PageOpacity = 1.f;
	float Elapsed = 0.f;
	bool bActive = false;
	FTSTicker::FDelegateHandle Ticker;
	TSharedPtr<FKnightSettingsExitInput> Input;
};
