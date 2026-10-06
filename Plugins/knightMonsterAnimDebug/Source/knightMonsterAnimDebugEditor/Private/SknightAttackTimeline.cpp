#include "SknightAttackTimeline.h"

#include "Animation/AnimMontage.h"
#include "Fonts/SlateFontInfo.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "knightAttackPreviewModel.h"

namespace knightAttackTimeline
{
	constexpr float LabelWidth = 70.f;
	constexpr float RightPadding = 10.f;
	constexpr float RowHeight = 18.f;
	constexpr float RowGap = 6.f;
	constexpr int32 NumRows = 3;

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

		void Text(float Left, float Top, const FString& String, const FLinearColor& Color, int32 LayerOffset = 2) const
		{
			FSlateDrawElement::MakeText(Elements, Layer + LayerOffset,
				Geometry.ToPaintGeometry(FVector2f(400.f, RowHeight), FSlateLayoutTransform(FVector2f(Left, Top))),
				String, FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8), ESlateDrawEffect::None, Color);
		}

		float RowTop(int32 Row) const { return 4.f + Row * (RowHeight + RowGap); }
	};
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

int32 SknightAttackTimeline::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace knightAttackTimeline;

	const FVector2f Size = AllottedGeometry.GetLocalSize();
	const FPainter P{ AllottedGeometry, OutDrawElements, LayerId, Model.IsValid() ? Model->GetDuration() : 1.f, FMath::Max(10.f, Size.X - LabelWidth - RightPadding) };
	P.Box(0.f, 0.f, Size.X, Size.Y, FLinearColor(0.02f, 0.02f, 0.02f, 1.f));
	if (!Model.IsValid())
	{
		return LayerId + 3;
	}

	const FLinearColor LabelColor(0.7f, 0.7f, 0.7f);
	const FLinearColor RowBack(0.08f, 0.08f, 0.08f);

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

	// 2행: 몽타주(어빌리티 시간 = 몽타주 시간 ÷ RateScale).
	{
		const float Top = P.RowTop(1);
		P.Text(4.f, Top + 2.f, TEXT("몽타주"), LabelColor);
		P.Box(LabelWidth, Top, P.Width, RowHeight, RowBack);
		if (const UAnimMontage* const Montage = Model->GetMontage())
		{
			const float Rate = Model->GetMontageRate();
			P.Box(P.X(0.f), Top, P.X(Montage->GetPlayLength() / Rate) - P.X(0.f), RowHeight, FLinearColor(0.25f, 0.25f, 0.3f), 1);
			for (const FAnimNotifyEvent& Notify : Montage->Notifies)
			{
				const float NotifyStart = Notify.GetTriggerTime() / Rate;
				if (!Notify.NotifyStateClass)
				{
					P.Box(P.X(NotifyStart) - 1.f, Top, 3.f, RowHeight * 0.4f, FLinearColor::Yellow, 2);
				}
			}
			for (const FknightHitWindow& Window : Model->GetHitWindows())
			{
				const float WindowStart = Window.Start / Rate;
				const float WindowEnd = Window.End / Rate;
				P.Box(P.X(WindowStart), Top + RowHeight * 0.5f, P.X(WindowEnd) - P.X(WindowStart), RowHeight * 0.5f, FLinearColor(1.f, 0.15f, 0.15f, 0.9f), 2);
			}
			for (const FCompositeSection& Section : Montage->CompositeSections)
			{
				P.Box(P.X(Section.GetTime() / Rate), Top, 1.f, RowHeight, FLinearColor(1.f, 1.f, 1.f, 0.8f), 3);
			}
			P.Text(P.X(0.f) + 3.f, Top + 1.f, FString::Printf(TEXT("%s%s"), *Montage->GetName(),
				FMath::IsNearlyEqual(Rate, 1.f) ? TEXT("") : *FString::Printf(TEXT(" x%.2f"), Rate)), FLinearColor::White, 4);
		}
		else
		{
			P.Text(LabelWidth + 4.f, Top + 2.f, TEXT("몽타주 없음"), LabelColor);
		}
	}

	// 3행: 더미 명중.
	{
		const float Top = P.RowTop(2);
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

void SknightAttackTimeline::ScrubTo(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	using namespace knightAttackTimeline;
	if (!Model.IsValid())
	{
		return;
	}
	const FVector2f Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const float Width = FMath::Max(10.f, MyGeometry.GetLocalSize().X - LabelWidth - RightPadding);
	Model->bPlaying = false;
	Model->SetTime((Local.X - LabelWidth) / Width * Model->GetDuration());
}

FReply SknightAttackTimeline::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	ScrubTo(MyGeometry, MouseEvent);
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SknightAttackTimeline::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return HasMouseCapture() ? FReply::Handled().ReleaseMouseCapture() : FReply::Unhandled();
}

FReply SknightAttackTimeline::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (HasMouseCapture())
	{
		ScrubTo(MyGeometry, MouseEvent);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}
