#include "HUD/knightHUDLockOnMarker.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Combat/knightTargetLockComponent.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Engine/LocalPlayer.h"
#include "Foundation/knightUISubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "ViewModels/knightVM_PlayerStatus.h"

namespace
{
	float Phase(float Time, float Start, float Duration)
	{
		return Duration > 0.f ? FMath::Clamp((Time - Start) / Duration, 0.f, 1.f) : (Time >= Start ? 1.f : 0.f);
	}

	float EaseOut(float Alpha)
	{
		return 1.f - FMath::Pow(1.f - Alpha, 3.f);
	}

	void PlaceInCanvas(UWidget* Widget, const FVector2D& Position, const FVector2D& Alignment)
	{
		if (UCanvasPanelSlot* const Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
		{
			Slot->SetPosition(Position);
			Slot->SetAlignment(Alignment);
		}
	}
}

void UknightHUDLockOnMarker::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);

	const ULocalPlayer* const LocalPlayer = GetOwningLocalPlayer();
	const UknightUISubsystem* const UI = LocalPlayer ? LocalPlayer->GetSubsystem<UknightUISubsystem>() : nullptr;
	UknightVM_PlayerStatus* const VM = UI ? UI->GetPlayerStatusViewModel() : nullptr;
	ViewModel = VM;
	if (VM)
	{
		using FFields = UknightVM_PlayerStatus::FFieldNotificationClassDescriptor;
		VM->AddFieldValueChangedDelegate(FFields::bLockedOn, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleViewModelChanged));
		VM->AddFieldValueChangedDelegate(FFields::LockedTargetName, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleViewModelChanged));
		VM->AddFieldValueChangedDelegate(FFields::bHasLockedPart, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleViewModelChanged));
		VM->AddFieldValueChangedDelegate(FFields::LockedPartName, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleViewModelChanged));
		VM->AddFieldValueChangedDelegate(FFields::LockedPartCondition, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleViewModelChanged));
		VM->AddFieldValueChangedDelegate(FFields::LockedTargetStagger, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleStaggerChanged));
		VM->AddFieldValueChangedDelegate(FFields::bLockedTargetHasStagger, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleStaggerChanged));
		PartConditionHandle = VM->OnLockedPartConditionChanged.AddUObject(this, &ThisClass::HandleLockedPartConditionChanged);
	}
	RefreshLockState();
}

void UknightHUDLockOnMarker::NativeDestruct()
{
	if (UknightVM_PlayerStatus* const VM = ViewModel.Get())
	{
		VM->RemoveAllFieldValueChangedDelegates(this);
		VM->OnLockedPartConditionChanged.Remove(PartConditionHandle);
	}
	PartConditionHandle.Reset();
	ViewModel.Reset();
	StopTicking();
	Super::NativeDestruct();
}

void UknightHUDLockOnMarker::HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId)
{
	RefreshLockState();
}

UknightTargetLockComponent* UknightHUDLockOnMarker::FindTargetLock() const
{
	const APawn* const Pawn = GetOwningPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<UknightTargetLockComponent>() : nullptr;
}

void UknightHUDLockOnMarker::RefreshLockState()
{
	const UknightVM_PlayerStatus* const VM = ViewModel.Get();
	const UknightTargetLockComponent* const TargetLock = FindTargetLock();
	AActor* const Target = VM && VM->IsLockedOn() && TargetLock ? TargetLock->GetLockedTarget() : nullptr;

	if (!Target)
	{
		ShownTarget.Reset();
		StopTicking();
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (Txt_TargetName)
	{
		Txt_TargetName->SetText(VM->GetLockedTargetName());
	}
	RefreshPart();
	RefreshStagger();
	if (ShownTarget.Get() != Target)
	{
		// A new lock (or a switch to another target) replays the whole open motion.
		ShownTarget = Target;
		AnimTime = 0.f;
		FlashElapsed = TNumericLimits<float>::Max();
		StaggerFlashElapsed = TNumericLimits<float>::Max();
		ShownStagger = VM->GetLockedTargetStagger();
		ShownDistance = -1;
		if (LockOnSound)
		{
			UGameplayStatics::PlaySound2D(this, LockOnSound);
		}
	}
	if (!Ticker.IsValid() && !IsDesignTime())
	{
		Ticker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime) { return TickMarker(DeltaTime); }));
	}
	// Placed before the first frame so the marker never flashes at the screen origin.
	SetVisibility(UpdateLayout() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ApplyAnimation();
}

void UknightHUDLockOnMarker::RefreshPart()
{
	const UknightVM_PlayerStatus* const VM = ViewModel.Get();
	const bool bHasPart = VM && VM->HasLockedPart();
	const ESlateVisibility FrameVisibility = bHasPart ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (PartFrame)
	{
		PartFrame->SetVisibility(FrameVisibility);
		UImage* const Image = Cast<UImage>(PartFrame);
		if (bHasPart && Image)
		{
			const EknightPartCondition Condition = VM->GetLockedPartCondition();
			Image->SetColorAndOpacity(Condition == EknightPartCondition::Normal ? NormalColor
				: Condition == EknightPartCondition::Critical ? CriticalColor
				: Condition == EknightPartCondition::Broken ? BrokenColor
				: UnbreakableColor);
		}
	}
	if (PartFlash)
	{
		PartFlash->SetVisibility(FrameVisibility);
	}
	if (Txt_PartName)
	{
		const FText PartName = bHasPart ? VM->GetLockedPartName() : FText::GetEmpty();
		Txt_PartName->SetText(PartName);
		Txt_PartName->SetVisibility(PartName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UknightHUDLockOnMarker::HandleStaggerChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId)
{
	const UknightVM_PlayerStatus* const VM = ViewModel.Get();
	const float NewStagger = VM ? VM->GetLockedTargetStagger() : 0.f;
	// Blink once when the gauge fills on the target already shown (a fresh lock on a staggered target does not blink).
	if (ShownTarget.IsValid() && NewStagger >= 1.f && ShownStagger < 1.f)
	{
		StaggerFlashElapsed = 0.f;
	}
	ShownStagger = NewStagger;
	RefreshStagger();
}

void UknightHUDLockOnMarker::RefreshStagger()
{
	const UknightVM_PlayerStatus* const VM = ViewModel.Get();
	const bool bShow = VM && VM->LockedTargetHasStagger();
	if (Stagger)
	{
		Stagger->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Bar_Stagger && bShow)
	{
		Bar_Stagger->SetPercent(VM->GetLockedTargetStagger());
	}
}

void UknightHUDLockOnMarker::HandleLockedPartConditionChanged(EknightPartCondition NewCondition)
{
	FlashElapsed = 0.f;
	if (PartConditionSound && ShownTarget.IsValid())
	{
		UGameplayStatics::PlaySound2D(this, PartConditionSound);
	}
}

void UknightHUDLockOnMarker::StopTicking()
{
	if (Ticker.IsValid())
	{
		FTSTicker::RemoveTicker(Ticker);
		Ticker.Reset();
	}
}

bool UknightHUDLockOnMarker::TickMarker(float DeltaTime)
{
	if (!ShownTarget.IsValid())
	{
		Ticker.Reset();
		SetVisibility(ESlateVisibility::Collapsed);
		return false;
	}
	const float Step = FMath::Min(DeltaTime, 0.05f);
	AnimTime += Step;
	if (FlashElapsed < FlashTime)
	{
		FlashElapsed += Step;
	}
	if (StaggerFlashElapsed < StaggerFlashTime)
	{
		StaggerFlashElapsed += Step;
	}
	SetVisibility(UpdateLayout() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ApplyAnimation();
	return true;
}

bool UknightHUDLockOnMarker::UpdateLayout()
{
	APlayerController* const PC = GetOwningPlayer();
	const UknightTargetLockComponent* const TargetLock = FindTargetLock();
	FVector WorldLocation;
	if (!PC || !TargetLock || !TargetLock->GetLockedTargetLocation(WorldLocation))
	{
		return false;
	}

	FVector2D ScreenPixels;
	if (!UGameplayStatics::ProjectWorldToScreen(PC, WorldLocation, ScreenPixels, true))
	{
		// Behind the camera.
		return false;
	}

	// Pixels to slate units, so the layout matches the 1080p numbers at any resolution.
	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	const FVector2D Center = ScreenPixels / Scale;
	const FVector2D Viewport = UWidgetLayoutLibrary::GetViewportSize(this) / Scale;

	bFlipped = Center.X > Viewport.X * FlipScreenFraction;
	const float Side = bFlipped ? -1.f : 1.f;
	const float Half = BoxSize * 0.5f;
	const float Radians = FMath::DegreesToRadians(DiagonalAngle);

	const FVector2D Corner = Center + FVector2D(Side * Half, -Half);
	const FVector2D Elbow = Corner + FVector2D(Side * FMath::Cos(Radians) * DiagonalLength, -FMath::Sin(Radians) * DiagonalLength);
	const FVector2D ShelfEnd = Elbow + FVector2D(Side * ShelfLength, 0.f);

	PlaceInCanvas(Box, Center, FVector2D(0.5f, 0.5f));
	PlaceInCanvas(PartFrame, Center, FVector2D(0.5f, 0.5f));
	PlaceInCanvas(PartFlash, Center, FVector2D(0.5f, 0.5f));

	// Lines start at their pivot end and are rotated around it; mirroring flips the angle to the other side.
	PlaceInCanvas(Line_Diagonal, Corner, FVector2D(0.f, 0.5f));
	if (Line_Diagonal)
	{
		Line_Diagonal->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
		Line_Diagonal->SetRenderTransformAngle(bFlipped ? -(180.f - DiagonalAngle) : -DiagonalAngle);
		if (UCanvasPanelSlot* const LineSlot = Cast<UCanvasPanelSlot>(Line_Diagonal->Slot))
		{
			LineSlot->SetSize(FVector2D(DiagonalLength, LineSlot->GetSize().Y));
		}
	}
	PlaceInCanvas(Line_Shelf, Elbow, FVector2D(bFlipped ? 1.f : 0.f, 0.5f));
	if (Line_Shelf)
	{
		Line_Shelf->SetRenderTransformPivot(FVector2D(bFlipped ? 1.f : 0.f, 0.5f));
		if (UCanvasPanelSlot* const LineSlot = Cast<UCanvasPanelSlot>(Line_Shelf->Slot))
		{
			LineSlot->SetSize(FVector2D(ShelfLength, LineSlot->GetSize().Y));
		}
	}

	// The plate sits on the shelf, starting at the elbow, and never leaves the safe zone.
	FVector2D PlatePosition = Elbow + FVector2D(Side * PlateOffset.X, PlateOffset.Y);
	PlatePosition.X = FMath::Clamp(PlatePosition.X, SafeMargin.X, Viewport.X - SafeMargin.X);
	PlatePosition.Y = FMath::Clamp(PlatePosition.Y, SafeMargin.Y, Viewport.Y - SafeMargin.Y);
	PlaceInCanvas(Plate, PlatePosition, FVector2D(bFlipped ? 1.f : 0.f, 1.f));

	PlaceInCanvas(Stagger, Center + FVector2D(0.f, StaggerOffset), FVector2D(0.5f, 0.f));
	PlaceInCanvas(Txt_Distance, Center + FVector2D(0.f, DistanceOffset), FVector2D(0.5f, 0.f));
	if (Txt_Distance)
	{
		// Whole meters from the player, so the text only changes when the number does.
		const APawn* const Pawn = GetOwningPlayerPawn();
		const int32 Meters = Pawn ? FMath::RoundToInt(FVector::Dist(Pawn->GetActorLocation(), WorldLocation) / 100.f) : 0;
		if (Meters != ShownDistance)
		{
			ShownDistance = Meters;
			Txt_Distance->SetText(FText::Format(NSLOCTEXT("knightHUD", "LockDistance", "[ {0}M ]"), FText::AsNumber(Meters)));
		}
	}
	return true;
}

void UknightHUDLockOnMarker::ApplyAnimation()
{
	// Box closes in, then the line draws out (diagonal, then shelf), then the plate slides in.
	const float BoxAlpha = EaseOut(Phase(AnimTime, 0.f, BoxInTime));
	const float DiagonalAlpha = EaseOut(Phase(AnimTime, BoxInTime, LineDrawTime * 0.5f));
	const float ShelfAlpha = EaseOut(Phase(AnimTime, BoxInTime + LineDrawTime * 0.5f, LineDrawTime * 0.5f));
	const float PlateAlpha = EaseOut(Phase(AnimTime, BoxInTime + LineDrawTime, PlateInTime));

	if (Box)
	{
		Box->SetRenderScale(FVector2D(FMath::Lerp(BoxStartScale, 1.f, BoxAlpha)));
		Box->SetRenderOpacity(BoxAlpha);
	}
	if (PartFrame)
	{
		// The frame opens with the box, then blinks slowly while the part is critical.
		const UknightVM_PlayerStatus* const VM = ViewModel.Get();
		float Opacity = BoxAlpha;
		if (VM && VM->GetLockedPartCondition() == EknightPartCondition::Critical)
		{
			const float Wave = 0.5f + 0.5f * FMath::Cos(AnimTime * UE_TWO_PI / CriticalBlinkPeriod);
			Opacity *= FMath::Lerp(CriticalBlinkMinOpacity, 1.f, Wave);
		}
		PartFrame->SetRenderScale(FVector2D(FMath::Lerp(BoxStartScale, 1.f, BoxAlpha)));
		PartFrame->SetRenderOpacity(Opacity);
	}
	if (PartFlash)
	{
		const float FlashAlpha = FlashTime > 0.f && FlashElapsed < FlashTime ? FlashElapsed / FlashTime : 1.f;
		PartFlash->SetRenderScale(FVector2D(FMath::Lerp(FlashStartScale, 1.f, EaseOut(FlashAlpha))));
		PartFlash->SetRenderOpacity(1.f - FlashAlpha);
	}
	if (Line_Diagonal)
	{
		FWidgetTransform Transform = Line_Diagonal->GetRenderTransform();
		Transform.Scale = FVector2D(DiagonalAlpha, 1.f);
		Line_Diagonal->SetRenderTransform(Transform);
	}
	if (Line_Shelf)
	{
		Line_Shelf->SetRenderScale(FVector2D(ShelfAlpha, 1.f));
	}
	if (Stagger)
	{
		Stagger->SetRenderOpacity(PlateAlpha);
	}
	if (Txt_Distance)
	{
		Txt_Distance->SetRenderOpacity(PlateAlpha);
	}
	if (Bar_Stagger)
	{
		const bool bBlinking = StaggerFlashElapsed < StaggerFlashTime;
		const bool bLit = bBlinking && FMath::Fmod(StaggerFlashElapsed, StaggerBlinkPeriod) < StaggerBlinkPeriod * 0.5f;
		Bar_Stagger->SetFillColorAndOpacity(bLit ? StaggerFullColor : StaggerColor);
	}
	if (Plate)
	{
		Plate->SetRenderOpacity(PlateAlpha);
		Plate->SetRenderTranslation(FVector2D((bFlipped ? 8.f : -8.f) * (1.f - PlateAlpha), 0.f));
	}
}
