#include "knightMonsterRecipeLibrary.h"

#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterAreaAttack.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterDeath.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterMeleeAttack.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterReaction.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/Attributes/knightMonsterAttributeSet.h"
#include "AbilitySystem/knightAbilitySet.h"
#include "AI/knightBossAIController.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/knightAnimNode_LegPlacement.h"
#include "Animation/knightAnimNode_WingClearance.h"
#include "Animation/knightAnimNotifyState_MeleeTrace.h"
#include "Animation/knightAnimNotifyState_PlayRate.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "EdGraph/EdGraph.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "FileHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/AdditionalEffectsGameplayEffectComponent.h"
#include "IAssetTools.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/MessageLog.h"
#include "MeshUtilitiesCommon.h"
#include "MeshUtilitiesEngine.h"
#include "Misc/PackageName.h"
#include "Monster/knightMonsterBodyBlockerComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDefinition.h"
#include "Monster/knightMonsterHoverComponent.h"
#include "Monster/knightMonsterPartData.h"
#include "Monster/knightMonsterPushOutComponent.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "knightEditor.h"
#include "knightFootLockLibrary.h"
#include "knightMonsterRecipe.h"

#define LOCTEXT_NAMESPACE "knightMonsterRecipe"

namespace knightRecipe
{
	// 공용 에셋. 몬스터별 차이는 레시피와 몬스터 DA에만 둔다.
	const TCHAR* const SharedRoot = TEXT("/Game/Knight/Creatures/Shared");
	const TCHAR* const SharedStateTree = TEXT("/Game/Knight/Creatures/Shared/AI/ST_Monster_Default");
	const TCHAR* const SharedController = TEXT("/Game/Knight/Creatures/Shared/AI/BP_MonsterAIController");
	const TCHAR* const SharedAnimBlueprint = TEXT("/Game/Knight/Creatures/Shared/Animations/ABP_Monster_Base");
	const TCHAR* const SharedEnrage = TEXT("/Game/Knight/Creatures/Shared/Effects/GE_Monster_Enrage");
	const TCHAR* const SharedExhaust = TEXT("/Game/Knight/Creatures/Shared/Effects/GE_Monster_Exhaust");
	const TCHAR* const SharedRageReset = TEXT("/Game/Knight/Creatures/Shared/Effects/GE_Monster_RageReset");
	const TCHAR* const SharedStaminaRestore = TEXT("/Game/Knight/Creatures/Shared/Effects/GE_Monster_StaminaRestore");

	// 공용 에셋을 처음 만들 때 바탕으로 쓰는 잔열 에셋.
	const TCHAR* const SourceRoot = TEXT("/Game/Knight/Creatures/Janyeol");

	const FName MessageLogName = TEXT("knightMonsterRecipe");

	/** Build 결과. 끝나면 출력 로그와 메시지 로그(에디터)에 남긴다. */
	struct FLog
	{
		TArray<FString> Errors;
		TArray<FString> Warnings;
		TArray<FString> Infos;

		void Error(const FString& Message) { Errors.Add(Message); }
		void Warning(const FString& Message) { Warnings.Add(Message); }
		void Info(const FString& Message) { Infos.Add(Message); }

		void Flush(const FString& Title) const
		{
			UE_LOG(LogknightEditor, Display, TEXT("RECIPE| %s: %d error(s), %d warning(s)"), *Title, Errors.Num(), Warnings.Num());
			for (const FString& Message : Errors) { UE_LOG(LogknightEditor, Error, TEXT("RECIPE|   오류: %s"), *Message); }
			for (const FString& Message : Warnings) { UE_LOG(LogknightEditor, Warning, TEXT("RECIPE|   경고: %s"), *Message); }
			for (const FString& Message : Infos) { UE_LOG(LogknightEditor, Display, TEXT("RECIPE|   %s"), *Message); }

			if (GIsEditor && !IsRunningCommandlet())
			{
				FMessageLog Log(MessageLogName);
				Log.NewPage(FText::FromString(Title));
				for (const FString& Message : Errors) { Log.Error(FText::FromString(Message)); }
				for (const FString& Message : Warnings) { Log.Warning(FText::FromString(Message)); }
				for (const FString& Message : Infos) { Log.Info(FText::FromString(Message)); }
				Log.Notify(FText::FromString(Title), EMessageSeverity::Info, true);
			}
		}
	};

	FGameplayTag Tag(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name));
	}

	FString ObjectPath(const FString& PackagePath)
	{
		return PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath);
	}

	template<typename T>
	T* LoadAsset(const FString& PackagePath)
	{
		if (!FPackageName::DoesPackageExist(PackagePath) && !FindPackage(nullptr, *PackagePath))
		{
			return nullptr;
		}
		return LoadObject<T>(nullptr, *ObjectPath(PackagePath));
	}

	UClass* LoadBlueprintClass(const FString& PackagePath)
	{
		const UBlueprint* const Blueprint = LoadAsset<UBlueprint>(PackagePath);
		return Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
	}

	// 리플렉션으로 값을 쓴다. 게임 클래스의 protected 프로퍼티(어빌리티 설정 등)를 에디터 모듈에서 채우기 위한 것.

	template<typename T>
	T* ValuePtr(const UStruct* Type, void* Container, FName Name, FLog* Log)
	{
		FProperty* const Property = FindFProperty<FProperty>(Type, Name);
		if (!Property || Property->GetElementSize() != sizeof(T))
		{
			if (Log)
			{
				Log->Error(FString::Printf(TEXT("%s에 %s 프로퍼티가 없거나 형식이 다릅니다. 레시피 생성기를 확인하세요."), *Type->GetName(), *Name.ToString()));
			}
			return nullptr;
		}
		return Property->ContainerPtrToValuePtr<T>(Container);
	}

	template<typename T>
	void SetValue(UObject* Object, FName Name, const T& Value, FLog& Log)
	{
		if (T* const Ptr = ValuePtr<T>(Object->GetClass(), Object, Name, &Log))
		{
			*Ptr = Value;
		}
	}

	template<typename T>
	bool GetValue(const UObject* Object, FName Name, T& OutValue)
	{
		if (const T* const Ptr = ValuePtr<T>(Object->GetClass(), const_cast<UObject*>(Object), Name, nullptr))
		{
			OutValue = *Ptr;
			return true;
		}
		return false;
	}

	void SetBool(UObject* Object, FName Name, bool bValue, FLog& Log)
	{
		if (FBoolProperty* const Property = FindFProperty<FBoolProperty>(Object->GetClass(), Name))
		{
			Property->SetPropertyValue_InContainer(Object, bValue);
			return;
		}
		Log.Error(FString::Printf(TEXT("%s에 %s 프로퍼티가 없습니다."), *Object->GetClass()->GetName(), *Name.ToString()));
	}

	bool GetBool(const UObject* Object, FName Name)
	{
		const FBoolProperty* const Property = FindFProperty<FBoolProperty>(Object->GetClass(), Name);
		return Property && Property->GetPropertyValue_InContainer(Object);
	}

	void SetObject(UObject* Object, FName Name, UObject* Value, FLog& Log)
	{
		if (FObjectPropertyBase* const Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name))
		{
			Property->SetObjectPropertyValue_InContainer(Object, Value);
			return;
		}
		Log.Error(FString::Printf(TEXT("%s에 %s 프로퍼티가 없습니다."), *Object->GetClass()->GetName(), *Name.ToString()));
	}

	template<typename T = UObject>
	T* GetObject(const UObject* Object, FName Name)
	{
		const FObjectPropertyBase* const Property = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
		return Property ? Cast<T>(Property->GetObjectPropertyValue_InContainer(Object)) : nullptr;
	}

	/** 만들거나 고친 패키지. Build가 끝날 때 한꺼번에 저장한다. */
	struct FContext
	{
		const UknightMonsterRecipe* Recipe = nullptr;
		FLog Log;
		FString Root;
		FString Prefix;
		TSet<UPackage*> Touched;

		FString Path(const TCHAR* Folder, const FString& Name) const
		{
			return Folder && *Folder ? Root / Folder / Name : Root / Name;
		}

		void Touch(UObject* Object)
		{
			Object->MarkPackageDirty();
			Touched.Add(Object->GetPackage());
		}
	};

	template<typename T>
	T* FindOrCreateAsset(const FString& PackagePath, bool& bOutCreated)
	{
		if (T* const Existing = LoadAsset<T>(PackagePath))
		{
			bOutCreated = false;
			return Existing;
		}
		UPackage* const Package = CreatePackage(*PackagePath);
		T* const Asset = NewObject<T>(Package, *FPackageName::GetShortName(PackagePath), RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Asset);
		bOutCreated = true;
		return Asset;
	}

	UBlueprint* FindOrCreateBlueprint(FContext& Context, const FString& PackagePath, UClass* ParentClass)
	{
		if (UBlueprint* const Existing = LoadAsset<UBlueprint>(PackagePath))
		{
			if (Existing->ParentClass != ParentClass)
			{
				Context.Log.Warning(FString::Printf(TEXT("%s의 부모 클래스가 %s입니다(레시피는 %s). 부모는 바꾸지 않습니다."), *PackagePath,
					*GetNameSafe(Existing->ParentClass), *GetNameSafe(ParentClass)));
			}
			return Existing;
		}
		UPackage* const Package = CreatePackage(*PackagePath);
		UBlueprint* const Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, *FPackageName::GetShortName(PackagePath), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		FAssetRegistryModule::AssetCreated(Blueprint);
		Context.Log.Info(FString::Printf(TEXT("만듦 %s"), *PackagePath));
		return Blueprint;
	}

	UObject* DuplicateAsset(const FString& SourcePackage, const FString& TargetPackage)
	{
		UObject* const Source = LoadAsset<UObject>(SourcePackage);
		if (!Source)
		{
			return nullptr;
		}
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		return AssetTools.DuplicateAsset(FPackageName::GetShortName(TargetPackage), FPackageName::GetLongPackagePath(TargetPackage), Source);
	}

	bool SavePackages(const TSet<UPackage*>& Packages)
	{
		TArray<UPackage*> List = Packages.Array();
		return List.IsEmpty() || UEditorLoadingAndSavingUtils::SavePackages(List, false);
	}

	UObject* GetCDO(const UBlueprint* Blueprint)
	{
		return Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
	}

	bool HasBoneOrSocket(const USkeletalMesh* Mesh, FName Name)
	{
		return Mesh && (Mesh->GetRefSkeleton().FindBoneIndex(Name) != INDEX_NONE || Mesh->FindSocket(Name) != nullptr);
	}
}

using namespace knightRecipe;

////////////////////////////////////////////////////////////////////
// 몽타주

namespace knightRecipe
{
	int32 FindOrAddNotifyTrack(UAnimMontage* Montage, FName TrackName)
	{
		const int32 Existing = Montage->AnimNotifyTracks.IndexOfByPredicate([TrackName](const FAnimNotifyTrack& Track) { return Track.TrackName == TrackName; });
		if (Existing != INDEX_NONE)
		{
			return Existing;
		}
		return Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(TrackName, FLinearColor::White));
	}

	void AddNotifyState(UAnimMontage* Montage, UAnimNotifyState* NotifyState, float Start, float End, int32 TrackIndex)
	{
		FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
		Event.NotifyName = FName(*NotifyState->GetNotifyName());
		Event.NotifyStateClass = NotifyState;
		Event.Link(Montage, Start);
		Event.TriggerTimeOffset = GetTriggerTimeOffsetForType(Montage->CalculateOffsetForNotify(Start));
		Event.SetDuration(End - Start);
		Event.EndLink.Link(Montage, End);
		Event.EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Montage->CalculateOffsetForNotify(End));
		Event.TrackIndex = TrackIndex;
		Event.Guid = FGuid::NewGuid();
	}

	/**
	 * 시퀀스들을 DefaultSlot에 이어 붙인 몽타주를 만들거나 고친다. 판정(Knight Melee Trace)과 재생 속도(Knight Play Rate)
	 * 노티파이는 레시피 것으로 바꾸고, 그 밖의 노티파이(사운드 등)는 남긴다. bLoopLast면 마지막 시퀀스가 Loop 섹션이 된다.
	 */
	UAnimMontage* BuildMontage(FContext& Context, const FString& Name, TConstArrayView<UAnimSequence*> Sequences, bool bLoopLast, bool bAutoBlendOut,
		TConstArrayView<FknightRecipePlayRateWindow> PlayRates = {}, TConstArrayView<FknightRecipeHitWindow> Hits = {})
	{
		if (Sequences.IsEmpty() || !Sequences[0])
		{
			return nullptr;
		}
		const FString PackagePath = Context.Path(TEXT("Montages"), Name);
		bool bCreated = false;
		UAnimMontage* const Montage = FindOrCreateAsset<UAnimMontage>(PackagePath, bCreated);
		Montage->Modify();
		if (Montage->GetSkeleton() != Sequences[0]->GetSkeleton())
		{
			Montage->SetSkeleton(Sequences[0]->GetSkeleton());
		}
		if (Montage->SlotAnimTracks.IsEmpty())
		{
			Montage->SlotAnimTracks.AddDefaulted();
		}

		FSlotAnimationTrack& SlotTrack = Montage->SlotAnimTracks[0];
		SlotTrack.SlotName = FAnimSlotGroup::DefaultSlotName;
		SlotTrack.AnimTrack.AnimSegments.Reset();
		float Length = 0.f;
		for (UAnimSequence* const Sequence : Sequences)
		{
			FAnimSegment Segment;
			Segment.SetAnimReference(Sequence, true);
			Segment.StartPos = Length;
			SlotTrack.AnimTrack.AnimSegments.Add(Segment);
			Length += Sequence->GetPlayLength();
		}
		Montage->SetCompositeLength(Length);

		Montage->CompositeSections.Reset();
		FCompositeSection DefaultSection;
		DefaultSection.SectionName = TEXT("Default");
		DefaultSection.Link(Montage, 0.f);
		const bool bLoop = bLoopLast && Sequences.Num() > 1;
		if (bLoop)
		{
			DefaultSection.NextSectionName = TEXT("Loop");
		}
		Montage->CompositeSections.Add(DefaultSection);
		if (bLoop)
		{
			FCompositeSection LoopSection;
			LoopSection.SectionName = TEXT("Loop");
			LoopSection.Link(Montage, Length - Sequences.Last()->GetPlayLength());
			LoopSection.NextSectionName = TEXT("Loop");
			Montage->CompositeSections.Add(LoopSection);
		}
		Montage->bEnableAutoBlendOut = bAutoBlendOut;

		Montage->Notifies.RemoveAll([](const FAnimNotifyEvent& Event)
		{
			return Event.NotifyStateClass && (Event.NotifyStateClass->IsA<UknightAnimNotifyState_MeleeTrace>() || Event.NotifyStateClass->IsA<UknightAnimNotifyState_PlayRate>());
		});
		for (int32 Index = 0; Index < Hits.Num(); ++Index)
		{
			const FknightRecipeHitWindow& Hit = Hits[Index];
			UknightAnimNotifyState_MeleeTrace* const Trace = NewObject<UknightAnimNotifyState_MeleeTrace>(Montage, NAME_None, RF_Transactional);
			SetValue(Trace, TEXT("TraceParams"), Hit.Trace, Context.Log);
			AddNotifyState(Montage, Trace, Hit.Start, Hit.End, FindOrAddNotifyTrack(Montage, *FString::Printf(TEXT("Hit%d"), Index + 1)));
		}
		for (const FknightRecipePlayRateWindow& Window : PlayRates)
		{
			UknightAnimNotifyState_PlayRate* const PlayRate = NewObject<UknightAnimNotifyState_PlayRate>(Montage, NAME_None, RF_Transactional);
			SetValue<float>(PlayRate, TEXT("PlayRate"), Window.PlayRate, Context.Log);
			AddNotifyState(Montage, PlayRate, Window.Start, Window.End, FindOrAddNotifyTrack(Montage, TEXT("PlayRate")));
		}
		Montage->SortNotifies();
		Montage->RefreshCacheData();
		Montage->PostEditChange();
		Context.Touch(Montage);
		if (bCreated)
		{
			Context.Log.Info(FString::Printf(TEXT("만듦 %s"), *PackagePath));
		}
		return Montage;
	}

	UAnimMontage* BuildMontage(FContext& Context, const FString& Name, UAnimSequence* Sequence, bool bAutoBlendOut = true)
	{
		return Sequence ? BuildMontage(Context, Name, TArray<UAnimSequence*>{ Sequence }, false, bAutoBlendOut) : nullptr;
	}
}

////////////////////////////////////////////////////////////////////
// 어빌리티

namespace knightRecipe
{
	TSubclassOf<UGameplayAbility> BuildAttackAbility(FContext& Context, const FknightRecipeAttack& Attack)
	{
		const FString Name = FString::Printf(TEXT("%s_%s"), *Context.Prefix, *Attack.Name.ToString());
		UAnimMontage* const Montage = BuildMontage(Context, TEXT("AM_") + Name, TArray<UAnimSequence*>{ Attack.Animation.Get() }, false, true, Attack.PlayRateWindows,
			Attack.Type == EknightRecipeAttackType::Melee ? TConstArrayView<FknightRecipeHitWindow>(Attack.HitWindows) : TConstArrayView<FknightRecipeHitWindow>());

		UClass* const Parent = Attack.Type == EknightRecipeAttackType::Melee ? UknightGameplayAbility_MonsterMeleeAttack::StaticClass()
			: UknightGameplayAbility_MonsterAreaAttack::StaticClass();
		UBlueprint* const Blueprint = FindOrCreateBlueprint(Context, Context.Path(TEXT("Abilities"), TEXT("GA_") + Name), Parent);
		UObject* const CDO = GetCDO(Blueprint);
		FLog& Log = Context.Log;

		SetValue(CDO, TEXT("Selection"), Attack.Selection, Log);
		SetValue(CDO, TEXT("HitParams"), Attack.HitParams, Log);
		SetObject(CDO, TEXT("Montage"), Montage, Log);
		SetValue<float>(CDO, TEXT("StartTurnYaw"), Attack.StartTurnYaw, Log);
		SetValue<float>(CDO, TEXT("StartTurnRate"), Attack.StartTurnRate, Log);
		SetValue<float>(CDO, TEXT("StartTurnMaxTime"), Attack.StartTurnMaxTime, Log);
		if (Attack.Type == EknightRecipeAttackType::Area)
		{
			SetValue<float>(CDO, TEXT("WindupTime"), Attack.WindupTime, Log);
			SetValue<float>(CDO, TEXT("ActiveTime"), Attack.ActiveTime, Log);
			SetValue<float>(CDO, TEXT("RecoveryTime"), Attack.RecoveryTime, Log);
			SetValue(CDO, TEXT("AreaOffset"), Attack.AreaOffset, Log);
			SetValue<float>(CDO, TEXT("AreaRadius"), Attack.AreaRadius, Log);
			SetValue(CDO, TEXT("TelegraphCue"), Attack.TelegraphCue, Log);
			SetValue<float>(CDO, TEXT("LungeMaxDistance"), Attack.LungeMaxDistance, Log);
			SetValue<float>(CDO, TEXT("LungeStartTime"), Attack.LungeStartTime, Log);
			SetValue<float>(CDO, TEXT("LungeEndTime"), Attack.LungeEndTime, Log);
		}

		FGameplayTagContainer AbilityTags(Tag(TEXT("Ability.Attack")));
		if (Attack.bRanged)
		{
			AbilityTags.AddTag(Tag(TEXT("Ability.Attack.Ranged")));
		}
		SetValue(CDO, TEXT("AbilityTags"), AbilityTags, Log);

		// 부위가 부서져 고를 수 없게 된 공격은 다른 경로로도 발동하지 않게 한다.
		FGameplayTagContainer Blocked;
		Blocked.AddTag(Tag(TEXT("State.Dead")));
		Blocked.AddTag(Tag(TEXT("State.HitReact")));
		Blocked.AppendTags(Attack.Selection.BlockedOwnerTags);
		SetValue(CDO, TEXT("ActivationBlockedTags"), Blocked, Log);

		Context.Touch(Blueprint);
		return Blueprint->GeneratedClass.Get();
	}

	struct FReactionSettings
	{
		FString Name;
		FGameplayTag StateTag;
		float HoldTime = 0.f;
		float RecoverTime = 0.f;
		UAnimMontage* HoldMontage = nullptr;
		UAnimMontage* RecoverMontage = nullptr;
		bool bRestorePoise = false;
		float MaxPoiseGrowth = 0.25f;
		bool bCancelOtherReactions = false;
		FGameplayTag TriggerTag;
		EGameplayAbilityTriggerSource::Type TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	};

	TSubclassOf<UGameplayAbility> BuildReactionAbility(FContext& Context, const FReactionSettings& Settings)
	{
		UBlueprint* const Blueprint = FindOrCreateBlueprint(Context, Context.Path(TEXT("Abilities"), FString::Printf(TEXT("GA_%s_%s"), *Context.Prefix, *Settings.Name)),
			UknightGameplayAbility_MonsterReaction::StaticClass());
		UObject* const CDO = GetCDO(Blueprint);
		FLog& Log = Context.Log;

		SetValue(CDO, TEXT("StateTag"), Settings.StateTag, Log);
		SetValue<float>(CDO, TEXT("HoldTime"), Settings.HoldTime, Log);
		SetValue<float>(CDO, TEXT("RecoverTime"), Settings.RecoverTime, Log);
		SetObject(CDO, TEXT("HoldMontage"), Settings.HoldMontage, Log);
		SetObject(CDO, TEXT("RecoverMontage"), Settings.RecoverMontage, Log);
		SetBool(CDO, TEXT("bRestorePoise"), Settings.bRestorePoise, Log);
		SetValue<float>(CDO, TEXT("MaxPoiseGrowth"), Settings.MaxPoiseGrowth, Log);
		SetBool(CDO, TEXT("bCancelOtherReactions"), Settings.bCancelOtherReactions, Log);

		FAbilityTriggerData Trigger;
		Trigger.TriggerTag = Settings.TriggerTag;
		Trigger.TriggerSource = Settings.TriggerSource;
		SetValue(CDO, TEXT("AbilityTriggers"), TArray<FAbilityTriggerData>{ Trigger }, Log);

		Context.Touch(Blueprint);
		return Blueprint->GeneratedClass.Get();
	}

	void BuildReactionAbilities(FContext& Context, TArray<TSubclassOf<UGameplayAbility>>& OutAbilities)
	{
		const FknightRecipeReactions& Reactions = Context.Recipe->Reactions;
		const FString& Prefix = Context.Prefix;

		if (UAnimMontage* const Hit = BuildMontage(Context, FString::Printf(TEXT("AM_%s_Hit"), *Prefix), Reactions.Hit))
		{
			const float StaggerTime = Reactions.StaggerTime > 0.f ? Reactions.StaggerTime : Hit->GetPlayLength();
			FReactionSettings Parried;
			Parried.Name = TEXT("Parried");
			Parried.StateTag = Tag(TEXT("State.HitReact.Stagger"));
			Parried.HoldTime = StaggerTime;
			Parried.HoldMontage = Hit;
			Parried.TriggerTag = Tag(TEXT("Event.Parried"));
			OutAbilities.Add(BuildReactionAbility(Context, Parried));

			FReactionSettings BreakFlinch = Parried;
			BreakFlinch.Name = TEXT("BreakFlinch");
			BreakFlinch.TriggerTag = Tag(TEXT("Event.PartBroken"));
			OutAbilities.Add(BuildReactionAbility(Context, BreakFlinch));
		}

		if (Reactions.DownStart)
		{
			TArray<UAnimSequence*> DownSequences{ Reactions.DownStart.Get() };
			if (Reactions.DownLoop)
			{
				DownSequences.Add(Reactions.DownLoop.Get());
			}
			FReactionSettings Down;
			Down.Name = TEXT("Down");
			Down.StateTag = Tag(TEXT("State.HitReact.Knockdown"));
			Down.HoldTime = Reactions.DownTime;
			Down.HoldMontage = BuildMontage(Context, FString::Printf(TEXT("AM_%s_Down"), *Prefix), DownSequences, true, true);
			Down.RecoverMontage = BuildMontage(Context, FString::Printf(TEXT("AM_%s_GetUp"), *Prefix), Reactions.GetUp);
			Down.RecoverTime = Down.RecoverMontage ? Down.RecoverMontage->GetPlayLength() : 1.f;
			Down.bRestorePoise = true;
			Down.MaxPoiseGrowth = Reactions.MaxPoiseGrowth;
			// 패링으로 Poise가 깨지면 패리 경직과 같은 프레임에 켜지므로 다운이 이어받는다.
			Down.bCancelOtherReactions = true;
			Down.TriggerTag = Tag(TEXT("Event.PoiseBroken"));
			OutAbilities.Add(BuildReactionAbility(Context, Down));
		}

		if (UAnimMontage* const Roar = BuildMontage(Context, FString::Printf(TEXT("AM_%s_Roar"), *Prefix), Reactions.EngageRoar))
		{
			FReactionSettings Engage;
			Engage.Name = TEXT("EngageRoar");
			Engage.StateTag = Tag(TEXT("State.SuperArmor"));
			Engage.HoldTime = Roar->GetPlayLength();
			Engage.HoldMontage = Roar;
			Engage.TriggerTag = Tag(TEXT("Event.Monster.Engaged"));
			OutAbilities.Add(BuildReactionAbility(Context, Engage));
		}

		if (UAnimMontage* const EnrageStart = BuildMontage(Context, FString::Printf(TEXT("AM_%s_EnrageStart"), *Prefix), Reactions.EnrageRoar))
		{
			FReactionSettings Enrage;
			Enrage.Name = TEXT("EnrageRoar");
			Enrage.StateTag = Tag(TEXT("State.SuperArmor"));
			Enrage.HoldTime = EnrageStart->GetPlayLength();
			Enrage.HoldMontage = EnrageStart;
			Enrage.TriggerTag = Tag(TEXT("State.Monster.Enraged"));
			Enrage.TriggerSource = EGameplayAbilityTriggerSource::OwnedTagAdded;
			OutAbilities.Add(BuildReactionAbility(Context, Enrage));
		}

		if (UAnimMontage* const Death = BuildMontage(Context, FString::Printf(TEXT("AM_%s_Death"), *Prefix), Reactions.Death, false))
		{
			UBlueprint* const Blueprint = FindOrCreateBlueprint(Context, Context.Path(TEXT("Abilities"), FString::Printf(TEXT("GA_%s_Death"), *Prefix)),
				UknightGameplayAbility_MonsterDeath::StaticClass());
			SetObject(GetCDO(Blueprint), TEXT("DeathMontage"), Death, Context.Log);
			Context.Touch(Blueprint);
			OutAbilities.Add(Blueprint->GeneratedClass.Get());
		}
	}
}

////////////////////////////////////////////////////////////////////
// 효과, 데이터

namespace knightRecipe
{
	FGameplayAttribute Attribute(UClass* AttributeSetClass, const TCHAR* Name)
	{
		return FGameplayAttribute(FindFProperty<FProperty>(AttributeSetClass, Name));
	}

	TSubclassOf<UGameplayEffect> BuildInitEffect(FContext& Context)
	{
		const UknightMonsterRecipe& Recipe = *Context.Recipe;
		UBlueprint* const Blueprint = FindOrCreateBlueprint(Context, Context.Path(TEXT("Effects"), FString::Printf(TEXT("GE_%s_Init"), *Context.Prefix)),
			UGameplayEffect::StaticClass());
		UGameplayEffect* const Effect = CastChecked<UGameplayEffect>(GetCDO(Blueprint));

		UClass* const Core = UknightCoreAttributeSet::StaticClass();
		UClass* const Monster = UknightMonsterAttributeSet::StaticClass();
		Effect->Modifiers.Reset();
		auto Override = [Effect](const FGameplayAttribute& Attribute, float Value)
		{
			FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
			Modifier.Attribute = Attribute;
			Modifier.ModifierOp = EGameplayModOp::Override;
			Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
		};
		Override(Attribute(Core, TEXT("MaxHealth")), Recipe.MaxHealth);
		Override(Attribute(Core, TEXT("Health")), Recipe.MaxHealth);
		if (Recipe.AttackPower > 0.f)
		{
			Override(Attribute(Core, TEXT("AttackPower")), Recipe.AttackPower);
		}
		if (Recipe.MaxPoise > 0.f)
		{
			Override(Attribute(Core, TEXT("MaxPoise")), Recipe.MaxPoise);
			Override(Attribute(Core, TEXT("Poise")), Recipe.MaxPoise);
		}
		if (Recipe.MaxRage > 0.f)
		{
			Override(Attribute(Monster, TEXT("MaxRage")), Recipe.MaxRage);
		}
		if (Recipe.MaxStamina > 0.f)
		{
			Override(Attribute(Monster, TEXT("MaxStamina")), Recipe.MaxStamina);
			Override(Attribute(Monster, TEXT("Stamina")), Recipe.MaxStamina);
		}
		Context.Touch(Blueprint);
		return Blueprint->GeneratedClass.Get();
	}

	/** 공용 분노·탈진 효과의 자식. 지속 시간만 몬스터별로 다르다. */
	TSubclassOf<UGameplayEffect> BuildTimedEffect(FContext& Context, const TCHAR* Suffix, const TCHAR* SharedPath, float Duration)
	{
		UClass* const Parent = LoadBlueprintClass(SharedPath);
		if (!Parent)
		{
			Context.Log.Error(FString::Printf(TEXT("공용 효과 %s가 없습니다."), SharedPath));
			return nullptr;
		}
		UBlueprint* const Blueprint = FindOrCreateBlueprint(Context, Context.Path(TEXT("Effects"), FString::Printf(TEXT("GE_%s_%s"), *Context.Prefix, Suffix)), Parent);
		UGameplayEffect* const Effect = CastChecked<UGameplayEffect>(GetCDO(Blueprint));
		Effect->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Duration));
		Context.Touch(Blueprint);
		return Blueprint->GeneratedClass.Get();
	}

	UknightMonsterPartData* BuildPartData(FContext& Context)
	{
		bool bCreated = false;
		const FString PackagePath = Context.Path(TEXT("Data"), FString::Printf(TEXT("DA_%s_Parts"), *Context.Prefix));
		UknightMonsterPartData* const PartData = FindOrCreateAsset<UknightMonsterPartData>(PackagePath, bCreated);
		PartData->Modify();
		PartData->Parts = Context.Recipe->Parts;
		Context.Touch(PartData);
		return PartData;
	}

	/** 레시피가 만든 어빌리티(GA_<몬스터>_*)는 새 목록으로 바꾸고, 손으로 넣은 다른 항목은 남긴다. */
	UknightAbilitySet* BuildAbilitySet(FContext& Context, const TArray<TSubclassOf<UGameplayAbility>>& Abilities, TSubclassOf<UGameplayEffect> InitEffect)
	{
		bool bCreated = false;
		UknightAbilitySet* const AbilitySet = FindOrCreateAsset<UknightAbilitySet>(Context.Path(TEXT("Data"), FString::Printf(TEXT("AS_%s"), *Context.Prefix)), bCreated);
		AbilitySet->Modify();
		const FString OwnedPrefix = Context.Path(TEXT("Abilities"), FString::Printf(TEXT("GA_%s_"), *Context.Prefix));

		if (TArray<FknightAbilitySet_GameplayAbility>* const Granted = ValuePtr<TArray<FknightAbilitySet_GameplayAbility>>(UknightAbilitySet::StaticClass(), AbilitySet,
			TEXT("GrantedGameplayAbilities"), &Context.Log))
		{
			Granted->RemoveAll([&OwnedPrefix](const FknightAbilitySet_GameplayAbility& Entry)
			{
				return !Entry.Ability || Entry.Ability->GetPackage()->GetName().StartsWith(OwnedPrefix);
			});
			for (const TSubclassOf<UGameplayAbility>& Ability : Abilities)
			{
				if (Ability)
				{
					FknightAbilitySet_GameplayAbility& Entry = Granted->AddDefaulted_GetRef();
					Entry.Ability = TSubclassOf<UknightGameplayAbility>(Ability.Get());
				}
			}
		}
		if (TArray<FknightAbilitySet_GameplayEffect>* const Effects = ValuePtr<TArray<FknightAbilitySet_GameplayEffect>>(UknightAbilitySet::StaticClass(), AbilitySet,
			TEXT("GrantedGameplayEffects"), &Context.Log))
		{
			Effects->RemoveAll([InitEffect](const FknightAbilitySet_GameplayEffect& Entry) { return !Entry.GameplayEffect || Entry.GameplayEffect == InitEffect; });
			FknightAbilitySet_GameplayEffect& Entry = Effects->AddDefaulted_GetRef();
			Entry.GameplayEffect = InitEffect;
		}
		Context.Touch(AbilitySet);
		return AbilitySet;
	}

	UknightMonsterDefinition* BuildDefinition(FContext& Context, UknightAbilitySet* AbilitySet, UknightMonsterPartData* PartData,
		TSubclassOf<UGameplayEffect> Enrage, TSubclassOf<UGameplayEffect> Exhaust)
	{
		const UknightMonsterRecipe& Recipe = *Context.Recipe;
		bool bCreated = false;
		UknightMonsterDefinition* const Definition = FindOrCreateAsset<UknightMonsterDefinition>(Context.Path(TEXT("Data"), TEXT("DA_") + Context.Prefix), bCreated);
		Definition->Modify();
		Definition->DisplayName = Recipe.DisplayName;
		Definition->AbilitySets.AddUnique(AbilitySet);
		Definition->PartData = PartData;
		Definition->EnrageEffect = Enrage;
		Definition->ExhaustEffect = Exhaust;
		Definition->ParriedPoiseDamageRatio = Recipe.ParriedPoiseDamageRatio;
		Definition->TargetRadius = Recipe.TargetRadius;
		Definition->FallbackLockOnSocket = Recipe.FallbackLockOnSocket;
		Definition->ChaseStopDistance = Recipe.ChaseStopDistance;
		Definition->RangedMinDistance = Recipe.RangedMinDistance;
		Definition->RangedMaxDistance = Recipe.RangedMaxDistance;
		Context.Touch(Definition);
		return Definition;
	}
}

////////////////////////////////////////////////////////////////////
// 애님 Blueprint, 몬스터 Blueprint

namespace knightRecipe
{
	/** 공용 ABP_Monster_Base를 복사한 뒤 스켈레톤, 블렌드스페이스, 발 뼈만 레시피 값으로 바꾼다. */
	const FName WingClearanceNodeClass = TEXT("AnimGraphNode_knightWingClearance");

	/**
	 * Knight Leg Placement 노드 출력 바로 뒤에 Knight Wing Clearance 노드를 끼운다. 공용 애님 Blueprint에는 이 노드가 없어서
	 * 날짐승 레시피에서만 넣는다. 그래프 노드 클래스는 knightAnimGraph 모듈에 있어 이름으로 찾는다.
	 */
	UEdGraphNode* InsertWingClearanceNode(UEdGraphNode* LegGraphNode)
	{
		UClass* const WingClass = FindObject<UClass>(nullptr, TEXT("/Script/knightAnimGraph.AnimGraphNode_knightWingClearance"));
		UEdGraphPin* const LegOut = LegGraphNode ? LegGraphNode->FindPin(TEXT("Pose"), EGPD_Output) : nullptr;
		if (!WingClass || !LegOut)
		{
			return nullptr;
		}
		UEdGraph* const Graph = LegGraphNode->GetGraph();
		Graph->Modify();
		UEdGraphNode* const WingNode = NewObject<UEdGraphNode>(Graph, WingClass, NAME_None, RF_Transactional);
		Graph->AddNode(WingNode, false, false);
		WingNode->CreateNewGuid();
		WingNode->PostPlacedNewNode();
		WingNode->AllocateDefaultPins();
		WingNode->NodePosX = LegGraphNode->NodePosX + 320;
		WingNode->NodePosY = LegGraphNode->NodePosY;

		UEdGraphPin* const WingIn = WingNode->FindPin(TEXT("ComponentPose"), EGPD_Input);
		UEdGraphPin* const WingOut = WingNode->FindPin(TEXT("Pose"), EGPD_Output);
		if (!WingIn || !WingOut)
		{
			Graph->RemoveNode(WingNode);
			return nullptr;
		}
		const TArray<UEdGraphPin*> Targets = LegOut->LinkedTo;
		LegOut->BreakAllPinLinks();
		LegOut->MakeLinkTo(WingIn);
		for (UEdGraphPin* const Target : Targets)
		{
			WingOut->MakeLinkTo(Target);
		}
		return WingNode;
	}

	UAnimBlueprint* BuildAnimBlueprint(FContext& Context)
	{
		const UknightMonsterRecipe& Recipe = *Context.Recipe;
		const FString PackagePath = Context.Path(TEXT("Animations"), TEXT("ABP_") + Context.Prefix);
		UAnimBlueprint* AnimBlueprint = LoadAsset<UAnimBlueprint>(PackagePath);
		if (!AnimBlueprint)
		{
			AnimBlueprint = Cast<UAnimBlueprint>(DuplicateAsset(SharedAnimBlueprint, PackagePath));
			if (!AnimBlueprint)
			{
				Context.Log.Error(FString::Printf(TEXT("공용 애님 Blueprint %s를 복사하지 못했습니다."), SharedAnimBlueprint));
				return nullptr;
			}
			Context.Log.Info(FString::Printf(TEXT("만듦 %s"), *PackagePath));
		}
		AnimBlueprint->Modify();
		AnimBlueprint->TargetSkeleton = Recipe.Mesh->GetSkeleton();
		AnimBlueprint->SetPreviewMesh(Recipe.Mesh);

		bool bFoundBlendSpace = false;
		bool bFoundLegs = false;
		UEdGraphNode* LegGraphNode = nullptr;
		UEdGraphNode* WingGraphNode = nullptr;
		TArray<UEdGraph*> Graphs;
		AnimBlueprint->GetAllGraphs(Graphs);
		for (UEdGraph* const Graph : Graphs)
		{
			for (UEdGraphNode* const Node : Graph->Nodes)
			{
				if (UAnimGraphNode_BlendSpacePlayer* const BlendSpacePlayer = Cast<UAnimGraphNode_BlendSpacePlayer>(Node))
				{
					BlendSpacePlayer->Modify();
					BlendSpacePlayer->SetAnimationAsset(Recipe.Locomotion);
					bFoundBlendSpace = true;
				}
				else if (Node && Node->GetClass()->GetFName() == TEXT("AnimGraphNode_knightLegPlacement"))
				{
					// 그래프 노드 클래스는 내보내지 않으므로 런타임 노드(Node 프로퍼티)를 리플렉션으로 고친다.
					if (FAnimNode_knightLegPlacement* const LegNode = ValuePtr<FAnimNode_knightLegPlacement>(Node->GetClass(), Node, TEXT("Node"), &Context.Log))
					{
						Node->Modify();
						LegNode->Legs.Reset();
						for (const FName FootBone : Recipe.FootBones)
						{
							FknightLegPlacementLeg& Leg = LegNode->Legs.AddDefaulted_GetRef();
							Leg.FootBone.BoneName = FootBone;
						}
						LegNode->PelvisBone.BoneName = Recipe.PelvisBone;
						bFoundLegs = true;
						LegGraphNode = Node;
					}
				}
				else if (Node && Node->GetClass()->GetFName() == WingClearanceNodeClass)
				{
					WingGraphNode = Node;
				}
			}
		}
		if (!Recipe.HoverWings.IsEmpty())
		{
			if (!WingGraphNode)
			{
				WingGraphNode = InsertWingClearanceNode(LegGraphNode);
				if (WingGraphNode)
				{
					Context.Log.Info(FString::Printf(TEXT("%s에 Knight Wing Clearance 노드를 넣었습니다."), *PackagePath));
				}
				else
				{
					Context.Log.Error(FString::Printf(TEXT("%s에 Knight Wing Clearance 노드를 넣지 못했습니다(Knight Leg Placement 노드 뒤에 넣습니다)."), *PackagePath));
				}
			}
			if (FAnimNode_knightWingClearance* const WingNode = WingGraphNode
				? ValuePtr<FAnimNode_knightWingClearance>(WingGraphNode->GetClass(), WingGraphNode, TEXT("Node"), &Context.Log) : nullptr)
			{
				WingGraphNode->Modify();
				WingNode->Wings.Reset();
				for (const FknightRecipeWing& RecipeWing : Recipe.HoverWings)
				{
					FknightWingClearanceWing& Wing = WingNode->Wings.AddDefaulted_GetRef();
					Wing.RootBone.BoneName = RecipeWing.RootBone;
					for (const FName Tip : RecipeWing.TipBones)
					{
						Wing.TipBones.AddDefaulted_GetRef().BoneName = Tip;
					}
				}
				WingNode->Clearance = Recipe.HoverWingClearance;
			}
		}
		else if (WingGraphNode)
		{
			if (FAnimNode_knightWingClearance* const WingNode = ValuePtr<FAnimNode_knightWingClearance>(WingGraphNode->GetClass(), WingGraphNode, TEXT("Node"), &Context.Log))
			{
				WingGraphNode->Modify();
				WingNode->Wings.Reset();
			}
		}
		if (!bFoundBlendSpace)
		{
			Context.Log.Error(FString::Printf(TEXT("%s에 Blendspace Player 노드가 없습니다."), *PackagePath));
		}
		if (!bFoundLegs && !Recipe.FootBones.IsEmpty())
		{
			Context.Log.Warning(FString::Printf(TEXT("%s에 Knight Leg Placement 노드가 없어 발 뼈를 넣지 못했습니다."), *PackagePath));
		}

		FKismetEditorUtilities::CompileBlueprint(AnimBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
		if (AnimBlueprint->Status == BS_Error)
		{
			Context.Log.Error(FString::Printf(TEXT("%s 컴파일 오류. 애님 Blueprint를 열어 확인하세요."), *PackagePath));
		}
		Context.Touch(AnimBlueprint);
		return AnimBlueprint;
	}

	/** Locomotion 샘플 애니메이션마다 발 고정용 접지 커브를 다시 굽는다. */
	void BuildFootLockCurves(FContext& Context)
	{
		const UknightMonsterRecipe& Recipe = *Context.Recipe;
		if (!Recipe.bBakeFootLockCurves || Recipe.FootBones.IsEmpty() || !Recipe.Locomotion)
		{
			return;
		}
		TSet<UAnimSequence*> Sequences;
		for (const FBlendSample& Sample : Recipe.Locomotion->GetBlendSamples())
		{
			if (UAnimSequence* const Sequence = Cast<UAnimSequence>(Sample.Animation.Get()))
			{
				Sequences.Add(Sequence);
			}
		}
		for (UAnimSequence* const Sequence : Sequences)
		{
			if (UknightFootLockLibrary::BakeFootLockCurves(Sequence, Recipe.FootBones) > 0)
			{
				Context.Touch(Sequence);
			}
			else
			{
				Context.Log.Warning(FString::Printf(TEXT("%s에 접지 커브를 굽지 못했습니다. 발 뼈 이름을 확인하세요."), *Sequence->GetName()));
			}
		}
		Context.Log.Info(FString::Printf(TEXT("이동 애니메이션 %d개에 접지 커브를 구웠습니다."), Sequences.Num()));
	}

	UBlueprint* BuildMonsterBlueprint(FContext& Context, UknightMonsterDefinition* Definition, UAnimBlueprint* AnimBlueprint)
	{
		const UknightMonsterRecipe& Recipe = *Context.Recipe;
		UBlueprint* const Blueprint = FindOrCreateBlueprint(Context, Context.Path(nullptr, TEXT("BP_") + Context.Prefix), AknightMonsterCharacterBase::StaticClass());
		Blueprint->Modify();

		// 몸 아래 차단 상자는 Blueprint 컴포넌트다(캡슐에 붙인다).
		USimpleConstructionScript* const Construction = Blueprint->SimpleConstructionScript;
		USCS_Node* BlockerNode = Construction ? Construction->FindSCSNode(TEXT("BodyBlocker")) : nullptr;
		if (Construction && !Recipe.BodyBlockerExtent.IsNearlyZero())
		{
			if (!BlockerNode)
			{
				BlockerNode = Construction->CreateNode(UknightMonsterBodyBlockerComponent::StaticClass(), TEXT("BodyBlocker"));
				BlockerNode->bIsParentComponentNative = true;
				BlockerNode->ParentComponentOrVariableName = ACharacter::CapsuleComponentName;
				Construction->AddNode(BlockerNode);
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			}
			if (UBoxComponent* const Box = Cast<UBoxComponent>(BlockerNode->ComponentTemplate))
			{
				Box->Modify();
				Box->SetBoxExtent(Recipe.BodyBlockerExtent, false);
				Box->SetRelativeLocation_Direct(Recipe.BodyBlockerOffset);
			}
		}
		else if (BlockerNode)
		{
			Context.Log.Warning(TEXT("레시피의 BodyBlockerExtent가 0이지만 BP에 BodyBlocker 컴포넌트가 있습니다. 컴포넌트는 그대로 둡니다."));
		}
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);

		AknightMonsterCharacterBase* const CDO = CastChecked<AknightMonsterCharacterBase>(GetCDO(Blueprint));
		CDO->Modify();
		SetObject(CDO, TEXT("MonsterDefinition"), Definition, Context.Log);
		if (UClass* const Controller = LoadBlueprintClass(SharedController))
		{
			CDO->AIControllerClass = Controller;
		}
		else
		{
			Context.Log.Error(FString::Printf(TEXT("공용 AI 컨트롤러 %s가 없습니다."), SharedController));
		}

		USkeletalMeshComponent* const Mesh = CDO->GetMesh();
		Mesh->Modify();
		Mesh->SetSkeletalMeshAsset(Recipe.Mesh);
		Mesh->AnimClass = AnimBlueprint ? AnimBlueprint->GeneratedClass.Get() : nullptr;
		Mesh->SetRelativeLocation_Direct(FVector(0.f, 0.f, -Recipe.CapsuleHalfHeight));
		Mesh->SetRelativeRotation_Direct(FRotator(0.f, Recipe.MeshYaw, 0.f));
		Mesh->SetRelativeScale3D_Direct(FVector(Recipe.MeshScale));

		CDO->GetCapsuleComponent()->Modify();
		CDO->GetCapsuleComponent()->SetCapsuleSize(Recipe.CapsuleRadius, Recipe.CapsuleHalfHeight, false);
		CDO->GetCharacterMovement()->Modify();
		CDO->GetCharacterMovement()->MaxWalkSpeed = Recipe.MaxWalkSpeed;

		if (UknightMonsterPushOutComponent* const PushOut = GetObject<UknightMonsterPushOutComponent>(CDO, TEXT("PushOut")))
		{
			PushOut->Modify();
			if (Recipe.PushOutBodyAxisHalfLength > 0.f)
			{
				SetValue<float>(PushOut, TEXT("BodyAxisHalfLength"), Recipe.PushOutBodyAxisHalfLength, Context.Log);
			}
			SetBool(PushOut, TEXT("bPushOutOfMesh"), Recipe.bPushOutOfMesh, Context.Log);
		}
		if (UknightMonsterHoverComponent* const Hover = GetObject<UknightMonsterHoverComponent>(CDO, TEXT("Hover")))
		{
			Hover->Modify();
			SetValue<float>(Hover, TEXT("HoverDrop"), Recipe.HoverDrop, Context.Log);
			SetValue<TArray<FName>>(Hover, TEXT("FootBones"), Recipe.HoverFootBones, Context.Log);
			SetValue<float>(Hover, TEXT("FootGroundHeight"), Recipe.HoverFootHeight, Context.Log);
		}
		Context.Touch(Blueprint);
		return Blueprint;
	}
}

////////////////////////////////////////////////////////////////////
// 검사, Build

TArray<FString> UknightMonsterRecipeLibrary::ValidateMonsterRecipe(const UknightMonsterRecipe* Recipe)
{
	TArray<FString> Problems;
	auto Error = [&Problems](const FString& Message) { Problems.Add(TEXT("오류: ") + Message); };
	auto Warning = [&Problems](const FString& Message) { Problems.Add(TEXT("경고: ") + Message); };
	if (!Recipe)
	{
		Error(TEXT("레시피가 없습니다."));
		return Problems;
	}

	const FString Name = Recipe->MonsterName.ToString();
	if (Recipe->MonsterName.IsNone() || Name.Contains(TEXT(" ")) || !FName::IsValidXName(Name, INVALID_OBJECTNAME_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS))
	{
		Error(TEXT("MonsterName이 비었거나 에셋 이름에 쓸 수 없는 글자가 있습니다."));
	}
	if (Recipe->DisplayName.IsEmpty())
	{
		Warning(TEXT("DisplayName이 비어 있습니다(락온 이름표에 표시)."));
	}
	const FString OutputPath = Recipe->GetOutputPath();
	if (!OutputPath.StartsWith(TEXT("/Game/")))
	{
		Error(FString::Printf(TEXT("OutputFolder(%s)가 /Game 아래가 아닙니다."), *OutputPath));
	}
	for (const TCHAR* Shared : { SharedStateTree, SharedController, SharedAnimBlueprint, SharedEnrage, SharedExhaust })
	{
		if (!FPackageName::DoesPackageExist(Shared))
		{
			Error(FString::Printf(TEXT("공용 에셋 %s가 없습니다. CreateSharedMonsterAssets를 먼저 실행하세요."), Shared));
		}
	}

	const USkeletalMesh* const Mesh = Recipe->Mesh;
	const USkeleton* const Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
	if (!Skeleton)
	{
		Error(TEXT("Mesh가 없거나 스켈레톤이 없습니다."));
	}
	else if (!Mesh->GetPhysicsAsset())
	{
		Warning(TEXT("Mesh에 피직스 에셋이 없어 공격이 맞지 않습니다. CreateMonsterPhysicsAsset으로 만드세요."));
	}
	auto CheckSequence = [&](const UAnimationAsset* Sequence, const FString& Label, bool bRequired)
	{
		if (!Sequence)
		{
			if (bRequired)
			{
				Error(Label + TEXT(": 애니메이션이 없습니다."));
			}
			return;
		}
		if (Skeleton && Sequence->GetSkeleton() != Skeleton)
		{
			Error(FString::Printf(TEXT("%s: %s의 스켈레톤이 메시와 다릅니다."), *Label, *Sequence->GetName()));
		}
	};
	CheckSequence(Recipe->Locomotion, TEXT("Locomotion"), true);
	for (const FName Bone : Recipe->FootBones)
	{
		if (Mesh && Mesh->GetRefSkeleton().FindBoneIndex(Bone) == INDEX_NONE)
		{
			Error(FString::Printf(TEXT("FootBones: %s 뼈가 메시에 없습니다."), *Bone.ToString()));
		}
	}
	if (Recipe->HoverDrop > 0.f && Recipe->HoverFootBones.IsEmpty())
	{
		Error(TEXT("HoverDrop을 쓰려면 HoverFootBones가 필요합니다."));
	}
	for (const FName Bone : Recipe->HoverFootBones)
	{
		if (Mesh && Mesh->GetRefSkeleton().FindBoneIndex(Bone) == INDEX_NONE)
		{
			Error(FString::Printf(TEXT("HoverFootBones: %s 뼈가 메시에 없습니다."), *Bone.ToString()));
		}
	}
	if (!Recipe->FootBones.IsEmpty() && Mesh && Mesh->GetRefSkeleton().FindBoneIndex(Recipe->PelvisBone) == INDEX_NONE)
	{
		Error(FString::Printf(TEXT("PelvisBone: %s 뼈가 메시에 없습니다."), *Recipe->PelvisBone.ToString()));
	}

	TSet<FGameplayTag> PartTags;
	for (const FknightMonsterPartDefinition& Part : Recipe->Parts)
	{
		const FString Label = FString::Printf(TEXT("부위 %s"), *Part.PartTag.ToString());
		if (!Part.PartTag.IsValid())
		{
			Error(TEXT("PartTag가 비어 있는 부위가 있습니다."));
			continue;
		}
		bool bDuplicate = false;
		PartTags.Add(Part.PartTag, &bDuplicate);
		if (bDuplicate)
		{
			Error(Label + TEXT(": 같은 PartTag가 두 번 있습니다."));
		}
		if (Part.DisplayName.IsEmpty())
		{
			Warning(Label + TEXT(": DisplayName이 비어 있습니다(락온 부위 이름)."));
		}
		for (const FName Bone : Part.Bones)
		{
			if (Mesh && Mesh->GetRefSkeleton().FindBoneIndex(Bone) == INDEX_NONE)
			{
				Warning(FString::Printf(TEXT("%s: %s 뼈가 메시에 없습니다."), *Label, *Bone.ToString()));
			}
		}
		if (!Part.LockOnSocket.IsNone() && Mesh && !HasBoneOrSocket(Mesh, Part.LockOnSocket))
		{
			Warning(FString::Printf(TEXT("%s: 락온 소켓 %s가 메시에 없습니다."), *Label, *Part.LockOnSocket.ToString()));
		}
	}
	if (Recipe->Parts.IsEmpty())
	{
		Warning(TEXT("부위가 없습니다. 부위가 없으면 락온은 FallbackLockOnSocket 한 곳만 씁니다."));
	}

	TSet<FName> AttackNames;
	bool bHasRanged = false;
	for (const FknightRecipeAttack& Attack : Recipe->Attacks)
	{
		const FString Label = FString::Printf(TEXT("공격 %s"), *Attack.Name.ToString());
		if (Attack.Name.IsNone() || Attack.Name.ToString().Contains(TEXT(" ")))
		{
			Error(TEXT("Name이 비었거나 공백이 있는 공격이 있습니다."));
		}
		bool bDuplicate = false;
		AttackNames.Add(Attack.Name, &bDuplicate);
		if (bDuplicate)
		{
			Error(Label + TEXT(": 같은 이름이 두 번 있습니다."));
		}
		for (const TCHAR* Reserved : { TEXT("Parried"), TEXT("BreakFlinch"), TEXT("Down"), TEXT("EngageRoar"), TEXT("EnrageRoar"), TEXT("Death") })
		{
			if (Attack.Name == Reserved)
			{
				Error(Label + TEXT(": 반응 어빌리티 이름과 겹칩니다."));
			}
		}
		CheckSequence(Attack.Animation, Label, true);
		const float Length = Attack.Animation ? Attack.Animation->GetPlayLength() : 0.f;
		auto CheckWindow = [&](float Start, float End, const TCHAR* Kind)
		{
			if (End <= Start)
			{
				Error(FString::Printf(TEXT("%s: %s 구간의 끝(%.2f)이 시작(%.2f)보다 빠릅니다."), *Label, Kind, End, Start));
			}
			else if (Attack.Animation && End > Length + KINDA_SMALL_NUMBER)
			{
				Error(FString::Printf(TEXT("%s: %s 구간(%.2f)이 애니메이션 길이(%.2f)를 넘습니다."), *Label, Kind, End, Length));
			}
		};
		for (const FknightRecipePlayRateWindow& Window : Attack.PlayRateWindows)
		{
			CheckWindow(Window.Start, Window.End, TEXT("재생 속도"));
		}
		if (Attack.Type == EknightRecipeAttackType::Melee)
		{
			if (Attack.HitWindows.IsEmpty())
			{
				Warning(Label + TEXT(": 판정 구간(HitWindows)이 없어 맞지 않는 공격이 됩니다."));
			}
			for (const FknightRecipeHitWindow& Hit : Attack.HitWindows)
			{
				CheckWindow(Hit.Start, Hit.End, TEXT("판정"));
				for (const FName Socket : { Hit.Trace.StartSocket, Hit.Trace.EndSocket })
				{
					if (Hit.Trace.MeshComponentTag.IsNone() && Mesh && !HasBoneOrSocket(Mesh, Socket))
					{
						Warning(FString::Printf(TEXT("%s: 판정 소켓 %s가 메시에 없습니다."), *Label, *Socket.ToString()));
					}
				}
			}
		}
		else if (Attack.Animation && Attack.WindupTime + Attack.ActiveTime > Length + KINDA_SMALL_NUMBER && Attack.PlayRateWindows.IsEmpty())
		{
			Warning(FString::Printf(TEXT("%s: 예고+판정 시간(%.2f)이 애니메이션 길이(%.2f)보다 깁니다."), *Label, Attack.WindupTime + Attack.ActiveTime, Length));
		}
		const FknightMonsterAttackSelection& Selection = Attack.Selection;
		if (Selection.MinRange > Selection.MaxRange)
		{
			Error(Label + TEXT(": MinRange가 MaxRange보다 큽니다."));
		}
		if (Selection.MinAngle > Selection.MaxAngle)
		{
			Error(Label + TEXT(": MinAngle이 MaxAngle보다 큽니다."));
		}
		if (!Attack.HitParams.DamageEffect)
		{
			Error(Label + TEXT(": HitParams.DamageEffect가 비어 있습니다."));
		}
		bHasRanged |= Attack.bRanged;
	}
	if (Recipe->Attacks.IsEmpty())
	{
		Warning(TEXT("공격이 없습니다."));
	}
	if (bHasRanged && Recipe->RangedMaxDistance <= Recipe->RangedMinDistance)
	{
		Error(TEXT("원거리 공격이 있는데 RangedMaxDistance가 RangedMinDistance보다 작습니다."));
	}

	const FknightRecipeReactions& Reactions = Recipe->Reactions;
	CheckSequence(Reactions.Hit, TEXT("반응 Hit"), false);
	CheckSequence(Reactions.DownStart, TEXT("반응 DownStart"), false);
	CheckSequence(Reactions.DownLoop, TEXT("반응 DownLoop"), false);
	CheckSequence(Reactions.GetUp, TEXT("반응 GetUp"), false);
	CheckSequence(Reactions.EngageRoar, TEXT("반응 EngageRoar"), false);
	CheckSequence(Reactions.EnrageRoar, TEXT("반응 EnrageRoar"), false);
	CheckSequence(Reactions.Death, TEXT("반응 Death"), false);
	if (!Reactions.Death)
	{
		Warning(TEXT("Death 애니메이션이 없어 사망 연출 없이 현재 포즈로 남습니다."));
	}
	if (!Reactions.Hit)
	{
		Warning(TEXT("Hit 애니메이션이 없어 퍼펙트 가드·부위 파괴 경직을 만들지 않습니다."));
	}
	if (!Reactions.DownStart)
	{
		Warning(TEXT("DownStart 애니메이션이 없어 다운을 만들지 않습니다(경직도가 깨져도 반응 없음)."));
	}
	else if (!Reactions.GetUp)
	{
		Warning(TEXT("GetUp 애니메이션이 없어 다운 후 1초 뒤 바로 일어납니다."));
	}
	return Problems;
}

bool UknightMonsterRecipeLibrary::BuildMonsterRecipe(UknightMonsterRecipe* Recipe)
{
	FContext Context;
	Context.Recipe = Recipe;
	const FString Title = FString::Printf(TEXT("몬스터 레시피 Build: %s"), Recipe ? *Recipe->MonsterName.ToString() : TEXT("(없음)"));

	for (const FString& Problem : ValidateMonsterRecipe(Recipe))
	{
		if (Problem.StartsWith(TEXT("오류: ")))
		{
			Context.Log.Error(Problem.RightChop(4));
		}
		else
		{
			Context.Log.Warning(Problem.RightChop(4));
		}
	}
	if (!Context.Log.Errors.IsEmpty())
	{
		Context.Log.Flush(Title + TEXT(" (검사 실패, 만들지 않음)"));
		return false;
	}

	Context.Root = Recipe->GetOutputPath();
	Context.Prefix = Recipe->MonsterName.ToString();

	TArray<TSubclassOf<UGameplayAbility>> Abilities;
	for (const FknightRecipeAttack& Attack : Recipe->Attacks)
	{
		Abilities.Add(BuildAttackAbility(Context, Attack));
	}
	BuildReactionAbilities(Context, Abilities);

	const TSubclassOf<UGameplayEffect> InitEffect = BuildInitEffect(Context);
	const TSubclassOf<UGameplayEffect> Enrage = BuildTimedEffect(Context, TEXT("Enrage"), SharedEnrage, Recipe->EnrageDuration);
	const TSubclassOf<UGameplayEffect> Exhaust = BuildTimedEffect(Context, TEXT("Exhaust"), SharedExhaust, Recipe->ExhaustDuration);
	UknightMonsterPartData* const PartData = BuildPartData(Context);
	UknightAbilitySet* const AbilitySet = BuildAbilitySet(Context, Abilities, InitEffect);
	UknightMonsterDefinition* const Definition = BuildDefinition(Context, AbilitySet, PartData, Enrage, Exhaust);
	UAnimBlueprint* const AnimBlueprint = BuildAnimBlueprint(Context);
	BuildFootLockCurves(Context);
	BuildMonsterBlueprint(Context, Definition, AnimBlueprint);

	if (!SavePackages(Context.Touched))
	{
		Context.Log.Error(TEXT("저장하지 못한 에셋이 있습니다."));
	}
	Context.Log.Info(FString::Printf(TEXT("%s 아래 에셋 %d개를 만들거나 고쳤습니다."), *Context.Root, Context.Touched.Num()));
	Context.Log.Flush(Title);
	return Context.Log.Errors.IsEmpty();
}

////////////////////////////////////////////////////////////////////
// 공용 에셋

namespace knightRecipe
{
	/** 공용 효과의 OnCompleteAlways(분노 끝 → 분노 초기화)를 공용 효과로 바꾼다. */
	void RetargetCompleteEffect(FLog& Log, const TCHAR* EffectPath, const TCHAR* CompletePath, TSet<UPackage*>& Touched)
	{
		UGameplayEffect* const Effect = Cast<UGameplayEffect>(GetCDO(LoadAsset<UBlueprint>(EffectPath)));
		UClass* const Complete = LoadBlueprintClass(CompletePath);
		const UAdditionalEffectsGameplayEffectComponent* const Additional = Effect ? Effect->FindComponent<UAdditionalEffectsGameplayEffectComponent>() : nullptr;
		if (!Additional || !Complete)
		{
			Log.Error(FString::Printf(TEXT("%s의 종료 효과를 바꾸지 못했습니다."), EffectPath));
			return;
		}
		UAdditionalEffectsGameplayEffectComponent* const Mutable = const_cast<UAdditionalEffectsGameplayEffectComponent*>(Additional);
		Mutable->Modify();
		Mutable->OnCompleteAlways = { TSubclassOf<UGameplayEffect>(Complete) };
		Effect->MarkPackageDirty();
		Touched.Add(Effect->GetPackage());
	}

	/** 잔열 StateTree를 복사해 몬스터 기반 클래스를 컨텍스트로 쓰고, 거리 값을 Knight Monster Target 출력에 바인딩한다. */
	void SetupSharedStateTree(FLog& Log, UStateTree* StateTree)
	{
		UStateTreeEditorData* const EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
		UStateTreeAIComponentSchema* const Schema = EditorData ? Cast<UStateTreeAIComponentSchema>(EditorData->Schema) : nullptr;
		if (!Schema)
		{
			Log.Error(TEXT("공용 StateTree의 스키마를 찾지 못했습니다."));
			return;
		}
		EditorData->Modify();
		Schema->Modify();
		SetObject(Schema, TEXT("AIControllerClass"), AknightBossAIController::StaticClass(), Log);
		SetObject(Schema, TEXT("ContextActorClass"), AknightMonsterCharacterBase::StaticClass(), Log);
		Schema->PostLoad();

		if (EditorData->Evaluators.IsEmpty())
		{
			Log.Error(TEXT("공용 StateTree에 Knight Monster Target 평가기가 없습니다."));
			return;
		}
		const FGuid EvaluatorID = EditorData->Evaluators[0].ID;

		auto FindState = [EditorData](FName Name) -> UStateTreeState*
		{
			TArray<UStateTreeState*> Pending(EditorData->SubTrees);
			while (!Pending.IsEmpty())
			{
				UStateTreeState* const State = Pending.Pop();
				if (State->Name == Name)
				{
					return State;
				}
				Pending.Append(State->Children);
			}
			return nullptr;
		};
		auto Bind = [&](const FGuid& TargetID, FName TargetProperty, FName SourceProperty)
		{
			const FPropertyBindingPath Target(TargetID, TargetProperty);
			EditorData->EditorBindings.RemoveBindings(Target);
			EditorData->EditorBindings.AddBinding(FPropertyBindingPath(EvaluatorID, SourceProperty), Target);
		};

		const UStateTreeState* const Chase = FindState(TEXT("Chase"));
		if (Chase && !Chase->Tasks.IsEmpty())
		{
			Bind(Chase->Tasks[0].ID, TEXT("AcceptableRadius"), TEXT("ChaseStopDistance"));
		}
		else
		{
			Log.Error(TEXT("공용 StateTree에 Chase 상태의 Move To 태스크가 없습니다."));
		}

		// Ranged 진입 조건: [1] 거리 >= 최소, [2] 거리 <= 최대 (잔열 StateTree 순서).
		const UStateTreeState* const Ranged = FindState(TEXT("Ranged"));
		int32 BoundRanged = 0;
		if (Ranged)
		{
			for (const FStateTreeEditorNode& Condition : Ranged->EnterConditions)
			{
				if (Condition.Node.GetScriptStruct() && Condition.Node.GetScriptStruct()->GetName() == TEXT("StateTreeCompareFloatCondition"))
				{
					Bind(Condition.ID, TEXT("Right"), BoundRanged == 0 ? TEXT("RangedMinDistance") : TEXT("RangedMaxDistance"));
					++BoundRanged;
				}
			}
		}
		if (BoundRanged != 2)
		{
			Log.Error(TEXT("공용 StateTree의 Ranged 상태에서 거리 조건 2개를 찾지 못했습니다."));
		}
		if (!UknightMonsterRecipeLibrary::SetFaceTargetSideAttacks(StateTree, TEXT("Aim"), true, FGameplayTagContainer(Tag(TEXT("Ability.Attack.Ranged")))))
		{
			Log.Error(TEXT("공용 StateTree의 Aim 상태에서 Knight Face Target을 찾지 못했습니다."));
		}

		FStateTreeCompilerLog CompilerLog;
		if (!UStateTreeEditingSubsystem::CompileStateTree(StateTree, CompilerLog))
		{
			Log.Error(TEXT("공용 StateTree 컴파일 실패. 에디터에서 열어 확인하세요."));
		}
		StateTree->MarkPackageDirty();
	}
}

bool UknightMonsterRecipeLibrary::SetFaceTargetSideAttacks(UStateTree* StateTree, FName StateName, bool bSkip, const FGameplayTagContainer& SideAttackTags)
{
	UStateTreeEditorData* const EditorData = StateTree ? Cast<UStateTreeEditorData>(StateTree->EditorData) : nullptr;
	if (!EditorData)
	{
		return false;
	}
	TArray<UStateTreeState*> Pending(EditorData->SubTrees);
	while (!Pending.IsEmpty())
	{
		UStateTreeState* const State = Pending.Pop();
		Pending.Append(State->Children);
		if (State->Name != StateName)
		{
			continue;
		}
		for (FStateTreeEditorNode& Task : State->Tasks)
		{
			const UScriptStruct* const InstanceStruct = Task.Instance.GetScriptStruct();
			const FBoolProperty* const SkipProperty = InstanceStruct ? FindFProperty<FBoolProperty>(InstanceStruct, TEXT("bSkipForSideAttacks")) : nullptr;
			const FStructProperty* const TagsProperty = InstanceStruct ? FindFProperty<FStructProperty>(InstanceStruct, TEXT("SideAttackTags")) : nullptr;
			if (!SkipProperty || !TagsProperty || TagsProperty->Struct != FGameplayTagContainer::StaticStruct())
			{
				continue;
			}
			State->Modify();
			SkipProperty->SetPropertyValue_InContainer(Task.Instance.GetMutableMemory(), bSkip);
			*TagsProperty->ContainerPtrToValuePtr<FGameplayTagContainer>(Task.Instance.GetMutableMemory()) = SideAttackTags;
			FStateTreeCompilerLog CompilerLog;
			const bool bCompiled = UStateTreeEditingSubsystem::CompileStateTree(StateTree, CompilerLog);
			StateTree->MarkPackageDirty();
			return bCompiled;
		}
	}
	return false;
}

bool UknightMonsterRecipeLibrary::CreateSharedMonsterAssets()
{
	FLog Log;
	TSet<UPackage*> Touched;
	auto Copy = [&](const TCHAR* SourceName, const TCHAR* TargetPath) -> UObject*
	{
		if (FPackageName::DoesPackageExist(TargetPath))
		{
			return nullptr;
		}
		UObject* const Copied = DuplicateAsset(FString(SourceRoot) / SourceName, TargetPath);
		if (!Copied)
		{
			Log.Error(FString::Printf(TEXT("%s를 복사하지 못했습니다."), SourceName));
			return nullptr;
		}
		Touched.Add(Copied->GetPackage());
		Log.Info(FString::Printf(TEXT("만듦 %s"), TargetPath));
		return Copied;
	};

	Copy(TEXT("Effects/GE_Janyeol_RageReset"), SharedRageReset);
	Copy(TEXT("Effects/GE_Janyeol_StaminaRestore"), SharedStaminaRestore);
	if (Copy(TEXT("Effects/GE_Janyeol_Enrage"), SharedEnrage))
	{
		RetargetCompleteEffect(Log, SharedEnrage, SharedRageReset, Touched);
	}
	if (Copy(TEXT("Effects/GE_Janyeol_Exhaust"), SharedExhaust))
	{
		RetargetCompleteEffect(Log, SharedExhaust, SharedStaminaRestore, Touched);
	}
	Copy(TEXT("Animations/ABP_Janyeol"), SharedAnimBlueprint);
	if (UStateTree* const StateTree = Cast<UStateTree>(Copy(TEXT("AI/ST_Janyeol"), SharedStateTree)))
	{
		SetupSharedStateTree(Log, StateTree);
	}
	if (UBlueprint* const Controller = Cast<UBlueprint>(Copy(TEXT("AI/BP_JanyeolAIController"), SharedController)))
	{
		const AknightBossAIController* const CDO = Cast<AknightBossAIController>(GetCDO(Controller));
		UStateTreeAIComponent* const StateTreeAI = CDO ? CDO->GetStateTreeComponent() : nullptr;
		FStateTreeReference* const Reference = StateTreeAI ? ValuePtr<FStateTreeReference>(StateTreeAI->GetClass(), StateTreeAI, TEXT("StateTreeRef"), &Log) : nullptr;
		if (Reference)
		{
			StateTreeAI->Modify();
			Reference->SetStateTree(LoadAsset<UStateTree>(SharedStateTree));
			Controller->MarkPackageDirty();
		}
	}

	if (!SavePackages(Touched))
	{
		Log.Error(TEXT("저장하지 못한 공용 에셋이 있습니다."));
	}
	Log.Flush(TEXT("몬스터 공용 에셋"));
	return Log.Errors.IsEmpty();
}

////////////////////////////////////////////////////////////////////
// 기존 몬스터 → 레시피

namespace knightRecipe
{
	UAnimSequence* SegmentSequence(const UAnimMontage* Montage, int32 Index)
	{
		if (!Montage || Montage->SlotAnimTracks.IsEmpty() || !Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.IsValidIndex(Index))
		{
			return nullptr;
		}
		return Cast<UAnimSequence>(Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[Index].GetAnimReference());
	}

	void ReadMontageWindows(const UAnimMontage* Montage, FknightRecipeAttack& Attack)
	{
		if (!Montage)
		{
			return;
		}
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			const float Start = Event.GetTime();
			const float End = Start + Event.GetDuration();
			if (const UknightAnimNotifyState_MeleeTrace* const Trace = Cast<UknightAnimNotifyState_MeleeTrace>(Event.NotifyStateClass))
			{
				FknightRecipeHitWindow& Window = Attack.HitWindows.AddDefaulted_GetRef();
				Window.Start = Start;
				Window.End = End;
				GetValue(Trace, TEXT("TraceParams"), Window.Trace);
			}
			else if (const UknightAnimNotifyState_PlayRate* const PlayRate = Cast<UknightAnimNotifyState_PlayRate>(Event.NotifyStateClass))
			{
				FknightRecipePlayRateWindow& Window = Attack.PlayRateWindows.AddDefaulted_GetRef();
				Window.Start = Start;
				Window.End = End;
				GetValue(PlayRate, TEXT("PlayRate"), Window.PlayRate);
			}
		}
		Attack.HitWindows.Sort([](const FknightRecipeHitWindow& A, const FknightRecipeHitWindow& B) { return A.Start < B.Start; });
		Attack.PlayRateWindows.Sort([](const FknightRecipePlayRateWindow& A, const FknightRecipePlayRateWindow& B) { return A.Start < B.Start; });
	}

	float StaticMagnitude(const FGameplayEffectModifierMagnitude& Magnitude)
	{
		float Value = 0.f;
		Magnitude.GetStaticMagnitudeIfPossible(1.f, Value);
		return Value;
	}
}

UknightMonsterRecipe* UknightMonsterRecipeLibrary::CreateRecipeFromMonster(TSubclassOf<AknightMonsterCharacterBase> MonsterClass, const FString& RecipePackagePath,
	FName MonsterName, const FString& OutputFolder)
{
	FLog Log;
	const AknightMonsterCharacterBase* const Monster = MonsterClass ? MonsterClass->GetDefaultObject<AknightMonsterCharacterBase>() : nullptr;
	const UknightMonsterDefinition* const Definition = Monster ? Monster->GetMonsterDefinition() : nullptr;
	if (!Definition)
	{
		Log.Error(TEXT("몬스터 클래스나 몬스터 DA가 없습니다."));
		Log.Flush(TEXT("레시피 가져오기"));
		return nullptr;
	}

	bool bCreated = false;
	UknightMonsterRecipe* const Recipe = FindOrCreateAsset<UknightMonsterRecipe>(RecipePackagePath, bCreated);
	Recipe->Modify();
	Recipe->MonsterName = MonsterName;
	Recipe->OutputFolder.Path = OutputFolder;
	Recipe->DisplayName = Definition->DisplayName;

	const USkeletalMeshComponent* const Mesh = Monster->GetMesh();
	Recipe->Mesh = Mesh->GetSkeletalMeshAsset();
	Recipe->MeshScale = Mesh->GetRelativeScale3D().X;
	Recipe->MeshYaw = Mesh->GetRelativeRotation().Yaw;
	Recipe->CapsuleRadius = Monster->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	Recipe->CapsuleHalfHeight = Monster->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	Recipe->MaxWalkSpeed = Monster->GetCharacterMovement()->MaxWalkSpeed;
	if (const UknightMonsterPushOutComponent* const PushOut = GetObject<UknightMonsterPushOutComponent>(Monster, TEXT("PushOut")))
	{
		GetValue(PushOut, TEXT("BodyAxisHalfLength"), Recipe->PushOutBodyAxisHalfLength);
		Recipe->bPushOutOfMesh = GetBool(PushOut, TEXT("bPushOutOfMesh"));
	}
	if (const UknightMonsterHoverComponent* const Hover = GetObject<UknightMonsterHoverComponent>(Monster, TEXT("Hover")))
	{
		GetValue(Hover, TEXT("HoverDrop"), Recipe->HoverDrop);
		GetValue(Hover, TEXT("FootBones"), Recipe->HoverFootBones);
		GetValue(Hover, TEXT("FootGroundHeight"), Recipe->HoverFootHeight);
	}
	if (const UBlueprint* const MonsterBlueprint = Cast<UBlueprint>(MonsterClass->ClassGeneratedBy))
	{
		const USCS_Node* const Blocker = MonsterBlueprint->SimpleConstructionScript ? MonsterBlueprint->SimpleConstructionScript->FindSCSNode(TEXT("BodyBlocker")) : nullptr;
		if (const UBoxComponent* const Box = Blocker ? Cast<UBoxComponent>(Blocker->ComponentTemplate) : nullptr)
		{
			Recipe->BodyBlockerExtent = Box->GetUnscaledBoxExtent();
			Recipe->BodyBlockerOffset = Box->GetRelativeLocation();
		}
	}

	if (const UAnimBlueprint* const AnimBlueprint = Mesh->AnimClass ? Cast<UAnimBlueprint>(Mesh->AnimClass->ClassGeneratedBy) : nullptr)
	{
		TArray<UEdGraph*> Graphs;
		AnimBlueprint->GetAllGraphs(Graphs);
		for (const UEdGraph* const Graph : Graphs)
		{
			for (UEdGraphNode* const Node : Graph->Nodes)
			{
				if (const UAnimGraphNode_BlendSpacePlayer* const BlendSpacePlayer = Cast<UAnimGraphNode_BlendSpacePlayer>(Node))
				{
					Recipe->Locomotion = Cast<UBlendSpace>(BlendSpacePlayer->GetAnimationAsset());
				}
				else if (Node && Node->GetClass()->GetFName() == TEXT("AnimGraphNode_knightLegPlacement"))
				{
					if (const FAnimNode_knightLegPlacement* const LegNode = ValuePtr<FAnimNode_knightLegPlacement>(Node->GetClass(), Node, TEXT("Node"), &Log))
					{
						Recipe->FootBones.Reset();
						for (const FknightLegPlacementLeg& Leg : LegNode->Legs)
						{
							Recipe->FootBones.Add(Leg.FootBone.BoneName);
						}
						Recipe->PelvisBone = LegNode->PelvisBone.BoneName;
					}
				}
			}
		}
	}

	Recipe->ParriedPoiseDamageRatio = Definition->ParriedPoiseDamageRatio;
	Recipe->TargetRadius = Definition->TargetRadius;
	Recipe->FallbackLockOnSocket = Definition->FallbackLockOnSocket;
	Recipe->ChaseStopDistance = Definition->ChaseStopDistance;
	Recipe->RangedMinDistance = Definition->RangedMinDistance;
	Recipe->RangedMaxDistance = Definition->RangedMaxDistance;
	Recipe->Parts = Definition->PartData ? Definition->PartData->Parts : TArray<FknightMonsterPartDefinition>();
	if (const UGameplayEffect* const Enrage = Definition->EnrageEffect ? Definition->EnrageEffect->GetDefaultObject<UGameplayEffect>() : nullptr)
	{
		Recipe->EnrageDuration = StaticMagnitude(Enrage->DurationMagnitude);
	}
	if (const UGameplayEffect* const Exhaust = Definition->ExhaustEffect ? Definition->ExhaustEffect->GetDefaultObject<UGameplayEffect>() : nullptr)
	{
		Recipe->ExhaustDuration = StaticMagnitude(Exhaust->DurationMagnitude);
	}

	Recipe->Attacks.Reset();
	Recipe->Reactions = FknightRecipeReactions();
	const FString AbilityPrefix = TEXT("GA_") + MonsterName.ToString() + TEXT("_");
	for (const UknightAbilitySet* const AbilitySet : Definition->AbilitySets)
	{
		TArray<FknightAbilitySet_GameplayAbility> Granted;
		TArray<FknightAbilitySet_GameplayEffect> Effects;
		GetValue(AbilitySet, TEXT("GrantedGameplayAbilities"), Granted);
		GetValue(AbilitySet, TEXT("GrantedGameplayEffects"), Effects);

		for (const FknightAbilitySet_GameplayEffect& Entry : Effects)
		{
			const UGameplayEffect* const Effect = Entry.GameplayEffect ? Entry.GameplayEffect->GetDefaultObject<UGameplayEffect>() : nullptr;
			for (const FGameplayModifierInfo& Modifier : Effect ? Effect->Modifiers : TArray<FGameplayModifierInfo>())
			{
				const FString Attribute = Modifier.Attribute.GetName();
				const float Value = StaticMagnitude(Modifier.ModifierMagnitude);
				if (Attribute == TEXT("MaxHealth")) { Recipe->MaxHealth = Value; }
				else if (Attribute == TEXT("AttackPower")) { Recipe->AttackPower = Value; }
				else if (Attribute == TEXT("MaxPoise")) { Recipe->MaxPoise = Value; }
				else if (Attribute == TEXT("MaxRage")) { Recipe->MaxRage = Value; }
				else if (Attribute == TEXT("MaxStamina")) { Recipe->MaxStamina = Value; }
			}
		}

		for (const FknightAbilitySet_GameplayAbility& Entry : Granted)
		{
			const UGameplayAbility* const CDO = Entry.Ability ? Entry.Ability->GetDefaultObject<UGameplayAbility>() : nullptr;
			if (!CDO)
			{
				continue;
			}
			FString ShortName = Entry.Ability->GetName();
			ShortName.RemoveFromEnd(TEXT("_C"));
			ShortName.RemoveFromStart(AbilityPrefix);

			if (CDO->IsA<UknightGameplayAbility_MonsterAttack>())
			{
				FknightRecipeAttack& Attack = Recipe->Attacks.AddDefaulted_GetRef();
				Attack.Name = *ShortName;
				Attack.Type = CDO->IsA<UknightGameplayAbility_MonsterAreaAttack>() ? EknightRecipeAttackType::Area : EknightRecipeAttackType::Melee;
				FGameplayTagContainer AbilityTags;
				GetValue(CDO, TEXT("AbilityTags"), AbilityTags);
				Attack.bRanged = AbilityTags.HasTagExact(Tag(TEXT("Ability.Attack.Ranged")));
				const UAnimMontage* const Montage = GetObject<UAnimMontage>(CDO, TEXT("Montage"));
				Attack.Animation = SegmentSequence(Montage, 0);
				ReadMontageWindows(Montage, Attack);
				GetValue(CDO, TEXT("Selection"), Attack.Selection);
				GetValue(CDO, TEXT("HitParams"), Attack.HitParams);
				GetValue(CDO, TEXT("StartTurnYaw"), Attack.StartTurnYaw);
				GetValue(CDO, TEXT("StartTurnRate"), Attack.StartTurnRate);
				GetValue(CDO, TEXT("StartTurnMaxTime"), Attack.StartTurnMaxTime);
				if (Attack.Type == EknightRecipeAttackType::Area)
				{
					Attack.HitWindows.Reset();
					GetValue(CDO, TEXT("WindupTime"), Attack.WindupTime);
					GetValue(CDO, TEXT("ActiveTime"), Attack.ActiveTime);
					GetValue(CDO, TEXT("RecoveryTime"), Attack.RecoveryTime);
					GetValue(CDO, TEXT("AreaOffset"), Attack.AreaOffset);
					GetValue(CDO, TEXT("AreaRadius"), Attack.AreaRadius);
					GetValue(CDO, TEXT("TelegraphCue"), Attack.TelegraphCue);
					GetValue(CDO, TEXT("LungeMaxDistance"), Attack.LungeMaxDistance);
					GetValue(CDO, TEXT("LungeStartTime"), Attack.LungeStartTime);
					GetValue(CDO, TEXT("LungeEndTime"), Attack.LungeEndTime);
				}
			}
			else if (CDO->IsA<UknightGameplayAbility_MonsterDeath>())
			{
				Recipe->Reactions.Death = SegmentSequence(GetObject<UAnimMontage>(CDO, TEXT("DeathMontage")), 0);
			}
			else if (CDO->IsA<UknightGameplayAbility_MonsterReaction>())
			{
				TArray<FAbilityTriggerData> Triggers;
				GetValue(CDO, TEXT("AbilityTriggers"), Triggers);
				const FGameplayTag Trigger = Triggers.IsEmpty() ? FGameplayTag() : Triggers[0].TriggerTag;
				const UAnimMontage* const Hold = GetObject<UAnimMontage>(CDO, TEXT("HoldMontage"));
				float HoldTime = 0.f;
				GetValue(CDO, TEXT("HoldTime"), HoldTime);
				FknightRecipeReactions& Reactions = Recipe->Reactions;
				if (Trigger == Tag(TEXT("Event.Parried")) || Trigger == Tag(TEXT("Event.PartBroken")))
				{
					Reactions.Hit = SegmentSequence(Hold, 0);
					Reactions.StaggerTime = HoldTime;
				}
				else if (Trigger == Tag(TEXT("Event.PoiseBroken")))
				{
					Reactions.DownStart = SegmentSequence(Hold, 0);
					Reactions.DownLoop = SegmentSequence(Hold, 1);
					Reactions.GetUp = SegmentSequence(GetObject<UAnimMontage>(CDO, TEXT("RecoverMontage")), 0);
					Reactions.DownTime = HoldTime;
					GetValue(CDO, TEXT("MaxPoiseGrowth"), Reactions.MaxPoiseGrowth);
				}
				else if (Trigger == Tag(TEXT("Event.Monster.Engaged")))
				{
					Reactions.EngageRoar = SegmentSequence(Hold, 0);
				}
				else if (Trigger == Tag(TEXT("State.Monster.Enraged")))
				{
					Reactions.EnrageRoar = SegmentSequence(Hold, 0);
				}
				else
				{
					Log.Warning(FString::Printf(TEXT("%s: 레시피가 모르는 반응(%s)이라 가져오지 않았습니다."), *ShortName, *Trigger.ToString()));
				}
			}
			else
			{
				Log.Warning(FString::Printf(TEXT("%s: 레시피가 지원하지 않는 어빌리티라 가져오지 않았습니다."), *ShortName));
			}
		}
	}
	Recipe->Attacks.Sort([](const FknightRecipeAttack& A, const FknightRecipeAttack& B) { return A.Name.LexicalLess(B.Name); });

	Recipe->MarkPackageDirty();
	SavePackages({ Recipe->GetPackage() });
	Log.Info(FString::Printf(TEXT("%s: 공격 %d개, 부위 %d개"), *RecipePackagePath, Recipe->Attacks.Num(), Recipe->Parts.Num()));
	Log.Flush(TEXT("레시피 가져오기"));
	return Recipe;
}

UAnimMontage* UknightMonsterRecipeLibrary::BuildSectionMontage(const FString& PackagePath, const TArray<UAnimSequence*>& Sequences,
	const TArray<FName>& SectionNames, FName LoopSection)
{
	if (Sequences.IsEmpty() || Sequences.Num() != SectionNames.Num() || Sequences.Contains(nullptr))
	{
		UE_LOG(LogTemp, Error, TEXT("BuildSectionMontage %s: 시퀀스와 섹션 이름의 개수가 같아야 합니다."), *PackagePath);
		return nullptr;
	}
	bool bCreated = false;
	UAnimMontage* const Montage = knightRecipe::FindOrCreateAsset<UAnimMontage>(PackagePath, bCreated);
	Montage->Modify();
	Montage->SetSkeleton(Sequences[0]->GetSkeleton());
	if (Montage->SlotAnimTracks.IsEmpty())
	{
		Montage->SlotAnimTracks.AddDefaulted();
	}
	FSlotAnimationTrack& SlotTrack = Montage->SlotAnimTracks[0];
	SlotTrack.SlotName = FAnimSlotGroup::DefaultSlotName;
	SlotTrack.AnimTrack.AnimSegments.Reset();
	Montage->CompositeSections.Reset();

	float Length = 0.f;
	for (int32 Index = 0; Index < Sequences.Num(); ++Index)
	{
		FAnimSegment Segment;
		Segment.SetAnimReference(Sequences[Index], true);
		Segment.StartPos = Length;
		SlotTrack.AnimTrack.AnimSegments.Add(Segment);

		FCompositeSection Section;
		Section.SectionName = SectionNames[Index];
		Section.Link(Montage, Length);
		Section.NextSectionName = SectionNames[Index] == LoopSection ? LoopSection
			: SectionNames.IsValidIndex(Index + 1) ? SectionNames[Index + 1] : NAME_None;
		Montage->CompositeSections.Add(Section);
		Length += Sequences[Index]->GetPlayLength();
	}
	Montage->SetCompositeLength(Length);
	Montage->RefreshCacheData();
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return Montage;
}

bool UknightMonsterRecipeLibrary::ResampleBlendSpace(UBlendSpace* BlendSpace)
{
	if (!BlendSpace)
	{
		return false;
	}
	BlendSpace->Modify();
	BlendSpace->ValidateSampleData();
	BlendSpace->ResampleData();
	BlendSpace->MarkPackageDirty();
	return true;
}

bool UknightMonsterRecipeLibrary::SetMontageSegmentRange(UAnimMontage* Montage, int32 SegmentIndex, float StartTime, float EndTime)
{
	if (!Montage || Montage->SlotAnimTracks.IsEmpty())
	{
		return false;
	}
	TArray<FAnimSegment>& Segments = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments;
	const UAnimSequenceBase* const Sequence = Segments.IsValidIndex(SegmentIndex) ? Segments[SegmentIndex].GetAnimReference().Get() : nullptr;
	if (!Sequence || StartTime < 0.f || EndTime > Sequence->GetPlayLength() + UE_KINDA_SMALL_NUMBER || StartTime >= EndTime)
	{
		UE_LOG(LogTemp, Error, TEXT("SetMontageSegmentRange %s: 구간 %d의 범위 %.3f~%.3f가 잘못됐습니다."), *GetNameSafe(Montage), SegmentIndex, StartTime, EndTime);
		return false;
	}
	Montage->Modify();
	Segments[SegmentIndex].AnimStartTime = StartTime;
	Segments[SegmentIndex].AnimEndTime = EndTime;

	float Position = 0.f;
	for (FAnimSegment& Segment : Segments)
	{
		Segment.StartPos = Position;
		Position += Segment.GetLength();
	}
	if (Montage->CompositeSections.Num() == Segments.Num())
	{
		for (int32 Index = 0; Index < Segments.Num(); ++Index)
		{
			Montage->CompositeSections[Index].Link(Montage, Segments[Index].StartPos);
		}
	}
	Montage->SetCompositeLength(Position);
	Montage->RefreshCacheData();
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return true;
}

bool UknightMonsterRecipeLibrary::SetMontageSegmentSequence(UAnimMontage* Montage, int32 SegmentIndex, UAnimSequenceBase* Sequence)
{
	if (!Montage || !Sequence || Montage->SlotAnimTracks.IsEmpty() || !Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.IsValidIndex(SegmentIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("SetMontageSegmentSequence %s: 구간 %d이나 시퀀스가 없습니다."), *GetNameSafe(Montage), SegmentIndex);
		return false;
	}
	Montage->Modify();
	Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[SegmentIndex].SetAnimReference(Sequence, true);
	return SetMontageSegmentRange(Montage, SegmentIndex, 0.f, Sequence->GetPlayLength());
}

int32 UknightMonsterRecipeLibrary::SetMontageSoundTime(UAnimMontage* Montage, const USoundBase* Sound, float Time)
{
	if (!Montage || !Sound || Time < 0.f || Time > Montage->GetPlayLength())
	{
		UE_LOG(LogTemp, Error, TEXT("SetMontageSoundTime %s: 몽타주나 소리가 없거나 시간 %.3f가 범위 밖입니다."), *GetNameSafe(Montage), Time);
		return 0;
	}
	Montage->Modify();
	int32 Moved = 0;
	for (FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const UAnimNotify_PlaySound* const PlaySound = Cast<UAnimNotify_PlaySound>(Event.Notify);
		if (PlaySound && PlaySound->Sound == Sound)
		{
			Event.Link(Montage, Time);
			Event.TriggerTimeOffset = GetTriggerTimeOffsetForType(Montage->CalculateOffsetForNotify(Time));
			++Moved;
		}
	}
	if (Moved > 0)
	{
		Montage->SortNotifies();
		Montage->RefreshCacheData();
		Montage->PostEditChange();
		Montage->MarkPackageDirty();
	}
	return Moved;
}

bool UknightMonsterRecipeLibrary::SetMontagePlayRateWindow(UAnimMontage* Montage, float Start, float End, float PlayRate)
{
	if (!Montage || Start < 0.f || End <= Start || End > Montage->GetPlayLength() || PlayRate <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("SetMontagePlayRateWindow %s: 구간 %.3f~%.3f이나 속도 %.2f가 잘못됐습니다."), *GetNameSafe(Montage), Start, End, PlayRate);
		return false;
	}
	Montage->Modify();
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		UknightAnimNotifyState_PlayRate* const Existing = Cast<UknightAnimNotifyState_PlayRate>(Event.NotifyStateClass);
		if (Existing && FMath::IsNearlyEqual(Event.GetTime(), Start, 0.001f) && FMath::IsNearlyEqual(Event.GetEndTriggerTime(), End, 0.001f))
		{
			Existing->Modify();
			if (float* const Rate = knightRecipe::ValuePtr<float>(Existing->GetClass(), Existing, TEXT("PlayRate"), nullptr))
			{
				*Rate = PlayRate;
			}
			Montage->PostEditChange();
			Montage->MarkPackageDirty();
			return true;
		}
	}
	UknightAnimNotifyState_PlayRate* const NotifyState = NewObject<UknightAnimNotifyState_PlayRate>(Montage, NAME_None, RF_Transactional);
	if (float* const Rate = knightRecipe::ValuePtr<float>(NotifyState->GetClass(), NotifyState, TEXT("PlayRate"), nullptr))
	{
		*Rate = PlayRate;
	}
	knightRecipe::AddNotifyState(Montage, NotifyState, Start, End, knightRecipe::FindOrAddNotifyTrack(Montage, TEXT("PlayRate")));
	Montage->SortNotifies();
	Montage->RefreshCacheData();
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return true;
}

UPhysicsAsset* UknightMonsterRecipeLibrary::CreateMonsterPhysicsAsset(USkeletalMesh* Mesh, const FString& PackagePath,
	const TArray<FName>& CapsuleBones, const TArray<FName>& BoxBones)
{
	if (!Mesh || (CapsuleBones.IsEmpty() && BoxBones.IsEmpty()))
	{
		UE_LOG(LogTemp, Error, TEXT("CreateMonsterPhysicsAsset %s: 메시와 뼈 목록이 필요합니다."), *PackagePath);
		return nullptr;
	}
	const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
	TArray<TPair<int32, EPhysAssetFitGeomType>> Bodies;
	for (const TPair<const TArray<FName>*, EPhysAssetFitGeomType> List : { TPair<const TArray<FName>*, EPhysAssetFitGeomType>(&CapsuleBones, EFG_Sphyl),
		TPair<const TArray<FName>*, EPhysAssetFitGeomType>(&BoxBones, EFG_Box) })
	{
		for (const FName Bone : *List.Key)
		{
			const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
			if (BoneIndex == INDEX_NONE)
			{
				UE_LOG(LogTemp, Error, TEXT("CreateMonsterPhysicsAsset %s: 뼈 %s가 메시에 없습니다."), *PackagePath, *Bone.ToString());
				return nullptr;
			}
			Bodies.Emplace(BoneIndex, List.Value);
		}
	}

	bool bCreated = false;
	UPhysicsAsset* const PhysicsAsset = knightRecipe::FindOrCreateAsset<UPhysicsAsset>(PackagePath, bCreated);
	PhysicsAsset->Modify();
	PhysicsAsset->SkeletalBodySetups.Reset();
	PhysicsAsset->ConstraintSetup.Reset();
	PhysicsAsset->CollisionDisableTable.Reset();
	PhysicsAsset->UpdateBodySetupIndexMap();

	// 목록에 없는 자식 뼈의 정점은 가장 가까운 목록 조상 바디에 들어간다(BoneIndicesInCreation).
	TArray<int32> BoneIndicesInCreation;
	for (const TPair<int32, EPhysAssetFitGeomType>& Body : Bodies)
	{
		BoneIndicesInCreation.Add(Body.Key);
	}
	TArray<FBoneVertInfo> Infos;
	FMeshUtilitiesEngine::CalcBoneVertInfos(Mesh, Infos, true, 0);

	for (const TPair<int32, EPhysAssetFitGeomType>& Body : Bodies)
	{
		FPhysAssetCreateParams Params;
		Params.GeomType = Body.Value;
		Params.bCreateConstraints = false;
		const FName BoneName = RefSkeleton.GetBoneName(Body.Key);
		const int32 BodyIndex = FPhysicsAssetUtils::CreateNewBody(PhysicsAsset, BoneName, Params);
		USkeletalBodySetup* const BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];
		if (!FPhysicsAssetUtils::CreateCollisionFromBone(BodySetup, Mesh, Body.Key, Params, Infos[Body.Key], PhysicsAsset, BoneIndicesInCreation))
		{
			UE_LOG(LogTemp, Warning, TEXT("CreateMonsterPhysicsAsset %s: 뼈 %s에 붙은 정점이 없어 바디를 만들지 못했습니다."), *PackagePath, *BoneName.ToString());
		}
		// 피격 판정만 받는다. 시뮬레이션하지 않고 애니메이션을 따라간다.
		BodySetup->PhysicsType = PhysType_Kinematic;
	}
	PhysicsAsset->UpdateBodySetupIndexMap();
	PhysicsAsset->UpdateBoundsBodiesArray();
	PhysicsAsset->SetPreviewMesh(Mesh);
	PhysicsAsset->PostEditChange();
	PhysicsAsset->MarkPackageDirty();

	if (Mesh->GetPhysicsAsset() != PhysicsAsset)
	{
		Mesh->Modify();
		Mesh->SetPhysicsAsset(PhysicsAsset);
		Mesh->MarkPackageDirty();
	}
	return PhysicsAsset;
}

#undef LOCTEXT_NAMESPACE
