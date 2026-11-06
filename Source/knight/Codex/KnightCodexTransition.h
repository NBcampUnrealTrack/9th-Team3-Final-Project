#pragma once

#include "CoreMinimal.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SlateWrapperTypes.h"
#include "Slate/WidgetTransform.h"
#include "KnightCodexTransition.generated.h"

class UKnightMainMenuWidget;
class UCanvasPanel;
class UKnightCodexMotionShield;

USTRUCT(BlueprintType)
struct FKnightCodexTransitionSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,meta=(ClampMin="0.1")) float EnterDuration=.43f;
	UPROPERTY(EditAnywhere,meta=(ClampMin="0.1")) float ExitDuration=.31f;
	UPROPERTY(EditAnywhere,meta=(ClampMin="0.1")) float HeaderDuration=.21f;
	UPROPERTY(EditAnywhere,meta=(ClampMin="0.0")) float SlideDistance=42.f;
	UPROPERTY(EditAnywhere,meta=(ClampMin="4.0")) float RevealFeather=28.f;
	UPROPERTY(EditAnywhere,meta=(ClampMin="0.0",ClampMax="1.0")) float ScanIntensity=.5f;
};

UENUM()
enum class EKnightCodexPhase : uint8 { Hidden, Entering, Visible, Exiting };

USTRUCT()
struct FKnightCodexMotionTrack
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UWidget> Widget;
	FWidgetTransform Transform;
	float Opacity=1.f;
	float Direction=0.f;
};

USTRUCT()
struct FKnightCodexHomeChild
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UWidget> Widget;
	FAnchorData Layout;
	int32 ZOrder=0;
	bool bAutoSize=false;
};

/** Codex-only lifecycle; navigation/reward/data loading stay with their existing owners. */
UCLASS()
class KNIGHT_API UKnightCodexTransition : public UObject
{
	GENERATED_BODY()
	friend class UKnightCodexCheckCommandlet;
public:
	void Enter(UKnightMainMenuWidget* Menu);
	void Exit(UKnightMainMenuWidget* Menu);
	void Tick(float Dt);
	void Cancel();
	bool IsActive() const { return Phase==EKnightCodexPhase::Entering || Phase==EKnightCodexPhase::Exiting; }
	EKnightCodexPhase GetPhase() const { return Phase; }
	float GetReveal() const { return Reveal; }
	float GetOpacity() const { return ContentAlpha; }
	float GetHomeOpacity() const { return HomeAlpha; }
private:
	void Prepare(UKnightMainMenuWidget* Menu);
	void PrepareHome();
	void Restore();
	void Apply();
	UPROPERTY() TObjectPtr<UKnightMainMenuWidget> Owner;
	UPROPERTY() TObjectPtr<UCanvasPanel> HomeOverlay;
	UPROPERTY() TObjectPtr<UKnightCodexMotionShield> Shield;
	UPROPERTY() TArray<FKnightCodexHomeChild> HomeChildren;
	UPROPERTY() TArray<FKnightCodexMotionTrack> Tracks;
	FKnightCodexTransitionSettings Settings;
	EKnightCodexPhase Phase=EKnightCodexPhase::Hidden;
	ESlateVisibility PageVisibility=ESlateVisibility::SelfHitTestInvisible;
	float Elapsed=0,HeaderAlpha=0,SideAlpha=0,Offset=1,ContentAlpha=0,Reveal=0,Glow=0,HomeAlpha=1;
	float StartHeader=0,StartSide=0,StartOffset=0,StartContent=0,StartReveal=0,StartGlow=0,StartHome=0;
};
