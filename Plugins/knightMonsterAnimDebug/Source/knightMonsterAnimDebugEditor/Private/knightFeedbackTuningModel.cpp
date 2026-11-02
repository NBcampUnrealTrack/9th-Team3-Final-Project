#include "knightFeedbackTuningModel.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Editor.h"
#include "Engine/DataAsset.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Misc/OutputDeviceNull.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "knightAnimDebugSettings.h"
#include "knightAnimDebugSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogknightFeedbackTuning, Log, All);

namespace knightFeedbackTuning
{
	/** 어빌리티 → 몽타주 → 노티파이 → 셰이크 → 패턴까지 내려가면 충분하다. */
	static constexpr int32 MaxObjectDepth = 4;
	static constexpr int32 MaxStructDepth = 4;

	static FString CleanName(const UObject* Object)
	{
		if (!Object)
		{
			return FString();
		}
		FString Name = Object->HasAnyFlags(RF_ClassDefaultObject) ? Object->GetClass()->GetName() : Object->GetName();
		Name.RemoveFromStart(TEXT("Default__"));
		Name.RemoveFromEnd(TEXT("_C"));
		return Name;
	}

	static bool IsInWorld(const UObject* Object)
	{
		return Object->GetTypedOuter<UWorld>() != nullptr;
	}

	static bool ArchetypeChainContains(const UObject* Object, const UObject* Archetype)
	{
		for (const UObject* It = Object->GetArchetype(); It; It = It->GetArchetype())
		{
			if (It == Archetype)
			{
				return true;
			}
			if (It->HasAnyFlags(RF_ClassDefaultObject))
			{
				break;
			}
		}
		return false;
	}

	static FString FormatNumber(double Value)
	{
		return FString::SanitizeFloat(Value, 0);
	}

	/**
	 * 콤보 2단 이후처럼 앞 단계 태그(Combo.*)가 있어야 나가는 공격은 단독으로 실행할 수 없다. 강제 실행할 때만 빠진 콤보 태그를
	 * 잠깐 붙였다가 실행 직후 뗀다. 다른 조건(달리기 중 등)은 건드리지 않는다.
	 */
	static FGameplayTagContainer FindMissingComboTags(const UAbilitySystemComponent& ASC, const UGameplayAbility& Ability)
	{
		FGameplayTagContainer Missing;
		static const FStructProperty* const RequiredProperty =
			FindFProperty<FStructProperty>(UGameplayAbility::StaticClass(), TEXT("ActivationRequiredTags"));
		const FGameplayTagContainer* const Required = RequiredProperty ? RequiredProperty->ContainerPtrToValuePtr<FGameplayTagContainer>(&Ability) : nullptr;
		if (!Required)
		{
			return Missing;
		}
		static const FGameplayTag ComboRoot = FGameplayTag::RequestGameplayTag(TEXT("Combo"), false);
		for (const FGameplayTag& Tag : *Required)
		{
			if (ComboRoot.IsValid() && Tag.MatchesTag(ComboRoot) && !ASC.HasMatchingGameplayTag(Tag))
			{
				Missing.AddTag(Tag);
			}
		}
		return Missing;
	}

	static bool ActivateAndLog(UAbilitySystemComponent& ASC, const FGameplayAbilitySpecHandle Handle, const FString& Name)
	{
		const FGameplayAbilitySpec* const FoundSpec = ASC.FindAbilitySpecFromHandle(Handle);
		const FGameplayTagContainer ComboTags = FoundSpec && FoundSpec->Ability ? FindMissingComboTags(ASC, *FoundSpec->Ability) : FGameplayTagContainer();
		ASC.AddLooseGameplayTags(ComboTags);
		const bool bActivated = ASC.TryActivateAbility(Handle);
		ASC.RemoveLooseGameplayTags(ComboTags);
		if (bActivated)
		{
			UE_LOG(LogknightFeedbackTuning, Display, TEXT("플레이어 공격 %s 실행%s"), *Name,
				ComboTags.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (콤보 태그 %s 잠깐 붙임)"), *ComboTags.ToStringSimple()));
			return true;
		}
		// 왜 막혔는지(막는 태그, 스탠스, 꺼 둔 어빌리티)를 같이 적는다.
		FGameplayTagContainer FailTags;
		if (const FGameplayAbilitySpec* const Spec = ASC.FindAbilitySpecFromHandle(Handle); Spec && Spec->Ability)
		{
			Spec->Ability->CanActivateAbility(Handle, ASC.AbilityActorInfo.Get(), nullptr, nullptr, &FailTags);
		}
		FGameplayTagContainer Owned;
		ASC.GetOwnedGameplayTags(Owned);
		UE_LOG(LogknightFeedbackTuning, Warning, TEXT("플레이어 공격 %s 실행 못 함. 실패 태그 [%s], 플레이어 태그 [%s]"),
			*Name, *FailTags.ToStringSimple(), *Owned.ToStringSimple());
		return false;
	}
}

FknightFeedbackTuningModel& FknightFeedbackTuningModel::Get()
{
	static FknightFeedbackTuningModel Model;
	if (!Model.EndPIEHandle.IsValid())
	{
		Model.EndPIEHandle = FEditorDelegates::EndPIE.AddRaw(&Model, &FknightFeedbackTuningModel::HandleEndPIE);
	}
	return Model;
}

void FknightFeedbackTuningModel::Shutdown()
{
	FknightFeedbackTuningModel& Model = Get();
	FEditorDelegates::EndPIE.Remove(Model.EndPIEHandle);
	Model.EndPIEHandle.Reset();
	Model.OnReset.Clear();
	Model.OnCollected.Clear();
}

void FknightFeedbackTuningModel::HandleEndPIE(bool bIsSimulating)
{
	const int32 Reverted = RevertUnsaved();
	if (Reverted > 0)
	{
		UE_LOG(LogknightFeedbackTuning, Display, TEXT("PIE가 끝나 저장하지 않은 값 %d개를 되돌렸다."), Reverted);
	}
	Clear();
	OnReset.Broadcast(Reverted);
}

void FknightFeedbackTuningModel::Clear()
{
	World.Reset();
	Monster.Reset();
	Player.Reset();
	Sources.Reset();
	Entries.Reset();
	Visited.Reset();
}

void FknightFeedbackTuningModel::Collect(UWorld* PlayWorld)
{
	// 원래 값을 새로 읽으므로, 바꾼 값이 원래 값으로 읽히지 않게 먼저 돌린다.
	RevertUnsaved();
	Clear();
	if (!PlayWorld)
	{
		return;
	}
	World = PlayWorld;
	UknightAnimDebugSubsystem* const Subsystem = UknightAnimDebugSubsystem::Get(PlayWorld);
	Monster = Subsystem ? Subsystem->ResolveTarget() : nullptr;
	const APlayerController* const PC = PlayWorld->GetFirstPlayerController();
	Player = PC ? PC->GetPawn() : nullptr;

	const UknightAnimDebugSettings* const Settings = GetDefault<UknightAnimDebugSettings>();
	Keywords = Settings->FeedbackPropertyKeywords;
	AllPropertyClasses.Reset();
	for (const TSoftClassPtr<UObject>& ClassPtr : Settings->FeedbackAllPropertyClasses)
	{
		if (const UClass* const Class = ClassPtr.LoadSynchronous())
		{
			AllPropertyClasses.Add(Class);
		}
	}

	auto AddPawn = [this](APawn* Pawn, EknightFeedbackGroup Group)
	{
		if (!Pawn)
		{
			return;
		}
		const FString Owner = knightFeedbackTuning::CleanName(Pawn->GetClass()->GetDefaultObject());
		AddObject(Pawn, Owner, Group, 0);
		TInlineComponentArray<UActorComponent*> Components(Pawn);
		for (UActorComponent* const Component : Components)
		{
			AddObject(Component, Owner + TEXT(" › ") + Component->GetName(), Group, 1);
		}
		if (const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
		{
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.Ability)
				{
					AddObject(Spec.Ability, knightFeedbackTuning::CleanName(Spec.Ability), Group, 1);
				}
			}
		}
	};
	// 몬스터 → 카메라 → 플레이어 순서. 플레이어 폰의 카메라 컴포넌트가 카메라 묶음에 들어가게 카메라를 먼저 훑는다.
	AddPawn(Monster.Get(), EknightFeedbackGroup::Monster);
	AddCamera(PlayWorld->GetFirstPlayerController());
	AddPawn(Player.Get(), EknightFeedbackGroup::Player);
	OnCollected.Broadcast();
}

void FknightFeedbackTuningModel::AddCamera(APlayerController* PC)
{
	const EknightFeedbackGroup Group = EknightFeedbackGroup::Camera;
	if (APlayerCameraManager* const CameraManager = PC ? PC->PlayerCameraManager.Get() : nullptr)
	{
		const FString Label = knightFeedbackTuning::CleanName(CameraManager->GetClass()->GetDefaultObject());
		AddObject(CameraManager, Label, Group, 0);
		TArray<UObject*> Inner;
		GetObjectsWithOuter(CameraManager, Inner);
		for (UObject* const Object : Inner)
		{
			AddObject(Object, Label + TEXT(" › ") + knightFeedbackTuning::CleanName(FindPersistentArchetype(Object)), Group, 1);
		}
	}

	if (APawn* const PlayerPawn = Player.Get())
	{
		const FString Owner = knightFeedbackTuning::CleanName(PlayerPawn->GetClass()->GetDefaultObject());
		TInlineComponentArray<UCameraComponent*> Cameras(PlayerPawn);
		for (UCameraComponent* const Camera : Cameras)
		{
			const FString Label = Owner + TEXT(" › ") + Camera->GetName();
			AddObject(Camera, Label, Group, 1);
			// 카메라 모드 인스턴스, 가림·페이드 처리기처럼 컴포넌트가 만든 객체.
			TArray<UObject*> Inner;
			GetObjectsWithOuter(Camera, Inner);
			for (UObject* const Object : Inner)
			{
				AddObject(Object, Label + TEXT(" › ") + knightFeedbackTuning::CleanName(FindPersistentArchetype(Object)), Group, 2);
			}
		}
	}

	// 지금 PIE에 없는 모드·모디파이어(공격 중에만 생기는 어둠·귀먹음 등)도 미리 바꿀 수 있게 클래스 기본값을 잡는다.
	for (const TSoftClassPtr<UObject>& ClassPtr : GetDefault<UknightAnimDebugSettings>()->FeedbackCameraClasses)
	{
		const UClass* const Base = ClassPtr.LoadSynchronous();
		if (!Base)
		{
			continue;
		}
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* const Class = *It;
			if (!Class->IsChildOf(Base) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists | CLASS_Deprecated)
				|| Class->GetOutermost()->GetFName() == TEXT("/Script/Engine")
				|| Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
			{
				continue;
			}
			AddObject(Class->GetDefaultObject(), knightFeedbackTuning::CleanName(Class->GetDefaultObject()), Group, 1);
		}
	}
}

const TCHAR* FknightFeedbackTuningModel::GroupLabel(EknightFeedbackGroup Group)
{
	switch (Group)
	{
	case EknightFeedbackGroup::Monster: return TEXT("[몬스터]");
	case EknightFeedbackGroup::Player:  return TEXT("[플레이어]");
	case EknightFeedbackGroup::Camera:  return TEXT("[카메라]");
	}
	return TEXT("");
}

void FknightFeedbackTuningModel::GetPlayerAttackNames(TArray<FString>& OutNames) const
{
	for (const FknightFeedbackEntry& Entry : Entries)
	{
		const FknightFeedbackSource& Source = Sources[Entry.SourceIndex];
		const UObject* const Object = Source.Object.Get();
		if (Source.Group == EknightFeedbackGroup::Player && Object && Object->IsA<UGameplayAbility>() && Entry.Path.EndsWith(TEXT("HitStopDuration")))
		{
			OutNames.AddUnique(knightFeedbackTuning::CleanName(Object));
		}
	}
}

void FknightFeedbackTuningModel::PlacePlayer(float Distance) const
{
	const APawn* const MonsterPawn = Monster.Get();
	APawn* const PlayerPawn = Player.Get();
	if (!MonsterPawn || !PlayerPawn)
	{
		return;
	}
	const FVector Forward = MonsterPawn->GetActorForwardVector().GetSafeNormal2D();
	FVector Location = MonsterPawn->GetActorLocation() + Forward * Distance + FVector(0.f, 0.f, 400.f);

	// 위에서 떨어뜨리면 착지 동작이 공격을 끊고, 몬스터 턱처럼 캡슐 밖으로 나온 부분 위에 서면 공중 상태가 된다.
	// 바닥을 찾아 그 위에 바로 세우되, 플레이어 캡슐이 무언가와 겹치면 뒤로 물러나며 빈자리를 찾는다.
	const ACharacter* const PlayerCharacter = Cast<ACharacter>(PlayerPawn);
	const UCapsuleComponent* const Capsule = PlayerCharacter ? PlayerCharacter->GetCapsuleComponent() : nullptr;
	UWorld* const PlayWorld = PlayerPawn->GetWorld();
	if (PlayWorld && Capsule)
	{
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(knightFeedbackTuningPlace), false);
		TraceParams.AddIgnoredActor(MonsterPawn);
		TraceParams.AddIgnoredActor(PlayerPawn);
		FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(knightFeedbackTuningPlaceFree), false);
		OverlapParams.AddIgnoredActor(PlayerPawn);
		const FCollisionShape Shape = Capsule->GetCollisionShape();
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		constexpr float Step = 50.f;
		constexpr int32 MaxSteps = 30;
		for (int32 StepIndex = 0; StepIndex <= MaxSteps; ++StepIndex)
		{
			const FVector Ground = MonsterPawn->GetActorLocation() + Forward * (Distance + Step * StepIndex);
			FHitResult Hit;
			if (!PlayWorld->LineTraceSingleByChannel(Hit, Ground + FVector(0.f, 0.f, 2000.f), Ground - FVector(0.f, 0.f, 5000.f), ECC_Visibility, TraceParams))
			{
				continue;
			}
			const FVector Candidate = Hit.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 2.f);
			if (!PlayWorld->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, Capsule->GetCollisionObjectType(), Shape, OverlapParams,
				FCollisionResponseParams(Capsule->GetCollisionResponseToChannels())))
			{
				Location = Candidate;
				break;
			}
		}
	}
	const FRotator Facing = (-Forward).Rotation();
	PlayerPawn->TeleportTo(Location, Facing, false, true);
	if (AController* const Controller = PlayerPawn->GetController())
	{
		Controller->SetControlRotation(Facing);
	}
}

float FknightFeedbackTuningModel::GetPlayerAttackDistance() const
{
	// 거대 몬스터는 캡슐이 몸 크기를 따르므로 캡슐 반지름을 몸통 가장자리로 본다.
	const ACharacter* const Character = Cast<ACharacter>(Monster.Get());
	const float Radius = Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
	return Radius + GetDefault<UknightAnimDebugSettings>()->FeedbackPlayerAttackGap;
}

bool FknightFeedbackTuningModel::PlayPlayerAttack(const FString& Name, bool bPlace) const
{
	UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player.Get());
	if (!ASC)
	{
		return false;
	}
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && knightFeedbackTuning::CleanName(Spec.Ability).Equals(Name, ESearchCase::IgnoreCase))
		{
			ACharacter* const Character = Cast<ACharacter>(Player.Get());
			UWorld* const PlayWorld = World.Get();
			if (!bPlace || !Character || !PlayWorld)
			{
				return knightFeedbackTuning::ActivateAndLog(*ASC, Spec.Handle, Name);
			}

			// 순간이동 직후에는 이동 컴포넌트가 바닥을 다시 찾는 동안 공중(State.Airborne)일 수 있다.
			// 바닥에 SettleTime만큼 머문 뒤 실행해야 착지 동작에 공격이 끊기지 않는다.
			PlacePlayer(GetPlayerAttackDistance());
			const TWeakObjectPtr<ACharacter> WeakCharacter = Character;
			const TWeakObjectPtr<UAbilitySystemComponent> WeakASC = ASC;
			const FGameplayAbilitySpecHandle Handle = Spec.Handle;
			const double Deadline = PlayWorld->GetTimeSeconds() + 2.0;
			constexpr double SettleTime = 0.2;
			const TSharedRef<FTimerHandle> Timer = MakeShared<FTimerHandle>();
			const TSharedRef<double> LandedSince = MakeShared<double>(-1.0);
			PlayWorld->GetTimerManager().SetTimer(*Timer, FTimerDelegate::CreateLambda([WeakCharacter, WeakASC, Handle, Deadline, Timer, LandedSince, Name]()
			{
				ACharacter* const Waiting = WeakCharacter.Get();
				UAbilitySystemComponent* const WaitingASC = WeakASC.Get();
				UWorld* const WaitingWorld = Waiting ? Waiting->GetWorld() : nullptr;
				if (!WaitingASC || !WaitingWorld)
				{
					return;
				}
				const double Now = WaitingWorld->GetTimeSeconds();
				const bool bLanded = !Waiting->GetCharacterMovement() || !Waiting->GetCharacterMovement()->IsFalling();
				*LandedSince = bLanded ? (*LandedSince < 0.0 ? Now : *LandedSince) : -1.0;
				const bool bSettled = *LandedSince >= 0.0 && Now - *LandedSince >= SettleTime;
				if (!bSettled && Now < Deadline)
				{
					return;
				}
				// 타이머를 지우면 이 람다(캡처한 값 포함)가 사라지므로 쓸 값을 먼저 꺼내 둔다.
				const FString AttackName = Name;
				const FGameplayAbilitySpecHandle AttackHandle = Handle;
				WaitingWorld->GetTimerManager().ClearTimer(*Timer);
				knightFeedbackTuning::ActivateAndLog(*WaitingASC, AttackHandle, AttackName);
			}), 0.05f, true);
			return true;
		}
	}
	UE_LOG(LogknightFeedbackTuning, Warning, TEXT("플레이어에게 %s 어빌리티가 없다."), *Name);
	return false;
}

bool FknightFeedbackTuningModel::MatchesFilter(int32 EntryIndex, const FString& Filter) const
{
	const FknightFeedbackEntry& Entry = Entries[EntryIndex];
	const FknightFeedbackSource& Source = Sources[Entry.SourceIndex];
	const FString Text = FString(GroupLabel(Source.Group)) + TEXT(" ") + Source.Label + TEXT(" · ") + Entry.Path;
	TArray<FString> Words;
	Filter.ParseIntoArrayWS(Words);
	for (const FString& Word : Words)
	{
		if (!Text.Contains(Word))
		{
			return false;
		}
	}
	return true;
}

void FknightFeedbackTuningModel::FindEntries(const FString& Filter, TArray<int32>& OutIndices) const
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (MatchesFilter(Index, Filter))
		{
			OutIndices.Add(Index);
		}
	}
}

FString FknightFeedbackTuningModel::DescribeEntry(int32 EntryIndex) const
{
	const FknightFeedbackEntry& Entry = Entries[EntryIndex];
	const FknightFeedbackSource& Source = Sources[Entry.SourceIndex];
	const UObject* const Archetype = Source.Object.Get();
	const UWorld* const PlayWorld = World.Get();

	// PIE 복사본 값도 같이 적어 원본만 바뀌고 복사본이 안 바뀐 경우를 바로 볼 수 있게 한다.
	int32 Copies = 0;
	int32 Mismatch = 0;
	const double Value = GetValue(EntryIndex);
	if (Archetype && PlayWorld)
	{
		ForEachObjectOfClass(Archetype->GetClass(), [&](UObject* Object)
		{
			if (Object != Archetype && !Object->HasAnyFlags(RF_ClassDefaultObject) && Object->GetTypedOuter<UWorld>() == PlayWorld
				&& knightFeedbackTuning::ArchetypeChainContains(Object, Archetype))
			{
				++Copies;
				if (const void* const Ptr = ResolveValue(Object, Entry.Chain); Ptr && !FMath::IsNearlyEqual(ReadNumber(Entry.Chain.Last(), Ptr), Value, 1e-6))
				{
					++Mismatch;
				}
			}
		}, false);
	}
	return FString::Printf(TEXT("%s %s · %s = %s (원래 %s, PIE 복사본 %d개, 다른 값 %d개)%s"),
		GroupLabel(Source.Group), *Source.Label, *Entry.Path,
		*knightFeedbackTuning::FormatNumber(Value), *knightFeedbackTuning::FormatNumber(Entry.Original), Copies, Mismatch,
		IsSaveLocked(Entry.SourceIndex) ? TEXT(" [저장 잠금]") : TEXT(""));
}

UObject* FknightFeedbackTuningModel::FindPersistentArchetype(UObject* Live)
{
	UObject* Object = Live;
	while (Object && knightFeedbackTuning::IsInWorld(Object))
	{
		Object = Object->GetArchetype();
	}
	return Object;
}

void FknightFeedbackTuningModel::AddObject(UObject* Live, const FString& Label, EknightFeedbackGroup Group, int32 Depth)
{
	if (!Live || Depth > knightFeedbackTuning::MaxObjectDepth)
	{
		return;
	}
	UObject* const Archetype = FindPersistentArchetype(Live);
	if (!Archetype || Visited.Contains(Archetype))
	{
		return;
	}
	Visited.Add(Archetype);

	FknightFeedbackSource& Source = Sources.AddDefaulted_GetRef();
	Source.Object = Archetype;
	Source.Label = Label;
	Source.Group = Group;
	Source.bNative = Archetype->GetOutermost()->GetName().StartsWith(TEXT("/Script/"));
	const int32 SourceIndex = Sources.Num() - 1;
	const int32 EntryCountBefore = Entries.Num();

	bool bAllPropertyClass = false;
	for (const UClass* const Class : AllPropertyClasses)
	{
		bAllPropertyClass |= Archetype->IsA(Class);
	}
	// 객체 이름이 걸리면(GuardPush 컴포넌트, BreakFlinch 어빌리티 …) 그 객체의 숫자는 모두 피드백 값으로 본다.
	// 카메라 묶음은 값 이름이 제각각(FieldOfView, ArmLength …)이라 게임 클래스가 선언한 숫자를 모두 보인다.
	const bool bIncludeAll = bAllPropertyClass || Group == EknightFeedbackGroup::Camera
		|| MatchesKeyword(knightFeedbackTuning::CleanName(Archetype));

	TArray<const FProperty*> Chain;
	AddProperties(SourceIndex, Archetype->GetClass(), Archetype, Chain, FString(), bIncludeAll, !bAllPropertyClass, 0);

	// 값이 없는 출처는 뺀다. 참조한 몽타주 노티파이 등은 아래에서 각자 출처가 된다.
	if (Entries.Num() == EntryCountBefore)
	{
		Sources.Pop();
	}

	// 참조를 따라 내려간다. 프로퍼티 값 수집과 따로 해서 출처 순서가 "자기 값 → 참조한 것" 순이 되게 한다.
	auto Follow = [&](UObject* Referenced)
	{
		if (!Referenced)
		{
			return;
		}
		if (const UAnimMontage* const Montage = Cast<UAnimMontage>(Referenced))
		{
			for (const FAnimNotifyEvent& Event : Montage->Notifies)
			{
				UObject* const Notify = Event.Notify ? static_cast<UObject*>(Event.Notify) : static_cast<UObject*>(Event.NotifyStateClass);
				if (Notify)
				{
					const FString NotifyName = Event.NotifyName.IsNone() ? knightFeedbackTuning::CleanName(Notify) : Event.NotifyName.ToString();
					AddObject(Notify, FString::Printf(TEXT("%s › %s @%.2fs"), *Montage->GetName(), *NotifyName, Event.GetTime()), Group, Depth + 1);
				}
			}
			return;
		}
		AddObject(Referenced, Label + TEXT(" › ") + knightFeedbackTuning::CleanName(Referenced), Group, Depth + 1);
	};

	TFunction<void(const UStruct*, const void*, int32)> Walk = [&](const UStruct* Struct, const void* Container, int32 StructDepth)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* const Property = *It;
			if (const FClassProperty* const ClassProperty = CastField<FClassProperty>(Property))
			{
				const UClass* const Class = Cast<UClass>(ClassProperty->GetObjectPropertyValue_InContainer(Container));
				if (Class && Class->IsChildOf(UCameraShakeBase::StaticClass()))
				{
					Follow(Class->GetDefaultObject());
				}
			}
			else if (const FObjectPropertyBase* const ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				UObject* const Value = ObjectProperty->GetObjectPropertyValue_InContainer(Container);
				const bool bInstanced = Property->HasAnyPropertyFlags(CPF_InstancedReference | CPF_PersistentInstance);
				if (Value && (bInstanced || Value->IsA<UAnimMontage>() || Value->IsA<UDataAsset>()))
				{
					Follow(Value);
				}
			}
			else if (const FStructProperty* const StructProperty = CastField<FStructProperty>(Property))
			{
				if (StructDepth < knightFeedbackTuning::MaxStructDepth)
				{
					Walk(StructProperty->Struct, StructProperty->ContainerPtrToValuePtr<void>(Container), StructDepth + 1);
				}
			}
			else if (const FArrayProperty* const ArrayProperty = CastField<FArrayProperty>(Property))
			{
				// 배열은 값 칸으로 보이지 않지만 안에 든 몽타주·인스턴스 객체(노티파이 스텝 등)는 따라간다.
				FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Container));
				const FProperty* const Inner = ArrayProperty->Inner;
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					const uint8* const Element = Helper.GetRawPtr(Index);
					if (const FObjectPropertyBase* const InnerObject = CastField<FObjectPropertyBase>(Inner))
					{
						UObject* const Value = InnerObject->GetObjectPropertyValue(Element);
						const bool bInstanced = Inner->HasAnyPropertyFlags(CPF_InstancedReference | CPF_PersistentInstance)
							|| Property->HasAnyPropertyFlags(CPF_InstancedReference | CPF_PersistentInstance);
						if (Value && (bInstanced || Value->IsA<UAnimMontage>()))
						{
							Follow(Value);
						}
					}
					else if (const FStructProperty* const InnerStruct = CastField<FStructProperty>(Inner))
					{
						if (StructDepth < knightFeedbackTuning::MaxStructDepth)
						{
							Walk(InnerStruct->Struct, Element, StructDepth + 1);
						}
					}
				}
			}
		}
	};
	Walk(Archetype->GetClass(), Archetype, 0);
}

bool FknightFeedbackTuningModel::MatchesKeyword(const FString& Name) const
{
	for (const FString& Keyword : Keywords)
	{
		if (Name.Contains(Keyword, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

void FknightFeedbackTuningModel::AddProperties(int32 SourceIndex, const UStruct* Struct, const void* Container,
	TArray<const FProperty*>& Chain, const FString& Prefix, bool bIncludeAll, bool bSkipEngineProperties, int32 Depth)
{
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		const FProperty* const Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Deprecated))
		{
			continue;
		}
		// 엔진 클래스가 선언한 값(이동 컴포넌트의 PushForceFactor 등)은 이름이 걸려도 전투 피드백이 아니다.
		const UClass* const OwnerClass = Property->GetOwnerClass();
		if (bSkipEngineProperties && OwnerClass && OwnerClass->GetOutermost()->GetFName() == TEXT("/Script/Engine"))
		{
			continue;
		}
		const FString Name = Property->GetName();
		const FString Path = Prefix.IsEmpty() ? Name : Prefix + TEXT(".") + Name;

		if (const FStructProperty* const StructProperty = CastField<FStructProperty>(Property))
		{
			if (Depth < knightFeedbackTuning::MaxStructDepth)
			{
				Chain.Add(Property);
				AddProperties(SourceIndex, StructProperty->Struct, StructProperty->ContainerPtrToValuePtr<void>(Container), Chain, Path,
					bIncludeAll || MatchesKeyword(Name), bSkipEngineProperties, Depth + 1);
				Chain.Pop();
			}
			continue;
		}

		const FNumericProperty* const Numeric = CastField<FNumericProperty>(Property);
		if (!Numeric || Numeric->IsEnum() || Property->ArrayDim != 1)
		{
			continue;
		}
		if (!bIncludeAll && !MatchesKeyword(Name))
		{
			continue;
		}

		FknightFeedbackEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.SourceIndex = SourceIndex;
		Entry.Path = Path;
		Entry.Chain = Chain;
		Entry.Chain.Add(Property);
		Entry.bInteger = Numeric->IsInteger();
		Entry.Original = ReadNumber(Property, Property->ContainerPtrToValuePtr<void>(Container));
		Entry.Saved = Entry.Original;
	}
}

void* FknightFeedbackTuningModel::ResolveValue(UObject* Object, const TArray<const FProperty*>& Chain)
{
	if (!Object || Chain.IsEmpty())
	{
		return nullptr;
	}
	void* Container = Object;
	for (const FProperty* const Property : Chain)
	{
		Container = Property->ContainerPtrToValuePtr<void>(Container);
	}
	return Container;
}

double FknightFeedbackTuningModel::ReadNumber(const FProperty* Leaf, const void* Ptr)
{
	const FNumericProperty* const Numeric = CastFieldChecked<const FNumericProperty>(Leaf);
	return Numeric->IsFloatingPoint() ? Numeric->GetFloatingPointPropertyValue(Ptr) : static_cast<double>(Numeric->GetSignedIntPropertyValue(Ptr));
}

void FknightFeedbackTuningModel::WriteNumber(const FProperty* Leaf, void* Ptr, double Value)
{
	const FNumericProperty* const Numeric = CastFieldChecked<const FNumericProperty>(Leaf);
	if (Numeric->IsFloatingPoint())
	{
		Numeric->SetFloatingPointPropertyValue(Ptr, Value);
	}
	else
	{
		Numeric->SetIntPropertyValue(Ptr, static_cast<int64>(FMath::RoundToDouble(Value)));
	}
}

double FknightFeedbackTuningModel::GetValue(int32 EntryIndex) const
{
	const FknightFeedbackEntry& Entry = Entries[EntryIndex];
	UObject* const Object = Sources[Entry.SourceIndex].Object.Get();
	const void* const Ptr = ResolveValue(Object, Entry.Chain);
	return Ptr ? ReadNumber(Entry.Chain.Last(), Ptr) : Entry.Saved;
}

void FknightFeedbackTuningModel::SetValue(int32 EntryIndex, double Value)
{
	const FknightFeedbackEntry& Entry = Entries[EntryIndex];
	UObject* const Object = Sources[Entry.SourceIndex].Object.Get();
	if (void* const Ptr = ResolveValue(Object, Entry.Chain))
	{
		WriteNumber(Entry.Chain.Last(), Ptr, Value);
		ApplyToLiveCopies(Entry, Value);
	}
}

void FknightFeedbackTuningModel::ApplyToLiveCopies(const FknightFeedbackEntry& Entry, double Value) const
{
	const UObject* const Archetype = Sources[Entry.SourceIndex].Object.Get();
	const UWorld* const PlayWorld = World.Get();
	if (!Archetype || !PlayWorld)
	{
		return;
	}
	ForEachObjectOfClass(Archetype->GetClass(), [&](UObject* Object)
	{
		if (Object != Archetype && !Object->HasAnyFlags(RF_ClassDefaultObject) && Object->GetTypedOuter<UWorld>() == PlayWorld
			&& knightFeedbackTuning::ArchetypeChainContains(Object, Archetype))
		{
			if (void* const Ptr = ResolveValue(Object, Entry.Chain))
			{
				WriteNumber(Entry.Chain.Last(), Ptr, Value);
			}
		}
	}, false);
}

bool FknightFeedbackTuningModel::IsChanged(int32 EntryIndex) const
{
	return !FMath::IsNearlyEqual(GetValue(EntryIndex), Entries[EntryIndex].Saved, 1e-6);
}

bool FknightFeedbackTuningModel::IsSaveLocked(int32 SourceIndex) const
{
	const FknightFeedbackSource& Source = Sources[SourceIndex];
	if (Source.bNative)
	{
		return true;
	}
	const UknightAnimDebugSettings* const Settings = GetDefault<UknightAnimDebugSettings>();
	if (Source.Group == EknightFeedbackGroup::Player)
	{
		return !Settings->bFeedbackAllowSavingPlayerValues;
	}
	if (Source.Group == EknightFeedbackGroup::Camera)
	{
		// 카메라 반응 표처럼 따로 만든 에셋은 저장하고, 플레이어 담당 에셋(셰이크 큐, 카메라 모드 등)은 잠근다.
		const UObject* const Object = Source.Object.Get();
		const FString Path = Object ? Object->GetOutermost()->GetName() : FString();
		for (const FString& Prefix : Settings->FeedbackLockedPathPrefixes)
		{
			if (Path.StartsWith(Prefix))
			{
				return !Settings->bFeedbackAllowSavingPlayerValues;
			}
		}
	}
	return false;
}

void FknightFeedbackTuningModel::GetImpactMoments(TArray<FString>& OutMoments, FString& OutSourceLabel) const
{
	const UClass* const ImpactClass = GetDefault<UknightAnimDebugSettings>()->FeedbackImpactSettingsClass.Get();
	for (const FknightFeedbackSource& Source : Sources)
	{
		const UObject* const Object = Source.Object.Get();
		if (!ImpactClass || !Object || !Object->IsA(ImpactClass))
		{
			continue;
		}
		OutSourceLabel = Source.Label;
		for (TFieldIterator<FStructProperty> It(Object->GetClass()); It; ++It)
		{
			OutMoments.Add(It->GetName());
		}
		return;
	}
}

bool FknightFeedbackTuningModel::PreviewImpactMoment(const FString& Moment) const
{
	UWorld* const PlayWorld = World.Get();
	APlayerController* const PC = PlayWorld ? PlayWorld->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* const CameraManager = PC ? PC->PlayerCameraManager.Get() : nullptr;
	const FName Function = GetDefault<UknightAnimDebugSettings>()->FeedbackImpactPreviewFunction;
	if (!CameraManager || Function.IsNone() || !CameraManager->FindFunction(Function))
	{
		UE_LOG(LogknightFeedbackTuning, Warning, TEXT("카메라 매니저에 %s 함수가 없다."), *Function.ToString());
		return false;
	}
	// 게임 모듈에 의존하지 않도록 이름으로 부른다. 세기 0 = 그 항목의 최대 세기.
	FOutputDeviceNull Output;
	return CameraManager->CallFunctionByNameWithArguments(*FString::Printf(TEXT("%s %s 0"), *Function.ToString(), *Moment), Output, nullptr, true);
}

int32 FknightFeedbackTuningModel::CountChanged() const
{
	int32 Count = 0;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Count += IsChanged(Index) ? 1 : 0;
	}
	return Count;
}

int32 FknightFeedbackTuningModel::SaveChanged(FString& OutMessage)
{
	TArray<UPackage*> Packages;
	TArray<int32> Saving;
	int32 Locked = 0;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (!IsChanged(Index))
		{
			continue;
		}
		const int32 SourceIndex = Entries[Index].SourceIndex;
		UObject* const Object = Sources[SourceIndex].Object.Get();
		if (!Object || IsSaveLocked(SourceIndex))
		{
			++Locked;
			continue;
		}
		UPackage* const Package = Object->GetOutermost();
		Package->MarkPackageDirty();
		Packages.AddUnique(Package);
		Saving.Add(Index);
	}

	if (!Packages.IsEmpty() && !UEditorLoadingAndSavingUtils::SavePackages(Packages, false))
	{
		OutMessage = TEXT("저장하지 못했어요. 파일이 읽기 전용인지 확인하세요.");
		return 0;
	}
	for (const int32 Index : Saving)
	{
		Entries[Index].Saved = GetValue(Index);
	}
	OutMessage = FString::Printf(TEXT("값 %d개를 에셋 %d개에 저장했어요."), Saving.Num(), Packages.Num());
	if (Locked > 0)
	{
		OutMessage += FString::Printf(TEXT(" 저장 잠긴 값 %d개(플레이어·네이티브)는 그대로 남겨 두었어요."), Locked);
	}
	return Saving.Num();
}

int32 FknightFeedbackTuningModel::RevertUnsaved()
{
	int32 Count = 0;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (IsChanged(Index))
		{
			SetValue(Index, Entries[Index].Saved);
			++Count;
		}
	}
	return Count;
}

FString FknightFeedbackTuningModel::DescribeChanges() const
{
	FString Text;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FknightFeedbackEntry& Entry = Entries[Index];
		const double Current = GetValue(Index);
		if (FMath::IsNearlyEqual(Current, Entry.Original, 1e-6))
		{
			continue;
		}
		const FknightFeedbackSource& Source = Sources[Entry.SourceIndex];
		Text += FString::Printf(TEXT("%s%s · %s: %s → %s%s\n"),
			Source.Group == EknightFeedbackGroup::Monster ? TEXT("") : *(FString(GroupLabel(Source.Group)) + TEXT(" ")),
			*Source.Label, *Entry.Path,
			*knightFeedbackTuning::FormatNumber(Entry.Original), *knightFeedbackTuning::FormatNumber(Current),
			IsChanged(Index) ? TEXT(" (저장 안 함)") : TEXT(""));
	}
	return Text;
}

namespace knightFeedbackTuning
{
	/** PIE 월드에서 모델을 쓴다. 아직 이 월드에서 모으지 않았으면 먼저 모은다. */
	static FknightFeedbackTuningModel* PrepareModel(UWorld* World)
	{
		UWorld* const PlayWorld = World && World->IsPlayInEditor() ? World : (GEditor ? GEditor->PlayWorld.Get() : nullptr);
		if (!PlayWorld)
		{
			UE_LOG(LogknightFeedbackTuning, Warning, TEXT("PIE 중에만 쓸 수 있다."));
			return nullptr;
		}
		FknightFeedbackTuningModel& Model = FknightFeedbackTuningModel::Get();
		if (!Model.IsCollected(PlayWorld))
		{
			Model.Collect(PlayWorld);
		}
		return &Model;
	}

	static void ListCommand(const TArray<FString>& Args, UWorld* World)
	{
		FknightFeedbackTuningModel* const Model = PrepareModel(World);
		if (!Model)
		{
			return;
		}
		const bool bRefresh = Args.Num() > 0 && Args[0] == TEXT("-refresh");
		if (bRefresh)
		{
			Model->Collect(GEditor->PlayWorld);
		}
		TArray<int32> Indices;
		Model->FindEntries(bRefresh ? FString() : FString::Join(Args, TEXT(" ")), Indices);
		UE_LOG(LogknightFeedbackTuning, Display, TEXT("대상 %s · 출처 %d개 · 값 %d개 중 %d개"),
			Model->GetMonster() ? *Model->GetMonster()->GetName() : TEXT("없음"), Model->GetSources().Num(), Model->GetEntries().Num(), Indices.Num());
		for (const int32 Index : Indices)
		{
			UE_LOG(LogknightFeedbackTuning, Display, TEXT("  %s"), *Model->DescribeEntry(Index));
		}
	}

	static void SetCommand(const TArray<FString>& Args, UWorld* World)
	{
		FknightFeedbackTuningModel* const Model = PrepareModel(World);
		if (!Model || Args.Num() < 2)
		{
			UE_LOG(LogknightFeedbackTuning, Warning, TEXT("knight.FeedbackTuning.Set <Filter> <Value>"));
			return;
		}
		// 마지막 인자가 값이고 앞의 인자는 모두 찾을 낱말이다.
		TArray<FString> Words = Args;
		const FString ValueText = Words.Pop();
		TArray<int32> Indices;
		Model->FindEntries(FString::Join(Words, TEXT(" ")), Indices);
		const double Value = FCString::Atod(*ValueText);
		for (const int32 Index : Indices)
		{
			Model->SetValue(Index, Value);
			UE_LOG(LogknightFeedbackTuning, Display, TEXT("  %s"), *Model->DescribeEntry(Index));
		}
		UE_LOG(LogknightFeedbackTuning, Display, TEXT("값 %d개를 %s(으)로 바꿨다."), Indices.Num(), *ValueText);
	}

	static void SaveCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (FknightFeedbackTuningModel* const Model = PrepareModel(World))
		{
			FString Message;
			Model->SaveChanged(Message);
			UE_LOG(LogknightFeedbackTuning, Display, TEXT("%s"), *Message);
		}
	}

	static void RevertCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (FknightFeedbackTuningModel* const Model = PrepareModel(World))
		{
			UE_LOG(LogknightFeedbackTuning, Display, TEXT("값 %d개를 되돌렸다."), Model->RevertUnsaved());
		}
	}

	static void PlayerAttackCommand(const TArray<FString>& Args, UWorld* World)
	{
		FknightFeedbackTuningModel* const Model = PrepareModel(World);
		if (!Model)
		{
			return;
		}
		if (Args.IsEmpty())
		{
			TArray<FString> Names;
			Model->GetPlayerAttackNames(Names);
			UE_LOG(LogknightFeedbackTuning, Display, TEXT("플레이어 공격: %s"), *FString::Join(Names, TEXT(", ")));
			return;
		}
		Model->PlayPlayerAttack(Args[0], Args.Num() < 2 || Args[1] != TEXT("0"));
	}

	static void ImpactCommand(const TArray<FString>& Args, UWorld* World)
	{
		FknightFeedbackTuningModel* const Model = PrepareModel(World);
		if (!Model)
		{
			return;
		}
		if (Args.IsEmpty())
		{
			TArray<FString> Moments;
			FString Label;
			Model->GetImpactMoments(Moments, Label);
			UE_LOG(LogknightFeedbackTuning, Display, TEXT("카메라 반응(%s): %s"), *Label, *FString::Join(Moments, TEXT(", ")));
			return;
		}
		UE_LOG(LogknightFeedbackTuning, Display, TEXT("카메라 반응 %s 미리보기 %s"), *Args[0], Model->PreviewImpactMoment(Args[0]) ? TEXT("재생") : TEXT("실패"));
	}

	static FAutoConsoleCommandWithWorldAndArgs ImpactCmd(TEXT("knight.FeedbackTuning.Impact"),
		TEXT("knight.FeedbackTuning.Impact [Moment]. Previews a camera impact reaction at full strength; no argument lists the moments."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ImpactCommand));
	static FAutoConsoleCommandWithWorldAndArgs PlayerAttackCmd(TEXT("knight.FeedbackTuning.PlayerAttack"),
		TEXT("knight.FeedbackTuning.PlayerAttack [Name] [Place=1]. Places the player in reach of the monster and activates a player attack; no name lists them."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PlayerAttackCommand));
	static FAutoConsoleCommandWithWorldAndArgs ListCmd(TEXT("knight.FeedbackTuning.List"),
		TEXT("knight.FeedbackTuning.List [Filter|-refresh]. Lists feedback values found on the target monster and the player, with their PIE copies."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ListCommand));
	static FAutoConsoleCommandWithWorldAndArgs SetCmd(TEXT("knight.FeedbackTuning.Set"),
		TEXT("knight.FeedbackTuning.Set <Filter> <Value>. Sets every value whose 'Source · Path' contains Filter."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetCommand));
	static FAutoConsoleCommandWithWorldAndArgs SaveCmd(TEXT("knight.FeedbackTuning.Save"),
		TEXT("knight.FeedbackTuning.Save. Saves changed monster values to their assets."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SaveCommand));
	static FAutoConsoleCommandWithWorldAndArgs RevertCmd(TEXT("knight.FeedbackTuning.Revert"),
		TEXT("knight.FeedbackTuning.Revert. Restores unsaved values."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RevertCommand));
}
