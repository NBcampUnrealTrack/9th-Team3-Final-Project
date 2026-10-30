#include "knightRootMotionLibrary.h"

#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimData/CurveIdentifier.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"

#define LOCTEXT_NAMESPACE "knightRootMotionLibrary"

UAnimSequence* UknightRootMotionLibrary::BakeRootMotionFromBone(UAnimSequence* Source, FName Bone, float HoldAfterTime, const FString& PackagePath)
{
	const USkeleton* const Skeleton = Source ? Source->GetSkeleton() : nullptr;
	if (!Skeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("BakeRootMotionFromBone %s: 시퀀스나 스켈레톤이 없습니다."), *PackagePath);
		return nullptr;
	}
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
	if (BoneIndex == INDEX_NONE || RefSkeleton.GetParentIndex(BoneIndex) != 0)
	{
		// 루트 바로 아래 뼈여야 루트를 옮긴 만큼 그 뼈 하나만 되돌려서 나머지 몸이 그대로 남는다.
		UE_LOG(LogTemp, Error, TEXT("BakeRootMotionFromBone %s: %s는 루트의 바로 아래 뼈여야 합니다."), *PackagePath, *Bone.ToString());
		return nullptr;
	}
	const FName RootName = RefSkeleton.GetBoneName(0);

	UAnimSequence* Target = LoadObject<UAnimSequence>(nullptr, *PackagePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Target)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		Target = Cast<UAnimSequence>(AssetTools.DuplicateAsset(FPackageName::GetShortName(PackagePath), FPackageName::GetLongPackagePath(PackagePath), Source));
		if (!Target)
		{
			UE_LOG(LogTemp, Error, TEXT("BakeRootMotionFromBone %s: 복제에 실패했습니다."), *PackagePath);
			return nullptr;
		}
	}
	Target->Modify();

	// 원본 키를 그대로 읽는다(압축 데이터가 아님). 트랙이 없는 뼈는 기준 포즈다.
	const IAnimationDataModel* const SourceModel = Source->GetDataModel();
	const int32 NumKeys = SourceModel->GetNumberOfKeys();
	const FFrameRate FrameRate = SourceModel->GetFrameRate();
	auto ReadLocal = [&](FName Name, int32 Index, int32 Key)
	{
		return SourceModel->IsValidBoneTrackName(Name) ? SourceModel->GetBoneTrackTransform(Name, FFrameNumber(Key)) : RefSkeleton.GetRefBonePose()[Index];
	};

	const int32 HoldKey = HoldAfterTime < 0.f ? NumKeys - 1
		: FMath::Clamp(FrameRate.AsFrameTime(HoldAfterTime).RoundToFrame().Value, 0, NumKeys - 1);
	const FVector BoneStart = (ReadLocal(Bone, BoneIndex, 0) * ReadLocal(RootName, 0, 0)).GetLocation();
	FVector RootOffset = FVector::ZeroVector;

	TArray<FVector> RootPositions, BonePositions, RootScales, BoneScales;
	TArray<FQuat> RootRotations, BoneRotations;
	for (int32 Key = 0; Key < NumKeys; ++Key)
	{
		const FTransform RootLocal = ReadLocal(RootName, 0, Key);
		const FTransform BoneComponent = ReadLocal(Bone, BoneIndex, Key) * RootLocal;
		if (Key <= HoldKey)
		{
			// 몸이 움직인 수평 거리만 루트로 옮긴다. 높이는 몸에 남겨 루트가 땅을 벗어나지 않게 한다.
			RootOffset = BoneComponent.GetLocation() - BoneStart;
			RootOffset.Z = 0.0;
		}
		FTransform NewRoot = RootLocal;
		NewRoot.AddToTranslation(RootOffset);

		// 뼈의 컴포넌트 공간 위치: 수평은 "새 루트 + 첫 프레임 위치"(HoldKey 뒤의 되돌아오기는 버림), 높이·회전은 원본.
		FTransform NewBoneComponent = BoneComponent;
		NewBoneComponent.SetLocation(FVector(BoneStart.X + RootOffset.X, BoneStart.Y + RootOffset.Y, BoneComponent.GetLocation().Z));
		const FTransform NewBoneLocal = NewBoneComponent.GetRelativeTransform(NewRoot);

		RootPositions.Add(NewRoot.GetLocation());
		RootRotations.Add(NewRoot.GetRotation());
		RootScales.Add(NewRoot.GetScale3D());
		BonePositions.Add(NewBoneLocal.GetLocation());
		BoneRotations.Add(NewBoneLocal.GetRotation());
		BoneScales.Add(NewBoneLocal.GetScale3D());
	}

	IAnimationDataController& Controller = Target->GetController();
	Controller.OpenBracket(LOCTEXT("BakeRootMotion", "루트 모션 굽기"), false);
	for (const FName Name : { RootName, Bone })
	{
		if (!Target->GetDataModel()->IsValidBoneTrackName(Name))
		{
			Controller.AddBoneCurve(Name, false);
		}
	}
	Controller.SetBoneTrackKeys(RootName, RootPositions, RootRotations, RootScales, false);
	Controller.SetBoneTrackKeys(Bone, BonePositions, BoneRotations, BoneScales, false);
	Controller.CloseBracket(false);

	Target->bEnableRootMotion = true;
	// 루트를 애니메이션 첫 프레임에 고정한다. 기준 포즈(RefPose)로 고정하면 원본 루트 트랙이 기준 포즈와 다를 때
	// 루트 모션이 켜지는 순간 몸 전체가 그 차이만큼 밀린다.
	Target->RootMotionRootLock = ERootMotionRootLock::AnimFirstFrame;
	Target->PostEditChange();
	Target->MarkPackageDirty();

	UE_LOG(LogTemp, Display, TEXT("BakeRootMotionFromBone %s: 루트 이동 (%.1f, %.1f), %d번 키부터 머묾"), *PackagePath, RootOffset.X, RootOffset.Y, HoldKey);
	return Target;
}

UAnimSequence* UknightRootMotionLibrary::BakeRootMotionFromFeet(UAnimSequence* Source, const TArray<FName>& FootBones, int32 SmoothKeys, const FString& PackagePath, bool bLoop)
{
	const USkeleton* const Skeleton = Source ? Source->GetSkeleton() : nullptr;
	if (!Skeleton || FootBones.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("BakeRootMotionFromFeet %s: 시퀀스, 스켈레톤이나 발 뼈가 없습니다."), *PackagePath);
		return nullptr;
	}
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const FName RootName = RefSkeleton.GetBoneName(0);
	const IAnimationDataModel* const SourceModel = Source->GetDataModel();
	const int32 NumKeys = SourceModel->GetNumberOfKeys();
	const FFrameRate FrameRate = SourceModel->GetFrameRate();
	if (NumKeys < 2)
	{
		return nullptr;
	}
	auto ReadLocal = [&](int32 Index, int32 Key)
	{
		const FName Name = RefSkeleton.GetBoneName(Index);
		return SourceModel->IsValidBoneTrackName(Name) ? SourceModel->GetBoneTrackTransform(Name, FFrameNumber(Key)) : RefSkeleton.GetRefBonePose()[Index];
	};
	// 루트 기준이 아니라 컴포넌트 공간 위치(루트 트랙 포함). 제자리 애니메이션이라 루트는 보통 움직이지 않는다.
	auto ComponentLocation = [&](int32 Index, int32 Key)
	{
		FTransform Result = ReadLocal(Index, Key);
		for (int32 Parent = RefSkeleton.GetParentIndex(Index); Parent != INDEX_NONE; Parent = RefSkeleton.GetParentIndex(Parent))
		{
			Result = Result * ReadLocal(Parent, Key);
		}
		return Result.GetLocation();
	};

	struct FFoot
	{
		int32 Index = INDEX_NONE;
		const FFloatCurve* Curve = nullptr;
	};
	TArray<FFoot> Feet;
	for (const FName FootBone : FootBones)
	{
		FFoot Foot;
		Foot.Index = RefSkeleton.FindBoneIndex(FootBone);
		Foot.Curve = SourceModel->FindFloatCurve(FAnimationCurveIdentifier(FName(*FString::Printf(TEXT("FootLock_%s"), *FootBone.ToString())), ERawCurveTrackTypes::RCT_Float));
		if (Foot.Index == INDEX_NONE || !Foot.Curve)
		{
			UE_LOG(LogTemp, Error, TEXT("BakeRootMotionFromFeet %s: %s 뼈나 FootLock 커브가 없습니다."), *PackagePath, *FootBone.ToString());
			return nullptr;
		}
		Feet.Add(Foot);
	}

	// 프레임 사이 루트 이동량(디딘 발 평균 이동의 반대). Steps[Key]는 Key-1 -> Key 구간.
	TArray<FVector> Steps;
	TArray<bool> bPlanted;
	Steps.SetNumZeroed(NumKeys);
	bPlanted.SetNumZeroed(NumKeys);
	for (int32 Key = 1; Key < NumKeys; ++Key)
	{
		const float Time0 = FrameRate.AsSeconds(FFrameTime(Key - 1));
		const float Time1 = FrameRate.AsSeconds(FFrameTime(Key));
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (const FFoot& Foot : Feet)
		{
			if (Foot.Curve->Evaluate(Time0) >= 0.5f && Foot.Curve->Evaluate(Time1) >= 0.5f)
			{
				Sum += ComponentLocation(Foot.Index, Key) - ComponentLocation(Foot.Index, Key - 1);
				++Count;
			}
		}
		if (Count > 0)
		{
			Steps[Key] = -Sum / Count;
			Steps[Key].Z = 0.0;
			bPlanted[Key] = true;
		}
	}
	// 구간 번호(1 ~ NumKeys-1)를 반복 애니면 끝에서 처음으로 감아 읽는다. 마지막 키와 첫 키가 같은 자세라 구간 수는 NumKeys-1.
	const int32 NumSteps = NumKeys - 1;
	auto StepIndex = [&](int32 Index)
	{
		return bLoop ? 1 + ((Index - 1) % NumSteps + NumSteps) % NumSteps : FMath::Clamp(Index, 1, NumKeys - 1);
	};
	// 디딘 발이 없는 구간은 앞 구간 속도를 이어 쓴다. 반복 애니면 첫 구간도 끝 쪽에서 이어 온다.
	for (int32 Key = 1; Key < NumKeys; ++Key)
	{
		if (bPlanted[Key])
		{
			continue;
		}
		for (int32 Back = 1; Back < NumSteps; ++Back)
		{
			const int32 Other = Key - Back;
			if (!bLoop && Other < 1)
			{
				break;
			}
			const int32 Index = StepIndex(Other);
			if (bPlanted[Index])
			{
				Steps[Key] = Steps[Index];
				break;
			}
		}
	}
	// 첫 프레임은 디딘 발이 있는 다음 값으로 채운다(앞 프레임이 없어서 0으로 남는다).
	Steps[0] = Steps[1];
	TArray<FVector> Smoothed = Steps;
	for (int32 Key = 1; Key < NumKeys; ++Key)
	{
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (int32 Offset = -SmoothKeys; Offset <= SmoothKeys; ++Offset)
		{
			const int32 Other = Key + Offset;
			if (bLoop || (Other >= 1 && Other < NumKeys))
			{
				Sum += Steps[StepIndex(Other)];
				++Count;
			}
		}
		Smoothed[Key] = Sum / FMath::Max(1, Count);
	}

	UAnimSequence* Target = LoadObject<UAnimSequence>(nullptr, *PackagePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Target)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		Target = Cast<UAnimSequence>(AssetTools.DuplicateAsset(FPackageName::GetShortName(PackagePath), FPackageName::GetLongPackagePath(PackagePath), Source));
		if (!Target)
		{
			UE_LOG(LogTemp, Error, TEXT("BakeRootMotionFromFeet %s: 복제에 실패했습니다."), *PackagePath);
			return nullptr;
		}
	}
	Target->Modify();

	TArray<FVector> RootPositions;
	TArray<FQuat> RootRotations;
	TArray<FVector> RootScales;
	FVector Offset = FVector::ZeroVector;
	for (int32 Key = 0; Key < NumKeys; ++Key)
	{
		if (Key > 0)
		{
			Offset += Smoothed[Key];
		}
		FTransform NewRoot = ReadLocal(0, Key);
		NewRoot.AddToTranslation(Offset);
		RootPositions.Add(NewRoot.GetLocation());
		RootRotations.Add(NewRoot.GetRotation());
		RootScales.Add(NewRoot.GetScale3D());
	}

	IAnimationDataController& Controller = Target->GetController();
	Controller.OpenBracket(LOCTEXT("BakeRootMotionFromFeet", "발 기준 루트 모션 굽기"), false);
	if (!Target->GetDataModel()->IsValidBoneTrackName(RootName))
	{
		Controller.AddBoneCurve(RootName, false);
	}
	Controller.SetBoneTrackKeys(RootName, RootPositions, RootRotations, RootScales, false);
	Controller.CloseBracket(false);

	Target->bEnableRootMotion = true;
	Target->RootMotionRootLock = ERootMotionRootLock::AnimFirstFrame;
	Target->PostEditChange();
	Target->MarkPackageDirty();

	UE_LOG(LogTemp, Display, TEXT("BakeRootMotionFromFeet %s: 루트 이동 (%.1f, %.1f) / %.2f s"), *PackagePath, Offset.X, Offset.Y, Source->GetPlayLength());
	return Target;
}

#undef LOCTEXT_NAMESPACE
