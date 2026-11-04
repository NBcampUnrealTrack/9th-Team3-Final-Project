#include "knightMontageEditLibrary.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"

bool UknightMontageEditLibrary::SetMontageAnimation(UAnimMontage* Montage, UAnimSequenceBase* Animation)
{
	if (!Montage || !Animation || Montage->SlotAnimTracks.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("SetMontageAnimation: 몽타주(슬롯 트랙 1개 이상)와 애니가 필요합니다."));
		return false;
	}
	Montage->Modify();
	// 섹션·노티파이는 시간과 함께 '그 시간에 재생되는 애니'를 기억한다. 애니를 바꾸기 전에 절대 시간을 적어 두었다가
	// 새 애니에 다시 이어 준다(그대로 두면 옛 애니를 계속 참조한다).
	TArray<float> SectionTimes;
	for (const FCompositeSection& Section : Montage->CompositeSections)
	{
		SectionTimes.Add(Section.GetTime());
	}
	TArray<TPair<float, float>> NotifyTimes;
	for (const FAnimNotifyEvent& Notify : Montage->Notifies)
	{
		NotifyTimes.Emplace(Notify.GetTime(), Notify.GetDuration());
	}

	// 구간을 새로 만들어(처음~끝, 배속 1, 1회) 첫 슬롯 트랙의 구간을 모두 바꾼다.
	FAnimSegment Segment;
	Segment.SetAnimReference(Animation, true);
	Segment.StartPos = 0.f;
	FAnimTrack& Track = Montage->SlotAnimTracks[0].AnimTrack;
	Track.AnimSegments.Reset();
	Track.AnimSegments.Add(Segment);
	Montage->SetCompositeLength(Montage->CalculateSequenceLength());

	for (int32 Index = 0; Index < Montage->CompositeSections.Num(); ++Index)
	{
		Montage->CompositeSections[Index].Link(Montage, SectionTimes[Index]);
	}
	for (int32 Index = 0; Index < Montage->Notifies.Num(); ++Index)
	{
		FAnimNotifyEvent& Notify = Montage->Notifies[Index];
		const float Start = NotifyTimes[Index].Key;
		const float Duration = NotifyTimes[Index].Value;
		Notify.Link(Montage, Start);
		if (Duration > 0.f)
		{
			Notify.EndLink.Link(Montage, Start + Duration);
		}
	}
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return true;
}

FString UknightMontageEditLibrary::DumpObjectProperties(UObject* Object)
{
	FString Out;
	if (!Object)
	{
		return Out;
	}
	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		const FProperty* const Property = *It;
		if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
		{
			continue;
		}
		for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
		{
			FString Value;
			Property->ExportText_InContainer(Index, Value, Object, nullptr, Object, PPF_None);
			const FString Name = Property->ArrayDim > 1 ? FString::Printf(TEXT("%s[%d]"), *Property->GetName(), Index) : Property->GetName();
			// 값 안의 줄바꿈은 한 줄로(이름=값 한 줄 형식 유지)
			Out += Name + TEXT("=") + Value.Replace(TEXT("\n"), TEXT("\\n")).Replace(TEXT("\r"), TEXT("")) + TEXT("\n");
		}
	}
	return Out;
}

bool UknightMontageEditLibrary::ImportObjectProperty(UObject* Object, const FString& PropertyName, const FString& ValueText)
{
	if (!Object)
	{
		return false;
	}
	FString Name = PropertyName;
	int32 Index = 0;
	int32 Bracket = INDEX_NONE;
	if (Name.FindChar(TEXT('['), Bracket) && Name.EndsWith(TEXT("]")))
	{
		Index = FCString::Atoi(*Name.Mid(Bracket + 1, Name.Len() - Bracket - 2));
		Name.LeftInline(Bracket);
	}
	FProperty* const Property = Object->GetClass()->FindPropertyByName(FName(*Name));
	if (!Property || Index < 0 || Index >= Property->ArrayDim)
	{
		UE_LOG(LogTemp, Error, TEXT("ImportObjectProperty: %s에 칸 %s가 없습니다."), *GetNameSafe(Object), *PropertyName);
		return false;
	}
	Object->Modify();
	void* const ValuePtr = Property->ContainerPtrToValuePtr<void>(Object, Index);
	if (!Property->ImportText_Direct(*ValueText, ValuePtr, Object, PPF_None))
	{
		UE_LOG(LogTemp, Error, TEXT("ImportObjectProperty: %s.%s 값을 읽지 못했습니다: %s"), *GetNameSafe(Object), *PropertyName, *ValueText);
		return false;
	}
	FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
	Object->PostEditChangeProperty(Event);
	Object->MarkPackageDirty();
	return true;
}
