#include "MechPartSkinBaker.h"

#include "Animation/Skeleton.h"
#include "AnimationRuntime.h"
#include "BoneWeights.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "ClothingAsset.h"
#include "ClothingAssetFactoryInterface.h"
#include "ClothingSystemEditorInterfaceModule.h"
#include "GeometryScript/MeshRemeshFunctions.h"
#include "GeometryScript/MeshRepairFunctions.h"
#include "GeometryScript/MeshVoxelFunctions.h"
#include "LODUtilities.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PointWeightMap.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "DynamicMesh/DynamicBoneAttribute.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMeshEditor.h"
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
	 * (예: upperarm_l 제외 → 팔 전체 가중치가 clavicle_l로, thigh_l 제외 → 다리 전체가 pelvis로.) 바뀐 정점 수를 돌려준다.
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

	/**
	 * 뼈마다 실린 가중치 비율(모든 정점 가중치 합 기준) 상위 10개를 로그로 남긴다 (10-12).
	 * 어떤 자세에서 파츠가 이상하게 끌려갈 때 어느 뼈가 끌고 가는지 보는 용도.
	 */
	static void LogBoneWeightShare(const UDynamicMesh* Mesh, const FString& MeshName)
	{
		Mesh->ProcessMesh([&MeshName](const UE::Geometry::FDynamicMesh3& ReadMesh)
		{
			const UE::Geometry::FDynamicMeshAttributeSet* const Attributes = ReadMesh.Attributes();
			const UE::Geometry::FDynamicMeshVertexSkinWeightsAttribute* const SkinWeights = Attributes && Attributes->HasBones()
				? Attributes->GetSkinWeightsAttribute(FSkeletalMeshAttributesShared::DefaultSkinWeightProfileName) : nullptr;
			if (!SkinWeights)
			{
				return;
			}
			const TArray<FName>& BoneNames = Attributes->GetBoneNames()->GetAttribValues();
			TArray<double> BoneTotals;
			BoneTotals.SetNumZeroed(BoneNames.Num());
			double Total = 0.0;
			TArray<int32> Bones;
			TArray<float> Weights;
			for (const int32 VertexId : ReadMesh.VertexIndicesItr())
			{
				SkinWeights->GetValue(VertexId, Bones, Weights);
				for (int32 Index = 0; Index < Bones.Num(); ++Index)
				{
					if (BoneTotals.IsValidIndex(Bones[Index]))
					{
						BoneTotals[Bones[Index]] += Weights[Index];
						Total += Weights[Index];
					}
				}
			}

			TArray<int32> Order;
			for (int32 Bone = 0; Bone < BoneTotals.Num(); ++Bone)
			{
				if (BoneTotals[Bone] > 0.0)
				{
					Order.Add(Bone);
				}
			}
			Order.Sort([&BoneTotals](const int32 A, const int32 B) { return BoneTotals[A] > BoneTotals[B]; });
			FString Summary;
			for (int32 Rank = 0; Rank < FMath::Min(Order.Num(), 10); ++Rank)
			{
				Summary += FString::Printf(TEXT("%s%s %.1f%%"), Rank > 0 ? TEXT(", ") : TEXT(""),
					*BoneNames[Order[Rank]].ToString(), 100.0 * BoneTotals[Order[Rank]] / FMath::Max(Total, UE_DOUBLE_SMALL_NUMBER));
			}
			UE_LOG(LogMechPartSkinBaker, Display, TEXT("%s: bone weight share [%s]"), *MeshName, *Summary);
		});
	}

	/** 스켈레탈 메시의 천 데이터를 모두 지운다 (엔진 Persona의 'Delete Clothing Data'와 같은 순서). */
	static void RemoveAllClothing(USkeletalMesh* Mesh)
	{
		if (Mesh->GetMeshClothingAssets().IsEmpty())
		{
			return;
		}
		FScopedSkeletalMeshPostEditChange ScopedPostEditChange(Mesh);
		Mesh->PreEditChange(nullptr);
		const TArray<TObjectPtr<UClothingAssetBase>> Assets = Mesh->GetMeshClothingAssets();
		for (UClothingAssetBase* const Asset : Assets)
		{
			if (Asset)
			{
				Asset->UnbindFromSkeletalMesh(Mesh, INDEX_NONE, INDEX_NONE);
				// 이름을 비켜 줘서 다시 만들 때 같은 이름을 쓸 수 있게 한다.
				Asset->Rename(nullptr, GetTransientPackage());
			}
		}
		Mesh->GetMeshClothingAssets().Empty();
	}

	/** LOD0만 남긴다 (천을 끄고 다시 구울 때 예전 천 계산용 LOD1을 치운다). */
	static void RemoveLodsAboveZero(USkeletalMesh* Mesh)
	{
		if (Mesh->GetLODNum() <= 1)
		{
			return;
		}
		FSkeletalMeshUpdateContext UpdateContext;
		UpdateContext.SkeletalMesh = Mesh;
		TArray<int32> Lods;
		for (int32 Lod = 1; Lod < Mesh->GetLODNum(); ++Lod)
		{
			Lods.Add(Lod);
		}
		FLODUtilities::RemoveLODs(UpdateContext, Lods);
	}

	/**
	 * 높이 기준(컴포넌트 공간 Z)으로 천 마스크 두 장을 칠한다. 위에서 FixedTopRatio만큼은 고정 구간, 그 아래로 맨 아래까지 0 → 1로 늘어나는 비율 t.
	 *   MaxDistance(흔들림 범위): ClothMaxDistance × t (고정 구간 0).
	 *   AnimDriveStiffness(형태 유지, 10-13): 1 − t (고정 구간 1, 자락 끝 0). 실제 세기는 천 설정의 Low(자락 끝 값)~High(위쪽 값) 사이로 보간된다.
	 */
	static void PaintClothMasks(UClothingAssetCommon& Cloth, const FOptions& Options)
	{
		FClothLODDataCommon& Lod = Cloth.LodData[0];
		const TArray<FVector3f>& Vertices = Lod.PhysicalMeshData.Vertices;
		float MinZ = TNumericLimits<float>::Max();
		float MaxZ = TNumericLimits<float>::Lowest();
		for (const FVector3f& Vertex : Vertices)
		{
			MinZ = FMath::Min(MinZ, Vertex.Z);
			MaxZ = FMath::Max(MaxZ, Vertex.Z);
		}
		const float FixedZ = MaxZ - (MaxZ - MinZ) * FMath::Clamp(Options.ClothFixedTopRatio, 0.f, 0.95f);
		const float FreeRange = FMath::Max(FixedZ - MinZ, KINDA_SMALL_NUMBER);

		auto FindOrAddMask = [&Lod](const EWeightMapTargetCommon Target, const TCHAR* const Name) -> FPointWeightMap&
		{
			FPointWeightMap* Mask = Lod.PointWeightMaps.FindByPredicate([Target](const FPointWeightMap& Map)
			{
				return Map.CurrentTarget == static_cast<uint8>(Target);
			});
			if (!Mask)
			{
				Mask = &Lod.PointWeightMaps.AddDefaulted_GetRef();
			}
			Mask->Name = Name;
			Mask->CurrentTarget = static_cast<uint8>(Target);
			Mask->bEnabled = true;
			return *Mask;
		};
		FPointWeightMap& MaxDistanceMask = FindOrAddMask(EWeightMapTargetCommon::MaxDistance, TEXT("MaxDistance (Mech Parts Workbench)"));
		FPointWeightMap& ShapeKeepMask = FindOrAddMask(EWeightMapTargetCommon::AnimDriveStiffness, TEXT("Shape Keep (Mech Parts Workbench)"));
		MaxDistanceMask.Values.SetNumZeroed(Vertices.Num());
		ShapeKeepMask.Values.SetNumZeroed(Vertices.Num());
		for (int32 Index = 0; Index < Vertices.Num(); ++Index)
		{
			const float Free = FMath::Clamp((FixedZ - Vertices[Index].Z) / FreeRange, 0.f, 1.f);
			MaxDistanceMask.Values[Index] = Options.ClothMaxDistance * Free;
			ShapeKeepMask.Values[Index] = 1.f - Free;
		}
	}

	/**
	 * 천 계산용 메시에서 몸 쪽(안쪽)을 향한 면을 지운다 (10-12, 10-13 판정 방식 바꿈). 지운 면 수를 돌려준다.
	 *   1. 면 중심에서 노멀 방향(+)과 반대 방향(-)으로 선을 쏴 몸에 맞는 거리를 잰다. +쪽이 더 가까우면 몸을 향한 안쪽 면.
	 *      둘 다 안 맞으면(몸 옆으로 퍼진 곳) 가장 가까운 몸 표면 점 방향과 노멀이 확실히(60도 넘게 기울어) 반대일 때만 안쪽으로 본다.
	 *      선은 몸 크기의 30%까지만 본다 — 자락 끝처럼 아래를 향한 테두리 면이 멀리 있는 발에 맞아 안쪽으로 지워지면,
	 *      천이 망토 끝까지 닿지 못해 끝에 있는 보이는 점이 천 밖으로 외삽되어 튄다(10-13).
	 *      (10-12엔 가장 가까운 몸 점 방향만 썼는데, 망토가 접힌 곳에서 자주 틀려 바깥 면에 구멍이 많이 났다.)
	 *   2. 이웃 면 다수결을 3번 돌려 판정이 섞인 곳의 자잘한 구멍·조각을 정리한다.
	 * 법선이 뒤집힌 모델처럼 거의 다 안쪽으로 나오면 지우지 않는다.
	 */
	static int32 RemoveInnerLayer(UDynamicMesh* SimMesh, const UDynamicMesh* BodyMesh)
	{
		int32 Removed = 0;
		BodyMesh->ProcessMesh([SimMesh, &Removed](const UE::Geometry::FDynamicMesh3& Body)
		{
			const UE::Geometry::FDynamicMeshAABBTree3 BodyTree(&Body, true);
			const double MaxRayLength = Body.GetBounds().MaxDim() * 0.3;
			SimMesh->EditMesh([&BodyTree, MaxRayLength, &Removed](UE::Geometry::FDynamicMesh3& Sim)
			{
				const double NoHit = TNumericLimits<double>::Max();
				auto HitDistance = [&BodyTree, NoHit, MaxRayLength](const FVector3d& Origin, const FVector3d& Direction)
				{
					double HitT = NoHit;
					int32 HitTriangle = IndexConstants::InvalidID;
					return BodyTree.FindNearestHitTriangle(FRay3d(Origin, Direction), HitT, HitTriangle) && HitT <= MaxRayLength ? HitT : NoHit;
				};

				// 1. 선 쏘기
				TArray<bool> bInner;
				bInner.SetNumZeroed(Sim.MaxTriangleID());
				for (const int32 TriangleId : Sim.TriangleIndicesItr())
				{
					const FVector3d Center = Sim.GetTriCentroid(TriangleId);
					const FVector3d Normal = Sim.GetTriNormal(TriangleId);
					const double Front = HitDistance(Center, Normal);
					const double Back = HitDistance(Center, -Normal);
					bInner[TriangleId] = (Front < NoHit || Back < NoHit)
						? Front < Back
						: (Center - BodyTree.FindNearestPoint(Center)).GetSafeNormal().Dot(Normal) < -0.5;
				}

				// 2. 이웃 다수결 (자기 + 변으로 붙은 이웃). 동점이면 그대로 둔다.
				for (int32 Pass = 0; Pass < 3; ++Pass)
				{
					TArray<bool> Next = bInner;
					for (const int32 TriangleId : Sim.TriangleIndicesItr())
					{
						int32 Votes = bInner[TriangleId] ? 1 : -1;
						const UE::Geometry::FIndex3i Neighbours = Sim.GetTriNeighbourTris(TriangleId);
						for (int32 Side = 0; Side < 3; ++Side)
						{
							if (Neighbours[Side] != IndexConstants::InvalidID)
							{
								Votes += bInner[Neighbours[Side]] ? 1 : -1;
							}
						}
						Next[TriangleId] = Votes > 0 || (Votes == 0 && bInner[TriangleId]);
					}
					bInner = MoveTemp(Next);
				}

				TArray<int32> InnerTriangles;
				for (const int32 TriangleId : Sim.TriangleIndicesItr())
				{
					if (bInner[TriangleId])
					{
						InnerTriangles.Add(TriangleId);
					}
				}
				if (InnerTriangles.Num() > Sim.TriangleCount() * 0.9)
				{
					return;
				}
				for (const int32 TriangleId : InnerTriangles)
				{
					if (Sim.RemoveTriangle(TriangleId, true, false) == UE::Geometry::EMeshResult::Ok)
					{
						++Removed;
					}
				}
			});
		});
		return Removed;
	}

	/**
	 * 천 충돌용 물리 에셋 (10-12). 뺄 뼈가 없으면 메카 물리 에셋을 그대로 쓰고, 있으면 복사본을 SK 안(같은 패키지)에 만들어
	 * NoCollisionRoots와 그 아래 뼈의 바디를 지운다. 다시 구우면 예전 복사본은 비켜 두고 새로 만든다.
	 */
	static UPhysicsAsset* MakeClothCollisionAsset(USkeletalMesh* Mesh, const USkeletalMesh* MechMesh, const TArray<FName>& NoCollisionRoots)
	{
		UPhysicsAsset* const Source = MechMesh->GetPhysicsAsset();
		static const FName CopyName(TEXT("ClothCollision"));
		if (UObject* const OldCopy = StaticFindObject(UPhysicsAsset::StaticClass(), Mesh, *CopyName.ToString()))
		{
			OldCopy->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		}
		if (!Source || NoCollisionRoots.IsEmpty())
		{
			return Source;
		}

		// 뺄 뼈: NoCollisionRoots와 그 아래 (뼈 목록은 부모가 자식보다 앞).
		const FReferenceSkeleton& RefSkeleton = MechMesh->GetRefSkeleton();
		TArray<bool> bExcluded;
		bExcluded.SetNumZeroed(RefSkeleton.GetNum());
		TSet<FName> ExcludedBones;
		for (int32 Bone = 0; Bone < RefSkeleton.GetNum(); ++Bone)
		{
			const int32 Parent = RefSkeleton.GetParentIndex(Bone);
			bExcluded[Bone] = NoCollisionRoots.Contains(RefSkeleton.GetBoneName(Bone)) || (Parent != INDEX_NONE && bExcluded[Parent]);
			if (bExcluded[Bone])
			{
				ExcludedBones.Add(RefSkeleton.GetBoneName(Bone));
			}
		}

		UPhysicsAsset* const Copy = DuplicateObject<UPhysicsAsset>(Source, Mesh, CopyName);
		Copy->ClearFlags(RF_Standalone);
		Copy->SetFlags(RF_Transactional);
		for (int32 Body = Copy->SkeletalBodySetups.Num() - 1; Body >= 0; --Body)
		{
			const USkeletalBodySetup* const BodySetup = Copy->SkeletalBodySetups[Body];
			if (BodySetup && ExcludedBones.Contains(BodySetup->BoneName))
			{
				FPhysicsAssetUtils::DestroyBody(Copy, Body);
			}
		}
		UE_LOG(LogMechPartSkinBaker, Display, TEXT("%s: cloth collision without [%s] -> %d of %d bodies."),
			*Mesh->GetName(), *FString::JoinBy(NoCollisionRoots, TEXT(", "), [](const FName& Name) { return Name.ToString(); }),
			Copy->SkeletalBodySetups.Num(), Source->SkeletalBodySetups.Num());
		return Copy;
	}

	/** 천 계산용 대리 메시 만들기의 단계별 결과 (로그용). */
	struct FClothProxyStats
	{
		int32 WrappedTriangles = 0;
		int32 RemovedInner = 0;
		int32 RemovedPieces = 0;
		int32 RemeshedTriangles = 0;
		double MeanEdgeLength = 0.0;
	};

	/**
	 * 천 계산용 대리 메시의 2~3단계 (MakeClothProxy 설명 참고): 부풀리기(Padding cm) → (바깥 한 장) → 고른 삼각형으로 다시 나누기
	 * → 작은 삼각형 정리 → 정점 번호 압축·정점 노멀. Proxy에는 감싼 결과와 원본 표면이 겹쳐 들어 있어야 한다.
	 */
	static void ShapeClothProxy(UDynamicMesh* Proxy, const UDynamicMesh* BodyMesh, const double Padding, const FOptions& Options,
		UGeometryScriptDebug* Debug, FClothProxyStats& OutStats)
	{
		FGeometryScriptMorphologyOptions MorphologyOptions;
		MorphologyOptions.SDFGridParameters.SizeMethod = EGeometryScriptGridSizingMethod::GridCellSize;
		MorphologyOptions.SDFGridParameters.GridCellSize = Options.ClothProxyCellSize;
		MorphologyOptions.Operation = EGeometryScriptMorphologicalOpType::Dilate;
		MorphologyOptions.Distance = static_cast<float>(Padding);
		UGeometryScriptLibrary_MeshVoxelFunctions::ApplyMeshMorphology(Proxy, MorphologyOptions, Debug);
		OutStats.WrappedTriangles = Proxy->GetTriangleCount();

		// 2. 바깥 한 장. 작은 조각 지우기에서 부피 조건은 끈다(열린 한 장은 부피가 0이라 다 지워진다). 전체의 2%보다 작은 조각만 지운다.
		if (Options.bClothOuterLayerOnly)
		{
			OutStats.RemovedInner = RemoveInnerLayer(Proxy, BodyMesh);
			Proxy->EditMesh([&OutStats](UE::Geometry::FDynamicMesh3& EditMesh)
			{
				UE::Geometry::FDynamicMeshEditor Editor(&EditMesh);
				OutStats.RemovedPieces = Editor.RemoveSmallComponents(TNumericLimits<double>::Lowest(), 0.0, FMath::Max(1, EditMesh.TriangleCount() / 50));
			});
		}

		// 3. 고른 삼각형으로 다시 나눈다 (닫힌 껍데기·열린 한 장 모두 정점 ≈ 삼각형/2).
		// 단순화(QEM)만 쓰면 아주 가늘고 긴 삼각형이 남고, 보이는 메시가 그 삼각형을 따라가며 점이 몇 m씩 튀어나간다(10-13).
		// 노멀 등 속성은 버린다: 남겨 두면 그 경계를 지키느라 삼각형을 합치지 못해 오히려 늘어난다(10-12 첫 시도, 1.2만 목표에 4.9만).
		FGeometryScriptRemeshOptions RemeshOptions;
		RemeshOptions.bDiscardAttributes = true;
		FGeometryScriptUniformRemeshOptions UniformOptions;
		UniformOptions.TargetTriangleCount = Options.ClothSimVertexCount * 2;
		UGeometryScriptLibrary_RemeshingFunctions::ApplyUniformRemesh(Proxy, RemeshOptions, UniformOptions, Debug);
		OutStats.RemeshedTriangles = Proxy->GetTriangleCount();

		// 작은·가는 삼각형 정리. 기준은 평균 변 길이 L에 맞춘다: 변 0.3L보다 짧으면 합치고, 넓이 0.05L²보다 작으면 지운다
		// (정삼각형 넓이 ≈ 0.43L²). 보이는 점이 이런 삼각형에 묶이면 삼각형 밖으로 수천 배 외삽되어 몇십 m씩 튄다
		// (10-13 진단: 최소 넓이 0.0012cm² 삼각형들, 외삽 최대 2697배). 엔진 천 생성도 꼭짓점이 겹친 삼각형이 있으면 실패한다.
		double MeanEdgeLength = 0.0;
		Proxy->ProcessMesh([&MeanEdgeLength](const UE::Geometry::FDynamicMesh3& ReadMesh)
		{
			int32 EdgeCount = 0;
			for (const int32 EdgeId : ReadMesh.EdgeIndicesItr())
			{
				const UE::Geometry::FIndex2i EdgeVertices = ReadMesh.GetEdgeV(EdgeId);
				MeanEdgeLength += FVector3d::Distance(ReadMesh.GetVertex(EdgeVertices.A), ReadMesh.GetVertex(EdgeVertices.B));
				++EdgeCount;
			}
			MeanEdgeLength /= FMath::Max(EdgeCount, 1);
		});
		OutStats.MeanEdgeLength = MeanEdgeLength;
		FGeometryScriptDegenerateTriangleOptions DegenerateOptions;
		DegenerateOptions.MinEdgeLength = FMath::Max(0.01, MeanEdgeLength * 0.3);
		DegenerateOptions.MinTriangleArea = FMath::Max(0.001, MeanEdgeLength * MeanEdgeLength * 0.05);
		UGeometryScriptLibrary_MeshRepairFunctions::RepairMeshDegenerateGeometry(Proxy, DegenerateOptions, Debug);
		Proxy->EditMesh([](UE::Geometry::FDynamicMesh3& EditMesh)
		{
			// 엔진 가중치 복사는 '옮긴 정점 수 = 정점 번호 최댓값'으로 성공을 판단해서, 지운 정점 자리가 비어 있으면 실패로 끝난다.
			EditMesh.CompactInPlace();
			// 다시 나누며 버린 노멀을 정점 노멀로 채운다 (감싸기 결과와 같은 방식).
			EditMesh.EnableAttributes();
			UE::Geometry::FMeshNormals::InitializeOverlayToPerVertexNormals(EditMesh.Attributes()->PrimaryNormals(), false);
		});
	}

	/**
	 * 천 계산용 대리 메시를 만든다 (10-12). 3D 생성 도구로 만든 망토는 두께 있는 껍데기 + 가시 장식이라, 원본을 줄이거나
	 * 안쪽 면만 지우면 서로 안 이어진 조각들이 되어 조각마다 따로 떨어진다(천이 처음 돌았을 때 본 모습). 그래서 따로 만든다:
	 *   1. 복셀로 감싸(Voxel Wrap) 구멍·가시 없는 매끈한 한 덩어리 표면을 만들고 (칸 크기 = ClothProxyCellSize),
	 *      원본 표면을 겹쳐 넣어 부풀린다 → 얇은 곳의 구멍과 가는 가시까지 덮는 닫힌 두툼한 껍데기.
	 *   2. (bClothOuterLayerOnly, 기본 끔) 몸 쪽 면을 지워 바깥 한 장만 남기고, 가장자리에서 떨어져 나온 작은 조각을 지운다.
	 *      10-13 시험: 한 장은 구멍·가장자리가 많아 보이는 점 5천여 개가 천 밖으로 외삽됐고, 닫힌 껍데기는 40개였다.
	 *   3. 정점이 ClothSimVertexCount쯤 되게 고른 삼각형으로 다시 나누고, 너무 작은 삼각형을 정리한다.
	 *      그래도 천 밖으로 크게 외삽되는 보이는 점은 천을 연결한 뒤 PinRiskyClothTriangles가 고정해 스킨만 따르게 한다.
	 *   4. 구운 파츠에서 스킨 가중치를 가장 가까운 표면 점 기준으로 복사한다 (팔·다리 제외가 이미 반영된 가중치).
	 * 보이는 메시(LOD0)는 원본 그대로이고, 이 천을 따라 움직인다.
	 */
	static UDynamicMesh* MakeClothProxy(UDynamicMesh* SkinnedPart, const UDynamicMesh* BodyMesh, const FOptions& Options,
		UGeometryScriptDebug* Debug, const FString& LogName)
	{
		UDynamicMesh* const Proxy = NewObject<UDynamicMesh>();
		SkinnedPart->ProcessMesh([Proxy](const UE::Geometry::FDynamicMesh3& Source)
		{
			Proxy->SetMesh(Source);
		});

		// 1. 감싸기. 가장자리가 열린 얇은 천 메시도 감싸지게 두께를 준다.
		FGeometryScriptSolidifyOptions SolidifyOptions;
		SolidifyOptions.GridParameters.SizeMethod = EGeometryScriptGridSizingMethod::GridCellSize;
		SolidifyOptions.GridParameters.GridCellSize = Options.ClothProxyCellSize;
		SolidifyOptions.ExtendBounds = Options.ClothProxyCellSize * 2.f;
		SolidifyOptions.bThickenShells = true;
		SolidifyOptions.ShellThickness = Options.ClothProxyCellSize;
		UGeometryScriptLibrary_MeshVoxelFunctions::ApplyMeshSolidify(Proxy, SolidifyOptions, Debug);

		// 1-2. 원본 표면도 겹쳐 넣고 부풀리기 (10-13): 칸보다 얇은 곳은 감싸기에서 구멍이 숭숭 나고, 가는 가시 장식은 아예 빠진다.
		//      감싼 결과에 원본 표면을 겹쳐 넣고 부풀리면, 구멍이 메워지고 가는 가시까지 두툼한 껍데기 안에 들어온다
		//      (부풀리기는 표면까지의 거리로 계산해서 가는 부분도 놓치지 않는다).
		//      보이는 망토의 안·밖 두 겹과 가시가 모두 이 두께 안에 들어와 천에 가깝게 묶인다.
		//      (진단: 감싼 결과만 부풀렸을 때 천 표면에서 최대 27cm 떨어진 가시 끝이 남아 길게 튀었다.)
		//      부풀리는 두께는 다시 나눌 삼각형 변 길이 L에 맞춘다(아래에서 L의 1~1.5배를 시험). 가시를 덮는 관이 삼각형보다 가늘면
		//      다시 나누기(3단계)에서 뭉개져 가시 끝이 다시 천 밖으로 나온다(10-13: 3cm로 부풀렸을 때 변 4.4cm로 나누자 튐 128개).
		//      L은 감싼 결과 넓이와 목표 삼각형 수(정점 목표 × 2)로 어림한다: 정삼각형 넓이 = (√3/4)L².
		double WrappedArea = 0.0;
		Proxy->ProcessMesh([&WrappedArea](const UE::Geometry::FDynamicMesh3& ReadMesh)
		{
			for (const int32 TriangleId : ReadMesh.TriangleIndicesItr())
			{
				WrappedArea += ReadMesh.GetTriArea(TriangleId);
			}
		});
		const double ExpectedEdgeLength = FMath::Sqrt(4.0 * WrappedArea / (FMath::Sqrt(3.0) * FMath::Max(Options.ClothSimVertexCount * 2, 1)));
		SkinnedPart->ProcessMesh([Proxy](const UE::Geometry::FDynamicMesh3& Source)
		{
			Proxy->EditMesh([&Source](UE::Geometry::FDynamicMesh3& EditMesh)
			{
				EditMesh.DiscardAttributes();
				UE::Geometry::FDynamicMesh3 SourceGeometry;
				SourceGeometry.Copy(Source, false, false, false, false);
				UE::Geometry::FMeshIndexMappings Mappings;
				UE::Geometry::FDynamicMeshEditor(&EditMesh).AppendMesh(&SourceGeometry, Mappings);
			});
		});
		// 2~3단계(부풀리기 → 다시 나누기 → 정리). 두께가 얇으면 가는 가시를 덮는 관이 다시 나누기에서 뭉개지고, 두꺼우면 가시가 몰린 곳이
		// 한 덩어리로 합쳐져 그 안쪽 점이 천 표면에서 멀어진다. 그래도 남는 '크게 외삽되는 점'은 천을 연결한 뒤 PinRiskyClothTriangles가 고정한다.
		// (10-13: 두께를 일곱 가지로 해 보고 엔진 연결 함수로 미리 재서 고르는 방식을 시도했는데, 엔진이 천 메시 삼각형 순서에 따라
		//  동점 삼각형을 다르게 골라 미리 잰 값과 실제가 달랐다(미리 3.8배 → 실제 80배). 굽기만 20초 넘게 늘어서 걷어냈다.)
		const double Padding = FMath::Max(Options.ClothProxyCellSize * 1.5, ExpectedEdgeLength * 1.25);
		FClothProxyStats Stats;
		ShapeClothProxy(Proxy, BodyMesh, Padding, Options, Debug, Stats);

		// 4. 스킨 가중치 (뼈 목록도 같이 복사된다).
		FGeometryScriptTransferBoneWeightsOptions TransferOptions;
		TransferOptions.TransferMethod = ETransferBoneWeightsMethod::ClosestPointOnSurface;
		UGeometryScriptLibrary_MeshBoneWeightFunctions::TransferBoneWeightsFromMesh(SkinnedPart, Proxy, TransferOptions, FGeometryScriptMeshSelection(), Debug);

		UE_LOG(LogMechPartSkinBaker, Display, TEXT("%s: cloth proxy cell %.1f cm, padding %.1f cm -> wrapped %d tris -> removed %d inner faces, %d small pieces -> remeshed %d tris (mean edge %.1f cm) -> %d vertices, %d tris (outer layer only %d)."),
			*LogName, Options.ClothProxyCellSize, Padding, Stats.WrappedTriangles, Stats.RemovedInner, Stats.RemovedPieces, Stats.RemeshedTriangles,
			Stats.MeanEdgeLength, Proxy->GetMeshRef().VertexCount(), Proxy->GetTriangleCount(), Options.bClothOuterLayerOnly);
		return Proxy;
	}

	/**
	 * (10-13) 보이는 점이 천 삼각형 밖으로 이 배율보다 크게 외삽되면 '튈 수 있는 점'으로 본다 (PinRiskyClothTriangles·진단 공용).
	 * 처음 2였는데, 1~2배라도 천 표면에서 멀리(최대 20cm) 떨어진 점은 천이 휠 때 작은 가시로 보여서 1.5로 낮췄다.
	 */
	static constexpr float RiskyMappingBary = 1.5f;

	/** (10-13) 고정한 천 정점 주변을 몇 칸(이웃 단계)에 걸쳐 단계적으로 묶을지 (PinRiskyClothTriangles). */
	static constexpr int32 PinFeatherRings = 3;

	/**
	 * (10-13) 엔진이 실제로 만든 천 연결(LOD0 섹션 0의 ClothMappingData)을 읽어, 천 삼각형 밖으로 RiskyMappingBary배 넘게 외삽된 보이는 점이
	 * 묶인 천 삼각형의 정점을 고정한다(흔들림 범위 0). 엔진 셰이더는 세 정점이 모두 고정된 삼각형에 묶인 점을 천 대신 스킨(뼈)만으로
	 * 움직이므로(GpuSkinCacheComputeShader: SourceMeshVertIndices.w == 0xFFFF면 천 영향 없음), 그 점은 더 이상 튀지 않는다.
	 * 고정 값은 천 데이터의 마스크에 저장되고, 메시를 다시 빌드해도 엔진은 같은 천 메시에서 같은 삼각형을 고르므로 유지된다.
	 * 연결 데이터 자체는 빌드 때마다 엔진이 다시 계산해서 직접 고쳐 둘 수 없다(10-13 확인).
	 *
	 * 고정한 점만 뼈 위치에 남고 바로 옆 천은 처지면 그 사이 삼각형이 늘어나 작은 가시처럼 보인다(10-13 시험). 그래서 고정한 정점에서
	 * 이웃 PinFeatherRings칸까지 흔들림 범위를 칸마다 25%·50%·75%로 줄이고 형태 유지를 1(위쪽 값)에 가깝게 올려, 주변 천이
	 * 뼈 모양에서 멀리 벌어지지 않게 한다. 천 LOD의 부드러운 전환(bSmoothTransition, MakeCloth에서 켬)은 그 경계의 보이는 점을
	 * 고정 비율만큼 스킨과 천 사이로 섞는다. 고정한 천 정점 수를 돌려준다(주변 칸 수는 OutFeathered).
	 */
	static int32 PinRiskyClothTriangles(USkeletalMesh* Mesh, UClothingAssetCommon* Cloth, int32& OutFeathered)
	{
		OutFeathered = 0;
		const FSkelMeshSection& Section = Mesh->GetImportedModel()->LODModels[0].Sections[0];
		FClothLODDataCommon& Lod = Cloth->LodData[0];
		auto FindMask = [&Lod](const EWeightMapTargetCommon Target)
		{
			return Lod.PointWeightMaps.FindByPredicate([Target](const FPointWeightMap& Map)
			{
				return Map.CurrentTarget == static_cast<uint8>(Target);
			});
		};
		FPointWeightMap* const MaxDistanceMask = FindMask(EWeightMapTargetCommon::MaxDistance);
		FPointWeightMap* const ShapeKeepMask = FindMask(EWeightMapTargetCommon::AnimDriveStiffness);
		if (Section.ClothMappingDataLODs.IsEmpty() || !MaxDistanceMask)
		{
			return 0;
		}

		// 튈 수 있는 점이 묶인 천 삼각형의 정점들 (이미 고정된 정점도 단계적으로 묶는 시작점에는 넣는다).
		TSet<int32> SeedVertices;
		for (const FMeshToMeshVertData& Entry : Section.ClothMappingDataLODs[0])
		{
			const FVector4f& Bary = Entry.PositionBaryCoordsAndDist;
			if (FMath::Max3(FMath::Abs(Bary.X), FMath::Abs(Bary.Y), FMath::Abs(Bary.Z)) <= RiskyMappingBary)
			{
				continue;
			}
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				if (MaxDistanceMask->Values.IsValidIndex(Entry.SourceMeshVertIndices[Corner]))
				{
					SeedVertices.Add(Entry.SourceMeshVertIndices[Corner]);
				}
			}
		}
		if (SeedVertices.IsEmpty())
		{
			return 0;
		}

		// 천 메시 이웃 관계 → 시작점에서 몇 칸 떨어졌는지 (너비 우선).
		const TArray<uint32>& Indices = Lod.PhysicalMeshData.Indices;
		TArray<TArray<int32>> Neighbours;
		Neighbours.SetNum(MaxDistanceMask->Values.Num());
		for (int32 Base = 0; Base + 2 < Indices.Num(); Base += 3)
		{
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 From = static_cast<int32>(Indices[Base + Corner]);
				const int32 To = static_cast<int32>(Indices[Base + (Corner + 1) % 3]);
				if (Neighbours.IsValidIndex(From) && Neighbours.IsValidIndex(To))
				{
					Neighbours[From].AddUnique(To);
					Neighbours[To].AddUnique(From);
				}
			}
		}
		TArray<int32> Ring;
		Ring.Init(INDEX_NONE, Neighbours.Num());
		TArray<int32> Frontier = SeedVertices.Array();
		for (const int32 Seed : Frontier)
		{
			Ring[Seed] = 0;
		}
		for (int32 Step = 1; Step <= PinFeatherRings && !Frontier.IsEmpty(); ++Step)
		{
			TArray<int32> Next;
			for (const int32 Vertex : Frontier)
			{
				for (const int32 Neighbour : Neighbours[Vertex])
				{
					if (Ring[Neighbour] == INDEX_NONE)
					{
						Ring[Neighbour] = Step;
						Next.Add(Neighbour);
					}
				}
			}
			Frontier = MoveTemp(Next);
		}

		// 칸마다 흔들림 범위 0·25%·50%·75%, 형태 유지 마스크 1·0.75·0.5·0.25 (원래 값보다 묶는 쪽으로만 바꾼다).
		int32 Pinned = 0;
		for (int32 Vertex = 0; Vertex < Ring.Num(); ++Vertex)
		{
			if (Ring[Vertex] == INDEX_NONE)
			{
				continue;
			}
			const float Keep = static_cast<float>(Ring[Vertex]) / (PinFeatherRings + 1);
			const bool bWasStatic = MaxDistanceMask->IsBelowThreshold(Vertex);
			MaxDistanceMask->Values[Vertex] *= Keep;
			if (ShapeKeepMask && ShapeKeepMask->Values.IsValidIndex(Vertex))
			{
				ShapeKeepMask->Values[Vertex] = FMath::Max(ShapeKeepMask->Values[Vertex], 1.f - Keep);
			}
			if (bWasStatic)
			{
				continue;
			}
			if (Ring[Vertex] == 0)
			{
				++Pinned;
			}
			else
			{
				++OutFeathered;
			}
		}
		// 바뀐 마스크를 천 데이터에 넣고, 보이는 점마다 천/스킨 비율을 다시 계산한다(메시를 다시 빌드하며 같은 연결을 다시 만든다).
		Cloth->ApplyParameterMasks(true);
		return Pinned;
	}

	/**
	 * (10-13) 진단: 만든 천 데이터와 연결(보이는 메시 점 → 천 삼각형) 값을 재서 로그 두 줄로 남긴다.
	 * 흔들림 범위를 5cm로 막아도 보이는 점이 몇십 m씩 튀어나가서(10-13 시험), 어느 데이터가 깨졌는지 보려는 용도.
	 *   cloth check   : 천 정점·삼각형 수, NaN 개수, 가장 작은 삼각형 넓이와 0.01cm²보다 작은 삼각형 수,
	 *                   뼈 가중치 합이 1이 아닌 정점 수, 뼈가 0개인 정점 수, 범위 밖 뼈 번호 수, 고정 정점 수
	 *   cloth mapping : 연결된 보이는 점 수, 바리센트릭 좌표 절댓값 최대와 3을 넘는 점 수(삼각형 밖으로 멀리 외삽)와
	 *                   그 점들이 보이는 메시 높이의 아래·중간·위 1/3 중 어디에 있는지, 천 표면에서 떨어진 거리 최대, 범위 밖 천 정점 번호 수
	 */
	static void LogClothDiagnostics(const USkeletalMesh* Mesh, const UClothingAssetCommon* Cloth)
	{
		const FClothPhysicalMeshData& Phys = Cloth->LodData[0].PhysicalMeshData;
		int32 NaNs = 0;
		for (int32 Index = 0; Index < Phys.Vertices.Num(); ++Index)
		{
			NaNs += Phys.Vertices[Index].ContainsNaN() || (Phys.Normals.IsValidIndex(Index) && Phys.Normals[Index].ContainsNaN());
		}
		double MinArea = TNumericLimits<double>::Max();
		int32 TinyTriangles = 0;
		for (int32 Base = 0; Base + 2 < Phys.Indices.Num(); Base += 3)
		{
			const FVector3f& A = Phys.Vertices[Phys.Indices[Base]];
			const FVector3f& B = Phys.Vertices[Phys.Indices[Base + 1]];
			const FVector3f& C = Phys.Vertices[Phys.Indices[Base + 2]];
			const double Area = 0.5 * FVector3f::CrossProduct(B - A, C - A).Size();
			MinArea = FMath::Min(MinArea, Area);
			TinyTriangles += Area < 0.01;
		}
		int32 BadWeightSum = 0;
		int32 NoInfluence = 0;
		int32 BadBoneIndex = 0;
		for (const FClothVertBoneData& Bones : Phys.BoneData)
		{
			float Sum = 0.f;
			for (int32 Influence = 0; Influence < FClothVertBoneData::MaxTotalInfluences; ++Influence)
			{
				if (Bones.BoneWeights[Influence] > 0.f)
				{
					Sum += Bones.BoneWeights[Influence];
					BadBoneIndex += Bones.BoneIndices[Influence] >= Cloth->UsedBoneNames.Num();
				}
			}
			BadWeightSum += FMath::Abs(Sum - 1.f) > 0.01f;
			NoInfluence += Bones.NumInfluences <= 0;
		}
		int32 FixedVertices = 0;
		if (const FPointWeightMap* const MaxDistances = Phys.FindWeightMap(EWeightMapTargetCommon::MaxDistance))
		{
			for (const float Value : MaxDistances->Values)
			{
				FixedVertices += Value < 0.1f;
			}
		}
		UE_LOG(LogMechPartSkinBaker, Display, TEXT("%s: cloth check - %d vertices, %d tris, %d NaN, min tri area %.4f cm2, %d tris under 0.01 cm2, %d bad weight sums, %d with no influence, %d bad bone indices (of %d used bones), %d fixed."),
			*Mesh->GetName(), Phys.Vertices.Num(), Phys.Indices.Num() / 3, NaNs, MinArea, TinyTriangles, BadWeightSum, NoInfluence, BadBoneIndex,
			Cloth->UsedBoneNames.Num(), FixedVertices);

		const FSkelMeshSection& Section = Mesh->GetImportedModel()->LODModels[0].Sections[0];
		float MaxBary = 0.f;
		int32 FarBary = 0;
		float MaxOffset = 0.f;
		int32 BadSimIndex = 0;
		int32 Mapped = 0;
		// 외삽이 큰 점(|bary| > 3)이 보이는 메시 높이의 아래·중간·위 1/3 중 어디에 몰렸는지 (10-13).
		float MinZ = TNumericLimits<float>::Max();
		float MaxZ = TNumericLimits<float>::Lowest();
		for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
		{
			MinZ = FMath::Min(MinZ, Vertex.Position.Z);
			MaxZ = FMath::Max(MaxZ, Vertex.Position.Z);
		}
		int32 FarByHeight[3] = { 0, 0, 0 };
		// (10-13) 천 대신 스킨만 따르는 점 수(세 천 정점이 모두 고정 → SourceMeshVertIndices[3] == 0xFFFF)와,
		// RiskyMappingBary배 넘게 외삽되는데도 아직 천을 따르는 점 수(튈 수 있는 점, PinRiskyClothTriangles 뒤엔 0이어야 한다).
		int32 SkinnedOnly = 0;
		int32 RiskySimulated = 0;
		if (!Section.ClothMappingDataLODs.IsEmpty())
		{
			const TArray<FMeshToMeshVertData>& Entries = Section.ClothMappingDataLODs[0];
			for (int32 VertexIndex = 0; VertexIndex < Entries.Num(); ++VertexIndex)
			{
				const FMeshToMeshVertData& Entry = Entries[VertexIndex];
				const FVector4f& Bary = Entry.PositionBaryCoordsAndDist;
				const float LargestBary = FMath::Max3(FMath::Abs(Bary.X), FMath::Abs(Bary.Y), FMath::Abs(Bary.Z));
				const bool bSkinnedOnly = Entry.SourceMeshVertIndices[3] == 0xFFFF;
				SkinnedOnly += bSkinnedOnly;
				RiskySimulated += LargestBary > RiskyMappingBary && !bSkinnedOnly;
				MaxBary = FMath::Max(MaxBary, LargestBary);
				FarBary += LargestBary > 3.f;
				if (LargestBary > 3.f && Section.SoftVertices.IsValidIndex(VertexIndex))
				{
					const float HeightRatio = (Section.SoftVertices[VertexIndex].Position.Z - MinZ) / FMath::Max(MaxZ - MinZ, KINDA_SMALL_NUMBER);
					++FarByHeight[FMath::Clamp(static_cast<int32>(HeightRatio * 3.f), 0, 2)];
				}
				MaxOffset = FMath::Max(MaxOffset, FMath::Abs(Bary.W));
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					BadSimIndex += Entry.SourceMeshVertIndices[Corner] >= Phys.Vertices.Num();
				}
				++Mapped;
			}
		}
		UE_LOG(LogMechPartSkinBaker, Display, TEXT("%s: cloth mapping - %d of %d render vertices mapped, max |bary| %.2f (%d over 3: bottom %d / middle %d / top %d), max offset %.1f cm, %d bad sim indices, %d skinned only, %d risky still simulated."),
			*Mesh->GetName(), Mapped, Section.SoftVertices.Num(), MaxBary, FarBary, FarByHeight[0], FarByHeight[1], FarByHeight[2], MaxOffset, BadSimIndex,
			SkinnedOnly, RiskySimulated);
	}

	/**
	 * 천 시뮬레이션을 만든다: 천 계산용 대리 메시(MakeClothProxy)를 LOD1로 넣고 → LOD1로 천 데이터 생성(충돌 = 메카 물리 에셋) →
	 * 흔들림 범위 칠하기 → LOD0(원래 메시)에 연결. LOD0은 LOD1의 천을 따라 움직인다.
	 */
	static bool MakeCloth(USkeletalMesh* Mesh, UDynamicMesh* SkinnedPart, const UDynamicMesh* BodyMesh, const USkeletalMesh* MechMesh,
		const FOptions& Options, UGeometryScriptDebug* Debug, FText& OutError)
	{
		// 1. 천 계산용 대리 메시 → LOD1.
		UDynamicMesh* const SimMesh = MakeClothProxy(SkinnedPart, BodyMesh, Options, Debug, Mesh->GetName());
		if (SimMesh->GetTriangleCount() == 0)
		{
			OutError = GetFirstError(Debug, LOCTEXT("ClothProxyFailed", "천 계산용 메시를 만들지 못했습니다 (감싸기 결과가 비었습니다)."));
			return false;
		}

		EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
		FGeometryScriptMeshWriteLOD SimLod;
		SimLod.LODIndex = 1;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToSkeletalMesh(SimMesh, Mesh, FGeometryScriptCopyMeshToAssetOptions(), SimLod, Outcome, Debug);
		if (Outcome != EGeometryScriptOutcomePins::Success || Mesh->GetLODNum() < 2)
		{
			OutError = GetFirstError(Debug, LOCTEXT("ClothSimMeshFailed", "천 계산용 메시(LOD1)를 만들지 못했습니다."));
			return false;
		}
		// LOD1은 천 계산용이라 화면에는 아주 작게 보일 때만 쓴다.
		if (FSkeletalMeshLODInfo* const SimLodInfo = Mesh->GetLODInfo(1))
		{
			SimLodInfo->ScreenSize.Default = 0.01f;
		}

		UClothingAssetCommon* Cloth = nullptr;
		bool bBoundLod0 = false;
		{
			// 이 범위가 끝날 때 메시를 한 번 다시 빌드한다.
			FScopedSkeletalMeshPostEditChange ScopedPostEditChange(Mesh);

			// 2. 천 데이터 (엔진 Skeletal Mesh 편집기의 'Create Clothing Data from Section'과 같은 함수).
			FSkeletalMeshClothBuildParams Params;
			Params.AssetName = TEXT("Cloth_") + Mesh->GetName();
			Params.LodIndex = 1;
			Params.SourceSection = 0;
			Params.bRemoveFromMesh = false;
			Params.PhysicsAsset = MakeClothCollisionAsset(Mesh, MechMesh, Options.ClothNoCollisionRootBones);
			UClothingAssetFactoryBase* const Factory =
				FModuleManager::LoadModuleChecked<FClothingSystemEditorInterfaceModule>("ClothingSystemEditorInterface").GetClothingAssetFactory();
			Cloth = Factory ? Cast<UClothingAssetCommon>(Factory->CreateFromSkeletalMesh(Mesh, Params)) : nullptr;
			if (!Cloth)
			{
				OutError = LOCTEXT("ClothCreateFailed", "천 데이터를 만들지 못했습니다.");
				return false;
			}
			Mesh->AddClothingAsset(Cloth);

			// 천 성질 (10-12, 10-13 형태 유지·중력 추가). 값은 워크벤치 미리보기 설정의 Cloth 항목에서 온다.
			// 형태 유지(Anim Drive)는 천을 원래(스킨) 모양 쪽으로 당기는 힘: 가중치 맵(PaintClothMasks)으로 위쪽 값~자락 끝 값 사이를 보간한다.
			// 게임 망토처럼 모양은 지키면서 자락만 휘날리게 하려는 것 (10-13 사용자: 중력에 한곳으로 쏠려 뭉치면 안 됨).
			if (UChaosClothConfig* const ClothConfig = Cloth->GetClothConfig<UChaosClothConfig>())
			{
				const float Follow = FMath::Clamp(Options.ClothFollowBody, 0.f, 1.f);
				const float Bending = FMath::Clamp(Options.ClothBendingStiffness, 0.f, 1.f);
				ClothConfig->LinearVelocityScale = FVector(Follow);
				ClothConfig->AngularVelocityScale = Follow;
				ClothConfig->BendingStiffnessWeighted = { Bending, Bending };
				ClothConfig->AnimDriveStiffness = { FMath::Clamp(Options.ClothShapeKeepHem, 0.f, 1.f), FMath::Clamp(Options.ClothShapeKeepTop, 0.f, 1.f) };
				ClothConfig->GravityScale = FMath::Max(Options.ClothGravityScale, 0.f);
			}
			// 원래 메시는 가장 가까운 천 삼각형 하나를 따라간다 (엔진 기본값). 10-12에 '여러 삼각형 섞기'를 켰었는데,
			// 그때 본 줄무늬는 천이 아예 안 돌던 때의 모습이었고, 섞기는 먼 삼각형까지 끌어와 점이 튀어나갈 수 있어서 껐다(10-13).
			// 부드러운 전환(10-13): 고정 정점이 일부 섞인 천 삼각형에 묶인 보이는 점을 고정 비율만큼 스킨과 천 사이로 섞는다.
			// 위쪽 고정 구간 경계와 PinRiskyClothTriangles가 고정한 곳 둘레에서 천이 급하게 꺾여 늘어나 보이는 것을 줄인다.
			Cloth->LodData[0].bSmoothTransition = true;

			// 3. 흔들림 범위·형태 유지 칠하기 → 4. 연결 (연결할 때 고정 정점을 계산하므로 칠한 값을 먼저 반영한다).
			// 같은 천 데이터는 메시 LOD 하나에만 연결된다. LOD0(원래 메시)에 연결하고, LOD1은 아주 멀 때만 보이는 천 없는 메시로 둔다.
			PaintClothMasks(*Cloth, Options);
			Cloth->ApplyParameterMasks();
			bBoundLod0 = Cloth->BindToSkeletalMesh(Mesh, 0, 0, 0);

			// 연결을 섹션 사용자 데이터에도 적는다 (엔진 Persona의 섹션 천 지정, FSkeletalMeshClothingSystemUtilities::AssignClothingToSection과 같은 순서).
			// BindToSkeletalMesh는 지금 LOD 데이터만 바꾸고, 메시를 다시 빌드할 때는 이 사용자 데이터로 천 연결을 되살린다.
			// (10-12) 이게 빠져서 범위가 끝나며 다시 빌드할 때 연결이 지워졌고, 천이 한 번도 돌지 않았다.
			if (bBoundLod0)
			{
				FSkeletalMeshLODModel& LodModel = Mesh->GetImportedModel()->LODModels[0];
				FSkelMeshSourceSectionUserData& SectionUserData = LodModel.UserSectionsData.FindOrAdd(LodModel.Sections[0].OriginalDataSectionIndex);
				int32 AssetIndex = INDEX_NONE;
				Mesh->GetMeshClothingAssets().Find(Cloth, AssetIndex);
				SectionUserData.CorrespondClothAssetIndex = static_cast<int16>(AssetIndex);
				SectionUserData.ClothingData.AssetGuid = Cloth->GetAssetGuid();
				SectionUserData.ClothingData.AssetLodIndex = 0;
			}
		}

		// 다시 빌드한 뒤에도 LOD0에 천이 붙어 있는지 확인한다.
		const bool bClothKept = bBoundLod0 && Mesh->GetImportedModel()->LODModels[0].Sections[0].HasClothingData();
		UE_LOG(LogMechPartSkinBaker, Display, TEXT("%s: cloth %d sim vertices (target %d), fixed top %.0f%%, max distance %.0f cm, follow body %.2f, bending %.2f, shape keep %.2f top / %.2f hem, gravity %.2f, bound %d, kept after rebuild %d, collision %s."),
			*Mesh->GetName(), Cloth->LodData[0].PhysicalMeshData.Vertices.Num(), Options.ClothSimVertexCount,
			Options.ClothFixedTopRatio * 100.f, Options.ClothMaxDistance, Options.ClothFollowBody, Options.ClothBendingStiffness,
			Options.ClothShapeKeepTop, Options.ClothShapeKeepHem, Options.ClothGravityScale,
			bBoundLod0, bClothKept, *GetNameSafe(Cloth->PhysicsAsset));
		if (!bClothKept)
		{
			OutError = LOCTEXT("ClothBindFailed", "천 데이터를 메시(LOD0)에 연결하지 못했습니다.");
			return false;
		}

		// 5. 크게 외삽된 보이는 점을 스킨만 따르게 고정하고, 그 주변 천을 단계적으로 묶는다 (10-13).
		int32 FeatheredVertices = 0;
		const int32 PinnedVertices = PinRiskyClothTriangles(Mesh, Cloth, FeatheredVertices);
		const bool bClothStillKept = Mesh->GetImportedModel()->LODModels[0].Sections[0].HasClothingData();
		UE_LOG(LogMechPartSkinBaker, Display, TEXT("%s: pinned %d cloth vertices under badly mapped render vertices (|bary| > %.1f), eased %d around them, cloth kept %d."),
			*Mesh->GetName(), PinnedVertices, RiskyMappingBary, FeatheredVertices, bClothStillKept);
		if (!bClothStillKept)
		{
			OutError = LOCTEXT("ClothPinFailed", "튀는 점 고정 뒤 천 연결이 사라졌습니다.");
			return false;
		}
		LogClothDiagnostics(Mesh, Cloth);
		return true;
	}

	USkeletalMesh* Bake(UStaticMesh* PartMesh, const FMechPartAttachment& Attachment, USkeletalMesh* MechMesh, const FOptions& Options, FText& OutError)
	{
		const TArray<FName>& ExcludedRootBones = Options.ExcludedRootBones;
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
		LogBoneWeightShare(PartDynamicMesh, PartMesh->GetName());

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
			// 모양이 바뀌면 예전 천은 맞지 않으므로 먼저 지운다 (천을 켜면 아래에서 새로 만든다).
			RemoveAllClothing(Result);
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

		// 천 시뮬레이션 (10-12). 끄면 예전 천 계산용 LOD1도 치운다.
		if (Options.bMakeCloth)
		{
			if (!MakeCloth(Result, PartDynamicMesh, BodyMesh, MechMesh, Options, Debug, OutError))
			{
				// 메시 자체는 만들어졌으니 저장은 하고, 천만 실패했다고 알린다.
				FEditorFileUtils::PromptForCheckoutAndSave({ Result->GetPackage() }, /*bCheckDirty=*/false, /*bPromptToSave=*/false);
				return nullptr;
			}
		}
		else
		{
			RemoveLodsAboveZero(Result);
		}

		FEditorFileUtils::PromptForCheckoutAndSave({ Result->GetPackage() }, /*bCheckDirty=*/false, /*bPromptToSave=*/false);
		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
