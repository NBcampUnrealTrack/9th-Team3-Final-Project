#include "knightAnimMirrorLibrary.h"

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationSettings.h"
#include "Animation/MirrorDataTable.h"
#include "Animation/Skeleton.h"
#include "AnimationRuntime.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "BonePose.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"

#define LOCTEXT_NAMESPACE "knightAnimMirrorLibrary"

UMirrorDataTable* UknightAnimMirrorLibrary::BuildMirrorDataTable(const FString& PackagePath, USkeleton* Skeleton)
{
	if (!Skeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("BuildMirrorDataTable %s: 스켈레톤이 없습니다."), *PackagePath);
		return nullptr;
	}

	UMirrorDataTable* Table = LoadObject<UMirrorDataTable>(nullptr, *PackagePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Table)
	{
		UPackage* const Package = CreatePackage(*PackagePath);
		Table = NewObject<UMirrorDataTable>(Package, *FPackageName::GetShortName(PackagePath), RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Table);
	}
	Table->Modify();
	// 에디터의 새 미러 테이블 팩토리와 같은 순서로 채운다.
	Table->RowStruct = FMirrorTableRow::StaticStruct();
	Table->Skeleton = Skeleton;
	Table->MirrorAxis = EAxis::X;
	Table->MirrorFindReplaceExpressions = UAnimationSettings::Get()->MirrorFindReplaceExpressions;
	Table->UpdateFromFindReplaceExpressions(UMirrorDataTable::FFindReplaceOptions::Sync());

	// 짝이 없는 뼈는 MirrorPose가 아예 건드리지 않는다(FillMirrorBoneIndexes가 INDEX_NONE으로 둔다). 그러면 꼬리나 가슴처럼
	// 가운데 뼈가 뒤집히지 않고 원래 방향으로 남으므로 자기 자신과 짝지어 둔다.
	TCustomBoneIndexArray<FSkeletonPoseBoneIndex, FSkeletonPoseBoneIndex> MirrorBoneIndexes;
	Table->FillMirrorBoneIndexes(Skeleton, MirrorBoneIndexes);
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	TArray<FString> SelfMapped;
	for (int32 BoneIndex = 0; BoneIndex < MirrorBoneIndexes.Num(); ++BoneIndex)
	{
		if (!MirrorBoneIndexes[BoneIndex].IsValid())
		{
			const FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
			Table->AddRow(BoneName, FMirrorTableRow(BoneName, BoneName, EMirrorRowType::Bone, true));
			SelfMapped.Add(BoneName.ToString());
		}
	}
	if (!SelfMapped.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("BuildMirrorDataTable %s: 짝이 없어 자기 자신과 짝지은 뼈 %d개: %s"), *PackagePath, SelfMapped.Num(),
			*FString::Join(SelfMapped, TEXT(", ")));
	}

	Table->PostEditChange();
	Table->MarkPackageDirty();
	return Table;
}

UAnimSequence* UknightAnimMirrorLibrary::BakeMirroredSequence(UAnimSequence* Source, UMirrorDataTable* MirrorTable, const FString& PackagePath)
{
	USkeleton* const Skeleton = Source ? Source->GetSkeleton() : nullptr;
	if (!Skeleton || !MirrorTable || MirrorTable->Skeleton != Skeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("BakeMirroredSequence %s: 시퀀스와 같은 스켈레톤의 미러 테이블이 필요합니다."), *PackagePath);
		return nullptr;
	}

	UAnimSequence* Target = LoadObject<UAnimSequence>(nullptr, *PackagePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Target)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		Target = Cast<UAnimSequence>(AssetTools.DuplicateAsset(FPackageName::GetShortName(PackagePath), FPackageName::GetLongPackagePath(PackagePath), Source));
		if (!Target)
		{
			UE_LOG(LogTemp, Error, TEXT("BakeMirroredSequence %s: 복제에 실패했습니다."), *PackagePath);
			return nullptr;
		}
	}
	else
	{
		// 다시 구울 때는 노티파이를 남기려고 기존 에셋을 쓰므로, 원본에서 바뀌었을 수 있는 재생 설정만 맞춘다.
		Target->Modify();
		Target->bEnableRootMotion = Source->bEnableRootMotion;
		Target->RootMotionRootLock = Source->RootMotionRootLock;
		Target->bForceRootLock = Source->bForceRootLock;
		Target->bUseNormalizedRootMotionScale = Source->bUseNormalizedRootMotionScale;
		Target->RateScale = Source->RateScale;
	}

	const IAnimationDataModel* const SourceModel = Source->GetDataModel();
	if (SourceModel->GetNumberOfFloatCurves() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BakeMirroredSequence %s: 원본에 커브가 있습니다. 커브 이름의 좌우는 바꾸지 않습니다."), *PackagePath);
	}

	// 스켈레톤의 모든 뼈로 포즈를 만든다. 트랙이 없는 뼈는 기준 포즈로 두는데, 짝 뼈에 트랙이 있으면 뒤집으면서 움직임이 생긴다.
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const int32 NumBones = RefSkeleton.GetNum();
	TArray<FBoneIndexType> RequiredBones;
	RequiredBones.Reserve(NumBones);
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		RequiredBones.Add(static_cast<FBoneIndexType>(BoneIndex));
	}
	FBoneContainer BoneContainer(RequiredBones, UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::None), *Skeleton);
	// FCompactPose는 메모리 스택에 할당하므로 표시를 먼저 둔다.
	FMemMark Mark(FMemStack::Get());
	FCompactPose Pose;
	Pose.SetBoneContainer(&BoneContainer);

	TArray<FName> TrackNames;
	SourceModel->GetBoneTrackNames(TrackNames);
	TArray<TPair<FCompactPoseBoneIndex, FName>> Tracks;
	for (const FName& TrackName : TrackNames)
	{
		const int32 SkeletonIndex = RefSkeleton.FindBoneIndex(TrackName);
		if (SkeletonIndex != INDEX_NONE)
		{
			Tracks.Emplace(BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(FSkeletonPoseBoneIndex(SkeletonIndex)), TrackName);
		}
	}

	// 원본 키를 프레임마다 그대로 읽어(압축 데이터가 아닌 원본) 뒤집는다. 런타임 Mirror 노드와 같은 함수다.
	const int32 NumKeys = SourceModel->GetNumberOfKeys();
	TArray<TArray<FVector>> Positions, Scales;
	TArray<TArray<FQuat>> Rotations;
	Positions.SetNum(NumBones);
	Rotations.SetNum(NumBones);
	Scales.SetNum(NumBones);
	for (int32 Key = 0; Key < NumKeys; ++Key)
	{
		Pose.ResetToRefPose();
		for (const TPair<FCompactPoseBoneIndex, FName>& Track : Tracks)
		{
			Pose[Track.Key] = SourceModel->GetBoneTrackTransform(Track.Value, FFrameNumber(Key));
		}
		FAnimationRuntime::MirrorPose(Pose, *MirrorTable);
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			const FTransform& Transform = Pose[BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(FSkeletonPoseBoneIndex(BoneIndex))];
			Positions[BoneIndex].Add(Transform.GetTranslation());
			Rotations[BoneIndex].Add(Transform.GetRotation());
			Scales[BoneIndex].Add(Transform.GetScale3D());
		}
	}

	IAnimationDataController& Controller = Target->GetController();
	Controller.OpenBracket(LOCTEXT("BakeMirrored", "미러 애니 굽기"), false);
	Controller.SetFrameRate(SourceModel->GetFrameRate(), false);
	Controller.SetNumberOfFrames(FFrameNumber(SourceModel->GetNumberOfFrames()), false);
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
	return Target;
}

#undef LOCTEXT_NAMESPACE
