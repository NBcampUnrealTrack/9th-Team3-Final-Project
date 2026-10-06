#include "knightAnimDebugSubsystem.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AttributeSet.h"
#include "BrainComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StateTreeComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "knightAnimDebugModule.h"
#include "knightAnimDebugSettings.h"
#include "knightPerfCounters.h"
#include "knightPerfTrackerSubsystem.h"

namespace knightAnimDebug
{
	constexpr int32 MaxRecentEvents = 10;
	constexpr int32 MaxPreviousStates = 3;
	/** 디버거가 마지막으로 그린 뒤 이 시간(초) 동안 폴링을 유지한다. */
	constexpr double ViewedKeepAliveSeconds = 1.0;
	constexpr double RefreshIntervalSeconds = 0.5;
	static const TCHAR* FreezeReason = TEXT("knight.AnimDebug.FreezeAI");

	/** "GA_Monster_Bite_C" → "GA_Monster_Bite". */
	static FString CleanClassName(const UClass* Class)
	{
		FString Name = GetNameSafe(Class);
		Name.RemoveFromEnd(TEXT("_C"));
		return Name;
	}

	static const TCHAR* EventTypeName(EknightAnimDebugEventType Type)
	{
		switch (Type)
		{
		case EknightAnimDebugEventType::State:        return TEXT("상태");
		case EknightAnimDebugEventType::AbilityStart: return TEXT("어빌리티 시작");
		case EknightAnimDebugEventType::AbilityEnd:   return TEXT("어빌리티 끝");
		case EknightAnimDebugEventType::MontageStart: return TEXT("몽타주 시작");
		case EknightAnimDebugEventType::MontageEnd:   return TEXT("몽타주 끝");
		case EknightAnimDebugEventType::NotifyBegin:  return TEXT("노티파이 시작");
		case EknightAnimDebugEventType::NotifyEnd:    return TEXT("노티파이 끝");
		case EknightAnimDebugEventType::TagAdded:     return TEXT("태그 +");
		case EknightAnimDebugEventType::TagRemoved:   return TEXT("태그 -");
		case EknightAnimDebugEventType::Control:      return TEXT("조작");
		}
		return TEXT("?");
	}

	static bool HasAnyPrefix(const FString& Text, const TArray<FString>& Prefixes)
	{
		for (const FString& Prefix : Prefixes)
		{
			if (Text.StartsWith(Prefix))
			{
				return true;
			}
		}
		return false;
	}

	static FString EscapeCsv(const FString& Text)
	{
		return TEXT("\"") + Text.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\"");
	}

	static FString EscapeMarkdown(const FString& Text)
	{
		return Text.Replace(TEXT("|"), TEXT("\\|"));
	}
}

float FknightAnimDebugAbilityRun::GetTotalPhaseTime() const
{
	float Total = 0.f;
	for (const FknightAnimDebugPhase& Phase : Phases)
	{
		Total += Phase.Duration;
	}
	return Total;
}

UknightAnimDebugSubsystem* UknightAnimDebugSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* const World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UknightAnimDebugSubsystem>() : nullptr;
}

bool UknightAnimDebugSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UknightAnimDebugSubsystem::Deinitialize()
{
	if (bRecording)
	{
		StopRecording();
	}
	for (FknightAnimDebugTrack& Track : Tracks)
	{
		EndTrack(Track);
	}
	Tracks.Reset();
	Super::Deinitialize();
}

TStatId UknightAnimDebugSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UknightAnimDebugSubsystem, STATGROUP_Tickables);
}

bool UknightAnimDebugSubsystem::IsTickable() const
{
	const bool bViewed = LastViewedRealTime >= 0.0 && FPlatformTime::Seconds() - LastViewedRealTime < knightAnimDebug::ViewedKeepAliveSeconds;
	return bViewed || bRecording || IsRepeating() || StepUntilFrame > 0;
}

void UknightAnimDebugSubsystem::MarkViewed()
{
	LastViewedRealTime = FPlatformTime::Seconds();
}

void UknightAnimDebugSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 실제 시간을 쓰므로 슬로 모션과 일시정지가 프레임 비용을 왜곡하지 않는다.
	const double Now = FPlatformTime::Seconds();
	const bool bResumed = LastTickRealTime <= 0.0 || Now - LastTickRealTime > 0.5;
	LastFrameMs = bResumed ? DeltaTime * 1000.f : static_cast<float>((Now - LastTickRealTime) * 1000.0);
	LastTickRealTime = Now;
	SmoothedFrameMs = SmoothedFrameMs > 0.f && !bResumed ? FMath::Lerp(SmoothedFrameMs, LastFrameMs, 0.1f) : LastFrameMs;

	// knightPerfTracker가 돌지 않으면 카운터의 프레임 경계를 여기서 잡는다. 트래커가 돌면 트래커가 잡으므로 건드리지 않는다.
	const UknightPerfTrackerSubsystem* const Tracker = GetWorld()->GetSubsystem<UknightPerfTrackerSubsystem>();
	if (!Tracker || !Tracker->IsTickable())
	{
		FknightPerfCounters::EndFrame();
	}

	if (Now - LastRefreshRealTime >= knightAnimDebug::RefreshIntervalSeconds)
	{
		RefreshTracks();
	}
	for (FknightAnimDebugTrack& Track : Tracks)
	{
		PollTrack(Track);
	}

	if (IsRepeating())
	{
		APawn* const Pawn = RepeatPawn.Get();
		FknightAnimDebugTrack* const Track = FindTrackMutable(Pawn);
		const double WorldTime = GetWorldTime();
		if (Track && !Track->bHasActiveRun && WorldTime - Track->LastRunEndWorldTime >= RepeatDelay)
		{
			if (!PlayAbility(Pawn, RepeatAbilityName))
			{
				// 실패하면 지연 시간 뒤에 다시 시도한다.
				Track->LastRunEndWorldTime = WorldTime;
			}
		}
	}

	// 배속은 월드 Tick 시작 때 반영되므로, 진행을 시작한 다음 프레임부터 Frames 프레임이 원래 배속으로 돈다.
	if (StepUntilFrame > 0 && GFrameCounter >= StepUntilFrame)
	{
		StepUntilFrame = 0;
		if (bTimePaused)
		{
			ApplyTimeDilation(0.f);
		}
	}

	if (bRecording && !bResumed)
	{
		SampleRecordingPerf();
	}
}

void UknightAnimDebugSubsystem::ResolveSettings()
{
	if (bSettingsResolved)
	{
		return;
	}
	bSettingsResolved = true;

	const UknightAnimDebugSettings* const Settings = GetDefault<UknightAnimDebugSettings>();
	TrackedClass = Settings->TrackedPawnClass.IsNull() ? nullptr : Settings->TrackedPawnClass.LoadSynchronous();
	if (!Settings->TrackedPawnClass.IsNull() && !TrackedClass)
	{
		UE_LOG(LogknightAnimDebug, Warning, TEXT("TrackedPawnClass '%s'를 찾지 못해 AI 폰을 모두 추적한다."), *Settings->TrackedPawnClass.ToString());
	}

	PlayableClasses.Reset();
	for (const TSoftClassPtr<UGameplayAbility>& SoftClass : Settings->PlayableAbilityClasses)
	{
		if (UClass* const Class = SoftClass.LoadSynchronous())
		{
			PlayableClasses.Add(Class);
		}
		else if (!SoftClass.IsNull())
		{
			UE_LOG(LogknightAnimDebug, Warning, TEXT("PlayableAbilityClasses의 '%s'를 찾지 못했다."), *SoftClass.ToString());
		}
	}
}

void UknightAnimDebugSubsystem::RefreshTracks()
{
	ResolveSettings();
	LastRefreshRealTime = FPlatformTime::Seconds();

	for (int32 Index = Tracks.Num() - 1; Index >= 0; --Index)
	{
		if (!Tracks[Index].Pawn.IsValid() || !Tracks[Index].ASC.IsValid())
		{
			EndTrack(Tracks[Index]);
			Tracks.RemoveAt(Index);
		}
	}

	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* const Pawn = *It;
		if (!IsValid(Pawn) || FindTrack(Pawn))
		{
			continue;
		}
		const bool bWanted = TrackedClass ? Pawn->IsA(TrackedClass) : Cast<AAIController>(Pawn->GetController()) != nullptr;
		if (bWanted)
		{
			BeginTrack(Pawn);
		}
	}
}

void UknightAnimDebugSubsystem::BeginTrack(APawn* Pawn)
{
	// 어빌리티 시스템은 빙의 시점에 초기화되므로, 아직 없으면 다음 갱신 때 다시 시도한다.
	UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!ASC)
	{
		return;
	}

	FknightAnimDebugTrack& Track = Tracks.AddDefaulted_GetRef();
	Track.Pawn = Pawn;
	Track.ASC = ASC;
	Track.DisplayName = Pawn->GetActorNameOrLabel();
	Track.AbilityActivatedHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &UknightAnimDebugSubsystem::HandleAbilityActivated);
	Track.AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &UknightAnimDebugSubsystem::HandleAbilityEnded);
	Track.TagHandle = ASC->RegisterGenericGameplayTagEvent().AddUObject(this, &UknightAnimDebugSubsystem::HandleTagChanged, TWeakObjectPtr<APawn>(Pawn));

	if (bAIFrozen)
	{
		if (const AAIController* const Controller = Cast<AAIController>(Pawn->GetController()))
		{
			if (UBrainComponent* const Brain = Controller->GetBrainComponent())
			{
				Brain->PauseLogic(knightAnimDebug::FreezeReason);
			}
		}
	}
}

void UknightAnimDebugSubsystem::EndTrack(FknightAnimDebugTrack& Track)
{
	if (UAbilitySystemComponent* const ASC = Track.ASC.Get())
	{
		ASC->AbilityActivatedCallbacks.Remove(Track.AbilityActivatedHandle);
		ASC->OnAbilityEnded.Remove(Track.AbilityEndedHandle);
		ASC->RegisterGenericGameplayTagEvent().Remove(Track.TagHandle);
	}
}

const FknightAnimDebugTrack* UknightAnimDebugSubsystem::FindTrack(const APawn* Pawn) const
{
	return Pawn ? Tracks.FindByPredicate([Pawn](const FknightAnimDebugTrack& Track) { return Track.Pawn.Get() == Pawn; }) : nullptr;
}

FknightAnimDebugTrack* UknightAnimDebugSubsystem::FindTrackMutable(const APawn* Pawn)
{
	return Pawn ? Tracks.FindByPredicate([Pawn](const FknightAnimDebugTrack& Track) { return Track.Pawn.Get() == Pawn; }) : nullptr;
}

void UknightAnimDebugSubsystem::PollTrack(FknightAnimDebugTrack& Track)
{
	APawn* const Pawn = Track.Pawn.Get();
	if (!Pawn)
	{
		return;
	}

#if WITH_GAMEPLAY_DEBUGGER
	// GetActiveStateNames는 디버그 전용 API라 Gameplay Debugger가 있는 빌드에서만 읽는다.
	if (const AAIController* const Controller = Cast<AAIController>(Pawn->GetController()))
	{
		if (const UStateTreeComponent* const StateTree = Controller->FindComponentByClass<UStateTreeComponent>())
		{
			FString Path;
			for (const FName& StateName : StateTree->GetActiveStateNames())
			{
				Path += Path.IsEmpty() ? StateName.ToString() : TEXT("/") + StateName.ToString();
			}
			if (Path != Track.StatePath)
			{
				const FString Detail = Track.StatePath.IsEmpty() ? Path : Track.StatePath + TEXT(" → ") + Path;
				if (!Track.StatePath.IsEmpty())
				{
					Track.PreviousStates.Insert(Track.StatePath, 0);
					Track.PreviousStates.SetNum(FMath::Min(Track.PreviousStates.Num(), knightAnimDebug::MaxPreviousStates));
				}
				Track.StatePath = Path;
				Track.StateEnterWorldTime = GetWorldTime();
				AddEvent(&Track, EknightAnimDebugEventType::State, Detail, true);
			}
		}
	}
#endif

	UAnimInstance* const AnimInstance = GetAnimInstance(Pawn);
	if (!AnimInstance)
	{
		return;
	}

	UAnimMontage* const Montage = AnimInstance->GetCurrentActiveMontage();
	if (Montage != Track.Montage.Get())
	{
		if (const UAnimMontage* const Previous = Track.Montage.Get())
		{
			AddEvent(&Track, EknightAnimDebugEventType::MontageEnd, Previous->GetName(), false);
		}
		if (Montage)
		{
			const FName Section = AnimInstance->Montage_GetCurrentSection(Montage);
			AddEvent(&Track, EknightAnimDebugEventType::MontageStart,
				FString::Printf(TEXT("%s [%s] %.2fs"), *Montage->GetName(), *Section.ToString(), Montage->GetPlayLength()), false);
		}
		Track.Montage = Montage;
	}

	TArray<FString> Notifies;
	for (const FAnimNotifyEvent& Event : AnimInstance->ActiveAnimNotifyState)
	{
		const FString Name = GetNotifyStateName(Event);
		if (!Name.IsEmpty())
		{
			Notifies.AddUnique(Name);
		}
	}
	for (const FString& Name : Notifies)
	{
		if (!Track.ActiveNotifies.Contains(Name))
		{
			AddEvent(&Track, EknightAnimDebugEventType::NotifyBegin, Name, IsHitWindowNotify(Name));
		}
	}
	for (const FString& Name : Track.ActiveNotifies)
	{
		if (!Notifies.Contains(Name))
		{
			AddEvent(&Track, EknightAnimDebugEventType::NotifyEnd, Name, false);
		}
	}
	Track.ActiveNotifies = MoveTemp(Notifies);
}

bool UknightAnimDebugSubsystem::IsPlayableAbility(const UGameplayAbility* Ability) const
{
	if (!Ability)
	{
		return false;
	}
	if (PlayableClasses.IsEmpty())
	{
		return true;
	}
	for (const TSubclassOf<UGameplayAbility>& Class : PlayableClasses)
	{
		if (Class && Ability->IsA(Class))
		{
			return true;
		}
	}
	return false;
}

void UknightAnimDebugSubsystem::HandleAbilityActivated(UGameplayAbility* Ability)
{
	FknightAnimDebugTrack* const Track = Ability ? FindTrackMutable(Cast<APawn>(Ability->GetAvatarActorFromActorInfo())) : nullptr;
	if (!Track || !IsPlayableAbility(Ability))
	{
		return;
	}

	const UknightAnimDebugSettings* const Settings = GetDefault<UknightAnimDebugSettings>();
	const UClass* const Class = Ability->GetClass();

	FknightAnimDebugAbilityRun Run;
	Run.Ability = Ability;
	Run.Name = knightAnimDebug::CleanClassName(Class);
	Run.StartWorldTime = GetWorldTime();

	// 타이밍은 게임 코드를 모르는 채로 설정의 프로퍼티 이름으로 읽는다.
	FString PhaseText;
	for (const FknightAnimDebugPhaseProperty& PhaseProperty : Settings->TimelinePhases)
	{
		if (const FFloatProperty* const Property = FindFProperty<FFloatProperty>(Class, PhaseProperty.PropertyName))
		{
			FknightAnimDebugPhase& Phase = Run.Phases.AddDefaulted_GetRef();
			Phase.Label = PhaseProperty.Label;
			Phase.Duration = Property->GetPropertyValue_InContainer(Ability);
			Phase.Color = PhaseProperty.Color;
			Phase.bHitAtEnd = PhaseProperty.bHitAtEnd;
			PhaseText += FString::Printf(TEXT("%s%s %.2fs"), PhaseText.IsEmpty() ? TEXT("") : TEXT(", "), *Phase.Label, Phase.Duration);
		}
	}

	const FStructProperty* const OffsetProperty = FindFProperty<FStructProperty>(Class, Settings->AreaOffsetProperty);
	const FFloatProperty* const RadiusProperty = FindFProperty<FFloatProperty>(Class, Settings->AreaRadiusProperty);
	if (OffsetProperty && RadiusProperty && OffsetProperty->Struct == TBaseStructure<FVector>::Get())
	{
		Run.bHasArea = true;
		Run.AreaOffset = *OffsetProperty->ContainerPtrToValuePtr<FVector>(Ability);
		Run.AreaRadius = RadiusProperty->GetPropertyValue_InContainer(Ability);
	}

	Track->ActiveRun = MoveTemp(Run);
	Track->bHasActiveRun = true;
	AddEvent(Track, EknightAnimDebugEventType::AbilityStart,
		PhaseText.IsEmpty() ? Track->ActiveRun.Name : FString::Printf(TEXT("%s (%s)"), *Track->ActiveRun.Name, *PhaseText), true);
}

void UknightAnimDebugSubsystem::HandleAbilityEnded(const FAbilityEndedData& Data)
{
	UGameplayAbility* const Ability = Data.AbilityThatEnded;
	FknightAnimDebugTrack* const Track = Ability ? FindTrackMutable(Cast<APawn>(Ability->GetAvatarActorFromActorInfo())) : nullptr;
	if (!Track || !IsPlayableAbility(Ability))
	{
		return;
	}

	const FString Name = knightAnimDebug::CleanClassName(Ability->GetClass());
	const double WorldTime = GetWorldTime();
	const bool bMatchesRun = Track->bHasActiveRun && Track->ActiveRun.Name == Name;
	const double Seconds = bMatchesRun ? WorldTime - Track->ActiveRun.StartWorldTime : 0.0;
	if (bMatchesRun)
	{
		Track->bHasActiveRun = false;
	}
	Track->LastRunName = Name;
	Track->LastRunEndWorldTime = WorldTime;

	AddEvent(Track, EknightAnimDebugEventType::AbilityEnd,
		FString::Printf(TEXT("%s %.2fs%s"), *Name, Seconds, Data.bWasCancelled ? TEXT(" (취소)") : TEXT("")), true);

	if (bRecording && bMatchesRun)
	{
		FAbilityStat& Stat = RecordedAbilities.FindOrAdd(Track->DisplayName + TEXT("|") + Name);
		Stat.Actor = Track->DisplayName;
		Stat.Ability = Name;
		++Stat.Count;
		Stat.Cancelled += Data.bWasCancelled ? 1 : 0;
		Stat.TotalSeconds += Seconds;
		Stat.MaxSeconds = FMath::Max(Stat.MaxSeconds, Seconds);
	}
}

void UknightAnimDebugSubsystem::HandleTagChanged(const FGameplayTag Tag, int32 NewCount, TWeakObjectPtr<APawn> WeakPawn)
{
	FknightAnimDebugTrack* const Track = FindTrackMutable(WeakPawn.Get());
	const FString TagName = Tag.ToString();
	if (Track && knightAnimDebug::HasAnyPrefix(TagName, GetDefault<UknightAnimDebugSettings>()->TagPrefixes))
	{
		AddEvent(Track, NewCount > 0 ? EknightAnimDebugEventType::TagAdded : EknightAnimDebugEventType::TagRemoved, TagName, false);
	}
}

void UknightAnimDebugSubsystem::AddEvent(FknightAnimDebugTrack* Track, EknightAnimDebugEventType Type, const FString& Detail, bool bBookmark)
{
	FknightAnimDebugEvent Event;
	Event.WorldTime = GetWorldTime();
	Event.RealTime = bRecording ? FPlatformTime::Seconds() - RecordingStartRealTime : 0.0;
	Event.Actor = Track ? Track->DisplayName : TEXT("-");
	Event.Type = Type;
	Event.Detail = Detail;
	Event.FrameMs = LastFrameMs;

	if (bBookmark)
	{
		TRACE_BOOKMARK(TEXT("AnimDebug %s %s: %s"), *Event.Actor, knightAnimDebug::EventTypeName(Type), *Detail);
	}
	UE_LOG(LogknightAnimDebug, Verbose, TEXT("%.3f %s %s: %s"), Event.WorldTime, *Event.Actor, knightAnimDebug::EventTypeName(Type), *Detail);

	if (bRecording)
	{
		RecordedEvents.Add(Event);
	}
	if (Track)
	{
		Track->RecentEvents.Add(MoveTemp(Event));
		if (Track->RecentEvents.Num() > knightAnimDebug::MaxRecentEvents)
		{
			Track->RecentEvents.RemoveAt(0);
		}
	}
}

void UknightAnimDebugSubsystem::SetFocusedPawn(APawn* Pawn)
{
	FocusedPawn = Pawn;
}

APawn* UknightAnimDebugSubsystem::ResolveTarget()
{
	if (Tracks.IsEmpty())
	{
		RefreshTracks();
	}
	if (APawn* const Focused = FocusedPawn.Get(); Focused && FindTrack(Focused))
	{
		return Focused;
	}

	const APlayerController* const PC = GetWorld()->GetFirstPlayerController();
	const APawn* const Player = PC ? PC->GetPawn() : nullptr;
	APawn* Best = nullptr;
	double BestDistanceSq = TNumericLimits<double>::Max();
	for (const FknightAnimDebugTrack& Track : Tracks)
	{
		APawn* const Pawn = Track.Pawn.Get();
		if (!Pawn)
		{
			continue;
		}
		const double DistanceSq = Player ? FVector::DistSquared(Player->GetActorLocation(), Pawn->GetActorLocation()) : 0.0;
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Pawn;
		}
	}
	return Best;
}

void UknightAnimDebugSubsystem::GetPlayableAbilityNames(const APawn* Pawn, TArray<FString>& OutNames) const
{
	const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!ASC)
	{
		return;
	}
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (IsPlayableAbility(Spec.Ability))
		{
			OutNames.AddUnique(knightAnimDebug::CleanClassName(Spec.Ability->GetClass()));
		}
	}
}

bool UknightAnimDebugSubsystem::PlayAbility(APawn* Pawn, const FString& Name)
{
	ResolveSettings();
	UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!ASC || Name.IsEmpty())
	{
		UE_LOG(LogknightAnimDebug, Warning, TEXT("실행할 몬스터나 어빌리티 이름이 없다."));
		return false;
	}

	const FGameplayAbilitySpec* Found = nullptr;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!IsPlayableAbility(Spec.Ability))
		{
			continue;
		}
		const FString SpecName = knightAnimDebug::CleanClassName(Spec.Ability->GetClass());
		if (SpecName.Equals(Name, ESearchCase::IgnoreCase))
		{
			Found = &Spec;
			break;
		}
		if (!Found && SpecName.Contains(Name, ESearchCase::IgnoreCase))
		{
			Found = &Spec;
		}
	}

	FknightAnimDebugTrack* const Track = FindTrackMutable(Pawn);
	if (!Found)
	{
		UE_LOG(LogknightAnimDebug, Warning, TEXT("%s에 '%s' 어빌리티가 없다. knight.AnimDebug.List로 목록을 확인한다."), *GetNameSafe(Pawn), *Name);
		return false;
	}

	const FString FoundName = knightAnimDebug::CleanClassName(Found->Ability->GetClass());
	const bool bActivated = ASC->TryActivateAbility(Found->Handle);
	AddEvent(Track, EknightAnimDebugEventType::Control,
		FString::Printf(TEXT("강제 실행 %s: %s"), *FoundName, bActivated ? TEXT("성공") : TEXT("실패")), false);
	if (!bActivated)
	{
		UE_LOG(LogknightAnimDebug, Warning, TEXT("%s 실행 실패: 이미 실행 중이거나, 차단 태그(반응 중 등)나 비용·쿨다운에 막혔을 수 있다."), *FoundName);
	}
	return bActivated;
}

void UknightAnimDebugSubsystem::StartRepeat(APawn* Pawn, const FString& Name, float Delay)
{
	RepeatPawn = Pawn;
	RepeatAbilityName = Name;
	RepeatDelay = FMath::Max(0.f, Delay);
	if (FknightAnimDebugTrack* const Track = FindTrackMutable(Pawn))
	{
		// 바로 한 번 실행되도록 한다.
		Track->LastRunEndWorldTime = -TNumericLimits<float>::Max();
		AddEvent(Track, EknightAnimDebugEventType::Control, FString::Printf(TEXT("반복 시작 %s (간격 %.1fs)"), *Name, RepeatDelay), false);
	}
}

void UknightAnimDebugSubsystem::StopRepeat()
{
	if (IsRepeating())
	{
		AddEvent(FindTrackMutable(RepeatPawn.Get()), EknightAnimDebugEventType::Control, TEXT("반복 끝"), false);
	}
	RepeatPawn.Reset();
	RepeatAbilityName.Reset();
}

void UknightAnimDebugSubsystem::SetAIFrozen(bool bFrozen)
{
	RefreshTracks();
	bAIFrozen = bFrozen;
	for (const FknightAnimDebugTrack& Track : Tracks)
	{
		AAIController* const Controller = Track.Pawn.IsValid() ? Cast<AAIController>(Track.Pawn->GetController()) : nullptr;
		UBrainComponent* const Brain = Controller ? Controller->GetBrainComponent() : nullptr;
		if (!Brain)
		{
			continue;
		}
		if (bFrozen)
		{
			Controller->StopMovement();
			Brain->PauseLogic(knightAnimDebug::FreezeReason);
		}
		else
		{
			Brain->ResumeLogic(knightAnimDebug::FreezeReason);
		}
	}
	AddEvent(nullptr, EknightAnimDebugEventType::Control, bFrozen ? TEXT("AI 정지") : TEXT("AI 재개"), false);
}

void UknightAnimDebugSubsystem::ApplyTimeDilation(float Dilation)
{
	// SetGlobalTimeDilation이 월드 설정의 최저·최고값으로 자르므로 0을 넘기면 최저값이 된다.
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), Dilation);
}

void UknightAnimDebugSubsystem::SetTimeScale(float Scale)
{
	const float Clamped = FMath::Clamp(Scale, 0.01f, 4.f);
	if (bTimePaused)
	{
		PausedTimeScale = Clamped;
	}
	else
	{
		ApplyTimeDilation(Clamped);
	}
	AddEvent(nullptr, EknightAnimDebugEventType::Control, FString::Printf(TEXT("배속 x%.2f"), Clamped), false);
}

float UknightAnimDebugSubsystem::GetTimeScale() const
{
	return bTimePaused ? PausedTimeScale : UGameplayStatics::GetGlobalTimeDilation(GetWorld());
}

void UknightAnimDebugSubsystem::SetPaused(bool bPause)
{
	StepUntilFrame = 0;
	if (bPause == bTimePaused)
	{
		return;
	}
	if (bPause)
	{
		PausedTimeScale = UGameplayStatics::GetGlobalTimeDilation(GetWorld());
		ApplyTimeDilation(0.f);
	}
	else
	{
		ApplyTimeDilation(PausedTimeScale);
	}
	bTimePaused = bPause;
	AddEvent(nullptr, EknightAnimDebugEventType::Control, bPause ? TEXT("일시정지") : TEXT("재개"), false);
}

void UknightAnimDebugSubsystem::StepFrames(int32 Frames)
{
	if (!bTimePaused)
	{
		SetPaused(true);
		return;
	}
	ApplyTimeDilation(PausedTimeScale);
	StepUntilFrame = GFrameCounter + FMath::Max(1, Frames);
}

double UknightAnimDebugSubsystem::GetWorldTime() const
{
	const UWorld* const World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

UAnimInstance* UknightAnimDebugSubsystem::GetAnimInstance(const APawn* Pawn)
{
	if (const ACharacter* const Character = Cast<ACharacter>(Pawn))
	{
		return Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	}
	const USkeletalMeshComponent* const Mesh = Pawn ? Pawn->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

FString UknightAnimDebugSubsystem::GetNotifyStateName(const FAnimNotifyEvent& Event)
{
	if (!Event.NotifyStateClass)
	{
		return Event.NotifyName.IsNone() ? FString() : Event.NotifyName.ToString();
	}
	// "knightAnimNotifyState_MeleeTrace" → "MeleeTrace", "ANS_Trail_C" → "ANS_Trail".
	FString Name = knightAnimDebug::CleanClassName(Event.NotifyStateClass->GetClass());
	const int32 Index = Name.Find(TEXT("AnimNotifyState_"));
	if (Index != INDEX_NONE)
	{
		Name.RightChopInline(Index + FCString::Strlen(TEXT("AnimNotifyState_")));
	}
	return Name;
}

bool UknightAnimDebugSubsystem::IsHitWindowNotify(const FString& NotifyName)
{
	for (const FString& Keyword : GetDefault<UknightAnimDebugSettings>()->HitWindowNotifyKeywords)
	{
		if (!Keyword.IsEmpty() && NotifyName.Contains(Keyword))
		{
			return true;
		}
	}
	return false;
}

bool UknightAnimDebugSubsystem::FindAttributeValue(const UAbilitySystemComponent* ASC, FName AttributeName, float& OutValue)
{
	if (!ASC || AttributeName.IsNone())
	{
		return false;
	}
	for (const UAttributeSet* const Set : ASC->GetSpawnedAttributes())
	{
		const FStructProperty* const Property = Set ? FindFProperty<FStructProperty>(Set->GetClass(), AttributeName) : nullptr;
		if (Property && Property->Struct && Property->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
		{
			OutValue = Property->ContainerPtrToValuePtr<FGameplayAttributeData>(Set)->GetCurrentValue();
			return true;
		}
	}
	return false;
}

void UknightAnimDebugSubsystem::GetPerfValues(TArray<FknightAnimDebugPerfValue>& OutValues, double& OutAliveCount) const
{
	const UknightAnimDebugSettings* const Settings = GetDefault<UknightAnimDebugSettings>();
	OutAliveCount = 0.0;
	for (int32 Index = 0; Index < FknightPerfCounters::Num(); ++Index)
	{
		const FName Name = FknightPerfCounters::GetName(Index);
		const EknightPerfCounterMode Mode = FknightPerfCounters::GetMode(Index);
		const double Value = Mode == EknightPerfCounterMode::Gauge ? FknightPerfCounters::GetValue(Index) : FknightPerfCounters::GetLastFrameValue(Index);
		if (Name == Settings->AliveCountGauge)
		{
			OutAliveCount = Value;
		}
		if (knightAnimDebug::HasAnyPrefix(Name.ToString(), Settings->PerfCounterPrefixes))
		{
			FknightAnimDebugPerfValue& Out = OutValues.AddDefaulted_GetRef();
			Out.Name = Name;
			Out.Value = Value;
			Out.bIsTimeMs = Mode == EknightPerfCounterMode::TimeMs;
			Out.bIsGauge = Mode == EknightPerfCounterMode::Gauge;
		}
	}
}

void UknightAnimDebugSubsystem::StartRecording(const FString& Label)
{
	if (bRecording)
	{
		StopRecording();
	}
	RefreshTracks();

	bRecording = true;
	RecordingLabel = FPaths::MakeValidFileName(Label.IsEmpty() ? TEXT("AnimDebug") : Label);
	RecordingStartDate = FDateTime::Now();
	RecordingStartRealTime = FPlatformTime::Seconds();
	RecordingStartWorldTime = GetWorldTime();
	RecordedEvents.Reset();
	RecordedFrameMs = FPerfAccumulator();
	RecordedCounters.Reset();
	RecordedAbilities.Reset();

	TRACE_BOOKMARK(TEXT("AnimDebug 녹화 시작: %s"), *RecordingLabel);
	AddEvent(nullptr, EknightAnimDebugEventType::Control, TEXT("녹화 시작 ") + RecordingLabel, false);
	UE_LOG(LogknightAnimDebug, Log, TEXT("녹화 시작: %s"), *RecordingLabel);
}

FString UknightAnimDebugSubsystem::StopRecording()
{
	if (!bRecording)
	{
		return FString();
	}
	AddEvent(nullptr, EknightAnimDebugEventType::Control, TEXT("녹화 끝"), false);
	TRACE_BOOKMARK(TEXT("AnimDebug 녹화 끝: %s"), *RecordingLabel);

	const FString Path = WriteRecording();
	bRecording = false;

	UE_LOG(LogknightAnimDebug, Log, TEXT("녹화 저장: %s"), *Path);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Green, TEXT("AnimDebug 녹화 저장: ") + Path);
	}
	return Path;
}

double UknightAnimDebugSubsystem::GetRecordingSeconds() const
{
	return bRecording ? FPlatformTime::Seconds() - RecordingStartRealTime : 0.0;
}

void UknightAnimDebugSubsystem::SampleRecordingPerf()
{
	RecordedFrameMs.Add(LastFrameMs);

	TArray<FknightAnimDebugPerfValue> Values;
	double AliveCount = 0.0;
	GetPerfValues(Values, AliveCount);
	for (const FknightAnimDebugPerfValue& Value : Values)
	{
		FPerfAccumulator& Accumulator = RecordedCounters.FindOrAdd(Value.Name);
		Accumulator.bIsTimeMs = Value.bIsTimeMs;
		Accumulator.bIsGauge = Value.bIsGauge;
		Accumulator.Add(Value.Value);
	}
}

FString UknightAnimDebugSubsystem::WriteRecording()
{
	using namespace knightAnimDebug;

	const UknightAnimDebugSettings* const Settings = GetDefault<UknightAnimDebugSettings>();
	const FString Directory = FPaths::ProfilingDir() / TEXT("knightAnimDebug");
	const FString BaseName = FString::Printf(TEXT("%s_%s"), *RecordingLabel, *RecordingStartDate.ToString(TEXT("%Y%m%d-%H%M%S")));
	const FString CsvPath = FPaths::ConvertRelativePathToFull(Directory / BaseName + TEXT(".csv"));
	const FString MarkdownPath = FPaths::ConvertRelativePathToFull(Directory / BaseName + TEXT(".md"));

	// CSV: 전체 이벤트. 엑셀에서 한글이 깨지지 않도록 BOM을 붙인다.
	FString Csv = TEXT("WorldTime,RealTime,Actor,Type,Detail,FrameMs\n");
	for (const FknightAnimDebugEvent& Event : RecordedEvents)
	{
		Csv += FString::Printf(TEXT("%.3f,%.3f,%s,%s,%s,%.2f\n"), Event.WorldTime - RecordingStartWorldTime, Event.RealTime,
			*EscapeCsv(Event.Actor), *EscapeCsv(EventTypeName(Event.Type)), *EscapeCsv(Event.Detail), Event.FrameMs);
	}
	FFileHelper::SaveStringToFile(Csv, *CsvPath, FFileHelper::EEncodingOptions::ForceUTF8);

	const double RealSeconds = FPlatformTime::Seconds() - RecordingStartRealTime;
	const double WorldSeconds = GetWorldTime() - RecordingStartWorldTime;

	FString Md;
	Md += FString::Printf(TEXT("# 몬스터 애니메이션 녹화: %s\n\n"), *RecordingLabel);
	Md += TEXT("| 항목 | 값 |\n|---|---|\n");
	Md += FString::Printf(TEXT("| 시작 | %s |\n"), *RecordingStartDate.ToString(TEXT("%Y-%m-%d %H:%M:%S")));
	Md += FString::Printf(TEXT("| 맵 | %s |\n"), *GetWorld()->GetMapName());
	Md += FString::Printf(TEXT("| 실행 환경 | %s, %s |\n"), LexToString(FApp::GetBuildConfiguration()),
		GetWorld()->WorldType == EWorldType::PIE ? TEXT("PIE") : TEXT("Standalone/Game"));
	Md += FString::Printf(TEXT("| 실제 시간 | %.1f s |\n"), RealSeconds);
	Md += FString::Printf(TEXT("| 월드 시간 | %.1f s (평균 배속 x%.2f) |\n"), WorldSeconds, RealSeconds > 0.0 ? WorldSeconds / RealSeconds : 1.0);
	Md += FString::Printf(TEXT("| 추적한 몬스터 | %d |\n"), Tracks.Num());
	Md += FString::Printf(TEXT("| 이벤트 | %d (전체: %s.csv) |\n\n"), RecordedEvents.Num(), *BaseName);

	Md += TEXT("## 어빌리티 요약\n\n");
	if (RecordedAbilities.IsEmpty())
	{
		Md += TEXT("녹화 중에 끝난 어빌리티가 없다.\n\n");
	}
	else
	{
		TArray<FAbilityStat> Stats;
		RecordedAbilities.GenerateValueArray(Stats);
		Stats.Sort([](const FAbilityStat& A, const FAbilityStat& B) { return A.Actor != B.Actor ? A.Actor < B.Actor : A.Ability < B.Ability; });
		Md += TEXT("| 몬스터 | 어빌리티 | 횟수 | 평균(s) | 최대(s) | 취소 |\n|---|---|---:|---:|---:|---:|\n");
		for (const FAbilityStat& Stat : Stats)
		{
			Md += FString::Printf(TEXT("| %s | %s | %d | %.2f | %.2f | %d |\n"), *EscapeMarkdown(Stat.Actor), *EscapeMarkdown(Stat.Ability),
				Stat.Count, Stat.Count > 0 ? Stat.TotalSeconds / Stat.Count : 0.0, Stat.MaxSeconds, Stat.Cancelled);
		}
		Md += TEXT("\n");
	}

	Md += TEXT("## 성능 (녹화 구간)\n\n");
	if (RecordedFrameMs.Count == 0)
	{
		Md += TEXT("표본이 없다.\n\n");
	}
	else
	{
		const FPerfAccumulator* const Alive = RecordedCounters.Find(Settings->AliveCountGauge);
		const double AliveAverage = Alive && Alive->Count > 0 ? Alive->Sum / Alive->Count : 0.0;

		// 프레임 평균은 시간(ms)과 게이지에, 녹화 합계는 횟수(실행·실패 수)와 시간에 의미가 있다.
		Md += TEXT("| 항목 | 프레임 평균 | 프레임 최대 | 녹화 합계 | 몬스터 1마리당 프레임 평균 |\n|---|---:|---:|---:|---:|\n");
		Md += FString::Printf(TEXT("| 프레임 (ms) | %.2f | %.2f | | |\n"), RecordedFrameMs.Sum / RecordedFrameMs.Count, RecordedFrameMs.Max);

		TArray<FName> Names;
		RecordedCounters.GetKeys(Names);
		Names.Sort(FNameLexicalLess());
		for (const FName& Name : Names)
		{
			const FPerfAccumulator& Accumulator = RecordedCounters[Name];
			const double Average = Accumulator.Count > 0 ? Accumulator.Sum / Accumulator.Count : 0.0;
			const FString PerMonster = Accumulator.bIsTimeMs && AliveAverage > 0.0 ? FString::Printf(TEXT("%.4f"), Average / AliveAverage) : FString();
			const FString Total = Accumulator.bIsGauge ? FString()
				: Accumulator.bIsTimeMs ? FString::Printf(TEXT("%.2f"), Accumulator.Sum) : FString::Printf(TEXT("%.0f"), Accumulator.Sum);
			Md += FString::Printf(TEXT("| %s%s | %.3f | %.3f | %s | %s |\n"), *Name.ToString(), Accumulator.bIsTimeMs ? TEXT(" (ms)") : TEXT(""),
				Average, Accumulator.Max, *Total, *PerMonster);
		}
		Md += FString::Printf(TEXT("\n프레임 표본 %d개. 카운터는 knightPerfTracker 값이다. PIE 수치에는 에디터 비용이 들어 있으므로, ")
			TEXT("비교용 측정은 Standalone에서 knight.Perf.Start로 한다.\n\n"), RecordedFrameMs.Count);
	}

	Md += TEXT("## 이벤트 타임라인\n\n");
	Md += TEXT("시간은 녹화 시작부터의 월드 시간(초)이다.\n\n");
	Md += TEXT("| 시간(s) | 몬스터 | 종류 | 내용 | 프레임(ms) |\n|---:|---|---|---|---:|\n");
	const int32 Rows = FMath::Min(RecordedEvents.Num(), Settings->MaxReportEventRows);
	for (int32 Index = 0; Index < Rows; ++Index)
	{
		const FknightAnimDebugEvent& Event = RecordedEvents[Index];
		Md += FString::Printf(TEXT("| %.3f | %s | %s | %s | %.1f |\n"), Event.WorldTime - RecordingStartWorldTime, *EscapeMarkdown(Event.Actor),
			EventTypeName(Event.Type), *EscapeMarkdown(Event.Detail), Event.FrameMs);
	}
	if (RecordedEvents.Num() > Rows)
	{
		Md += FString::Printf(TEXT("\n이벤트 %d개 중 앞의 %d개만 넣었다. 전체는 %s.csv에 있다.\n"), RecordedEvents.Num(), Rows, *BaseName);
	}

	FFileHelper::SaveStringToFile(Md, *MarkdownPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	return MarkdownPath;
}
