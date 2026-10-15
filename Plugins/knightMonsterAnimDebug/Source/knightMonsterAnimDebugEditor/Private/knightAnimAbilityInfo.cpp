#include "knightAnimAbilityInfo.h"

#include "Abilities/GameplayAbility.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Modules/ModuleManager.h"
#include "knightAnimDebugSettings.h"

namespace knightAnimAbilityInfo
{
	/** 데이터 에셋을 따라가는 최대 깊이. 몬스터 → MonsterDefinition → AbilitySet이면 2단계로 충분하다. */
	constexpr int32 MaxAssetDepth = 4;

	static void CollectAbilityClasses(const UObject* Object, int32 Depth, TSet<const UObject*>& Visited, TArray<UClass*>& OutClasses);

	static void CollectFromValue(const FProperty* Property, const void* Value, int32 Depth, TSet<const UObject*>& Visited, TArray<UClass*>& OutClasses)
	{
		if (const FClassProperty* const ClassProperty = CastField<FClassProperty>(Property))
		{
			UClass* const Class = Cast<UClass>(ClassProperty->GetObjectPropertyValue(Value));
			if (Class && Class->IsChildOf(UGameplayAbility::StaticClass()))
			{
				OutClasses.AddUnique(Class);
			}
		}
		else if (CastField<FSoftClassProperty>(Property))
		{
			const FSoftObjectPtr& SoftClass = *static_cast<const FSoftObjectPtr*>(Value);
			UClass* const Class = Cast<UClass>(SoftClass.LoadSynchronous());
			if (Class && Class->IsChildOf(UGameplayAbility::StaticClass()))
			{
				OutClasses.AddUnique(Class);
			}
		}
		else if (const FObjectPropertyBase* const ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			// 액터, 컴포넌트, 애니메이션 같은 에셋까지 따라가면 범위가 끝없이 넓어지므로 데이터 에셋만 따라간다.
			const UObject* const Object = ObjectProperty->GetObjectPropertyValue(Value);
			if (Object && Object->IsA<UDataAsset>())
			{
				CollectAbilityClasses(Object, Depth + 1, Visited, OutClasses);
			}
		}
		else if (const FStructProperty* const StructProperty = CastField<FStructProperty>(Property))
		{
			for (TFieldIterator<FProperty> It(StructProperty->Struct); It; ++It)
			{
				for (int32 Index = 0; Index < It->ArrayDim; ++Index)
				{
					CollectFromValue(*It, It->ContainerPtrToValuePtr<void>(Value, Index), Depth, Visited, OutClasses);
				}
			}
		}
		else if (const FArrayProperty* const ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(ArrayProperty, Value);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				CollectFromValue(ArrayProperty->Inner, Helper.GetRawPtr(Index), Depth, Visited, OutClasses);
			}
		}
	}

	static void CollectAbilityClasses(const UObject* Object, int32 Depth, TSet<const UObject*>& Visited, TArray<UClass*>& OutClasses)
	{
		if (!Object || Depth > MaxAssetDepth || Visited.Contains(Object))
		{
			return;
		}
		Visited.Add(Object);
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			for (int32 Index = 0; Index < It->ArrayDim; ++Index)
			{
				CollectFromValue(*It, It->ContainerPtrToValuePtr<void>(Object, Index), Depth, Visited, OutClasses);
			}
		}
	}

	/** "knightGameplayAbility_MonsterAttack" → "MonsterAttack". */
	static FString KindName(const UClass* BaseClass)
	{
		const FString Name = CleanName(BaseClass);
		int32 Index = INDEX_NONE;
		return Name.FindLastChar(TEXT('_'), Index) ? Name.RightChop(Index + 1) : Name;
	}

	/** 객체의 프로퍼티, 없으면 그 안 구조체 프로퍼티의 필드에서 이름이 같은 값을 찾는다. */
	template<typename PropertyType>
	static const PropertyType* FindNested(const UObject* Object, FName Name, const void*& OutContainer)
	{
		if (!Object || Name.IsNone())
		{
			return nullptr;
		}
		if (const PropertyType* const Direct = FindFProperty<PropertyType>(Object->GetClass(), Name))
		{
			OutContainer = Object;
			return Direct;
		}
		for (TFieldIterator<FStructProperty> It(Object->GetClass()); It; ++It)
		{
			if (const PropertyType* const Field = FindFProperty<PropertyType>(It->Struct, Name))
			{
				OutContainer = It->ContainerPtrToValuePtr<void>(Object);
				return Field;
			}
		}
		return nullptr;
	}

	FString CleanName(const UObject* Object)
	{
		FString Name = GetNameSafe(Object);
		Name.RemoveFromEnd(TEXT("_C"));
		return Name;
	}

	UObject* GetBlueprintOrClass(UClass* Class)
	{
		UBlueprint* const Blueprint = UBlueprint::GetBlueprintFromClass(Class);
		return Blueprint ? static_cast<UObject*>(Blueprint) : Class;
	}

	TArray<UClass*> FindMonsterClasses(FString& OutNotice)
	{
		TArray<UClass*> MonsterClasses;
		UClass* const TrackedClass = GetDefault<UknightAnimDebugSettings>()->TrackedPawnClass.LoadSynchronous();
		if (!TrackedClass)
		{
			OutNotice = TEXT("프로젝트 설정 > Knight > Monster Anim Debug의 TrackedPawnClass가 비었거나 찾을 수 없다.");
			return MonsterClasses;
		}

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		if (AssetRegistry.IsLoadingAssets())
		{
			OutNotice = TEXT("에셋 레지스트리가 아직 에셋을 읽는 중이라 일부 몬스터가 빠질 수 있다. 잠시 뒤 새로고침한다.");
		}

		TSet<FTopLevelAssetPath> DerivedNames;
		AssetRegistry.GetDerivedClassNames({ TrackedClass->GetClassPathName() }, {}, DerivedNames);
		for (const FTopLevelAssetPath& Path : DerivedNames)
		{
			const FString AssetName = Path.GetAssetName().ToString();
			if (AssetName.StartsWith(TEXT("SKEL_")) || AssetName.StartsWith(TEXT("REINST_")))
			{
				continue;
			}
			UClass* const Class = LoadObject<UClass>(nullptr, *Path.ToString());
			if (Class && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				MonsterClasses.AddUnique(Class);
			}
		}
		MonsterClasses.Sort([](const UClass& A, const UClass& B) { return A.GetName() < B.GetName(); });

		if (MonsterClasses.IsEmpty() && OutNotice.IsEmpty())
		{
			OutNotice = FString::Printf(TEXT("%s를 상속한 몬스터 Blueprint가 없다."), *TrackedClass->GetName());
		}
		return MonsterClasses;
	}

	void FindPlayableAbilities(UClass* MonsterClass, TArray<FknightAbilityAnimInfo>& OutAbilities)
	{
		const UknightAnimDebugSettings* const Settings = GetDefault<UknightAnimDebugSettings>();
		TArray<UClass*> PlayableClasses;
		for (const TSoftClassPtr<UGameplayAbility>& SoftClass : Settings->PlayableAbilityClasses)
		{
			if (UClass* const Class = SoftClass.LoadSynchronous())
			{
				PlayableClasses.Add(Class);
			}
		}
		const UClass* const AttackClass = Settings->AttackAbilityClass.LoadSynchronous();

		TArray<UClass*> AbilityClasses;
		TSet<const UObject*> Visited;
		CollectAbilityClasses(MonsterClass ? MonsterClass->GetDefaultObject() : nullptr, 0, Visited, AbilityClasses);

		for (UClass* const AbilityClass : AbilityClasses)
		{
			UClass* const* const Base = PlayableClasses.FindByPredicate([AbilityClass](const UClass* Class) { return AbilityClass->IsChildOf(Class); });
			if (!PlayableClasses.IsEmpty() && !Base)
			{
				continue;
			}

			FknightAbilityAnimInfo& Info = OutAbilities.AddDefaulted_GetRef();
			Info.AbilityClass = AbilityClass;
			Info.AbilityCDO = AbilityClass->GetDefaultObject<UGameplayAbility>();
			Info.Name = CleanName(AbilityClass);
			Info.Kind = Base ? KindName(*Base) : FString();
			Info.bIsAttack = AttackClass && AbilityClass->IsChildOf(AttackClass);

			// 타이밍: 런타임 디버거와 같은 설정 프로퍼티를 읽는다.
			for (const FknightAnimDebugPhaseProperty& PhaseProperty : Settings->TimelinePhases)
			{
				if (const FFloatProperty* const Property = FindFProperty<FFloatProperty>(AbilityClass, PhaseProperty.PropertyName))
				{
					FknightAnimDebugPhase& Phase = Info.Phases.AddDefaulted_GetRef();
					Phase.Label = PhaseProperty.Label;
					Phase.Duration = Property->GetPropertyValue_InContainer(Info.AbilityCDO);
					Phase.Color = PhaseProperty.Color;
					Phase.bHitAtEnd = PhaseProperty.bHitAtEnd;
					Info.PhaseTotal += Phase.Duration;
					if (Phase.bHitAtEnd && Info.HitTime < 0.f)
					{
						Info.HitTime = Info.PhaseTotal;
						Info.HitEndTime = Info.PhaseTotal;
						Info.HitPhaseProperty = Property;
					}
					else if (PhaseProperty.bHitWindow && Info.HitTime >= 0.f)
					{
						Info.HitEndTime = Info.PhaseTotal;
					}
				}
			}

			const FStructProperty* const OffsetProperty = FindFProperty<FStructProperty>(AbilityClass, Settings->AreaOffsetProperty);
			const FFloatProperty* const RadiusProperty = FindFProperty<FFloatProperty>(AbilityClass, Settings->AreaRadiusProperty);
			if (OffsetProperty && RadiusProperty && OffsetProperty->Struct == TBaseStructure<FVector>::Get())
			{
				Info.bHasArea = true;
				Info.AreaOffset = *OffsetProperty->ContainerPtrToValuePtr<FVector>(Info.AbilityCDO);
				Info.AreaRadius = RadiusProperty->GetPropertyValue_InContainer(Info.AbilityCDO);
			}

			for (TFieldIterator<FObjectProperty> It(AbilityClass); It; ++It)
			{
				// 편집 가능한 프로퍼티만 본다. UGameplayAbility::CurrentMontage 같은 실행 중 상태는 슬롯이 아니다.
				if (It->HasAnyPropertyFlags(CPF_Edit) && It->PropertyClass && It->PropertyClass->IsChildOf(UAnimMontage::StaticClass()))
				{
					Info.MontageProperties.Add(*It);
				}
			}
		}
	}

	FknightMonsterMeshInfo GetMonsterMeshInfo(UClass* MonsterClass)
	{
		FknightMonsterMeshInfo Info;
		const ACharacter* const Character = MonsterClass ? Cast<ACharacter>(MonsterClass->GetDefaultObject()) : nullptr;
		USkeletalMeshComponent* const MeshComponent = Character ? Character->GetMesh() : nullptr;
		if (!MeshComponent)
		{
			return Info;
		}
		Info.Mesh = MeshComponent->GetSkeletalMeshAsset();
		Info.AnimClass = MeshComponent->GetAnimClass();
		Info.MeshRelativeTransform = MeshComponent->GetRelativeTransform();
		Info.CapsuleHalfHeight = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 0.f;
		return Info;
	}

	void GetHitWindows(const UAnimMontage* Montage, TArray<FknightHitWindow>& OutWindows)
	{
		if (!Montage)
		{
			return;
		}
		const UknightAnimDebugSettings* const Settings = GetDefault<UknightAnimDebugSettings>();
		for (const FAnimNotifyEvent& Notify : Montage->Notifies)
		{
			const FString Name = UknightAnimDebugSubsystem::GetNotifyStateName(Notify);
			if (!Notify.NotifyStateClass || !UknightAnimDebugSubsystem::IsHitWindowNotify(Name))
			{
				continue;
			}
			FknightHitWindow& Window = OutWindows.AddDefaulted_GetRef();
			Window.Name = Name;
			Window.Start = Notify.GetTriggerTime();
			Window.End = Window.Start + Notify.GetDuration();

			const void* Container = nullptr;
			if (const FNameProperty* const StartProperty = FindNested<FNameProperty>(Notify.NotifyStateClass, Settings->TraceStartSocketProperty, Container))
			{
				Window.StartSocket = StartProperty->GetPropertyValue_InContainer(Container);
			}
			if (const FNameProperty* const EndProperty = FindNested<FNameProperty>(Notify.NotifyStateClass, Settings->TraceEndSocketProperty, Container))
			{
				Window.EndSocket = EndProperty->GetPropertyValue_InContainer(Container);
			}
			if (const FFloatProperty* const RadiusProperty = FindNested<FFloatProperty>(Notify.NotifyStateClass, Settings->TraceRadiusProperty, Container))
			{
				Window.Radius = RadiusProperty->GetPropertyValue_InContainer(Container);
			}
		}
		OutWindows.Sort([](const FknightHitWindow& A, const FknightHitWindow& B) { return A.Start < B.Start; });
	}

	float GetMontageRate(const UAnimMontage* Montage)
	{
		return Montage && Montage->RateScale > UE_KINDA_SMALL_NUMBER ? Montage->RateScale : 1.f;
	}

	bool GetPlayRateNotify(UClass*& OutClass, const FFloatProperty*& OutRateProperty)
	{
		const UknightAnimDebugSettings* const Settings = GetDefault<UknightAnimDebugSettings>();
		OutClass = Settings->PlayRateNotifyClass.LoadSynchronous();
		OutRateProperty = OutClass ? FindFProperty<FFloatProperty>(OutClass, Settings->PlayRateProperty) : nullptr;
		return OutRateProperty != nullptr;
	}

	void GetRateWindows(const UAnimMontage* Montage, TArray<FknightRateWindow>& OutWindows)
	{
		UClass* RateClass = nullptr;
		const FFloatProperty* RateProperty = nullptr;
		if (!Montage || !GetPlayRateNotify(RateClass, RateProperty))
		{
			return;
		}
		for (const FAnimNotifyEvent& Notify : Montage->Notifies)
		{
			if (Notify.NotifyStateClass && Notify.NotifyStateClass->IsA(RateClass))
			{
				FknightRateWindow& Window = OutWindows.AddDefaulted_GetRef();
				Window.Guid = Notify.Guid;
				Window.Start = Notify.GetTriggerTime();
				Window.End = Window.Start + Notify.GetDuration();
				Window.Rate = RateProperty->GetPropertyValue_InContainer(Notify.NotifyStateClass);
			}
		}
		OutWindows.Sort([](const FknightRateWindow& A, const FknightRateWindow& B) { return A.Start < B.Start; });
	}

	float GetFirstHitMarkerTime(const UAnimMontage* Montage)
	{
		const UknightAnimDebugSettings* const Settings = GetDefault<UknightAnimDebugSettings>();
		float First = -1.f;
		if (!Montage)
		{
			return First;
		}
		for (const FAnimNotifyEvent& Notify : Montage->Notifies)
		{
			const FString ClassName = Notify.Notify ? Notify.Notify->GetClass()->GetName() : FString();
			const bool bMarker = !ClassName.IsEmpty() && Settings->HitMarkerNotifyKeywords.ContainsByPredicate([&ClassName](const FString& Keyword) { return ClassName.Contains(Keyword); });
			if (bMarker && (First < 0.f || Notify.GetTriggerTime() < First))
			{
				First = Notify.GetTriggerTime();
			}
		}
		return First;
	}

	FknightMontageTimeMap MakeTimeMap(const UAnimMontage* Montage)
	{
		TArray<FknightRateWindow> Windows;
		GetRateWindows(Montage, Windows);
		FknightMontageTimeMap Map;
		Map.Build(Montage, Windows);
		return Map;
	}

	void GetAnimBlueprintSlots(const UClass* AnimClass, TSet<FName>& OutSlots)
	{
		const UObject* const AnimCDO = AnimClass ? AnimClass->GetDefaultObject() : nullptr;
		if (!AnimCDO)
		{
			return;
		}
		// AnimGraphRuntime에 의존하지 않도록 구조체 이름으로 Slot 노드를 찾는다.
		static const FName SlotNodeName(TEXT("AnimNode_Slot"));
		static const FName SlotNameProperty(TEXT("SlotName"));
		for (TFieldIterator<FStructProperty> It(AnimClass); It; ++It)
		{
			for (const UStruct* Struct = It->Struct; Struct; Struct = Struct->GetSuperStruct())
			{
				if (Struct->GetFName() == SlotNodeName)
				{
					if (const FNameProperty* const NameProperty = FindFProperty<FNameProperty>(It->Struct, SlotNameProperty))
					{
						OutSlots.Add(NameProperty->GetPropertyValue_InContainer(It->ContainerPtrToValuePtr<void>(AnimCDO)));
					}
					break;
				}
			}
		}
	}
}

UAnimMontage* FknightAbilityAnimInfo::GetMontage(const FObjectProperty* Property) const
{
	return Property && AbilityCDO ? Cast<UAnimMontage>(Property->GetObjectPropertyValue_InContainer(AbilityCDO)) : nullptr;
}

bool FknightAbilityAnimInfo::IsOptionalSlot(const FObjectProperty* Property) const
{
	return Property && GetDefault<UknightAnimDebugSettings>()->OptionalMontageProperties.Contains(Property->GetFName());
}

const FObjectProperty* FknightAbilityAnimInfo::GetPrimarySlot() const
{
	for (const FObjectProperty* const Property : MontageProperties)
	{
		if (!IsOptionalSlot(Property))
		{
			return Property;
		}
	}
	return nullptr;
}

UAnimMontage* FknightAbilityAnimInfo::GetPrimaryMontage() const
{
	return GetMontage(GetPrimarySlot());
}

FString FknightAbilityAnimInfo::GetTimingText() const
{
	FString Text;
	for (const FknightAnimDebugPhase& Phase : Phases)
	{
		Text += FString::Printf(TEXT("%s%s %.2f"), Text.IsEmpty() ? TEXT("") : TEXT(" / "), *Phase.Label, Phase.Duration);
	}
	return Text;
}

void FknightMontageTimeMap::Build(const UAnimMontage* Montage, const TArray<FknightRateWindow>& RateWindows)
{
	Segments.Reset();
	RealLength = 0.f;
	const float BaseRate = knightAnimAbilityInfo::GetMontageRate(Montage);
	const float MontageLength = Montage ? Montage->GetPlayLength() : 0.f;

	// 구간 경계마다 끊어, 각 조각 가운데를 덮는 구간 배율을 모두 곱한다.
	TArray<float> Bounds = { 0.f, MontageLength };
	for (const FknightRateWindow& Window : RateWindows)
	{
		Bounds.Add(FMath::Clamp(Window.Start, 0.f, MontageLength));
		Bounds.Add(FMath::Clamp(Window.End, 0.f, MontageLength));
	}
	Bounds.Sort();
	for (int32 Index = 0; Index + 1 < Bounds.Num(); ++Index)
	{
		const float Start = Bounds[Index];
		const float End = Bounds[Index + 1];
		if (End - Start <= UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const float Middle = (Start + End) * 0.5f;
		float Rate = BaseRate;
		for (const FknightRateWindow& Window : RateWindows)
		{
			if (Window.Start <= Middle && Middle < Window.End && Window.Rate > UE_KINDA_SMALL_NUMBER)
			{
				Rate *= Window.Rate;
			}
		}
		Segments.Add({ Start, RealLength, Rate });
		RealLength += (End - Start) / Rate;
	}
	if (Segments.IsEmpty())
	{
		Segments.Add({ 0.f, 0.f, BaseRate });
	}
}

float FknightMontageTimeMap::ToReal(float MontageTime) const
{
	if (Segments.IsEmpty())
	{
		return MontageTime;
	}
	const FSegment* Segment = &Segments[0];
	for (const FSegment& Candidate : Segments)
	{
		if (Candidate.MontageStart > MontageTime)
		{
			break;
		}
		Segment = &Candidate;
	}
	return Segment->RealStart + (MontageTime - Segment->MontageStart) / Segment->Rate;
}

float FknightMontageTimeMap::ToMontage(float RealTime) const
{
	if (Segments.IsEmpty())
	{
		return RealTime;
	}
	const FSegment* Segment = &Segments[0];
	for (const FSegment& Candidate : Segments)
	{
		if (Candidate.RealStart > RealTime)
		{
			break;
		}
		Segment = &Candidate;
	}
	return Segment->MontageStart + (RealTime - Segment->RealStart) * Segment->Rate;
}
