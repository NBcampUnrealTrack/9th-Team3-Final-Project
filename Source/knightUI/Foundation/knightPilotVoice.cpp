#include "Foundation/knightPilotVoice.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Foundation/knightUISettings.h"
#include "knightAudioSubsystem.h"
#include "LoadingScreenManager.h"
#include "Raid/knightRaidGameState.h"
#include "Sound/SoundBase.h"

void UknightPilotVoice::Preload()
{
	const UknightUISettings* const Settings = GetDefault<UknightUISettings>();
	TArray<FSoftObjectPath> Paths;
	for (const TSoftObjectPtr<USoundBase>* const Voice : { &Settings->SystemActivatedVoice, &Settings->EnemyDetectedVoice,
		&Settings->CombatModeVoice, &Settings->HealthWarningVoice, &Settings->RaidSuccessVoice })
	{
		if (!Voice->IsNull())
		{
			Paths.AddUnique(Voice->ToSoftObjectPath());
		}
	}
	if (!Paths.IsEmpty())
	{
		PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
	}
}

void UknightPilotVoice::BindPawn(UAbilitySystemComponent* InASC)
{
	if (BoundASC.Get() == InASC)
	{
		return;
	}
	UnbindPawn();
	BoundASC = InASC;
	// A new Knight may warn again.
	bHealthWarningPlayed = false;
	if (!InASC)
	{
		return;
	}
	BoundCombatTag = GetDefault<UknightUISettings>()->CombatStateTag;
	if (BoundCombatTag.IsValid())
	{
		CombatStateHandle = InASC->RegisterGameplayTagEvent(BoundCombatTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleCombatStateChanged);
	}
	HealthHandle = InASC->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthChanged);
}

void UknightPilotVoice::BindRaid(AknightRaidGameState* InRaidState)
{
	if (BoundRaidState.Get() == InRaidState)
	{
		return;
	}
	UnbindRaid();
	BoundRaidState = InRaidState;
	// Each raid greets and spots the enemy once.
	bSystemActivatedPlayed = false;
	bEnemyDetectedPlayed = false;
	if (InRaidState)
	{
		InRaidState->OnRaidPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleRaidPhaseChanged);
		FinishStageHandle = InRaidState->OnFinishStageChanged.AddUObject(this, &ThisClass::HandleFinishStageChanged);
	}
}

void UknightPilotVoice::Unbind()
{
	UnbindPawn();
	UnbindRaid();
}

void UknightPilotVoice::UnbindPawn()
{
	if (UAbilitySystemComponent* const ASC = BoundASC.Get())
	{
		if (BoundCombatTag.IsValid())
		{
			ASC->RegisterGameplayTagEvent(BoundCombatTag, EGameplayTagEventType::NewOrRemoved).Remove(CombatStateHandle);
		}
		ASC->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
	}
	CombatStateHandle.Reset();
	HealthHandle.Reset();
	BoundASC.Reset();
}

void UknightPilotVoice::UnbindRaid()
{
	if (AknightRaidGameState* const RaidState = BoundRaidState.Get())
	{
		RaidState->OnRaidPhaseChanged.RemoveDynamic(this, &ThisClass::HandleRaidPhaseChanged);
		RaidState->OnFinishStageChanged.Remove(FinishStageHandle);
	}
	FinishStageHandle.Reset();
	BoundRaidState.Reset();
}

void UknightPilotVoice::Refresh()
{
	const AknightRaidGameState* const RaidState = BoundRaidState.Get();
	if (bSystemActivatedPlayed || !RaidState || !BoundASC.IsValid())
	{
		return;
	}
	// The loading screen also blocks input, so once it is gone the Knight is in the player's hands.
	const UGameInstance* const GameInstance = RaidState->GetGameInstance();
	const ULoadingScreenManager* const LoadingScreen = GameInstance ? GameInstance->GetSubsystem<ULoadingScreenManager>() : nullptr;
	if (LoadingScreen && LoadingScreen->GetLoadingScreenDisplayStatus())
	{
		return;
	}
	bSystemActivatedPlayed = true;
	Play(GetDefault<UknightUISettings>()->SystemActivatedVoice);
}

void UknightPilotVoice::HandleCombatStateChanged(const FGameplayTag Tag, int32 NewCount)
{
	const double Now = FPlatformTime::Seconds();
	if (NewCount <= 0 || Now - LastCombatModeTime < GetDefault<UknightUISettings>()->CombatModeVoiceCooldown)
	{
		return;
	}
	LastCombatModeTime = Now;
	Play(GetDefault<UknightUISettings>()->CombatModeVoice);
}

void UknightPilotVoice::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	const UAbilitySystemComponent* const ASC = BoundASC.Get();
	if (bHealthWarningPlayed || !ASC)
	{
		return;
	}
	bool bFound = false;
	const float MaxHealth = ASC->GetGameplayAttributeValue(UknightCoreAttributeSet::GetMaxHealthAttribute(), bFound);
	const float Threshold = MaxHealth * GetDefault<UknightUISettings>()->HealthWarningRatio;
	// Only on the way down through the threshold, never for a Knight that is spawned or healed below it.
	if (MaxHealth > 0.f && Data.OldValue > Threshold && Data.NewValue <= Threshold && Data.NewValue > 0.f)
	{
		bHealthWarningPlayed = true;
		Play(GetDefault<UknightUISettings>()->HealthWarningVoice, true);
	}
}

void UknightPilotVoice::HandleRaidPhaseChanged(EknightRaidPhase OldPhase, EknightRaidPhase NewPhase)
{
	if (NewPhase == EknightRaidPhase::Combat && !bEnemyDetectedPlayed)
	{
		bEnemyDetectedPlayed = true;
		Play(GetDefault<UknightUISettings>()->EnemyDetectedVoice);
	}
}

void UknightPilotVoice::HandleFinishStageChanged(EknightRaidFinishStage Stage)
{
	if (Stage == EknightRaidFinishStage::Banner)
	{
		Play(GetDefault<UknightUISettings>()->RaidSuccessVoice, true);
	}
}

void UknightPilotVoice::Play(const TSoftObjectPtr<USoundBase>& Voice, bool bUrgent) const
{
	const AknightRaidGameState* const RaidState = BoundRaidState.Get();
	const UAbilitySystemComponent* const ASC = BoundASC.Get();
	const UWorld* const World = RaidState ? RaidState->GetWorld() : (ASC ? ASC->GetWorld() : nullptr);
	UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	UknightAudioSubsystem* const Audio = GameInstance ? GameInstance->GetSubsystem<UknightAudioSubsystem>() : nullptr;
	if (Audio && !Voice.IsNull())
	{
		Audio->PlayVoice(Voice.LoadSynchronous(), bUrgent);
	}
}
