#include "knightPartShellBaker.h"

#include "AssetUtils/CreateSkeletalMeshUtil.h"
#include "BoneWeights.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "DynamicMesh/MeshNormals.h"
#include "Engine/SkeletalMesh.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "MeshSimplification.h"
#include "Monster/knightMonsterPartData.h"
#include "SkeletalMeshAttributes.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightPartShell, Log, All);

using namespace UE::Geometry;
using UE::AnimationCore::FBoneWeights;

namespace
{
	/** 머티리얼이 부위별 값을 이만큼 받는다(Opacity0~7, Color0~7). */
	constexpr int32 MaxShellParts = 8;

	/** 뼈마다 속한 부위 번호(Parts 인덱스). 부위 뼈가 아니면 부모로 올라가며 찾는다(발가락 → 발). 없으면 INDEX_NONE. */
	TArray<int32> BuildBonePartMap(const FReferenceSkeleton& RefSkeleton, const UknightMonsterPartData& PartData)
	{
		TMap<FName, int32> Direct;
		for (int32 PartIndex = 0; PartIndex < PartData.Parts.Num(); ++PartIndex)
		{
			for (const FName& Bone : PartData.Parts[PartIndex].Bones)
			{
				Direct.Add(Bone, PartIndex);
			}
		}
		TArray<int32> Result;
		Result.Init(INDEX_NONE, RefSkeleton.GetRawBoneNum());
		for (int32 Bone = 0; Bone < Result.Num(); ++Bone)
		{
			for (int32 Current = Bone; Current != INDEX_NONE; Current = RefSkeleton.GetRawParentIndex(Current))
			{
				if (const int32* const Part = Direct.Find(RefSkeleton.GetRawRefBoneInfo()[Current].Name))
				{
					Result[Bone] = *Part;
					break;
				}
			}
		}
		return Result;
	}

	/** 가장 큰 가중치를 가진 뼈의 부위. */
	int32 GetVertexPart(const FDynamicMeshVertexSkinWeightsAttribute& Skin, int32 Vertex, const TArray<int32>& BonePart)
	{
		FBoneWeights Weights;
		Skin.GetValue(Vertex, Weights);
		int32 BestBone = INDEX_NONE;
		float BestWeight = -1.f;
		for (int32 Index = 0; Index < Weights.Num(); ++Index)
		{
			if (Weights[Index].GetWeight() > BestWeight)
			{
				BestWeight = Weights[Index].GetWeight();
				BestBone = Weights[Index].GetBoneIndex();
			}
		}
		return BonePart.IsValidIndex(BestBone) ? BonePart[BestBone] : INDEX_NONE;
	}

	/** 단순화를 막는 UV·법선 이음매 등을 지우고 위치, 삼각형, 스킨 가중치, 뼈만 남긴다. */
	void StripForSimplify(FDynamicMesh3& Mesh)
	{
		FDynamicMeshAttributeSet* const Attributes = Mesh.Attributes();
		Attributes->SetNumUVLayers(0);
		Attributes->SetNumNormalLayers(0);
		Attributes->DisableTangents();
		Attributes->DisablePrimaryColors();
		Attributes->DisableMaterialID();
		Attributes->SetNumPolygroupLayers(0);
		Attributes->SetNumWeightLayers(0);
	}

	/** 굽기 결과를 모으는 셸 정점(삼각형마다 꼭짓점을 따로 둔다). */
	struct FShellCorner
	{
		FVector3d Position;
		FVector3f Normal;
		FBoneWeights Weights;
		FVector2f Bary;
		FVector4f EdgeFlags;
		float Part;
		/** 부위 길이 축 위의 위치(원점 = 부위 중심). 굽기 끝에 부위 안 0~1 비율과 띠 단위로 바꾼다. */
		double Axis;
		/** 길이 축 둘레 각도(라디안). 굽기 끝에 둘레 칸 단위로 바꾼다. */
		double Angle;
	};

	/** 부위의 길이 방향 축(정점 분포의 주성분). 몸 중심에 가까운 끝이 음수 쪽이 되게 방향을 맞춘다. */
	FVector3d ComputePartAxis(const FDynamicMesh3& Mesh, const FVector3d& Centroid, const FVector3d& BodyCenter)
	{
		double C[3][3] = {};
		for (const int32 Vertex : Mesh.VertexIndicesItr())
		{
			const FVector3d D = Mesh.GetVertex(Vertex) - Centroid;
			for (int32 Row = 0; Row < 3; ++Row)
			{
				for (int32 Col = 0; Col < 3; ++Col)
				{
					C[Row][Col] += D[Row] * D[Col];
				}
			}
		}
		// 거듭제곱법으로 가장 큰 고유벡터.
		FVector3d Axis(0.31, 0.57, 0.76);
		for (int32 Step = 0; Step < 32; ++Step)
		{
			const FVector3d Next(
				C[0][0] * Axis.X + C[0][1] * Axis.Y + C[0][2] * Axis.Z,
				C[1][0] * Axis.X + C[1][1] * Axis.Y + C[1][2] * Axis.Z,
				C[2][0] * Axis.X + C[2][1] * Axis.Y + C[2][2] * Axis.Z);
			if (Next.SquaredLength() < UE_DOUBLE_SMALL_NUMBER)
			{
				break;
			}
			Axis = Next.GetSafeNormal();
		}
		return (Centroid - BodyCenter).Dot(Axis) < 0.0 ? -Axis : Axis;
	}
}

USkeletalMesh* UknightPartShellBaker::BakePartShell(USkeletalMesh* SourceMesh, UknightMonsterPartData* PartData,
	const FString& AssetPath, int32 TrianglesPerPart, float FeatureAngle, int32 BandsPerPart)
{
	if (!SourceMesh || !PartData || AssetPath.IsEmpty())
	{
		UE_LOG(LogKnightPartShell, Error, TEXT("BakePartShell: 메시, 부위 데이터, 경로가 모두 필요하다."));
		return nullptr;
	}

	FMeshDescription Description;
	if (!SourceMesh->CloneMeshDescription(0, Description))
	{
		UE_LOG(LogKnightPartShell, Error, TEXT("BakePartShell: %s LOD0 메시 설명을 읽지 못했다."), *SourceMesh->GetName());
		return nullptr;
	}
	FDynamicMesh3 Full;
	{
		FMeshDescriptionToDynamicMesh Converter;
		Converter.Convert(&Description, Full);
	}
	const FName SkinProfile = FSkeletalMeshAttributes::DefaultSkinWeightProfileName;
	const FDynamicMeshVertexSkinWeightsAttribute* const FullSkin = Full.HasAttributes() ? Full.Attributes()->GetSkinWeightsAttribute(SkinProfile) : nullptr;
	if (!FullSkin)
	{
		UE_LOG(LogKnightPartShell, Error, TEXT("BakePartShell: %s에 스킨 가중치가 없다."), *SourceMesh->GetName());
		return nullptr;
	}

	// 파괴 가능한 부위만 셸이 된다. 셸은 '부술 수 있는 곳'이라는 표시다.
	TArray<int32> ShellPartIndices;
	TArray<FGameplayTag> ShellTags;
	for (int32 PartIndex = 0; PartIndex < PartData->Parts.Num() && ShellPartIndices.Num() < MaxShellParts; ++PartIndex)
	{
		if (PartData->Parts[PartIndex].bBreakable)
		{
			ShellPartIndices.Add(PartIndex);
			ShellTags.Add(PartData->Parts[PartIndex].PartTag);
		}
	}

	const TArray<int32> BonePart = BuildBonePartMap(SourceMesh->GetRefSkeleton(), *PartData);
	TArray<int32> TrianglePart;
	TrianglePart.Init(INDEX_NONE, Full.MaxTriangleID());
	for (const int32 Triangle : Full.TriangleIndicesItr())
	{
		const FIndex3i Corners = Full.GetTriangle(Triangle);
		const int32 A = GetVertexPart(*FullSkin, Corners.A, BonePart);
		const int32 B = GetVertexPart(*FullSkin, Corners.B, BonePart);
		const int32 C = GetVertexPart(*FullSkin, Corners.C, BonePart);
		// 세 꼭짓점 중 둘 이상이 같은 부위면 그 부위, 모두 다르면 첫 꼭짓점.
		TrianglePart[Triangle] = (B == C) ? B : A;
	}

	FVector3d BodyCenter = FVector3d::ZeroVector;
	for (const int32 Vertex : Full.VertexIndicesItr())
	{
		BodyCenter += Full.GetVertex(Vertex);
	}
	BodyCenter /= FMath::Max(Full.VertexCount(), 1);
	TArray<double> PartRadius;
	PartRadius.Init(0.0, ShellPartIndices.Num());
	TArray<FVector2d> PartAxisRange;
	PartAxisRange.Init(FVector2d(TNumericLimits<double>::Max(), TNumericLimits<double>::Lowest()), ShellPartIndices.Num());

	const double FeatureCos = FMath::Cos(FMath::DegreesToRadians(static_cast<double>(FeatureAngle)));
	TArray<FShellCorner> Corners;
	for (int32 ShellIndex = 0; ShellIndex < ShellPartIndices.Num(); ++ShellIndex)
	{
		const int32 PartIndex = ShellPartIndices[ShellIndex];
		FDynamicMesh3 PartMesh(Full);
		StripForSimplify(PartMesh);
		for (const int32 Triangle : Full.TriangleIndicesItr())
		{
			if (TrianglePart[Triangle] != PartIndex)
			{
				PartMesh.RemoveTriangle(Triangle, true, false);
			}
		}
		const int32 SourceTriangles = PartMesh.TriangleCount();
		if (SourceTriangles == 0)
		{
			UE_LOG(LogKnightPartShell, Warning, TEXT("BakePartShell: %s 부위에 삼각형이 없다(뼈 가중치 확인)."), *ShellTags[ShellIndex].ToString());
			continue;
		}
		{
			FQEMSimplification Simplifier(&PartMesh);
			Simplifier.SimplifyToTriangleCount(FMath::Max(TrianglesPerPart, 8));
		}

		FMeshNormals Normals(&PartMesh);
		Normals.ComputeVertexNormals();
		const FDynamicMeshVertexSkinWeightsAttribute* const PartSkin = PartMesh.Attributes()->GetSkinWeightsAttribute(SkinProfile);
		FVector3d PartCentroid = FVector3d::ZeroVector;
		for (const int32 Vertex : PartMesh.VertexIndicesItr())
		{
			PartCentroid += PartMesh.GetVertex(Vertex);
		}
		PartCentroid /= FMath::Max(PartMesh.VertexCount(), 1);
		const FVector3d PartAxis = ComputePartAxis(PartMesh, PartCentroid, BodyCenter);
		// 둘레 각도의 기준 방향. 축과 평행하지 않은 아무 방향이면 된다.
		const FVector3d Reference = FMath::Abs(PartAxis.Z) < 0.9 ? FVector3d::UnitZ() : FVector3d::UnitX();
		const FVector3d Side = PartAxis.Cross(Reference).GetSafeNormal();
		const FVector3d Up = PartAxis.Cross(Side);
		double RadiusSum = 0.0;
		for (const int32 Vertex : PartMesh.VertexIndicesItr())
		{
			const FVector3d D = PartMesh.GetVertex(Vertex) - PartCentroid;
			RadiusSum += (D - PartAxis * D.Dot(PartAxis)).Length();
		}
		PartRadius[ShellIndex] = RadiusSum / FMath::Max(PartMesh.VertexCount(), 1);

		auto IsFeatureEdge = [&](int32 Edge, int32 Triangle, const FVector3d& Normal)
		{
			const FIndex2i EdgeTriangles = PartMesh.GetEdgeT(Edge);
			const int32 Other = EdgeTriangles.A == Triangle ? EdgeTriangles.B : EdgeTriangles.A;
			if (Other == IndexConstants::InvalidID)
			{
				return true;
			}
			return Normal.Dot(PartMesh.GetTriNormal(Other)) < FeatureCos;
		};

		for (const int32 Triangle : PartMesh.TriangleIndicesItr())
		{
			const FIndex3i Tri = PartMesh.GetTriangle(Triangle);
			const FIndex3i Edges = PartMesh.GetTriEdges(Triangle);
			const FVector3d Normal = PartMesh.GetTriNormal(Triangle);
			// GetTriEdges의 j번 모서리는 (j, j+1) 꼭짓점을 잇는다. i번 꼭짓점 맞은편은 (i+1)번 모서리.
			const FVector4f Flags(
				IsFeatureEdge(Edges[1], Triangle, Normal) ? 1.f : 0.f,
				IsFeatureEdge(Edges[2], Triangle, Normal) ? 1.f : 0.f,
				IsFeatureEdge(Edges[0], Triangle, Normal) ? 1.f : 0.f,
				1.f);
			const FVector2f Bary[3] = {FVector2f(1.f, 0.f), FVector2f(0.f, 1.f), FVector2f(0.f, 0.f)};
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				FShellCorner& Out = Corners.AddDefaulted_GetRef();
				Out.Position = PartMesh.GetVertex(Tri[Corner]);
				Out.Normal = FVector3f(Normals[Tri[Corner]]);
				PartSkin->GetValue(Tri[Corner], Out.Weights);
				Out.Bary = Bary[Corner];
				Out.EdgeFlags = Flags;
				Out.Part = static_cast<float>(ShellIndex);
				Out.Axis = (Out.Position - PartCentroid).Dot(PartAxis);
				Out.Angle = FMath::Atan2((Out.Position - PartCentroid).Dot(Up), (Out.Position - PartCentroid).Dot(Side));
				PartAxisRange[ShellIndex].X = FMath::Min(PartAxisRange[ShellIndex].X, Out.Axis);
				PartAxisRange[ShellIndex].Y = FMath::Max(PartAxisRange[ShellIndex].Y, Out.Axis);
			}
		}
		UE_LOG(LogKnightPartShell, Display, TEXT("BakePartShell: %s %d -> %d 삼각형"), *ShellTags[ShellIndex].ToString(),
			SourceTriangles, PartMesh.TriangleCount());
	}
	if (Corners.IsEmpty())
	{
		UE_LOG(LogKnightPartShell, Error, TEXT("BakePartShell: 셸로 만들 파괴 가능한 부위가 없다."));
		return nullptr;
	}

	// 띠 간격은 모든 부위에 같은 길이로 둔다(머리와 꼬리의 줄 간격이 같아야 한 스캔으로 읽힌다). 평균 부위 길이를 BandsPerPart로 나눈다.
	double LengthSum = 0.0;
	int32 LengthCount = 0;
	for (const FVector2d& Range : PartAxisRange)
	{
		if (Range.Y > Range.X)
		{
			LengthSum += Range.Y - Range.X;
			++LengthCount;
		}
	}
	const double BandSpacing = FMath::Max(LengthSum / FMath::Max(LengthCount, 1) / FMath::Max(BandsPerPart, 1), 1.0);
	// 삼각형마다 꼭짓점이 따로라서, 각도가 -π/π에서 끊기는 곳은 첫 꼭짓점 기준으로 이어 붙이면 이음매가 생기지 않는다.
	for (int32 Index = 0; Index + 2 < Corners.Num(); Index += 3)
	{
		for (int32 Corner = 1; Corner < 3; ++Corner)
		{
			double& Angle = Corners[Index + Corner].Angle;
			const double Base = Corners[Index].Angle;
			Angle += (Angle - Base > UE_DOUBLE_PI) ? -UE_DOUBLE_TWO_PI : (Base - Angle > UE_DOUBLE_PI) ? UE_DOUBLE_TWO_PI : 0.0;
		}
	}
	for (FShellCorner& Corner : Corners)
	{
		const FVector2d& Range = PartAxisRange[static_cast<int32>(Corner.Part)];
		const double Length = FMath::Max(Range.Y - Range.X, UE_DOUBLE_SMALL_NUMBER);
		// UV0.x = 부위 안 0~1(흐르는 스캔 빛), UV1.y = 띠 단위 거리(등고선).
		// 둘레 칸 수는 짝수로 맞춰 한 바퀴 돌았을 때 육각 무늬가 어긋나지 않게 한다.
		const double Around = 2.0 * FMath::Max(FMath::RoundToDouble(UE_DOUBLE_PI * PartRadius[static_cast<int32>(Corner.Part)] / BandSpacing), 2.0);
		// UV0.y = 둘레 위치(띠 단위, 육각 무늬).
		Corner.Bary = FVector2f(static_cast<float>((Corner.Axis - Range.X) / Length),
			static_cast<float>(Corner.Angle / UE_DOUBLE_TWO_PI * Around));
		Corner.Axis = (Corner.Axis - Range.X) / BandSpacing;
	}

	FDynamicMesh3 Shell;
	for (const FShellCorner& Corner : Corners)
	{
		Shell.AppendVertex(Corner.Position);
	}
	for (int32 Index = 0; Index + 2 < Corners.Num(); Index += 3)
	{
		Shell.AppendTriangle(Index, Index + 1, Index + 2);
	}
	Shell.EnableAttributes();
	FDynamicMeshAttributeSet* const Attributes = Shell.Attributes();
	Attributes->SetNumUVLayers(2);
	Attributes->EnablePrimaryColors();
	Attributes->CopyBoneAttributes(*Full.Attributes());
	FDynamicMeshUVOverlay* const BaryUV = Attributes->GetUVLayer(0);
	FDynamicMeshUVOverlay* const PartUV = Attributes->GetUVLayer(1);
	FDynamicMeshNormalOverlay* const NormalOverlay = Attributes->PrimaryNormals();
	FDynamicMeshColorOverlay* const ColorOverlay = Attributes->PrimaryColors();
	for (const int32 Triangle : Shell.TriangleIndicesItr())
	{
		FIndex3i Bary, Part, Normal, Color;
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const FShellCorner& Source = Corners[Triangle * 3 + Corner];
			Bary[Corner] = BaryUV->AppendElement(Source.Bary);
			Part[Corner] = PartUV->AppendElement(FVector2f(Source.Part, static_cast<float>(Source.Axis)));
			Normal[Corner] = NormalOverlay->AppendElement(Source.Normal);
			Color[Corner] = ColorOverlay->AppendElement(Source.EdgeFlags);
		}
		BaryUV->SetTriangle(Triangle, Bary);
		PartUV->SetTriangle(Triangle, Part);
		NormalOverlay->SetTriangle(Triangle, Normal);
		ColorOverlay->SetTriangle(Triangle, Color);
	}
	FDynamicMeshVertexSkinWeightsAttribute* const ShellSkin = new FDynamicMeshVertexSkinWeightsAttribute(&Shell);
	ShellSkin->Initialize();
	for (int32 Vertex = 0; Vertex < Corners.Num(); ++Vertex)
	{
		ShellSkin->SetValue(Vertex, Corners[Vertex].Weights);
	}
	Attributes->AttachSkinWeightsAttribute(SkinProfile, ShellSkin);

	// 같은 경로에 있으면 옛 에셋을 치우고 같은 패키지에 새로 만든다.
	const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
	UPackage* const ExistingPackage = FindPackage(nullptr, *AssetPath) ? FindPackage(nullptr, *AssetPath) : LoadPackage(nullptr, *AssetPath, LOAD_NoWarn | LOAD_Quiet);
	if (ExistingPackage)
	{
		if (UObject* const Existing = FindObject<UObject>(ExistingPackage, *AssetName))
		{
			Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			Existing->ClearFlags(RF_Public | RF_Standalone);
			Existing->MarkAsGarbage();
		}
	}

	FReferenceSkeleton RefSkeleton = SourceMesh->GetRefSkeleton();
	UE::AssetUtils::FSkeletalMeshAssetOptions Options;
	Options.UsePackage = ExistingPackage;
	Options.NewAssetPath = AssetPath;
	Options.Skeleton = SourceMesh->GetSkeleton();
	Options.RefSkeleton = &RefSkeleton;
	Options.NumSourceModels = 1;
	Options.NumMaterialSlots = 1;
	Options.bEnableRecomputeNormals = false;
	Options.bEnableRecomputeTangents = true;
	Options.SourceMeshes.DynamicMeshes.Add(&Shell);
	UE::AssetUtils::FSkeletalMeshResults Results;
	if (UE::AssetUtils::CreateSkeletalMeshAsset(Options, Results) != UE::AssetUtils::ECreateSkeletalMeshResult::Ok || !Results.SkeletalMesh)
	{
		UE_LOG(LogKnightPartShell, Error, TEXT("BakePartShell: %s 스켈레탈 메시를 만들지 못했다."), *AssetPath);
		return nullptr;
	}

	PartData->Modify();
	PartData->PartShellMesh = Results.SkeletalMesh;
	PartData->PartShellParts = ShellTags;
	PartData->MarkPackageDirty();
	UE_LOG(LogKnightPartShell, Display, TEXT("BakePartShell: %s 완료(부위 %d개, 삼각형 %d)"), *AssetPath, ShellTags.Num(), Shell.TriangleCount());
	return Results.SkeletalMesh;
}
