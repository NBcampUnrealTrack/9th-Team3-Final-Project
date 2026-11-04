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
