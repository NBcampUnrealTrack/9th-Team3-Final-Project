#pragma once

#include "CoreMinimal.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SlateWrapperTypes.h"
#include "Slate/WidgetTransform.h"
#include "KnightOperationMapTransition.generated.h"

class UKnightMainMenuWidget;
class UCanvasPanel;
class URetainerBox;

USTRUCT(BlueprintType)
struct FKnightOperationMapTransitionSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.1")) float EnterDuration = .38f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.1")) float ExitDuration = .30f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.0")) float RewardDelay = .06f;
	/** Fractions of the arranged widget width, independent of viewport DPI. */
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.0", ClampMax="0.2")) float MapSlideFraction = .035f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.0", ClampMax="0.2")) float RewardSlideFraction = .08f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="0.0", ClampMax="1.0")) float ScanIntensity = .38f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="1.0")) float ScanWidth = 1.5f;
	UPROPERTY(EditAnywhere, meta=(ClampMin="2.0")) float ScanGlowWidth = 16.f;
};

USTRUCT()
struct FKnightMapTransitionTrack
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UWidget> Widget;
	FWidgetTransform Transform;
	float Opacity = 1.f;
	float SlideFraction = 0.f;
	bool bReward = false;
};

USTRUCT()
struct FKnightMapHomeChild
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UWidget> Widget;
	FAnchorData Layout;
	int32 ZOrder = 0;
	bool bAutoSize = false;
};

USTRUCT()
struct FKnightMapCompositeLayer
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UWidget> Content;
	UPROPERTY() TObjectPtr<UCanvasPanel> Parent;
	UPROPERTY() TObjectPtr<URetainerBox> Group;
	FAnchorData Layout;
	int32 ZOrder = 0;
	bool bAutoSize = false;
};

/** Presentation only. Keeps the real Home children alive outside the page switcher during overlap. */
UCLASS()
class KNIGHT_API UKnightOperationMapTransition : public UObject
{
	GENERATED_BODY()
public:
	void Enter(UKnightMainMenuWidget* Menu);
	void Exit(UKnightMainMenuWidget* Menu);
	void Tick(float DeltaTime);
	void Cancel();
	bool IsActive() const { return Phase != EPhase::Idle; }
	bool IsExiting() const { return Phase == EPhase::Exiting; }
	float GetReveal() const { return Reveal; }
	float GetHomeOpacity() const { return HomeAlpha; }
	float GetRewardOpacity() const { return RewardAlpha; }
private:
	enum class EPhase : uint8 { Idle, Entering, Exiting };
	void Prepare(UKnightMainMenuWidget* Menu);
	void PrepareHome();
	void AddTrack(UWidget* Widget, float SlideFraction = 0.f, bool bReward = false);
	UWidget* Composite(UWidget* Content);
	void ReleaseComposites();
	void Apply();
	void Restore();
	UPROPERTY() TObjectPtr<UKnightMainMenuWidget> Owner;
	UPROPERTY() TObjectPtr<UCanvasPanel> HomeOverlay;
	UPROPERTY() TArray<FKnightMapHomeChild> HomeChildren;
	UPROPERTY() TArray<FKnightMapTransitionTrack> Tracks;
	UPROPERTY() TArray<FKnightMapCompositeLayer> Composites;
	FKnightOperationMapTransitionSettings Settings;
	ESlateVisibility StageVisibility = ESlateVisibility::SelfHitTestInvisible;
	ESlateVisibility BackVisibility = ESlateVisibility::Visible;
	EPhase Phase = EPhase::Idle;
	float Elapsed = 0.f;
	float MapAlpha = 0.f, RewardAlpha = 0.f, HomeAlpha = 1.f, Reveal = 0.f;
	float MapOffset = 1.f, RewardOffset = 1.f, Scan = 0.f;
	float StartMapAlpha = 0.f, StartRewardAlpha = 0.f, StartHomeAlpha = 0.f, StartReveal = 0.f;
	float StartMapOffset = 0.f, StartRewardOffset = 0.f, StartScan = 0.f;
};
