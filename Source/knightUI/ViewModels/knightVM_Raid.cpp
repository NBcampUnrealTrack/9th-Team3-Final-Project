#include "ViewModels/knightVM_Raid.h"

#include "Monster/knightMonsterDefinition.h"
#include "Raid/knightRaidDefinition.h"
#include "Raid/knightRaidGameState.h"

void UknightVM_Raid::Bind(AknightRaidGameState* InRaidState)
{
	Unbind();
	BoundRaidState = InRaidState;
	if (InRaidState)
	{
		InRaidState->OnRaidPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandlePhaseChanged);
	}
	UE_MVVM_SET_PROPERTY_VALUE(Phase, InRaidState ? InRaidState->GetRaidPhase() : EknightRaidPhase::None);
	RefreshTimer();
}

void UknightVM_Raid::Unbind()
{
	if (AknightRaidGameState* const RaidState = BoundRaidState.Get())
	{
		RaidState->OnRaidPhaseChanged.RemoveDynamic(this, &ThisClass::HandlePhaseChanged);
	}
	BoundRaidState.Reset();
}

void UknightVM_Raid::RefreshTimer()
{
	const AknightRaidGameState* const RaidState = BoundRaidState.Get();
	const float Remaining = RaidState ? RaidState->GetRemainingRaidTime() : -1.f;
	UE_MVVM_SET_PROPERTY_VALUE(bHasTimeLimit, Remaining >= 0.f);
	// Whole seconds only, so the HUD is notified once per second rather than every refresh.
	const float Rounded = Remaining >= 0.f ? FMath::CeilToFloat(Remaining) : -1.f;
	UE_MVVM_SET_PROPERTY_VALUE(RemainingTime, Rounded);

	FText Text;
	if (Rounded >= 0.f)
	{
		const int32 TotalSeconds = FMath::CeilToInt(Rounded);
		Text = FText::FromString(FString::Printf(TEXT("%02d:%02d"), TotalSeconds / 60, TotalSeconds % 60));
	}
	UE_MVVM_SET_PROPERTY_VALUE(RemainingTimeText, Text);

	// Refreshed with the timer because the GameMode may hand the definition to the GameState after the HUD binds.
	const UknightRaidDefinition* const Definition = RaidState ? RaidState->GetRaidDefinition() : nullptr;
	const UknightMonsterDefinition* const Target = Definition ? Definition->TargetMonster.Get() : nullptr;
	const FText Objective = Target && !Target->DisplayName.IsEmpty()
		? FText::Format(NSLOCTEXT("knightHUD", "HuntObjective", "{0} 토벌"), Target->DisplayName) : FText::GetEmpty();
	if (!Objective.EqualTo(ObjectiveText))
	{
		UE_MVVM_SET_PROPERTY_VALUE(ObjectiveText, Objective);
	}
}

void UknightVM_Raid::HandlePhaseChanged(EknightRaidPhase OldPhase, EknightRaidPhase NewPhase)
{
	UE_MVVM_SET_PROPERTY_VALUE(Phase, NewPhase);
	RefreshTimer();
}
