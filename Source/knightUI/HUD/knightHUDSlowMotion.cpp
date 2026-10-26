#include "HUD/knightHUDSlowMotion.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Engine/LocalPlayer.h"
#include "Foundation/knightUISubsystem.h"
#include "ViewModels/knightVM_PlayerStatus.h"

void UknightHUDSlowMotion::NativeConstruct()
{
	Super::NativeConstruct();
	// The groggy cue's cyan edge is now a scene-only radial pull. Keep the separate
	// player-knockdown white edge, impact flash and existing letterbox behavior.
	if (Img_Edge) Img_Edge->SetVisibility(ESlateVisibility::Collapsed);
	SetVisibility(ESlateVisibility::Collapsed);

	const ULocalPlayer* const LocalPlayer = GetOwningLocalPlayer();
	const UknightUISubsystem* const UI = LocalPlayer ? LocalPlayer->GetSubsystem<UknightUISubsystem>() : nullptr;
	UknightVM_PlayerStatus* const VM = UI ? UI->GetPlayerStatusViewModel() : nullptr;
	if (!VM || IsDesignTime())
	{
		return;
	}
	ViewModel = VM;
	GroggyHandle = VM->OnGroggyPresentation.AddUObject(this, &ThisClass::HandleGroggyPresentation);
	KnockdownHandle = VM->OnPlayerKnockdown.AddUObject(this, &ThisClass::HandleKnockdown);
}

void UknightHUDSlowMotion::NativeDestruct()
{
	if (UknightVM_PlayerStatus* const VM = ViewModel.Get())
	{
		VM->OnGroggyPresentation.Remove(GroggyHandle);
		VM->OnPlayerKnockdown.Remove(KnockdownHandle);
	}
	ViewModel.Reset();
	if (Ticker.IsValid())
	{
		FTSTicker::RemoveTicker(Ticker);
		Ticker.Reset();
	}
	SetHUDOpacity(1.f);
	Super::NativeDestruct();
}

void UknightHUDSlowMotion::HandleGroggyPresentation(bool bImpact)
{
	if (bImpact)
	{
		Flash = 1.f;
	}
	StartTicker();
}

void UknightHUDSlowMotion::HandleKnockdown()
{
	KnockdownTime = 0.f;
	StartTicker();
}

void UknightHUDSlowMotion::StartTicker()
{
	if (!Ticker.IsValid())
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
		// Real time: the knockdown slow scales the whole world, but the effect keeps its own pace.
		Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime) { return Tick(DeltaTime); }));
	}
	Tick(0.f);
}

bool UknightHUDSlowMotion::Tick(float DeltaTime)
{
	DeltaTime = FMath::Min(DeltaTime, 0.05f);
	const UknightVM_PlayerStatus* const VM = ViewModel.Get();

	// The component knows when every path ends (missed prediction, freeze without a slow, cancel), so follow it instead of timing it here.
	const bool bGroggy = VM && VM->IsGroggyPresentationActive();
	const float Step = bGroggy ? (GroggyEnterTime > 0.f ? DeltaTime / GroggyEnterTime : 1.f) : -(GroggyExitTime > 0.f ? DeltaTime / GroggyExitTime : 1.f);
	Cinema = FMath::Clamp(Cinema + Step, 0.f, 1.f);
	Flash = HitFlashTime > 0.f ? FMath::Max(0.f, Flash - DeltaTime / HitFlashTime) : 0.f;

	float Knockdown = 0.f;
	if (KnockdownTime >= 0.f)
	{
		KnockdownTime += DeltaTime;
		// Strong through the slow, then fades: 1 to 0.6 while held, 0.6 to 0 after.
		if (KnockdownTime < KnockdownHoldTime)
		{
			Knockdown = 1.f - 0.4f * KnockdownTime / KnockdownHoldTime;
		}
		else if (KnockdownTime < KnockdownHoldTime + KnockdownFadeTime)
		{
			Knockdown = 0.6f * (1.f - (KnockdownTime - KnockdownHoldTime) / KnockdownFadeTime);
		}
		else
		{
			KnockdownTime = -1.f;
		}
	}

	// Ease out so the bars slow down as they settle.
	const float Bars = 1.f - FMath::Square(1.f - Cinema);
	if (Img_LetterboxTop)
	{
		Img_LetterboxTop->SetRenderScale(FVector2D(1.f, Bars));
	}
	if (Img_LetterboxBottom)
	{
		Img_LetterboxBottom->SetRenderScale(FVector2D(1.f, Bars));
	}
	if (Img_Edge)
	{
		Img_Edge->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Img_Flash)
	{
		Img_Flash->SetRenderOpacity(HitFlashOpacity * Flash);
	}
	if (Img_KnockdownEdge)
	{
		Img_KnockdownEdge->SetRenderOpacity(KnockdownEdgeOpacity * Knockdown);
	}
	SetHUDOpacity(1.f - GroggyHUDDim * Bars);

	if (!bGroggy && Cinema <= 0.f && Flash <= 0.f && KnockdownTime < 0.f)
	{
		SetHUDOpacity(1.f);
		SetVisibility(ESlateVisibility::Collapsed);
		Ticker.Reset();
		return false;
	}
	return true;
}

void UknightHUDSlowMotion::SetHUDOpacity(float Opacity)
{
	const bool bDim = Opacity < 1.f;
	if (!bDim && !bHUDDimmed)
	{
		return;
	}
	bHUDDimmed = bDim;
	// Widgets drawn after this one in the layout are the HUD. ColorAndOpacity, not RenderOpacity, so widgets that set
	// their own render opacity (repair kit, groggy noise) keep it.
	UPanelWidget* const Parent = GetParent();
	if (!Parent)
	{
		return;
	}
	for (int32 Index = Parent->GetChildIndex(this) + 1; Index < Parent->GetChildrenCount(); ++Index)
	{
		if (UUserWidget* const Sibling = Cast<UUserWidget>(Parent->GetChildAt(Index)))
		{
			Sibling->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, Opacity));
		}
	}
}
