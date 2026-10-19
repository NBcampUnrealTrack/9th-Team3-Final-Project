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
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			FTransform Transform = ReadLocal(FirstModel, BoneIndex, FirstTime);
			if (Alpha > 0.f)
			{
				FTransform SecondTransform = ReadLocal(SecondModel, BoneIndex, SecondTime);
				if (BoneIndex == 0)
				{
					SecondTransform = SecondTransform.GetRelativeTransform(SecondStartRoot) * SeamRoot;
				}
				Transform.SetTranslation(FMath::Lerp(Transform.GetTranslation(), SecondTransform.GetTranslation(), Alpha));
				Transform.SetRotation(FQuat::Slerp(Transform.GetRotation(), SecondTransform.GetRotation(), Alpha).GetNormalized());
				Transform.SetScale3D(FMath::Lerp(Transform.GetScale3D(), SecondTransform.GetScale3D(), Alpha));
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

#undef LOCTEXT_NAMESPACE
