#include "knightAnimSpliceLibrary.h"

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"

#define LOCTEXT_NAMESPACE "knightAnimSpliceLibrary"

UAnimSequence* UknightAnimSpliceLibrary::BakeSplicedSequence(UAnimSequence* First, float FirstEndTime, UAnimSequence* Second, float SecondStartTime,
	float BlendTime, const FString& PackagePath)
{
	return BakeAlignedSplicedSequence(First, FirstEndTime, Second, SecondStartTime, BlendTime, FTransform::Identity, PackagePath);
}

UAnimSequence* UknightAnimSpliceLibrary::BakeAlignedSplicedSequence(UAnimSequence* First, float FirstEndTime, UAnimSequence* Second,
	float SecondStartTime, float BlendTime, const FTransform& SecondOffset, const FString& PackagePath)
{
	const USkeleton* const Skeleton = First ? First->GetSkeleton() : nullptr;
	if (!Skeleton || !Second || Second->GetSkeleton() != Skeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("BakeSplicedSequence %s: 같은 스켈레톤의 두 시퀀스가 필요합니다."), *PackagePath);
		return nullptr;
	}
	const IAnimationDataModel* const FirstModel = First->GetDataModel();
	const IAnimationDataModel* const SecondModel = Second->GetDataModel();
	const double FirstLength = FirstModel->GetPlayLength();
	const double SecondLength = SecondModel->GetPlayLength();
	if (FirstEndTime <= 0.f || FirstEndTime > FirstLength || SecondStartTime < 0.f || SecondStartTime >= SecondLength || BlendTime < 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("BakeSplicedSequence %s: 이음 시점이 잘못됐습니다 (앞 %.3f / %.3f, 뒤 %.3f / %.3f)."), *PackagePath,
			FirstEndTime, FirstLength, SecondStartTime, SecondLength);
		return nullptr;
	}

	UAnimSequence* Target = LoadObject<UAnimSequence>(nullptr, *PackagePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Target)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		Target = Cast<UAnimSequence>(AssetTools.DuplicateAsset(FPackageName::GetShortName(PackagePath), FPackageName::GetLongPackagePath(PackagePath), First));
		if (!Target)
		{
			UE_LOG(LogTemp, Error, TEXT("BakeSplicedSequence %s: 복제에 실패했습니다."), *PackagePath);
			return nullptr;
		}
	}
	Target->Modify();
	if (FirstModel->GetNumberOfFloatCurves() > 0 || SecondModel->GetNumberOfFloatCurves() > 0 || !First->Notifies.IsEmpty())
	{
		// 커브와 노티파이는 앞 애니의 시간 그대로 복제되므로, 이음매 뒤에 걸친 것은 다시 맞춰야 한다.
		UE_LOG(LogTemp, Warning, TEXT("BakeSplicedSequence %s: 원본에 커브나 노티파이가 있습니다. 이음매 뒤 타이밍을 확인하세요."), *PackagePath);
	}

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const int32 NumBones = RefSkeleton.GetNum();
	auto ReadLocal = [&RefSkeleton](const IAnimationDataModel* Model, int32 BoneIndex, double Time)
	{
		const FName Name = RefSkeleton.GetBoneName(BoneIndex);
		return Model->IsValidBoneTrackName(Name)
			? Model->EvaluateBoneTrackTransform(Name, Model->GetFrameRate().AsFrameTime(Time), EAnimInterpolationType::Linear)
			: RefSkeleton.GetRefBonePose()[BoneIndex];
	};

	// 이음매부터의 루트: 앞 애니가 이음매에서 멈춘 자리를 기준으로, 뒤 애니의 루트가 시작점에서 움직인 만큼만 움직인다.
	const FTransform SeamRoot = ReadLocal(FirstModel, 0, FirstEndTime);
	const FTransform SecondStartRoot = ReadLocal(SecondModel, 0, SecondStartTime);
	const bool bAligned = !SecondOffset.Equals(FTransform::Identity);

	const FFrameRate FrameRate = FirstModel->GetFrameRate();
	const double Length = FirstEndTime + (SecondLength - SecondStartTime);
	const int32 NumFrames = FMath::Max(1, FrameRate.AsFrameTime(Length).RoundToFrame().Value);
	TArray<TArray<FVector>> Positions, Scales;
	TArray<TArray<FQuat>> Rotations;
	Positions.SetNum(NumBones);
	Rotations.SetNum(NumBones);
	Scales.SetNum(NumBones);
	for (int32 Key = 0; Key <= NumFrames; ++Key)
	{
		const double Time = FMath::Min(FrameRate.AsSeconds(FFrameNumber(Key)), Length);
		const double SinceSeam = Time - FirstEndTime;
		const double SecondTime = FMath::Min(SecondStartTime + SinceSeam, SecondLength);
		// 이음매 앞은 앞 애니만, 블렌드 구간은 계속 재생하는 앞 애니에서 뒤 애니로 부드럽게, 그 뒤는 뒤 애니만.
		const float Alpha = SinceSeam <= 0.0 ? 0.f
			: BlendTime <= 0.f ? 1.f
			: FMath::SmoothStep(0.f, 1.f, static_cast<float>(SinceSeam / BlendTime));
		const double FirstTime = FMath::Min(SinceSeam <= 0.0 ? Time : FirstEndTime + SinceSeam, FirstLength);
		auto BlendLocal = [&](int32 BoneIndex, const FTransform& RootOffset)
		{
			FTransform Transform = ReadLocal(FirstModel, BoneIndex, FirstTime);
			if (Alpha > 0.f)
			{
				FTransform SecondTransform = ReadLocal(SecondModel, BoneIndex, SecondTime);
				if (BoneIndex == 0)
				{
					SecondTransform = SecondTransform.GetRelativeTransform(SecondStartRoot) * RootOffset * SeamRoot;
				}
				Transform.SetTranslation(FMath::Lerp(Transform.GetTranslation(), SecondTransform.GetTranslation(), Alpha));
				Transform.SetRotation(FQuat::Slerp(Transform.GetRotation(), SecondTransform.GetRotation(), Alpha).GetNormalized());
				Transform.SetScale3D(FMath::Lerp(Transform.GetScale3D(), SecondTransform.GetScale3D(), Alpha));
			}
			return Transform;
		};

		// 보이는 루트는 맞춘 자리, 실제로 쓰는 루트는 맞추지 않은 자리에서 맞춘 자리로 끝까지 천천히 옮겨 간다.
		// 그 차이만큼 루트 바로 아래 뼈를 되돌려 놓아 몸은 맞춘 자리에 그대로 보인다.
		FTransform VisualRoot = BlendLocal(0, SecondOffset);
		if (!bAligned && Alpha > 0.f && Alpha < 1.f && Key > 0)
		{
			// 블렌드 구간에서 위치를 섞으면 빠른 앞 애니에서 느린 뒤 애니로 넘어갈 때 루트가 뒤로 끌려 순간 멈춘다(달리다 멈추기).
			// 프레임마다 두 애니의 이동량을 섞어 쌓아 속도가 매끄럽게 바뀌게 한다. 블렌드가 끝나면 뒤 애니 궤적을 그 자리부터 따른다.
			const double PrevSinceSeam = FrameRate.AsSeconds(FFrameNumber(Key - 1)) - FirstEndTime;
			const FVector FirstStep = ReadLocal(FirstModel, 0, FirstTime).GetTranslation()
				- ReadLocal(FirstModel, 0, FMath::Min(FirstEndTime + PrevSinceSeam, FirstLength)).GetTranslation();
			const FVector SecondStep = SeamRoot.TransformVector(SecondStartRoot.InverseTransformVector(
				ReadLocal(SecondModel, 0, SecondTime).GetTranslation() - ReadLocal(SecondModel, 0, FMath::Clamp(SecondStartTime + PrevSinceSeam, SecondStartTime, SecondLength)).GetTranslation()));
			VisualRoot.SetTranslation(Positions[0].Last() + FMath::Lerp(FirstStep, SecondStep, Alpha));
		}
		else if (!bAligned && Alpha >= 1.f && Key > 0)
		{
			const double PrevSinceSeam = FrameRate.AsSeconds(FFrameNumber(Key - 1)) - FirstEndTime;
			const FVector SecondStep = SeamRoot.TransformVector(SecondStartRoot.InverseTransformVector(
				ReadLocal(SecondModel, 0, SecondTime).GetTranslation() - ReadLocal(SecondModel, 0, FMath::Clamp(SecondStartTime + PrevSinceSeam, SecondStartTime, SecondLength)).GetTranslation()));
			VisualRoot.SetTranslation(Positions[0].Last() + SecondStep);
		}
		FTransform Root = VisualRoot;
		if (bAligned && SinceSeam > 0.0)
		{
			const FTransform PlainRoot = BlendLocal(0, FTransform::Identity);
			const float Follow = FMath::SmoothStep(0.f, 1.f, static_cast<float>(SinceSeam / FMath::Max(Length - FirstEndTime, UE_KINDA_SMALL_NUMBER)));
			Root.SetTranslation(FMath::Lerp(PlainRoot.GetTranslation(), VisualRoot.GetTranslation(), Follow));
			Root.SetRotation(FQuat::Slerp(PlainRoot.GetRotation(), VisualRoot.GetRotation(), Follow).GetNormalized());
		}
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			FTransform Transform = BoneIndex == 0 ? Root : BlendLocal(BoneIndex, SecondOffset);
			if (bAligned && BoneIndex > 0 && RefSkeleton.GetParentIndex(BoneIndex) == 0)
			{
				Transform = (Transform * VisualRoot).GetRelativeTransform(Root);
			}
			Positions[BoneIndex].Add(Transform.GetTranslation());
			Rotations[BoneIndex].Add(Transform.GetRotation());
			Scales[BoneIndex].Add(Transform.GetScale3D());
		}
	}

	IAnimationDataController& Controller = Target->GetController();
	Controller.OpenBracket(LOCTEXT("BakeSpliced", "애니 이어 붙이기"), false);
	Controller.SetFrameRate(FrameRate, false);
	Controller.SetNumberOfFrames(FFrameNumber(NumFrames), false);
	Controller.RemoveAllBoneTracks(false);
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
		Controller.AddBoneCurve(BoneName, false);
		Controller.SetBoneTrackKeys(BoneName, Positions[BoneIndex], Rotations[BoneIndex], Scales[BoneIndex], false);
	}
	Controller.CloseBracket(false);

	Target->PostEditChange();
	Target->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("BakeSplicedSequence %s: %.3f초 (앞 0~%.3f + 뒤 %.3f~%.3f, 블렌드 %.2f)"), *PackagePath, Length,
		FirstEndTime, SecondStartTime, SecondLength, BlendTime);
	return Target;
}

bool UknightAnimSpliceLibrary::BakeBoneYawOffset(UAnimSequence* Sequence, FName Bone, float FadeInStart, float HoldStart, float HoldEnd,
	float FadeOutEnd, float YawDegrees)
{
	const USkeleton* const Skeleton = Sequence ? Sequence->GetSkeleton() : nullptr;
	const IAnimationDataModel* const Model = Sequence ? Sequence->GetDataModel() : nullptr;
	const FReferenceSkeleton* const RefSkeleton = Skeleton ? &Skeleton->GetReferenceSkeleton() : nullptr;
	const int32 BoneIndex = RefSkeleton ? RefSkeleton->FindBoneIndex(Bone) : INDEX_NONE;
	if (BoneIndex <= 0 || !Model->IsValidBoneTrackName(Bone) || !(FadeInStart <= HoldStart && HoldStart <= HoldEnd && HoldEnd <= FadeOutEnd))
	{
		UE_LOG(LogTemp, Error, TEXT("BakeBoneYawOffset %s: 뼈 %s가 없거나(루트 제외) 구간 %.3f/%.3f/%.3f/%.3f가 잘못됐습니다."),
			*GetNameSafe(Sequence), *Bone.ToString(), FadeInStart, HoldStart, HoldEnd, FadeOutEnd);
		return false;
	}
	auto ReadLocal = [Model, RefSkeleton](int32 Index, const FFrameNumber Frame)
	{
		const FName Name = RefSkeleton->GetBoneName(Index);
		return Model->IsValidBoneTrackName(Name) ? Model->GetBoneTrackTransform(Name, Frame) : RefSkeleton->GetRefBonePose()[Index];
	};
	auto Weight = [=](double Time)
	{
		if (Time < HoldStart)
		{
			return HoldStart > FadeInStart ? FMath::SmoothStep(FadeInStart, HoldStart, static_cast<float>(Time)) : (Time >= FadeInStart ? 1.f : 0.f);
		}
		if (Time > HoldEnd)
		{
			return FadeOutEnd > HoldEnd ? 1.f - FMath::SmoothStep(HoldEnd, FadeOutEnd, static_cast<float>(Time)) : 0.f;
		}
		return 1.f;
	};

	const FFrameRate FrameRate = Model->GetFrameRate();
	const int32 NumKeys = Model->GetNumberOfKeys();
	TArray<FVector> Positions;
	TArray<FQuat> Rotations;
	TArray<FVector> Scales;
	for (int32 Key = 0; Key < NumKeys; ++Key)
	{
		const FFrameNumber Frame(Key);
		FTransform Local = ReadLocal(BoneIndex, Frame);
		const float Alpha = Weight(FrameRate.AsSeconds(Frame));
		if (Alpha > 0.f)
		{
			// 부모의 컴포넌트 공간 회전 P에 대해, 컴포넌트 공간에서 R만큼 돌린 로컬 회전 = P⁻¹·R·P·로컬.
			FQuat Parent = FQuat::Identity;
			for (int32 Index = RefSkeleton->GetParentIndex(BoneIndex); Index != INDEX_NONE; Index = RefSkeleton->GetParentIndex(Index))
			{
				Parent = ReadLocal(Index, Frame).GetRotation() * Parent;
			}
			const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(YawDegrees * Alpha));
			Local.SetRotation((Parent.Inverse() * Yaw * Parent * Local.GetRotation()).GetNormalized());
		}
		Positions.Add(Local.GetTranslation());
		Rotations.Add(Local.GetRotation());
		Scales.Add(Local.GetScale3D());
	}

	Sequence->Modify();
	IAnimationDataController& Controller = Sequence->GetController();
	Controller.OpenBracket(LOCTEXT("BakeBoneYaw", "뼈 회전 굽기"), false);
	Controller.SetBoneTrackKeys(Bone, Positions, Rotations, Scales, false);
	Controller.CloseBracket(false);
	Sequence->PostEditChange();
	Sequence->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("BakeBoneYawOffset %s: %s %.1f도 (%.3f~%.3f~%.3f~%.3f)"), *GetNameSafe(Sequence), *Bone.ToString(), YawDegrees,
		FadeInStart, HoldStart, HoldEnd, FadeOutEnd);
	return true;
}

#undef LOCTEXT_NAMESPACE
