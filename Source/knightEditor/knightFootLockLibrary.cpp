#include "knightFootLockLibrary.h"

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/knightAnimNode_LegPlacement.h"

#define LOCTEXT_NAMESPACE "knightFootLockLibrary"

namespace knightFootLock
{
	/** 기준 포즈 또는 애니메이션 키로 뼈의 컴포넌트 공간 위치를 구한다(루트까지 부모를 곱한다). */
	FVector ComponentLocation(const FReferenceSkeleton& RefSkeleton, const IAnimationDataModel* Model, int32 BoneIndex, int32 Key)
	{
		FTransform Result = FTransform::Identity;
		for (int32 Index = BoneIndex; Index != INDEX_NONE; Index = RefSkeleton.GetParentIndex(Index))
		{
			const FName BoneName = RefSkeleton.GetBoneName(Index);
			// 트랙이 없는 뼈는 기준 포즈로 둔다(런타임 포즈와 같다).
			const FTransform Local = Model && Model->IsValidBoneTrackName(BoneName)
				? Model->GetBoneTrackTransform(BoneName, FFrameNumber(Key))
				: RefSkeleton.GetRefBonePose()[Index];
			Result = Result * Local;
		}
		return Result.GetLocation();
	}

	/** Value가 바뀌지 않고 이어지는 구간 중 MinKeys보다 짧은 것을 Target 값으로 덮는다. 처음과 끝 구간은 그대로 둔다. */
	void FillShortRuns(TArray<bool>& States, bool Target, int32 MinKeys)
	{
		int32 Start = 0;
		while (Start < States.Num())
		{
			int32 End = Start;
			while (End + 1 < States.Num() && States[End + 1] == States[Start])
			{
				++End;
			}
			const bool bInterior = Start > 0 && End < States.Num() - 1;
			if (States[Start] != Target && bInterior && End - Start + 1 < MinKeys)
			{
				for (int32 Index = Start; Index <= End; ++Index)
				{
					States[Index] = Target;
				}
			}
			Start = End + 1;
		}
	}
}

FName UknightFootLockLibrary::GetFootLockCurveName(FName FootBone)
{
	return FAnimNode_knightLegPlacement::MakeFootLockCurveName(FootBone);
}

int32 UknightFootLockLibrary::BakeFootLockCurves(UAnimSequence* Sequence, const TArray<FName>& FootBones, float HeightToleranceRatio, float MinPhaseTime)
{
	const USkeleton* const Skeleton = Sequence ? Sequence->GetSkeleton() : nullptr;
	if (!Skeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("BakeFootLockCurves: 시퀀스나 스켈레톤이 없습니다."));
		return 0;
	}
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const IAnimationDataModel* const Model = Sequence->GetDataModel();
	const int32 NumKeys = Model->GetNumberOfKeys();
	const FFrameRate FrameRate = Model->GetFrameRate();
	if (NumKeys < 2)
	{
		UE_LOG(LogTemp, Error, TEXT("BakeFootLockCurves %s: 키가 2개 미만입니다."), *Sequence->GetName());
		return 0;
	}
	const int32 MinKeys = FMath::Max(1, FMath::RoundToInt(MinPhaseTime * FrameRate.AsDecimal()));

	IAnimationDataController& Controller = Sequence->GetController();
	Controller.OpenBracket(LOCTEXT("BakeFootLock", "접지 커브 굽기"), false);
	int32 Written = 0;
	for (const FName FootBone : FootBones)
	{
		const int32 Foot = RefSkeleton.FindBoneIndex(FootBone);
		const int32 Joint = Foot != INDEX_NONE ? RefSkeleton.GetParentIndex(Foot) : INDEX_NONE;
		const int32 Hip = Joint != INDEX_NONE ? RefSkeleton.GetParentIndex(Joint) : INDEX_NONE;
		if (Hip == INDEX_NONE)
		{
			UE_LOG(LogTemp, Error, TEXT("BakeFootLockCurves %s: %s로 2본 다리 체인을 만들 수 없습니다."), *Sequence->GetName(), *FootBone.ToString());
			continue;
		}
		// 노드의 IK 체인(발 ~ 조부모)과 같은 길이.
		const double LegLength = (knightFootLock::ComponentLocation(RefSkeleton, nullptr, Foot, 0)
			- knightFootLock::ComponentLocation(RefSkeleton, nullptr, Hip, 0)).Size();
		const double Tolerance = LegLength * HeightToleranceRatio;

		TArray<double> Heights;
		Heights.Reserve(NumKeys);
		double Ground = TNumericLimits<double>::Max();
		for (int32 Key = 0; Key < NumKeys; ++Key)
		{
			Heights.Add(knightFootLock::ComponentLocation(RefSkeleton, Model, Foot, Key).Z);
			Ground = FMath::Min(Ground, Heights.Last());
		}

		// 디딜 때는 Tolerance, 뗄 때는 1.5배. 경계에서 값이 떨리지 않게 한다.
		TArray<bool> Planted;
		Planted.SetNum(NumKeys);
		bool bState = Heights[0] <= Ground + Tolerance;
		for (int32 Key = 0; Key < NumKeys; ++Key)
		{
			const double Above = Heights[Key] - Ground;
			bState = bState ? Above <= Tolerance * 1.5 : Above <= Tolerance;
			Planted[Key] = bState;
		}
		// 짧게 뜬 틈은 디딤으로, 그다음 짧은 디딤은 들기로 합친다.
		knightFootLock::FillShortRuns(Planted, true, MinKeys);
		knightFootLock::FillShortRuns(Planted, false, MinKeys);

		TArray<FRichCurveKey> Keys;
		int32 PlantedKeys = 0;
		for (int32 Key = 0; Key < NumKeys; ++Key)
		{
			PlantedKeys += Planted[Key] ? 1 : 0;
			if (Key == 0 || Planted[Key] != Planted[Key - 1])
			{
				FRichCurveKey& CurveKey = Keys.Add_GetRef(FRichCurveKey(static_cast<float>(FrameRate.AsSeconds(FFrameNumber(Key))), Planted[Key] ? 1.f : 0.f));
				CurveKey.InterpMode = RCIM_Constant;
			}
		}

		const FAnimationCurveIdentifier CurveId(GetFootLockCurveName(FootBone), ERawCurveTrackTypes::RCT_Float);
		if (Model->FindFloatCurve(CurveId))
		{
			Controller.RemoveCurve(CurveId, false);
		}
		Controller.AddCurve(CurveId, AACF_Editable, false);
		Controller.SetCurveKeys(CurveId, Keys, false);
		++Written;

		UE_LOG(LogTemp, Display, TEXT("BakeFootLockCurves %s %s: 다리 %.1f, 땅 %.1f, 여유 %.2f, 디딤 %d%%, 바뀜 %d번"),
			*Sequence->GetName(), *FootBone.ToString(), LegLength, Ground, Tolerance, PlantedKeys * 100 / NumKeys, Keys.Num() - 1);
	}
	Controller.CloseBracket(false);

	if (Written > 0)
	{
		Sequence->PostEditChange();
		Sequence->MarkPackageDirty();
	}
	return Written;
}

#undef LOCTEXT_NAMESPACE
