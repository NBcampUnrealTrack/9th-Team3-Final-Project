#include "MechPartSkinBaker.h"

#include "Animation/Skeleton.h"
#include "AnimationRuntime.h"
#include "BoneWeights.h"
#include "DynamicMesh/DynamicBoneAttribute.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "SkeletalMeshAttributes.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "FileHelpers.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBoneWeightFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "MechPartAttachment.h"
#include "Misc/PackageName.h"
#include "UDynamicMesh.h"

#define LOCTEXT_NAMESPACE "MechPartsWorkbench"

DEFINE_LOG_CATEGORY_STATIC(LogMechPartSkinBaker, Log, All);

namespace MechPartSkinBaker
{
	/** 메카 기본 자세에서 뼈 또는 소켓의 컴포넌트 공간 위치. 이름이 없거나 못 찾으면 메카 원점. */
	static FTransform GetRefPoseAttachTransform(const USkeletalMesh* MechMesh, FName SocketOrBone)
	{
		const FReferenceSkeleton& RefSkeleton = MechMesh->GetRefSkeleton();
		if (const USkeletalMeshSocket* const Socket = MechMesh->FindSocket(SocketOrBone))
		{
			const int32 BoneIndex = RefSkeleton.FindBoneIndex(Socket->BoneName);
			const FTransform BoneTransform = BoneIndex != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BoneIndex) : FTransform::Identity;
			return Socket->GetSocketLocalTransform() * BoneTransform;
		}
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(SocketOrBone);
		return BoneIndex != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BoneIndex) : FTransform::Identity;
	}

	/** Geometry Script가 남긴 첫 오류. 없으면 Fallback. */
	static FText GetFirstError(const UGeometryScriptDebug* Debug, const FText& Fallback)
	{
		for (const FGeometryScriptDebugMessage& Message : Debug->Messages)
		{
			if (Message.MessageType == EGeometryScriptDebugMessageType::ErrorMessage)
			{
				return FText::Format(LOCTEXT("BakeErrorWithDetail", "{0}\n({1})"), Fallback, Message.Message);
			}
		}
		return Fallback;
	}

	FString GetSkinnedMeshPackagePath(const UStaticMesh* StaticMesh)
	{
		FString Name = StaticMesh->GetName();
		Name = Name.StartsWith(TEXT("SM_")) ? TEXT("SK_") + Name.RightChop(3) : TEXT("SK_") + Name;
		return FPackageName::GetLongPackagePath(StaticMesh->GetPackage()->GetName()) / Name;
	}

	/**
	 * ExcludedRoots와 그 아래 뼈에 실린 가중치를, 제외되지 않은 가장 가까운 조상 뼈로 옮긴다.
	 * (예: upperarm_l 제외 → 팔 전체 가중치가 clavicle_l로, thigh_l 제외 → 다리 전체가 pelvis_l로.) 바뀐 정점 수를 돌려준다.
	 */
	static int32 MoveWeightsOffExcludedBones(UDynamicMesh* Mesh, const TArray<FName>& ExcludedRoots)
	{
		int32 MovedVertices = 0;
		if (ExcludedRoots.IsEmpty())
		{
			return MovedVertices;
		}
		Mesh->EditMesh([&ExcludedRoots, &MovedVertices](UE::Geometry::FDynamicMesh3& EditMesh)
		{
			UE::Geometry::FDynamicMeshAttributeSet* const Attributes = EditMesh.Attributes();
			if (!Attributes || !Attributes->HasBones())
			{
				return;
			}
			UE::Geometry::FDynamicMeshVertexSkinWeightsAttribute* const SkinWeights =
				Attributes->GetSkinWeightsAttribute(FSkeletalMeshAttributesShared::DefaultSkinWeightProfileName);
			if (!SkinWeights)
			{
				return;
			}

			// 뼈마다 옮겨 갈 뼈. 뼈 목록은 부모가 항상 자식보다 앞에 있어서 한 번 훑으면 된다.
			const TArray<FName>& BoneNames = Attributes->GetBoneNames()->GetAttribValues();
			const TArray<int32>& BoneParents = Attributes->GetBoneParentIndices()->GetAttribValues();
			TArray<bool> bExcluded;
			bExcluded.SetNumZeroed(BoneNames.Num());
			TArray<int32> TargetBone;
			TargetBone.SetNum(BoneNames.Num());
			for (int32 Bone = 0; Bone < BoneNames.Num(); ++Bone)
			{
				const int32 Parent = BoneParents[Bone];
				bExcluded[Bone] = ExcludedRoots.Contains(BoneNames[Bone]) || (Parent != INDEX_NONE && bExcluded[Parent]);
				TargetBone[Bone] = bExcluded[Bone] && Parent != INDEX_NONE ? TargetBone[Parent] : Bone;
			}

			TArray<int32> Bones;
			TArray<float> Weights;
			for (const int32 VertexId : EditMesh.VertexIndicesItr())
			{
				SkinWeights->GetValue(VertexId, Bones, Weights);
				bool bChanged = false;
				TArray<FBoneIndexType, TInlineAllocator<12>> NewBones;
				TArray<float, TInlineAllocator<12>> NewWeights;
				for (int32 Index = 0; Index < Bones.Num(); ++Index)
				{
					const int32 To = TargetBone.IsValidIndex(Bones[Index]) ? TargetBone[Bones[Index]] : Bones[Index];
					bChanged |= To != Bones[Index];
					const int32 Existing = NewBones.Find(static_cast<FBoneIndexType>(To));
					if (Existing != INDEX_NONE)
					{
						NewWeights[Existing] += Weights[Index];
					}
					else
					{
						NewBones.Add(static_cast<FBoneIndexType>(To));
						NewWeights.Add(Weights[Index]);
					}
				}
				if (bChanged)
				{
					SkinWeights->SetValue(VertexId, UE::AnimationCore::FBoneWeights::Create(NewBones.GetData(), NewWeights.GetData(), NewBones.Num()));
					++MovedVertices;
				}
			}
		});
		return MovedVertices;
	}

	USkeletalMesh* Bake(UStaticMesh* PartMesh, const FMechPartAttachment& Attachment, USkeletalMesh* MechMesh, const TArray<FName>& ExcludedRootBones, FText& OutError)
	{
		USkeleton* const Skeleton = MechMesh ? MechMesh->GetSkeleton() : nullptr;
		if (!PartMesh || !Skeleton)
		{
			OutError = LOCTEXT("BakeNoInput", "파츠 스태틱 메시 또는 미리보기 메카 메시(뼈대)가 없습니다.");
			return nullptr;
		}

		UGeometryScriptDebug* const Debug = NewObject<UGeometryScriptDebug>();
		EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;

		// 메카 몸 (기본 자세, 스킨 가중치 포함)
		UDynamicMesh* const BodyMesh = NewObject<UDynamicMesh>();
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(
			MechMesh, BodyMesh, FGeometryScriptCopyMeshFromAssetOptions(), FGeometryScriptMeshReadLOD(), Outcome, Debug);
		if (Outcome != EGeometryScriptOutcomePins::Success)
		{
			OutError = GetFirstError(Debug, LOCTEXT("BakeBodyFailed", "메카 메시를 읽지 못했습니다."));
			return nullptr;
		}

		// 파츠를 워크벤치·게임에서 붙는 자리로 옮긴다: RelativeTransform x (기본 자세의 뼈·소켓)
		UDynamicMesh* const PartDynamicMesh = NewObject<UDynamicMesh>();
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMeshV2(
			PartMesh, PartDynamicMesh, FGeometryScriptCopyMeshFromAssetOptions(), FGeometryScriptMeshReadLOD(), Outcome, true, Debug);
		if (Outcome != EGeometryScriptOutcomePins::Success)
		{
			OutError = GetFirstError(Debug, LOCTEXT("BakePartFailed", "파츠 스태틱 메시를 읽지 못했습니다."));
			return nullptr;
		}
		const FTransform PartToMech = Attachment.RelativeTransform * GetRefPoseAttachTransform(MechMesh, Attachment.AttachSocket);
		UGeometryScriptLibrary_MeshTransformFunctions::TransformMesh(PartDynamicMesh, PartToMech, true, Debug);

		// 몸의 스킨 가중치를 옮긴다 (겹쳐 입는 옷용). 뼈 목록도 몸에서 같이 복사된다.
		FGeometryScriptTransferBoneWeightsOptions TransferOptions;
		TransferOptions.TransferMethod = ETransferBoneWeightsMethod::InpaintWeights;
		TransferOptions.LayeredMeshSupport = true;
		UGeometryScriptLibrary_MeshBoneWeightFunctions::TransferBoneWeightsFromMesh(BodyMesh, PartDynamicMesh, TransferOptions, FGeometryScriptMeshSelection(), Debug);
		for (const FGeometryScriptDebugMessage& Message : Debug->Messages)
		{
			if (Message.MessageType == EGeometryScriptDebugMessageType::ErrorMessage)
			{
				OutError = GetFirstError(Debug, LOCTEXT("BakeTransferFailed", "스킨 가중치를 옮기지 못했습니다."));
				return nullptr;
			}
		}

		// 따라가지 않을 뼈(팔·다리 등)의 가중치를 몸통 쪽 뼈로 옮긴다 (10-12).
		const int32 MovedVertices = MoveWeightsOffExcludedBones(PartDynamicMesh, ExcludedRootBones);
		UE_LOG(LogMechPartSkinBaker, Display, TEXT("%s: excluded bones [%s] -> moved weights on %d vertices."),
			*PartMesh->GetName(), *FString::JoinBy(ExcludedRootBones, TEXT(", "), [](const FName& Name) { return Name.ToString(); }), MovedVertices);

		// 머티리얼은 원본 스태틱 메시 것을 그대로 쓴다.
		TArray<UMaterialInterface*> Materials;
		TArray<int32> MaterialIndices;
		TArray<FName> MaterialSlotNames;
		UGeometryScriptLibrary_StaticMeshFunctions::GetSectionMaterialListFromStaticMesh(
			PartMesh, FGeometryScriptMeshReadLOD(), Materials, MaterialIndices, MaterialSlotNames, Outcome, Debug);

		// 원본 옆에 만든다. 이미 있으면(다시 굽기) 그 에셋의 메시를 바꾼다.
		// 뼈 기준 자세는 반드시 메카 '메시'의 기본 자세(파츠를 놓은 자세)를 쓴다. 뼈대 에셋(USkeleton)의 기준 자세는
		// 메시와 다를 수 있어서, 그걸 쓰면 기본 자세에서도 망토가 팔 쪽으로 끌려 찢어진다 (10-12 첫 시도에서 생긴 문제).
		// 새로 만들 때 = bUseMeshBoneProportions, 덮어쓸 때 = CreateNewReferenceSkeleton (뼈 정보는 몸에서 복사된 것).
		const FString PackagePath = GetSkinnedMeshPackagePath(PartMesh);
		const FString ObjectPath = PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath);
		USkeletalMesh* Result = FindObject<USkeletalMesh>(nullptr, *ObjectPath);
		if (!Result && FPackageName::DoesPackageExist(PackagePath))
		{
			Result = LoadObject<USkeletalMesh>(nullptr, *ObjectPath);
		}
		if (Result)
		{
			FGeometryScriptCopyMeshToAssetOptions CopyOptions;
			CopyOptions.bReplaceMaterials = true;
			for (UMaterialInterface* const Material : Materials)
			{
				CopyOptions.NewMaterials.Add(Material);
			}
			CopyOptions.NewMaterialSlotNames = MaterialSlotNames;
			CopyOptions.BoneHierarchyMismatchHandling = EGeometryScriptBoneHierarchyMismatchHandling::CreateNewReferenceSkeleton;
			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToSkeletalMesh(
				PartDynamicMesh, Result, CopyOptions, FGeometryScriptMeshWriteLOD(), Outcome, Debug);
		}
		else
		{
			FGeometryScriptCreateNewSkeletalMeshAssetOptions CreateOptions;
			CreateOptions.bUseMeshBoneProportions = true;
			for (const TPair<FName, UMaterialInterface*>& Slot : UGeometryScriptLibrary_StaticMeshFunctions::ConvertMaterialListToMaterialMap(Materials, MaterialSlotNames))
			{
				CreateOptions.Materials.Add(Slot.Key, Slot.Value);
			}
			Result = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewSkeletalMeshAssetFromMesh(
				PartDynamicMesh, Skeleton, PackagePath, CreateOptions, Outcome, Debug);
		}
		if (!Result || Outcome != EGeometryScriptOutcomePins::Success)
		{
			OutError = GetFirstError(Debug, FText::Format(LOCTEXT("BakeCreateFailed", "스켈레탈 메시를 만들지 못했습니다: {0}"), FText::FromString(PackagePath)));
			return nullptr;
		}

		FEditorFileUtils::PromptForCheckoutAndSave({ Result->GetPackage() }, /*bCheckDirty=*/false, /*bPromptToSave=*/false);
		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
