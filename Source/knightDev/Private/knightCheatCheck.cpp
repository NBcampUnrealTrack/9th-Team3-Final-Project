// Explicit opt-in checks only. The namespace and UserDir guards protect normal profiles.
#if WITH_EDITOR
#include "Cheats/KnightCheatSubsystem.h"
#include "Cheats/KnightCheatOverlayWidget.h"
#include "knightCharacter.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/Effects/knightGameplayEffect_Burn.h"
#include "Item/knightItemDefinitions.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Progression/knightSaveGame.h"
#include "Progression/knightSaveSubsystem.h"
#include "Raid/knightRaidGameMode.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Containers/Ticker.h"

namespace
{
struct FCheatCheck
{
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UGameInstance> Instance;
	FString Mode;
	int32 Step = 0;
	int32 Assertions = 0;
	int32 Failures = 0;
	double Deadline = 0;
	int64 BaseMoney = 0;
	float PeriodicHealth = 0.f;
	float UnprotectedHealth = 0.f;
	float LastHitBefore = 0.f;
	double PeriodicStart = 0.0;
	TWeakObjectPtr<AknightCharacter> TestKnight;
	TWeakObjectPtr<AknightCharacter> ReplacementKnight;
	TMap<FPrimaryAssetId, int32> BaseItems;
	TArray<FPrimaryAssetId> MaterialIds;
	TArray<FPrimaryAssetId> OtherIds;

	void Check(bool bPassed, const TCHAR* Label)
	{
		++Assertions;
		if (!bPassed) ++Failures;
		UE_LOG(LogTemp, Display, TEXT("KnightCheatCheck %s %s: %s"), *Mode,
			bPassed ? TEXT("PASS") : TEXT("FAIL"), Label);
	}

	bool Finish()
	{
		UE_LOG(LogTemp, Display, TEXT("KnightCheatCheck COMPLETE mode=%s assertions=%d failures=%d"),
			*Mode, Assertions, Failures);
		FPlatformMisc::RequestExit(Failures > 0);
		return false;
	}

	bool ReadCatalog()
	{
		TArray<FPrimaryAssetId> Ids;
		if (!UAssetManager::IsInitialized()
			|| !UAssetManager::Get().GetPrimaryAssetIdList(UknightItemDefinition::ItemAssetType, Ids)) return false;
		for (const FPrimaryAssetId& Id : Ids)
		{
			const UknightItemDefinition* const Item = Cast<UknightItemDefinition>(UAssetManager::Get().GetPrimaryAssetObject(Id));
			const UknightItemDefinition* const Loaded = Item ? Item : Cast<UknightItemDefinition>(UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad());
			if (!Loaded || Loaded->GetPrimaryAssetId() != Id) return false;
			(Loaded->IsA<UknightMaterialItemDefinition>() ? MaterialIds : OtherIds).Add(Id);
		}
		return !MaterialIds.IsEmpty();
	}

	bool VerifyCounts(UknightSaveGame* Save, int32 Multiplier)
	{
		if (!Save || Save->Money != BaseMoney + int64(Multiplier) * 999999) return false;
		for (const FPrimaryAssetId& Id : MaterialIds)
			if (Save->ItemCounts.FindRef(Id) != BaseItems.FindRef(Id) + Multiplier * 99) return false;
		for (const FPrimaryAssetId& Id : OtherIds)
			if (Save->ItemCounts.FindRef(Id) != BaseItems.FindRef(Id)) return false;
		return true;
	}

	bool Tick(float)
	{
		if (FPlatformTime::Seconds() > Deadline) { Check(false, TEXT("bounded check timeout")); return Finish(); }
		UGameInstance* const GI = Instance.Get();
		if (!GI) { Check(false, TEXT("game instance exists")); return Finish(); }
		auto* const Saves = GI->GetSubsystem<UknightSaveSubsystem>();
		auto* const Progress = GI->GetSubsystem<UknightProgressionSubsystem>();
		auto* const Cheats = GI->GetSubsystem<UKnightCheatSubsystem>();
		if (!Saves || !Progress || !Cheats) { Check(false, TEXT("cheat dependencies exist")); return Finish(); }
		if (Saves->IsSaveInFlight()) return true;
		if (Saves->IsSaveRecoveryPending()) { Check(false, TEXT("isolated save succeeded")); return Finish(); }

		if (Mode == TEXT("Restore"))
		{
			Check(Saves->DoesSlotExist(TEXT("CheatA")) && Saves->SelectSlot(TEXT("CheatA")), TEXT("isolated profile reloaded in new process"));
			Check(ReadCatalog(), TEXT("runtime item catalog loads"));
			UknightSaveGame* const Save = Saves->GetSaveGame();
			Check(Save && Save->Money == 2 * 999999, TEXT("two money grants persisted"));
			bool bMaterialsPersisted = Save && !MaterialIds.IsEmpty();
			for (const FPrimaryAssetId& Id : MaterialIds) bMaterialsPersisted &= Save->ItemCounts.FindRef(Id) == 198;
			Check(bMaterialsPersisted, TEXT("two grants of every catalog material persisted"));
			bool bOtherItemsUntouched = Save != nullptr;
			for (const FPrimaryAssetId& Id : OtherIds) bOtherItemsUntouched &= Save->ItemCounts.FindRef(Id) == 0;
			Check(bOtherItemsUntouched, TEXT("weapons armor and sockets were not granted"));
			Check(!Cheats->IsInvulnerabilityEnabled(), TEXT("invulnerability did not persist across process restart"));
			return Finish();
		}
		if (Mode == TEXT("Travel"))
		{
			if (Step == 0)
			{
				Check(Saves->SelectSlot(Saves->GetActiveSlotId()), TEXT("isolated travel profile selected"));
				FText Feedback;
				Check(Cheats->ExecuteCommand(TEXT("power overwhelming"), Feedback), TEXT("invulnerability enabled before travel"));
				UGameplayStatics::OpenLevel(World.Get(), FName(TEXT("/Game/Knight/Maps/L_Raid01")));
				Step = 1;
				return true;
			}
			UWorld* const NewWorld = GI->GetWorld();
			if (!NewWorld || NewWorld == World.Get()) return true;
			APlayerController* const PC = GI->GetFirstLocalPlayerController();
			if (!PC || !PC->GetPawn()) return true;
			Check(Cheats->IsInvulnerabilityEnabled() && Cheats->IsPlayerInvulnerable(PC->GetPawn()),
				TEXT("new raid player remains protected after level travel"));
			return Finish();
		}
		if (Mode == TEXT("DotRespawn"))
		{
			APlayerController* const PC = GI->GetFirstLocalPlayerController();
			if (!PC || !World.IsValid()) { Check(false, TEXT("menu player controller and world exist")); return Finish(); }
			if (Step == 0)
			{
				UClass* const KnightClass = LoadClass<AknightCharacter>(nullptr,
					TEXT("/Game/Knight/Blueprints/Character/BP_Knight.BP_Knight_C"));
				FActorSpawnParameters Spawn;
				Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AknightCharacter* const Knight = KnightClass
					? World->SpawnActor<AknightCharacter>(KnightClass, FTransform::Identity, Spawn) : nullptr;
				if (Knight) PC->Possess(Knight);
				UAbilitySystemComponent* const ASC = Knight ? Knight->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
				const UknightCoreAttributeSet* const Core = ASC ? ASC->GetSet<UknightCoreAttributeSet>() : nullptr;
				Check(Knight && ASC && Core && PC->GetPawn() == Knight,
					TEXT("spawned Knight pawn initialized and possessed in isolated menu world"));
				if (!Knight || !ASC || !Core || PC->GetPawn() != Knight) return Finish();
				Check(Saves->SelectSlot(Saves->GetActiveSlotId()), TEXT("isolated menu damage profile selected"));
				FText Feedback;
				Check(Cheats->ExecuteCommand(TEXT("power overwhelming"), Feedback)
					&& Cheats->IsPlayerInvulnerable(Knight), TEXT("menu test pawn protected"));
				PeriodicHealth = ASC->GetNumericAttribute(UknightCoreAttributeSet::GetHealthAttribute());
				LastHitBefore = Core->GetLastHitTime();
				Check(ASC->ApplyGameplayEffectToSelf(GetDefault<UknightGameplayEffect_Burn>(), 1.f,
					ASC->MakeEffectContext()).IsValid(), TEXT("periodic burn effect applied to protected pawn"));
				TestKnight = Knight;
				PeriodicStart = FPlatformTime::Seconds();
				Step = 1;
				return true;
			}
			if (Step == 1)
			{
				if (FPlatformTime::Seconds() - PeriodicStart < 1.2) return true;
				if (!TestKnight.IsValid()) { Check(false, TEXT("protected test pawn remains valid")); return Finish(); }
				UAbilitySystemComponent* const ASC = TestKnight.IsValid()
					? TestKnight->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
				const UknightCoreAttributeSet* const Core = ASC ? ASC->GetSet<UknightCoreAttributeSet>() : nullptr;
				Check(ASC && Core && Core->GetLastHitTime() > LastHitBefore
					&& FMath::IsNearlyEqual(ASC->GetNumericAttribute(UknightCoreAttributeSet::GetHealthAttribute()), PeriodicHealth),
					TEXT("periodic damage executed without reducing protected HP"));
				FActorSpawnParameters Spawn;
				Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AknightCharacter* const Replacement = World->SpawnActor<AknightCharacter>(
					TestKnight->GetClass(), TestKnight->GetActorTransform(), Spawn);
				if (Replacement) PC->Possess(Replacement);
				Check(Replacement && Cheats->IsPlayerInvulnerable(Replacement),
					TEXT("replacement possessed Knight inherits session protection"));
				ReplacementKnight = Replacement;
				Check(Saves->SelectSlot(TEXT("DotOff")) && !Cheats->IsInvulnerabilityEnabled(),
					TEXT("profile switch clears protection on replacement pawn"));
				UAbilitySystemComponent* const ReplacementASC = Replacement
					? Replacement->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
				if (!ReplacementASC) { Check(false, TEXT("replacement pawn has ability component")); return Finish(); }
				UnprotectedHealth = ReplacementASC->GetNumericAttribute(UknightCoreAttributeSet::GetHealthAttribute());
				Check(ReplacementASC->ApplyGameplayEffectToSelf(GetDefault<UknightGameplayEffect_Burn>(), 1.f,
					ReplacementASC->MakeEffectContext()).IsValid(), TEXT("periodic burn effect applied to unprotected pawn"));
				PeriodicStart = FPlatformTime::Seconds();
				Step = 2;
				return true;
			}
			if (FPlatformTime::Seconds() - PeriodicStart < 1.2) return true;
			UAbilitySystemComponent* const ASC = ReplacementKnight.IsValid()
				? ReplacementKnight->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
			Check(ASC && ASC->GetNumericAttribute(UknightCoreAttributeSet::GetHealthAttribute()) < UnprotectedHealth,
				TEXT("same periodic burn reduces HP after protection is cleared"));
			return Finish();
		}

		if (Mode == TEXT("Damage"))
		{
			if (Step == 0)
			{
				Check(Saves->SelectSlot(Saves->GetActiveSlotId()), TEXT("isolated raid profile selected"));
				FText Feedback;
				Check(Cheats->ExecuteCommand(TEXT("power overwhelming"), Feedback), TEXT("invulnerability command succeeds"));
				Step = 1;
			}
			APlayerController* const PC = GI->GetFirstLocalPlayerController();
			APawn* const Pawn = PC ? PC->GetPawn() : nullptr;
			UAbilitySystemComponent* const PlayerASC = Pawn ? Pawn->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
			AknightRaidGameMode* const ModeActor = World.IsValid() ? Cast<AknightRaidGameMode>(World->GetAuthGameMode()) : nullptr;
			AknightMonsterCharacterBase* Monster = nullptr;
			if (ModeActor)
				for (const TWeakObjectPtr<AknightMonsterCharacterBase>& Candidate : ModeActor->GetTargetMonsters())
					if (Candidate.IsValid()) { Monster = Candidate.Get(); break; }
			UAbilitySystemComponent* const EnemyASC = Monster ? Monster->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
			if (!Pawn || !PlayerASC || !EnemyASC || World->IsPaused()) return true;
			Check(Cheats->IsPlayerInvulnerable(Pawn) && !Cheats->IsPlayerInvulnerable(Monster),
				TEXT("only current player pawn is protected"));
			const float PlayerBefore = PlayerASC->GetNumericAttribute(UknightCoreAttributeSet::GetHealthAttribute());
			const float EnemyBefore = EnemyASC->GetNumericAttribute(UknightCoreAttributeSet::GetHealthAttribute());
			UGameplayEffect* const Damage = NewObject<UGameplayEffect>(GI);
			Damage->DurationPolicy = EGameplayEffectDurationType::Instant;
			FGameplayModifierInfo& Modifier = Damage->Modifiers.AddDefaulted_GetRef();
			Modifier.Attribute = UknightCoreAttributeSet::GetDamageAttribute();
			Modifier.ModifierOp = EGameplayModOp::Additive;
			Modifier.ModifierMagnitude = FScalableFloat(10.f);
			PlayerASC->ApplyGameplayEffectToSelf(Damage, 1.f, PlayerASC->MakeEffectContext());
			Check(FMath::IsNearlyEqual(PlayerASC->GetNumericAttribute(UknightCoreAttributeSet::GetHealthAttribute()), PlayerBefore),
				TEXT("instant damage does not reduce player HP"));
			EnemyASC->ApplyGameplayEffectToSelf(Damage, 1.f, PlayerASC->MakeEffectContext());
			Check(EnemyASC->GetNumericAttribute(UknightCoreAttributeSet::GetHealthAttribute()) < EnemyBefore,
				TEXT("player-originated damage still reduces monster HP"));
			return Finish();
		}
		if (Mode == TEXT("Input"))
		{
			if (Step == 0)
			{
				Check(!Saves->DoesSlotExist(TEXT("CheatInput")) && Saves->SelectSlot(TEXT("CheatInput")),
					TEXT("fresh isolated input profile selected"));
				if (!FSlateApplication::IsInitialized())
				{
					Check(false, TEXT("Slate input is initialized"));
					return Finish();
				}
				const FKeyEvent Open(EKeys::Tilde, FModifierKeysState(), 0, false, 0, 0);
				FSlateApplication::Get().ProcessKeyDownEvent(Open);
				Check(Cheats->IsOverlayOpen(), TEXT("backtick through Slate opens runtime overlay"));
				TArray<UUserWidget*> Widgets;
				UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(), Widgets, UKnightCheatOverlayWidget::StaticClass(), false);
				UKnightCheatOverlayWidget* const Overlay = Widgets.IsEmpty() ? nullptr : Cast<UKnightCheatOverlayWidget>(Widgets[0]);
				Check(Overlay != nullptr, TEXT("runtime overlay widget exists"));
				if (Overlay) Overlay->SetCommandText(FText::FromString(TEXT("show me the money")));
				const FKeyEvent Enter(EKeys::Enter, FModifierKeysState(), 0, false, 0, 0);
				const FKeyEvent Repeat(EKeys::Enter, FModifierKeysState(), 0, true, 0, 0);
				const bool bEnterConsumed = FSlateApplication::Get().ProcessKeyDownEvent(Enter);
				const bool bRepeatConsumed = FSlateApplication::Get().ProcessKeyDownEvent(Repeat);
				Check(bEnterConsumed && bRepeatConsumed, TEXT("Enter and repeated key events are consumed"));
				Check(Progress->GetMoney() == 999999, TEXT("one Enter grants exactly once"));
				const FKeyEvent Close(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
				FSlateApplication::Get().ProcessKeyDownEvent(Close);
				Check(!Cheats->IsOverlayOpen(), TEXT("Esc through Slate closes overlay"));
				Step = 1;
				return true;
			}
			return Finish();
		}

		if (Step == 0)
		{
			Check(!Saves->DoesSlotExist(TEXT("CheatA")) && Saves->SelectSlot(TEXT("CheatA")),
				TEXT("fresh isolated profile selected"));
			Check(ReadCatalog(), TEXT("runtime item catalog includes material and nonmaterial definitions"));
			UknightSaveGame* const Save = Saves->GetSaveGame();
			BaseMoney = Save ? Save->Money : 0;
			if (Save) BaseItems = Save->ItemCounts;
			FText Feedback;
			Check(Cheats->ExecuteCommand(TEXT("  SHOW   ME   THE    MONEY  "), Feedback),
				TEXT("case and whitespace insensitive money command succeeds"));
			Check(VerifyCounts(Save, 1), TEXT("exact first grant from every catalog material"));
			Step = 1;
			return true;
		}
		if (Step == 1)
		{
			FText Feedback;
			Check(Cheats->ExecuteCommand(TEXT("show me the money"), Feedback), TEXT("repeated command succeeds"));
			Check(VerifyCounts(Saves->GetSaveGame(), 2), TEXT("exact second grant with no nonmaterial items"));
			Check(Cheats->ExecuteCommand(TEXT("power overwhelming"), Feedback)
				&& Cheats->ExecuteCommand(TEXT(" POWER   OVERWHELMING "), Feedback)
				&& Cheats->IsInvulnerabilityEnabled(), TEXT("repeated invulnerability is idempotent"));
			Step = 2;
			return true;
		}
		if (Step == 2)
		{
			Check(Saves->SelectSlot(TEXT("CheatB")) && !Cheats->IsInvulnerabilityEnabled(),
				TEXT("profile switch clears invulnerability"));
			Check(Saves->SelectSlot(TEXT("CheatA")) && VerifyCounts(Saves->GetSaveGame(), 2),
				TEXT("switching back keeps resource grants"));
			Step = 3;
			return true;
		}
		return Finish();
	}
};

void StartCheatCheck(const TArray<FString>& Args, UWorld* World)
{
	FString Namespace, UserDir;
	if (!World || Args.Num() != 1
		|| (Args[0] != TEXT("Seed") && Args[0] != TEXT("Restore") && Args[0] != TEXT("Damage")
			&& Args[0] != TEXT("Input") && Args[0] != TEXT("Travel") && Args[0] != TEXT("DotRespawn"))
		|| !FParse::Value(FCommandLine::Get(), TEXT("KnightSaveTestNamespace="), Namespace)
		|| !Namespace.StartsWith(TEXT("CheatCheck"))
		|| !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir)
		|| !UserDir.Contains(TEXT("KnightCheatCheck")))
	{
		UE_LOG(LogTemp, Error, TEXT("KnightCheatCheck requires isolated KnightCheatCheck UserDir and CheatCheck namespace"));
		return;
	}
	auto Run = MakeShared<FCheatCheck>();
	Run->World = World;
	Run->Instance = World->GetGameInstance();
	Run->Mode = Args[0];
	Run->Deadline = FPlatformTime::Seconds() + (Args[0] == TEXT("Travel") ? 600.0 : 120.0);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), .1f);
}

FAutoConsoleCommandWithWorldAndArgs CheatCheckCommand(TEXT("knight.CheatCheck"),
	TEXT("Isolated cheat regression check: Seed, Restore, Damage, Input, Travel, or DotRespawn."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartCheatCheck), ECVF_Cheat);
}
#endif
