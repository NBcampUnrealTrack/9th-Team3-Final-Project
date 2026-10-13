#include "MechBodyRegionSplitter.h"

#include "AnimationRuntime.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "DynamicMesh/DynamicBoneAttribute.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "Engine/SkeletalMesh.h"
#include "FileHelpers.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "MechBodyRegionSet.h"
#include "Misc/PackageName.h"
#include "SkeletalMeshAttributes.h"
#include "UDynamicMesh.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "MechPartsWorkbench"

DEFINE_LOG_CATEGORY_STATIC(LogMechBodyRegionSplitter, Log, All);

namespace MechBodyRegionSplitter
{
	/** 나눌 부위 순서. 인덱스는 아래 배열들과 같다. 0(몸통)이 기본. */
	static const EMechBodyRegion RegionOrder[] = {
		EMechBodyRegion::Torso, EMechBodyRegion::Head, EMechBodyRegion::ArmLeft,
		EMechBodyRegion::ArmRight, EMechBodyRegion::LegLeft, EMechBodyRegion::LegRight };
	static const TCHAR* RegionNames[] = { TEXT("Torso"), TEXT("Head"), TEXT("ArmLeft"), TEXT("ArmRight"), TEXT("LegLeft"), TEXT("LegRight") };
	static constexpr int32 RegionCount = UE_ARRAY_COUNT(RegionOrder);

	static FText GetFirstError(const UGeometryScriptDebug* Debug, const FText& Fallback)
	{
		for (const FGeometryScriptDebugMessage& Message : Debug->Messages)
		{
			if (Message.MessageType == EGeometryScriptDebugMessageType::ErrorMessage)
			{
				return FText::Format(LOCTEXT("SplitErrorWithDetail", "{0}\n({1})"), Fallback, Message.Message);
			}
		}
		return Fallback;
	}

	FString GetOutputFolder(const USkeletalMesh* SourceMesh)
	{
		return FPackageName::GetLongPackagePath(SourceMesh->GetPackage()->GetName()) / TEXT("BodyRegions");
	}

	/** 뼈마다 부위 인덱스. 시작 뼈 목록에 있으면 그 부위, 아니면 부모 부위(뿌리는 몸통). 뼈 목록은 부모가 자식보다 앞이다. */
	static TArray<int32> MakeBoneRegions(const TArray<FName>& BoneNames, const TArray<int32>& BoneParents, const FRootBones& Roots)
	{
		const TArray<FName>* const RootLists[RegionCount] = { nullptr, &Roots.Head, &Roots.ArmLeft, &Roots.ArmRight, &Roots.LegLeft, &Roots.LegRight };
		TArray<int32> BoneRegion;
		BoneRegion.SetNumZeroed(BoneNames.Num());
		for (int32 Bone = 0; Bone < BoneNames.Num(); ++Bone)
		{
			const int32 Parent = BoneParents[Bone];
			BoneRegion[Bone] = Parent != INDEX_NONE ? BoneRegion[Parent] : 0;
			for (int32 Region = 1; Region < RegionCount; ++Region)
			{
				if (RootLists[Region]->Contains(BoneNames[Bone]))
				{
					BoneRegion[Bone] = Region;
				}
			}
		}
		return BoneRegion;
	}

	/** 스켈레탈 메시 에셋을 새로 만들거나(없을 때) 기존 것의 메시를 바꾼다. 뼈 기준 자세는 원본 메시 것(동적 메시에 복사된 뼈 정보). */
	static USkeletalMesh* WriteSkeletalMesh(UDynamicMesh* Mesh, USkeletalMesh* SourceMesh, const FString& PackagePath,
		const TArray<UMaterialInterface*>& Materials, const TArray<FName>& SlotNames, UGeometryScriptDebug* Debug)
	{
		EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
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
			CopyOptions.NewMaterialSlotNames = SlotNames;
			CopyOptions.BoneHierarchyMismatchHandling = EGeometryScriptBoneHierarchyMismatchHandling::CreateNewReferenceSkeleton;
			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToSkeletalMesh(Mesh, Result, CopyOptions, FGeometryScriptMeshWriteLOD(), Outcome, Debug);
		}
		else
		{
			FGeometryScriptCreateNewSkeletalMeshAssetOptions CreateOptions;
			CreateOptions.bUseMeshBoneProportions = true;
			for (const TPair<FName, UMaterialInterface*>& Slot : UGeometryScriptLibrary_StaticMeshFunctions::ConvertMaterialListToMaterialMap(Materials, SlotNames))
			{
				CreateOptions.Materials.Add(Slot.Key, Slot.Value);
			}
			Result = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewSkeletalMeshAssetFromMesh(
				Mesh, SourceMesh->GetSkeleton(), PackagePath, CreateOptions, Outcome, Debug);
		}
		return Outcome == EGeometryScriptOutcomePins::Success ? Result : nullptr;
	}

	/** 세트 에셋을 찾거나 새로 만든다. */
	static UMechBodyRegionSet* FindOrCreateRegionSet(const FString& PackagePath)
	{
		const FString ShortName = FPackageName::GetShortName(PackagePath);
		const FString ObjectPath = PackagePath + TEXT(".") + ShortName;
		UMechBodyRegionSet* Set = FindObject<UMechBodyRegionSet>(nullptr, *ObjectPath);
		if (!Set && FPackageName::DoesPackageExist(PackagePath))
		{
			Set = LoadObject<UMechBodyRegionSet>(nullptr, *ObjectPath);
		}
		if (!Set)
		{
			UPackage* const Package = CreatePackage(*PackagePath);
			Set = NewObject<UMechBodyRegionSet>(Package, FName(*ShortName), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Set);
		}
		return Set;
	}

	UMechBodyRegionSet* Split(USkeletalMesh* SourceMesh, const FRootBones& Roots, FText& OutError)
	{
		if (!SourceMesh || !SourceMesh->GetSkeleton())
		{
			OutError = LOCTEXT("SplitNoMesh", "나눌 메카 메시(미리보기 설정의 Preview Mech Mesh)가 없습니다.");
			return nullptr;
		}

		UGeometryScriptDebug* const Debug = NewObject<UGeometryScriptDebug>();
		EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
		UDynamicMesh* const Source = NewObject<UDynamicMesh>();
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(
			SourceMesh, Source, FGeometryScriptCopyMeshFromAssetOptions(), FGeometryScriptMeshReadLOD(), Outcome, Debug);
		if (Outcome != EGeometryScriptOutcomePins::Success)
		{
			OutError = GetFirstError(Debug, LOCTEXT("SplitReadFailed", "메카 메시를 읽지 못했습니다."));
			return nullptr;
		}

		// 삼각형마다 부위: 세 꼭짓점의 가중치를 부위별로 더해 가장 큰 부위.
		// (10-13 2차) 가장 많이 따라가는 뼈가 부위 시작 뼈 자체면, 그 뼈 선분에서의 위치로 한 번 더 자른다(ArmSplitRatio·LegSplitRatio).
		// (10-13 3차) 몸통 삼각형 중 머리 축 가까이 목 관절보다 위에 있는 것은 머리로 옮긴다(HeadCaptureRadius).
		// 나이트는 가슴판 절반이 쇄골을 따라가서, 가중치만으로 나누면 팔을 숨길 때 가슴 절반이 같이 사라졌다.
		TArray<int32> TriangleRegion;
		bool bHasWeights = false;
		Source->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Mesh)
		{
			const UE::Geometry::FDynamicMeshAttributeSet* const Attributes = Mesh.Attributes();
			const UE::Geometry::FDynamicMeshVertexSkinWeightsAttribute* const SkinWeights = Attributes && Attributes->HasBones()
				? Attributes->GetSkinWeightsAttribute(FSkeletalMeshAttributesShared::DefaultSkinWeightProfileName) : nullptr;
			if (!SkinWeights)
			{
				return;
			}
			bHasWeights = true;
			const TArray<FName>& BoneNames = Attributes->GetBoneNames()->GetAttribValues();
			const TArray<int32>& BoneParents = Attributes->GetBoneParentIndices()->GetAttribValues();
			const TArray<int32> BoneRegion = MakeBoneRegions(BoneNames, BoneParents, Roots);
			const int32 BoneCount = BoneNames.Num();

			// 뼈마다 기본 자세 관절 위치(메시와 같은 컴포넌트 공간).
			const FReferenceSkeleton& RefSkeleton = SourceMesh->GetRefSkeleton();
			TArray<FVector> Joint;
			Joint.SetNum(BoneCount);
			for (int32 Bone = 0; Bone < BoneCount; ++Bone)
			{
				const int32 RefIndex = RefSkeleton.FindBoneIndex(BoneNames[Bone]);
				Joint[Bone] = RefIndex != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, RefIndex).GetLocation() : FVector::ZeroVector;
			}
			// 부위 시작 뼈(부모와 부위가 다른 뼈)의 선분 끝 = 가장 먼 자식 관절 (쇄골 → upperarm, 목 → head).
			TArray<int32> SegmentEnd;
			SegmentEnd.Init(INDEX_NONE, BoneCount);
			for (int32 Bone = 0; Bone < BoneCount; ++Bone)
			{
				const int32 Parent = BoneParents[Bone];
				const bool bParentIsRoot = Parent != INDEX_NONE && BoneRegion[Parent] != 0
					&& (BoneParents[Parent] == INDEX_NONE || BoneRegion[BoneParents[Parent]] != BoneRegion[Parent]);
				if (bParentIsRoot && (SegmentEnd[Parent] == INDEX_NONE
					|| FVector::DistSquared(Joint[Bone], Joint[Parent]) > FVector::DistSquared(Joint[SegmentEnd[Parent]], Joint[Parent])))
				{
					SegmentEnd[Parent] = Bone;
				}
			}

			// 머리로 끌어올 범위: 머리 부위 관절 중 가장 낮은 높이 + Offset부터 위, 머리 관절들의 수평 위치를 지나는 수직축에서 반지름 안.
			// (10-13 4차) 처음엔 '머리 시작 뼈 → 자식' 방향을 썼는데, 나이트 뼈대는 neck_01(173cm)이 head(169cm)보다 위라서
			// 방향이 아래로 나와 목 아래 몸통 전체(2.4만 개)가 머리로 갔다. 뼈 방향은 믿지 않고 위쪽(+Z)으로 고정한다.
			const int32 HeadRegion = 1;
			int32 HeadJointCount = 0;
			double HeadLowestZ = UE_BIG_NUMBER;
			FVector2D HeadAxisXY = FVector2D::ZeroVector;
			for (int32 Bone = 0; Bone < BoneCount; ++Bone)
			{
				if (BoneRegion[Bone] == HeadRegion)
				{
					HeadLowestZ = FMath::Min(HeadLowestZ, Joint[Bone].Z);
					HeadAxisXY += FVector2D(Joint[Bone].X, Joint[Bone].Y);
					++HeadJointCount;
				}
			}
			HeadAxisXY = HeadJointCount > 0 ? HeadAxisXY / HeadJointCount : HeadAxisXY;
			const double HeadCaptureZ = HeadLowestZ + Roots.HeadCaptureOffset;
			const bool bHeadCapture = HeadJointCount > 0 && Roots.HeadCaptureRadius > 0.f;
			int32 MovedToHead = 0;

			TriangleRegion.Init(0, Mesh.MaxTriangleID());
			TArray<FVector> TriangleCentroid;
			TriangleCentroid.SetNumZeroed(Mesh.MaxTriangleID());
			TArray<double> HeadDistances;
			TArray<int32> DominantCount;
			DominantCount.SetNumZeroed(BoneCount);
			TArray<FVector2D> DominantHeight;
			DominantHeight.Init(FVector2D(UE_BIG_NUMBER, -UE_BIG_NUMBER), BoneCount);
			int32 MovedToParent = 0;
			TArray<int32> Bones;
			TArray<float> Weights;
			TArray<float> BoneWeight;
			BoneWeight.SetNumZeroed(BoneCount);
			TArray<int32, TInlineAllocator<24>> Touched;
			for (const int32 TriangleId : Mesh.TriangleIndicesItr())
			{
				float RegionWeight[RegionCount] = {};
				const UE::Geometry::FIndex3i Triangle = Mesh.GetTriangle(TriangleId);
				Touched.Reset();
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					SkinWeights->GetValue(Triangle[Corner], Bones, Weights);
					for (int32 Index = 0; Index < Bones.Num(); ++Index)
					{
						if (BoneRegion.IsValidIndex(Bones[Index]))
						{
							RegionWeight[BoneRegion[Bones[Index]]] += Weights[Index];
							Touched.AddUnique(Bones[Index]);
							BoneWeight[Bones[Index]] += Weights[Index];
						}
					}
				}
				int32 Best = 0;
				for (int32 Region = 1; Region < RegionCount; ++Region)
				{
					Best = RegionWeight[Region] > RegionWeight[Best] ? Region : Best;
				}
				int32 Dominant = INDEX_NONE;
				for (const int32 Bone : Touched)
				{
					Dominant = Dominant == INDEX_NONE || BoneWeight[Bone] > BoneWeight[Dominant] ? Bone : Dominant;
				}
				for (const int32 Bone : Touched)
				{
					BoneWeight[Bone] = 0.f;
				}

				const FVector Centroid = (FVector(Mesh.GetVertex(Triangle.A)) + FVector(Mesh.GetVertex(Triangle.B)) + FVector(Mesh.GetVertex(Triangle.C))) / 3.0;
				if (Dominant != INDEX_NONE)
				{
					++DominantCount[Dominant];
					DominantHeight[Dominant].X = FMath::Min(DominantHeight[Dominant].X, Centroid.Z);
					DominantHeight[Dominant].Y = FMath::Max(DominantHeight[Dominant].Y, Centroid.Z);

					// 시작 뼈 자체에 실린 삼각형: 뼈 선분의 몸 쪽이면 부모 부위로 (팔·다리 비율 따로, 머리는 아래 규칙).
					const EMechBodyRegion DominantRegion = RegionOrder[BoneRegion[Dominant]];
					const float SplitRatio = DominantRegion == EMechBodyRegion::ArmLeft || DominantRegion == EMechBodyRegion::ArmRight ? Roots.ArmSplitRatio
						: DominantRegion == EMechBodyRegion::LegLeft || DominantRegion == EMechBodyRegion::LegRight ? Roots.LegSplitRatio : 0.f;
					if (SplitRatio > 0.f && SegmentEnd[Dominant] != INDEX_NONE && BoneRegion[Dominant] == Best && BoneParents[Dominant] != INDEX_NONE)
					{
						const FVector Axis = Joint[SegmentEnd[Dominant]] - Joint[Dominant];
						const double Fraction = Axis.SizeSquared() > UE_SMALL_NUMBER ? FVector::DotProduct(Centroid - Joint[Dominant], Axis) / Axis.SizeSquared() : 1.0;
						if (Fraction < SplitRatio)
						{
							Best = BoneRegion[BoneParents[Dominant]];
							++MovedToParent;
						}
					}
				}
				TriangleRegion[TriangleId] = Best;
				TriangleCentroid[TriangleId] = Centroid;
				// 머리 뼈를 가장 많이 따라가는 삼각형의 수평 거리(머리 폭을 재는 데 쓴다).
				if (Dominant != INDEX_NONE && BoneRegion[Dominant] == HeadRegion)
				{
					HeadDistances.Add(FVector2D::Distance(FVector2D(Centroid.X, Centroid.Y), HeadAxisXY));
				}
			}
			// (10-13 6차 시도, 되돌림) 시작 높이를 머리 삼각형 아래쪽 끝(약 164cm)으로 내려 봤는데 결과가 더 어색해서(사용자 확인)
			// head 관절 높이(169cm)부터로 둔다. 머리 안쪽에 조금 남는 부분은 그대로 둔다.

			// 몸통 삼각형 중 머리 관절 높이보다 위이고 머리 축 가까이 있는 것은 머리로 (얼굴이 가슴 뼈를 따라가는 메시용).
			// (10-13 5차) 반지름은 머리 자체의 폭(머리 뼈 삼각형 수평 거리의 95% 지점)을 넘지 않게 한다.
			// 설정 반지름(20cm)만 쓰면 머리 옆 목 가드 장식까지 머리로 가서, 몸통을 숨겨도 머리 주변이 남았다.
			double HeadFootprint = Roots.HeadCaptureRadius;
			if (!HeadDistances.IsEmpty())
			{
				HeadDistances.Sort();
				HeadFootprint = HeadDistances[FMath::Clamp(FMath::FloorToInt(HeadDistances.Num() * 0.95), 0, HeadDistances.Num() - 1)];
			}
			const double HeadCaptureRadius = FMath::Min<double>(Roots.HeadCaptureRadius, HeadFootprint);
			if (bHeadCapture)
			{
				for (const int32 TriangleId : Mesh.TriangleIndicesItr())
				{
					const FVector& Centroid = TriangleCentroid[TriangleId];
					if (TriangleRegion[TriangleId] == 0 && Centroid.Z >= HeadCaptureZ
						&& FVector2D::DistSquared(FVector2D(Centroid.X, Centroid.Y), HeadAxisXY) <= FMath::Square(HeadCaptureRadius))
					{
						TriangleRegion[TriangleId] = HeadRegion;
						++MovedToHead;
					}
				}
			}

			// 진단: 뼈마다 가장 많이 따라가는 삼각형 수와 그 삼각형들의 높이(Z, cm) 범위. 많은 순 상위 20개.
			TArray<int32> Order;
			for (int32 Bone = 0; Bone < BoneCount; ++Bone)
			{
				if (DominantCount[Bone] > 0)
				{
					Order.Add(Bone);
				}
			}
			Order.Sort([&DominantCount](int32 A, int32 B) { return DominantCount[A] > DominantCount[B]; });
			FString Dominance;
			for (int32 Index = 0; Index < Order.Num() && Index < 20; ++Index)
			{
				const int32 Bone = Order[Index];
				Dominance += FString::Printf(TEXT(" %s %d (z %.0f~%.0f, joint z %.0f, %s);"), *BoneNames[Bone].ToString(), DominantCount[Bone],
					DominantHeight[Bone].X, DominantHeight[Bone].Y, Joint[Bone].Z, RegionNames[BoneRegion[Bone]]);
			}
			UE_LOG(LogMechBodyRegionSplitter, Display, TEXT("%s: dominant bone per triangle:%s"), *SourceMesh->GetName(), *Dominance);
			UE_LOG(LogMechBodyRegionSplitter, Display, TEXT("%s: %d triangles on a region root bone kept in the parent region (arm ratio %.2f, leg ratio %.2f)."),
				*SourceMesh->GetName(), MovedToParent, Roots.ArmSplitRatio, Roots.LegSplitRatio);
			UE_LOG(LogMechBodyRegionSplitter, Display, TEXT("%s: %d torso triangles moved to the head (above z %.0f, within %.1f cm of x %.0f y %.0f = min(setting %.0f, head footprint %.1f from %d head triangles); offset %.0f cm)."),
				*SourceMesh->GetName(), MovedToHead, HeadCaptureZ, HeadCaptureRadius, HeadAxisXY.X, HeadAxisXY.Y,
				Roots.HeadCaptureRadius, HeadFootprint, HeadDistances.Num(), Roots.HeadCaptureOffset);
		});
		if (!bHasWeights)
		{
			OutError = LOCTEXT("SplitNoWeights", "메카 메시에 스킨 가중치가 없어 부위를 나눌 수 없습니다.");
			return nullptr;
		}

		TArray<UMaterialInterface*> Materials;
		TArray<FName> SlotNames;
		UGeometryScriptLibrary_StaticMeshFunctions::GetMaterialListFromSkeletalMesh(SourceMesh, Materials, SlotNames, Debug);

		const FString Folder = GetOutputFolder(SourceMesh);
		const FString SourceName = SourceMesh->GetName();
		TArray<FMechBodyRegionMesh> RegionMeshes;
		TArray<UPackage*> PackagesToSave;
		FString Summary;
		for (int32 Region = 0; Region < RegionCount; ++Region)
		{
			// 원본을 복사한 뒤 다른 부위 삼각형을 지운다. 경계 정점은 부위마다 따로 생기지만 위치·가중치·노멀이 같아 틈이 보이지 않는다.
			UDynamicMesh* const RegionMesh = NewObject<UDynamicMesh>();
			Source->ProcessMesh([RegionMesh](const UE::Geometry::FDynamicMesh3& Mesh) { RegionMesh->SetMesh(Mesh); });
			int32 TriangleCount = 0;
			RegionMesh->EditMesh([&TriangleRegion, Region, &TriangleCount](UE::Geometry::FDynamicMesh3& Mesh)
			{
				TArray<int32> ToRemove;
				for (const int32 TriangleId : Mesh.TriangleIndicesItr())
				{
					if (TriangleRegion[TriangleId] != Region)
					{
						ToRemove.Add(TriangleId);
					}
				}
				for (const int32 TriangleId : ToRemove)
				{
					Mesh.RemoveTriangle(TriangleId, /*bRemoveIsolatedVertices=*/true, /*bPreserveManifold=*/false);
				}
				Mesh.CompactInPlace();
				TriangleCount = Mesh.TriangleCount();
			});
			Summary += FString::Printf(TEXT(" %s %d"), RegionNames[Region], TriangleCount);
			if (TriangleCount == 0)
			{
				continue;
			}

			const FString PackagePath = Folder / FString::Printf(TEXT("SK_%s_%s"), *SourceName, RegionNames[Region]);
			USkeletalMesh* const Written = WriteSkeletalMesh(RegionMesh, SourceMesh, PackagePath, Materials, SlotNames, Debug);
			if (!Written)
			{
				OutError = GetFirstError(Debug, FText::Format(LOCTEXT("SplitWriteFailed", "부위 메시를 만들지 못했습니다: {0}"), FText::FromString(PackagePath)));
				return nullptr;
			}
			FMechBodyRegionMesh& Entry = RegionMeshes.AddDefaulted_GetRef();
			Entry.Region = RegionOrder[Region];
			Entry.Mesh = Written;
			PackagesToSave.Add(Written->GetPackage());
		}
		UE_LOG(LogMechBodyRegionSplitter, Display, TEXT("%s: split into body regions (triangles):%s"), *SourceName, *Summary);

		UMechBodyRegionSet* const Set = FindOrCreateRegionSet(Folder / FString::Printf(TEXT("DA_%s_BodyRegions"), *SourceName));
		Set->Modify();
		Set->SourceMesh = SourceMesh;
		Set->Regions = RegionMeshes;
		Set->MarkPackageDirty();
		PackagesToSave.Add(Set->GetPackage());

		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, /*bCheckDirty=*/false, /*bPromptToSave=*/false);
		return Set;
	}

	UMechBodyRegionSet* FindRegionSet(const USkeletalMesh* SourceMesh)
	{
		if (!SourceMesh)
		{
			return nullptr;
		}
		FARFilter Filter;
		Filter.ClassPaths.Add(UMechBodyRegionSet::StaticClass()->GetClassPathName());
		TArray<FAssetData> Assets;
		IAssetRegistry::GetChecked().GetAssets(Filter, Assets);
		const FSoftObjectPath SourcePath(SourceMesh);
		for (const FAssetData& Asset : Assets)
		{
			UMechBodyRegionSet* const Set = Cast<UMechBodyRegionSet>(Asset.GetAsset());
			if (Set && Set->SourceMesh.ToSoftObjectPath() == SourcePath)
			{
				return Set;
			}
		}
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
