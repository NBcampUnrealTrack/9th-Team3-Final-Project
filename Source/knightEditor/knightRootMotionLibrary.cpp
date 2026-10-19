#include "knightRootMotionLibrary.h"

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

#undef LOCTEXT_NAMESPACE
