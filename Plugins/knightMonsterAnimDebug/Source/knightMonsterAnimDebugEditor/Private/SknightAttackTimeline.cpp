#include "SknightAttackTimeline.h"

#include "Animation/AnimMontage.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "knightAttackPreviewModel.h"

#define LOCTEXT_NAMESPACE "knightAttackTimeline"

namespace knightAttackTimeline
{
	constexpr float LabelWidth = 70.f;
	constexpr float RightPadding = 10.f;
	constexpr float RowHeight = 18.f;
	constexpr float RowGap = 6.f;
	constexpr int32 NumRows = 4;
	constexpr int32 RateRow = 2;
	/** 구간 끝을 잡을 수 있는 거리(픽셀). */
	constexpr float EdgeGrabPixels = 5.f;
	/** 빈 곳을 이만큼(픽셀) 끌어야 새 구간을 만든다. 그보다 짧으면 클릭으로 보고 시간만 옮긴다. */
	constexpr float CreateDragPixels = 4.f;
	constexpr float NewWindowRate = 1.5f;
	constexpr float MinWindowLength = 1.f / 30.f;

	struct FPainter
	{
		const FGeometry& Geometry;
		FSlateWindowElementList& Elements;
		int32 Layer;
		float Duration;
		float Width;

		float X(float Time) const { return LabelWidth + FMath::Clamp(Time / Duration, 0.f, 1.f) * Width; }

		void Box(float Left, float Top, float BoxWidth, float BoxHeight, const FLinearColor& Color, int32 LayerOffset = 0) const
		{
			if (BoxWidth <= 0.f || BoxHeight <= 0.f)
			{
				return;
			}
			FSlateDrawElement::MakeBox(Elements, Layer + LayerOffset,
				Geometry.ToPaintGeometry(FVector2f(BoxWidth, BoxHeight), FSlateLayoutTransform(FVector2f(Left, Top))),
				FAppStyle::GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Color);
		}

		/** 테두리만 있는 사각형(선택 표시). */
		void Frame(float Left, float Top, float BoxWidth, float BoxHeight, const FLinearColor& Color, int32 LayerOffset = 0) const
		{
			Box(Left, Top, BoxWidth, 1.f, Color, LayerOffset);
			Box(Left, Top + BoxHeight - 1.f, BoxWidth, 1.f, Color, LayerOffset);
			Box(Left, Top, 1.f, BoxHeight, Color, LayerOffset);
			Box(Left + BoxWidth - 1.f, Top, 1.f, BoxHeight, Color, LayerOffset);
		}

		void Text(float Left, float Top, const FString& String, const FLinearColor& Color, int32 LayerOffset = 2) const
		{
			FSlateDrawElement::MakeText(Elements, Layer + LayerOffset,
				Geometry.ToPaintGeometry(FVector2f(400.f, RowHeight), FSlateLayoutTransform(FVector2f(Left, Top))),
				String, FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8), ESlateDrawEffect::None, Color);
		}

		static float RowTop(int32 Row) { return 4.f + Row * (RowHeight + RowGap); }
	};

	static float GetTrackWidth(const FGeometry& Geometry)
	{
		return FMath::Max(10.f, Geometry.GetLocalSize().X - LabelWidth - RightPadding);
	}

	/** 느리게(1보다 작게)는 파랑, 빠르게는 주황. 1에서 멀수록 진하다. */
	static FLinearColor RateColor(float Rate)
	{
		const float Strength = FMath::Clamp(FMath::Abs(FMath::Loge(FMath::Max(Rate, 0.01f))) / FMath::Loge(3.f), 0.25f, 1.f);
		return Rate < 1.f ? FLinearColor(0.2f, 0.45f, 1.f, 0.35f + 0.55f * Strength) : FLinearColor(1.f, 0.55f, 0.15f, 0.35f + 0.55f * Strength);
	}
}

void SknightAttackTimeline::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
}

FVector2D SknightAttackTimeline::ComputeDesiredSize(float) const
{
	using namespace knightAttackTimeline;
	return FVector2D(500.f, 4.f + NumRows * (RowHeight + RowGap) + 16.f);
}

const FknightMontageTimeMap& SknightAttackTimeline::GetPaintMap() const
{
	return Drag != EDrag::None && Drag != EDrag::Scrub ? DragMap : Model->GetTimeMap();
}

float SknightAttackTimeline::GetPaintDuration() const
{
	return Drag != EDrag::None && Drag != EDrag::Scrub ? DragDuration : Model->GetDuration();
}

int32 SknightAttackTimeline::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace knightAttackTimeline;

	const FVector2f Size = AllottedGeometry.GetLocalSize();
	const FPainter P{ AllottedGeometry, OutDrawElements, LayerId, Model.IsValid() ? GetPaintDuration() : 1.f, GetTrackWidth(AllottedGeometry) };
	P.Box(0.f, 0.f, Size.X, Size.Y, FLinearColor(0.02f, 0.02f, 0.02f, 1.f));
	if (!Model.IsValid())
	{
		return LayerId + 3;
	}

	const FLinearColor LabelColor(0.7f, 0.7f, 0.7f);
	const FLinearColor RowBack(0.08f, 0.08f, 0.08f);
	const FknightMontageTimeMap& Map = GetPaintMap();

	// 1행: 어빌리티 구간.
	const FknightAbilityAnimInfo* const Ability = Model->GetAbility();
	{
		const float Top = P.RowTop(0);
		P.Text(4.f, Top + 2.f, TEXT("어빌리티"), LabelColor);
		P.Box(LabelWidth, Top, P.Width, RowHeight, RowBack);
		if (Ability)
		{
			float Start = 0.f;
			for (const FknightAnimDebugPhase& Phase : Ability->Phases)
			{
				FLinearColor Color(Phase.Color);
				Color.A = 0.85f;
				P.Box(P.X(Start), Top, P.X(Start + Phase.Duration) - P.X(Start), RowHeight, Color, 1);
				P.Text(P.X(Start) + 3.f, Top + 2.f, FString::Printf(TEXT("%s %.2f"), *Phase.Label, Phase.Duration), FLinearColor::Black);
				Start += Phase.Duration;
			}
			if (Ability->HitTime >= 0.f)
			{
				P.Box(P.X(Ability->HitTime) - 1.f, Top - 2.f, 3.f, RowHeight + 4.f, FLinearColor::Red, 2);
			}
		}
	}

	// 2행: 몽타주. 노티파이·판정·섹션을 실제 시간 위치에 그린다(구간 속도가 있으면 몽타주 시간과 다르다).
	const UAnimMontage* const Montage = Model->GetMontage();
	{
		const float Top = P.RowTop(1);
		P.Text(4.f, Top + 2.f, TEXT("몽타주"), LabelColor);
		P.Box(LabelWidth, Top, P.Width, RowHeight, RowBack);
		if (Montage)
		{
			P.Box(P.X(0.f), Top, P.X(Map.GetRealLength()) - P.X(0.f), RowHeight, FLinearColor(0.25f, 0.25f, 0.3f), 1);
			for (const FAnimNotifyEvent& Notify : Montage->Notifies)
			{
				if (!Notify.NotifyStateClass)
				{
					P.Box(P.X(Map.ToReal(Notify.GetTriggerTime())) - 1.f, Top, 3.f, RowHeight * 0.4f, FLinearColor::Yellow, 2);
				}
			}
			for (const FknightHitWindow& Window : Model->GetHitWindows())
			{
				const float WindowStart = P.X(Map.ToReal(Window.Start));
				P.Box(WindowStart, Top + RowHeight * 0.5f, P.X(Map.ToReal(Window.End)) - WindowStart, RowHeight * 0.5f, FLinearColor(1.f, 0.15f, 0.15f, 0.9f), 2);
			}
			for (const FCompositeSection& Section : Montage->CompositeSections)
			{
				P.Box(P.X(Map.ToReal(Section.GetTime())), Top, 1.f, RowHeight, FLinearColor(1.f, 1.f, 1.f, 0.8f), 3);
			}
			const float RateScale = knightAnimAbilityInfo::GetMontageRate(Montage);
			FString Label = Montage->GetName();
			if (!FMath::IsNearlyEqual(RateScale, 1.f))
			{
				Label += FString::Printf(TEXT(" x%.2f"), RateScale);
			}
			if (!Model->GetRateWindows().IsEmpty())
			{
				Label += FString::Printf(TEXT("  실제 %.2fs (구간 속도 없이 %.2fs)"), Map.GetRealLength(), Montage->GetPlayLength() / RateScale);
			}
			P.Text(P.X(0.f) + 3.f, Top + 1.f, Label, FLinearColor::White, 4);
		}
		else
		{
			P.Text(LabelWidth + 4.f, Top + 2.f, TEXT("몽타주 없음"), LabelColor);
		}
	}

	// 3행: 구간 속도.
	{
		const float Top = P.RowTop(RateRow);
		P.Text(4.f, Top + 2.f, TEXT("구간 속도"), LabelColor);
		P.Box(LabelWidth, Top, P.Width, RowHeight, RowBack);
		if (Montage && !Model->CanEditRate())
		{
			P.Text(LabelWidth + 4.f, Top + 2.f, TEXT("설정의 PlayRateNotifyClass를 찾지 못했다"), LabelColor);
		}
		else if (Montage)
		{
			for (const FknightRateWindow& Window : Model->GetRateWindows())
			{
				const float Left = P.X(Map.ToReal(Window.Start));
				const float Right = P.X(Map.ToReal(Window.End));
				P.Box(Left, Top, Right - Left, RowHeight, RateColor(Window.Rate), 1);
				P.Text(Left + 3.f, Top + 2.f, FString::Printf(TEXT("x%.2g"), Window.Rate), FLinearColor::White);
				if (Window.Guid == Model->SelectedRateWindow)
				{
					P.Frame(Left, Top, Right - Left, RowHeight, FLinearColor::White, 3);
				}
			}
			if (Model->GetRateWindows().IsEmpty())
			{
				P.Text(LabelWidth + 4.f, Top + 2.f, TEXT("끌어서 구간 추가 · 휠: 배율 · 우클릭: 메뉴"), FLinearColor(0.45f, 0.45f, 0.45f));
			}
		}
	}

	// 4행: 더미 명중.
	{
		const float Top = P.RowTop(3);
		P.Text(4.f, Top + 2.f, TEXT("더미 명중"), LabelColor);
		P.Box(LabelWidth, Top, P.Width, RowHeight, RowBack);
		if (Model->GetMeleeHitTime() >= 0.f)
		{
			P.Box(P.X(Model->GetMeleeHitTime()) - 3.f, Top + 3.f, 7.f, RowHeight - 6.f, FLinearColor::Red, 2);
			P.Text(P.X(Model->GetMeleeHitTime()) + 6.f, Top + 2.f, FString::Printf(TEXT("근접 %.2fs"), Model->GetMeleeHitTime()), FLinearColor::White);
		}
		if (Ability && Ability->bHasArea && Model->IsAreaHit() && Ability->HitTime >= 0.f)
		{
			P.Box(P.X(Ability->HitTime) - 3.f, Top + 3.f, 7.f, RowHeight - 6.f, FLinearColor(1.f, 0.55f, 0.1f), 2);
			P.Text(P.X(Ability->HitTime) + 6.f, Top + 2.f, TEXT("범위"), FLinearColor::White);
		}
	}

	// 눈금: 0.1초마다 짧게, 0.5초마다 길게 숫자와 함께.
	const float RulerTop = P.RowTop(NumRows) - 2.f;
	for (int32 Tick = 0; Tick * 0.1f <= P.Duration + KINDA_SMALL_NUMBER; ++Tick)
	{
		const float TickTime = Tick * 0.1f;
		const bool bMajor = Tick % 5 == 0;
		P.Box(P.X(TickTime), RulerTop, 1.f, bMajor ? 6.f : 3.f, LabelColor, 1);
		if (bMajor)
		{
			P.Text(P.X(TickTime) + 2.f, RulerTop + 2.f, FString::Printf(TEXT("%.1f"), TickTime), LabelColor);
		}
	}

	// 현재 시간.
	P.Box(P.X(Model->GetTime()) - 1.f, 0.f, 2.f, RulerTop, FLinearColor::White, 5);
	return LayerId + 6;
}

float SknightAttackTimeline::LocalXToRealTime(const FGeometry& MyGeometry, float LocalX) const
{
	using namespace knightAttackTimeline;
	return FMath::Clamp((LocalX - LabelWidth) / GetTrackWidth(MyGeometry), 0.f, 1.f) * GetPaintDuration();
}

bool SknightAttackTimeline::IsInRateRow(const FGeometry& MyGeometry, const FVector2f& Local) const
{
	using namespace knightAttackTimeline;
	const float Top = FPainter::RowTop(RateRow);
	return Local.X >= LabelWidth && Local.Y >= Top - 2.f && Local.Y <= Top + RowHeight + 2.f;
}

SknightAttackTimeline::FRateHit SknightAttackTimeline::HitTestRate(const FGeometry& MyGeometry, const FVector2f& Local) const
{
	using namespace knightAttackTimeline;
	FRateHit Hit;
	if (!Model.IsValid() || !IsInRateRow(MyGeometry, Local))
	{
		return Hit;
	}
	const FknightMontageTimeMap& Map = GetPaintMap();
	const float Width = GetTrackWidth(MyGeometry);
	const float Duration = GetPaintDuration();
	auto ToX = [&](float MontageTime) { return LabelWidth + FMath::Clamp(Map.ToReal(MontageTime) / Duration, 0.f, 1.f) * Width; };

	// 끝 잡기를 안쪽 잡기보다 먼저 본다(짧은 구간도 길이를 바꿀 수 있게). 여러 끝이 가까우면 가장 가까운 것.
	float BestEdgeDistance = EdgeGrabPixels;
	for (const FknightRateWindow& Window : Model->GetRateWindows())
	{
		const float StartDistance = FMath::Abs(Local.X - ToX(Window.Start));
		const float EndDistance = FMath::Abs(Local.X - ToX(Window.End));
		if (StartDistance <= BestEdgeDistance)
		{
			BestEdgeDistance = StartDistance;
			Hit = { Window.Guid, EDrag::ResizeStart };
		}
		if (EndDistance <= BestEdgeDistance)
		{
			BestEdgeDistance = EndDistance;
			Hit = { Window.Guid, EDrag::ResizeEnd };
		}
	}
	if (Hit.Guid.IsValid())
	{
		return Hit;
	}
	for (const FknightRateWindow& Window : Model->GetRateWindows())
	{
		if (Local.X > ToX(Window.Start) && Local.X < ToX(Window.End))
		{
			return { Window.Guid, EDrag::Move };
		}
	}
	return Hit;
}

void SknightAttackTimeline::ScrubTo(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	if (!Model.IsValid())
	{
		return;
	}
	const FVector2f Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	Model->bPlaying = false;
	Model->SetTime(LocalXToRealTime(MyGeometry, Local.X));
}

FReply SknightAttackTimeline::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!Model.IsValid())
	{
		return FReply::Unhandled();
	}
	const FVector2f Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const bool bRateRow = IsInRateRow(MyGeometry, Local) && Model->CanEditRate();

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		const FRateHit Hit = bRateRow ? HitTestRate(MyGeometry, Local) : FRateHit();
		if (Hit.Guid.IsValid())
		{
			Model->SelectedRateWindow = Hit.Guid;
			OpenRateMenu(MouseEvent, Hit.Guid);
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	if (!bRateRow)
	{
		Drag = EDrag::Scrub;
		ScrubTo(MyGeometry, MouseEvent);
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	// 끌기 동안 쓸 시간축을 먼저 고정한 뒤 마우스 위치를 몽타주 시간으로 바꾼다.
	const FRateHit Hit = HitTestRate(MyGeometry, Local);
	DragMap = Model->GetTimeMap();
	DragDuration = Model->GetDuration();
	Drag = Hit.Guid.IsValid() ? Hit.Part : EDrag::Create;
	DragGuid = Hit.Guid;
	DragStartX = Local.X;
	DragAnchor = DragMap.ToMontage(LocalXToRealTime(MyGeometry, Local.X));
	Model->SelectedRateWindow = Hit.Guid;
	if (const FknightRateWindow* const Window = Model->GetSelectedRateWindow())
	{
		DragOriginalStart = Window->Start;
		DragOriginalEnd = Window->End;
		Model->BeginRateDrag(Drag == EDrag::Move ? LOCTEXT("MoveRate", "구간 속도 이동") : LOCTEXT("ResizeRate", "구간 속도 길이"));
	}
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

void SknightAttackTimeline::UpdateRateDrag(const FGeometry& MyGeometry, const FVector2f& Local)
{
	using namespace knightAttackTimeline;
	const UAnimMontage* const Montage = Model->GetMontage();
	if (!Montage)
	{
		return;
	}
	const float Length = Montage->GetPlayLength();
	const float MontageTime = FMath::Clamp(DragMap.ToMontage(LocalXToRealTime(MyGeometry, Local.X)), 0.f, Length);
	switch (Drag)
	{
	case EDrag::Create:
		if (!DragGuid.IsValid())
		{
			if (FMath::Abs(Local.X - DragStartX) < CreateDragPixels)
			{
				return;
			}
			Model->BeginRateDrag(LOCTEXT("AddRate", "구간 속도 추가"));
			DragGuid = Model->AddRateWindow(FMath::Min(DragAnchor, MontageTime), FMath::Max(DragAnchor, MontageTime), NewWindowRate);
			Model->SelectedRateWindow = DragGuid;
		}
		else
		{
			Model->SetRateWindowRange(DragGuid, FMath::Min(DragAnchor, MontageTime), FMath::Max(DragAnchor, MontageTime));
		}
		break;
	case EDrag::Move:
	{
		const float WindowLength = DragOriginalEnd - DragOriginalStart;
		const float Start = FMath::Clamp(DragOriginalStart + MontageTime - DragAnchor, 0.f, FMath::Max(0.f, Length - WindowLength));
		Model->SetRateWindowRange(DragGuid, Start, Start + WindowLength);
		break;
	}
	case EDrag::ResizeStart:
		Model->SetRateWindowRange(DragGuid, FMath::Min(MontageTime, DragOriginalEnd - MinWindowLength), DragOriginalEnd);
		break;
	case EDrag::ResizeEnd:
		Model->SetRateWindowRange(DragGuid, DragOriginalStart, FMath::Max(MontageTime, DragOriginalStart + MinWindowLength));
		break;
	default:
		break;
	}
}

FReply SknightAttackTimeline::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!HasMouseCapture())
	{
		return FReply::Unhandled();
	}
	if (Drag == EDrag::Scrub)
	{
		ScrubTo(MyGeometry, MouseEvent);
	}
	else if (Drag != EDrag::None)
	{
		UpdateRateDrag(MyGeometry, MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	}
	return FReply::Handled();
}

FReply SknightAttackTimeline::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!HasMouseCapture())
	{
		return FReply::Unhandled();
	}
	// 빈 곳을 끌지 않고 놓았으면 클릭이다: 그 시간으로 이동한다.
	const bool bClick = Drag == EDrag::Create && !DragGuid.IsValid();
	FinishDrag();
	if (bClick)
	{
		ScrubTo(MyGeometry, MouseEvent);
	}
	return FReply::Handled().ReleaseMouseCapture();
}

void SknightAttackTimeline::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	FinishDrag();
}

void SknightAttackTimeline::FinishDrag()
{
	if (Model.IsValid() && Model->IsRateDragging())
	{
		Model->EndRateDrag();
	}
	Drag = EDrag::None;
	DragGuid.Invalidate();
}

FReply SknightAttackTimeline::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!Model.IsValid() || Drag != EDrag::None)
	{
		return FReply::Unhandled();
	}
	const FRateHit Hit = HitTestRate(MyGeometry, MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	Model->SelectedRateWindow = Hit.Guid.IsValid() ? Hit.Guid : Model->SelectedRateWindow;
	const FknightRateWindow* const Window = Hit.Guid.IsValid() ? Model->GetSelectedRateWindow() : nullptr;
	if (!Window)
	{
		return FReply::Unhandled();
	}
	const float Step = MouseEvent.IsShiftDown() ? 0.5f : 0.1f;
	const float NewRate = FMath::RoundToFloat((Window->Rate + FMath::Sign(MouseEvent.GetWheelDelta()) * Step) * 20.f) / 20.f;
	Model->SetRateWindowRate(Hit.Guid, NewRate);
	return FReply::Handled();
}

FCursorReply SknightAttackTimeline::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	EDrag Part = Drag;
	if (Part == EDrag::None)
	{
		Part = HitTestRate(MyGeometry, MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition())).Part;
	}
	switch (Part)
	{
	case EDrag::ResizeStart:
	case EDrag::ResizeEnd:
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	case EDrag::Move:
		return FCursorReply::Cursor(Drag == EDrag::Move ? EMouseCursor::GrabHandClosed : EMouseCursor::GrabHand);
	default:
		return FCursorReply::Unhandled();
	}
}

void SknightAttackTimeline::OpenRateMenu(const FPointerEvent& MouseEvent, const FGuid& Guid)
{
	TSharedPtr<FknightAttackPreviewModel> MenuModel = Model;
	FMenuBuilder Menu(true, nullptr);
	Menu.BeginSection(NAME_None, LOCTEXT("RateSection", "배율"));
	for (const float Rate : { 0.5f, 0.75f, 1.25f, 1.5f, 2.f, 3.f })
	{
		Menu.AddMenuEntry(FText::FromString(FString::Printf(TEXT("x%.2g"), Rate)), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([MenuModel, Guid, Rate]() { MenuModel->SetRateWindowRate(Guid, Rate); })));
	}
	Menu.EndSection();
	Menu.AddSeparator();
	Menu.AddMenuEntry(LOCTEXT("RemoveRate", "구간 삭제"), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([MenuModel, Guid]() { MenuModel->RemoveRateWindow(Guid); })));

	FSlateApplication::Get().PushMenu(AsShared(), FWidgetPath(), Menu.MakeWidget(), MouseEvent.GetScreenSpacePosition(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

#undef LOCTEXT_NAMESPACE
