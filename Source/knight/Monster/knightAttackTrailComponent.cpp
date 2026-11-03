#include "Monster/knightAttackTrailComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "DynamicMeshBuilder.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "PrimitiveSceneProxy.h"
#include "SceneInterface.h"
#include "SceneManagement.h"

namespace
{
	/** 기록한 두 프레임 사이를 이만큼(월드 cm)마다 나눠 곡선으로 메운다. */
	constexpr float SubdivideLength = 30.f;
	constexpr int32 MaxSubdivisions = 12;
	/** 애니메이션에서 직접 읽는 프레임 사이 중간 점의 최대 개수. */
	constexpr int32 MaxInBetweenSamples = 16;

	struct FTrailRenderData
	{
		TArray<FDynamicMeshVertex> Vertices;
		TArray<uint32> Indices;
	};

	class FAttackTrailSceneProxy final : public FPrimitiveSceneProxy
	{
	public:
		FAttackTrailSceneProxy(UknightAttackTrailComponent* Component)
			: FPrimitiveSceneProxy(Component)
			, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetShaderPlatform()))
		{
			Material = Component->GetMaterial(0);
			if (!Material)
			{
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}
		}

		virtual SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}

		void SetData_RenderThread(FTrailRenderData* NewData)
		{
			Data.Reset(NewData);
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			if (!Data.IsValid() || Data->Indices.Num() == 0)
			{
				return;
			}
			const FMaterialRenderProxy* const MaterialProxy = Material->GetRenderProxy();
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					FDynamicMeshBuilder Builder(Views[ViewIndex]->GetFeatureLevel());
					Builder.AddVertices(Data->Vertices);
					Builder.AddTriangles(Data->Indices);
					Builder.GetMesh(GetLocalToWorld(), MaterialProxy, SDPG_World, true, false, ViewIndex, Collector);
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View);
			Result.bShadowRelevance = false;
			Result.bDynamicRelevance = true;
			Result.bRenderInMainPass = ShouldRenderInMainPass();
			MaterialRelevance.SetPrimitiveViewRelevance(Result);
			return Result;
		}

		virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }

	private:
		UMaterialInterface* Material = nullptr;
		FMaterialRelevance MaterialRelevance;
		TUniquePtr<FTrailRenderData> Data;
	};

	FVector CatmullRom(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float T)
	{
		const float T2 = T * T;
		const float T3 = T2 * T;
		return 0.5f * ((2.f * P1) + (P2 - P0) * T + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2 + (3.f * P1 - P0 - 3.f * P2 + P3) * T3);
	}
}

UknightAttackTrailComponent::UknightAttackTrailComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 애니메이션이 뼈를 움직인 뒤에 읽어야 이번 프레임 자세가 들어간다.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	bTickInEditor = true;
	bAutoActivate = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	CastShadow = false;
	bReceivesDecals = false;
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

UknightAttackTrailComponent* UknightAttackTrailComponent::SpawnOn(USkeletalMeshComponent* InSource, UMaterialInterface* Material, const FSettings& InSettings)
{
	if (!InSource || !InSource->GetWorld() || InSource->GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	UknightAttackTrailComponent* const Trail = NewObject<UknightAttackTrailComponent>(InSource->GetOwner() ? static_cast<UObject*>(InSource->GetOwner()) : InSource);
	Trail->SetMaterial(0, Material);
	Trail->RegisterComponent();
	Trail->Start(InSource, InSettings);
	return Trail;
}

void UknightAttackTrailComponent::Start(USkeletalMeshComponent* InSource, const FSettings& InSettings)
{
	Source = InSource;
	Settings = InSettings;
	Settings.Lifetime = FMath::Max(Settings.Lifetime, 0.01f);
	bEmitting = true;
	StartTime = GetNow();
	Prev = FPrevFrame();
	BuildChain(Settings.BaseSocket, BaseChain);
	BuildChain(Settings.TipSocket, TipChain);

	FVector Base, Tip;
	if (ReadSource(Base, Tip))
	{
		AddInBetweenSamples(Base, Tip, StartTime, 0.f);
		Samples.Add({ Base, Tip, StartTime, 0.f });
	}
}

void UknightAttackTrailComponent::Stop()
{
	bEmitting = false;
}

float UknightAttackTrailComponent::GetNow() const
{
	const UWorld* const World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

bool UknightAttackTrailComponent::ReadSource(FVector& OutBase, FVector& OutTip) const
{
	const USkeletalMeshComponent* const Mesh = Source.Get();
	if (!Mesh || !Mesh->DoesSocketExist(Settings.BaseSocket) || !Mesh->DoesSocketExist(Settings.TipSocket))
	{
		return false;
	}
	MakeEdge(Mesh->GetSocketLocation(Settings.BaseSocket), Mesh->GetSocketLocation(Settings.TipSocket), OutBase, OutTip);
	return true;
}

void UknightAttackTrailComponent::MakeEdge(const FVector& RawBase, const FVector& RawTip, FVector& OutBase, FVector& OutTip) const
{
	const FVector Dir = (RawTip - RawBase).GetSafeNormal();
	OutBase = FMath::Lerp(RawBase, RawTip, Settings.BaseRatio);
	OutTip = RawTip + Dir * Settings.TipExtension;
}

void UknightAttackTrailComponent::BuildChain(FName Bone, TArray<int32>& OutChain) const
{
	OutChain.Reset();
	const USkeletalMeshComponent* const Mesh = Source.Get();
	const USkeletalMesh* const Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	// 뼈 번호 변환 함수가 const가 아니다.
	USkeleton* const Skeleton = Asset ? const_cast<USkeleton*>(Asset->GetSkeleton()) : nullptr;
	if (!Skeleton)
	{
		return;
	}
	const FReferenceSkeleton& RefSkeleton = Asset->GetRefSkeleton();
	// 루트는 루트 모션이 실릴 수 있어 기본 자세로 두고, 그 아래 뼈만 애니메이션에서 읽는다.
	for (int32 MeshIndex = RefSkeleton.FindBoneIndex(Bone); MeshIndex > 0; MeshIndex = RefSkeleton.GetParentIndex(MeshIndex))
	{
		const int32 SkeletonIndex = Skeleton->GetSkeletonBoneIndexFromMeshBoneIndex(Asset, MeshIndex);
		if (SkeletonIndex == INDEX_NONE)
		{
			OutChain.Reset();
			return;
		}
		OutChain.Insert(SkeletonIndex, 0);
	}
}

bool UknightAttackTrailComponent::EvaluateAt(const UAnimMontage* Montage, float Position, const FTransform& MeshToWorld, FVector& OutBase, FVector& OutTip) const
{
	const USkeletalMeshComponent* const Mesh = Source.Get();
	const USkeletalMesh* const Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || !Montage || Montage->SlotAnimTracks.Num() == 0 || BaseChain.Num() == 0 || TipChain.Num() == 0)
	{
		return false;
	}
	const FAnimSegment* const Segment = Montage->SlotAnimTracks[0].AnimTrack.GetSegmentAtTime(Position);
	const UAnimSequence* const Sequence = Segment ? Cast<UAnimSequence>(Segment->GetAnimReference()) : nullptr;
	if (!Sequence)
	{
		return false;
	}
	const FAnimExtractContext Context(static_cast<double>(Segment->ConvertTrackPosToAnimPos(Position)));
	const FTransform Root = Asset->GetRefSkeleton().GetRefBonePose()[0];
	auto Evaluate = [&](const TArray<int32>& Chain)
	{
		FTransform ComponentSpace = Root;
		for (const int32 SkeletonIndex : Chain)
		{
			FTransform Local;
			Sequence->GetBoneTransform(Local, FSkeletonPoseBoneIndex(SkeletonIndex), Context, false);
			ComponentSpace = Local * ComponentSpace;
		}
		return MeshToWorld.TransformPosition(ComponentSpace.GetLocation());
	};
	MakeEdge(Evaluate(BaseChain), Evaluate(TipChain), OutBase, OutTip);
	return true;
}

void UknightAttackTrailComponent::AddInBetweenSamples(const FVector& Base, const FVector& Tip, float Now, float Strength)
{
	const USkeletalMeshComponent* const Mesh = Source.Get();
	const UAnimInstance* const AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	const FAnimMontageInstance* const MontageInstance = AnimInstance ? AnimInstance->GetActiveMontageInstance() : nullptr;
	const UAnimMontage* const Montage = MontageInstance ? MontageInstance->Montage.Get() : nullptr;
	if (!Montage)
	{
		Prev = FPrevFrame();
		return;
	}
	const float Position = MontageInstance->GetPosition();
	const FTransform MeshToWorld = Mesh->GetComponentTransform();

	FPrevFrame Current;
	FVector EvalBase, EvalTip;
	if (EvaluateAt(Montage, Position, MeshToWorld, EvalBase, EvalTip))
	{
		Current.Montage = Montage;
		Current.MontagePosition = Position;
		Current.MeshToWorld = MeshToWorld;
		Current.BaseError = Base - EvalBase;
		Current.TipError = Tip - EvalTip;
		Current.bValid = true;
	}

	if (Current.bValid && Prev.bValid && Prev.Montage == Montage && Position > Prev.MontagePosition && Samples.Num() > 0)
	{
		const FSample Last = Samples.Last();
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(FVector::Dist(Last.Tip, Tip) / SubdivideLength), 1, MaxInBetweenSamples);
		for (int32 Step = 1; Step < Steps; ++Step)
		{
			const float Alpha = static_cast<float>(Step) / Steps;
			FTransform Blended;
			Blended.Blend(Prev.MeshToWorld, MeshToWorld, Alpha);
			if (EvaluateAt(Montage, FMath::Lerp(Prev.MontagePosition, Position, Alpha), Blended, EvalBase, EvalTip))
			{
				Samples.Add({ EvalBase + FMath::Lerp(Prev.BaseError, Current.BaseError, Alpha), EvalTip + FMath::Lerp(Prev.TipError, Current.TipError, Alpha),
					FMath::Lerp(Last.Time, Now, Alpha), FMath::Lerp(Last.Strength, Strength, Alpha) });
			}
		}
	}
	Prev = Current;
}

void UknightAttackTrailComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float Now = GetNow();
	FVector Base, Tip;
	if (bEmitting && ReadSource(Base, Tip))
	{
		float Strength = Settings.FadeTime > 0.f ? FMath::Clamp((Now - StartTime) / Settings.FadeTime, 0.f, 1.f) : 1.f;
		if (Settings.FullSpeed > Settings.MinSpeed && Samples.Num() > 0)
		{
			const float Elapsed = Now - Samples.Last().Time;
			// 일시정지로 시간이 흐르지 않은 프레임은 속도를 잴 수 없으니 건너뛴다.
			if (Elapsed <= KINDA_SMALL_NUMBER)
			{
				Strength = -1.f;
			}
			else
			{
				const float Speed = FVector::Dist(Tip, Samples.Last().Tip) / Elapsed;
				Strength *= FMath::SmoothStep(Settings.MinSpeed, Settings.FullSpeed, Speed);
			}
		}
		if (Strength >= 0.f)
		{
			AddInBetweenSamples(Base, Tip, Now, Strength);
			Samples.Add({ Base, Tip, Now, Strength });
		}
	}
	else if (bEmitting)
	{
		bEmitting = false;
	}

	// 뒤쪽 두 점이 모두 수명을 넘겼을 때만 지운다. 하나만 넘긴 점은 사라지는 끝을 이어 주는 데 쓴다.
	int32 NumExpired = 0;
	while (NumExpired + 1 < Samples.Num() && Now - Samples[NumExpired + 1].Time >= Settings.Lifetime)
	{
		++NumExpired;
	}
	if (NumExpired > 0)
	{
		Samples.RemoveAt(0, NumExpired);
	}

	if (!bEmitting && (Samples.Num() < 2 || Now - Samples.Last().Time >= Settings.Lifetime))
	{
		DestroyComponent();
		return;
	}

	UpdateBounds();
	MarkRenderTransformDirty();
	MarkRenderDynamicDataDirty();
}

void UknightAttackTrailComponent::BuildVertices(TArray<FDynamicMeshVertex>& OutVertices, TArray<uint32>& OutIndices) const
{
	if (Samples.Num() < 2)
	{
		return;
	}
	const float Now = GetNow();
	auto AddPair = [&](const FVector& Base, const FVector& Tip, float Time, float Strength)
	{
		const float Age = FMath::Clamp((Now - Time) / Settings.Lifetime, 0.f, 1.f);
		const FColor Color(255, 255, 255, static_cast<uint8>(FMath::Clamp(Strength, 0.f, 1.f) * 255.f));
		const int32 First = OutVertices.Num();
		OutVertices.Emplace(FVector3f(Base), FVector2f(Age, 0.f), Color);
		OutVertices.Emplace(FVector3f(Tip), FVector2f(Age, 1.f), Color);
		if (First >= 2)
		{
			OutIndices.Append({ uint32(First - 2), uint32(First - 1), uint32(First), uint32(First - 1), uint32(First + 1), uint32(First) });
		}
	};

	const int32 Num = Samples.Num();
	for (int32 i = 0; i < Num - 1; ++i)
	{
		const FSample& S0 = Samples[FMath::Max(i - 1, 0)];
		const FSample& S1 = Samples[i];
		const FSample& S2 = Samples[i + 1];
		const FSample& S3 = Samples[FMath::Min(i + 2, Num - 1)];
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(FVector::Dist(S1.Tip, S2.Tip) / SubdivideLength), 1, MaxSubdivisions);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float T = static_cast<float>(Step) / Steps;
			AddPair(CatmullRom(S0.Base, S1.Base, S2.Base, S3.Base, T), CatmullRom(S0.Tip, S1.Tip, S2.Tip, S3.Tip, T),
				FMath::Lerp(S1.Time, S2.Time, T), FMath::Lerp(S1.Strength, S2.Strength, T));
		}
	}
	const FSample& Last = Samples.Last();
	AddPair(Last.Base, Last.Tip, Last.Time, Last.Strength);
}

FPrimitiveSceneProxy* UknightAttackTrailComponent::CreateSceneProxy()
{
	return new FAttackTrailSceneProxy(this);
}

void UknightAttackTrailComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	Super::CreateRenderState_Concurrent(Context);
	SendRenderDynamicData_Concurrent();
}

void UknightAttackTrailComponent::SendRenderDynamicData_Concurrent()
{
	Super::SendRenderDynamicData_Concurrent();
	if (!SceneProxy)
	{
		return;
	}
	FTrailRenderData* const NewData = new FTrailRenderData;
	BuildVertices(NewData->Vertices, NewData->Indices);
	FAttackTrailSceneProxy* const Proxy = static_cast<FAttackTrailSceneProxy*>(SceneProxy);
	ENQUEUE_RENDER_COMMAND(SendAttackTrailData)([Proxy, NewData](FRHICommandListImmediate&)
	{
		Proxy->SetData_RenderThread(NewData);
	});
}

FBoxSphereBounds UknightAttackTrailComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox Box(ForceInit);
	for (const FSample& Sample : Samples)
	{
		Box += Sample.Base;
		Box += Sample.Tip;
	}
	if (!Box.IsValid)
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}
	return FBoxSphereBounds(Box.ExpandBy(50.f));
}
