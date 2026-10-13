#include "SMechPartsWorkbenchViewport.h"

#include "AdvancedPreviewScene.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EditorViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "MechBodyRegionSet.h"
#include "MechBodyRegionSplitter.h"
#include "MechPartAttachment.h"
#include "MechPartReflection.h"
#include "MechPartsWorkbenchSettings.h"
#include "UnrealWidget.h"

/** 미리보기 장면을 매 프레임 진행(애니메이션 자세 반영)하고, 창이 처음 열릴 때 카메라를 맞춘다. */
class FMechPartsWorkbenchViewportClient : public FEditorViewportClient
{
public:
	FMechPartsWorkbenchViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SEditorViewport>& InViewport)
		: FEditorViewportClient(nullptr, &InPreviewScene, InViewport)
	{
		SetRealtime(true);
		// 미리보기 장면에 바닥이 있어서 격자는 끈다.
		DrawHelper.bDrawGrid = false;
		// 이동 기즈모는 1b 단계에서 붙인다.
		if (Widget)
		{
			Widget->SetDefaultVisibility(false);
		}
		SetViewLocation(FVector(300.f, 300.f, 150.f));
		SetViewRotation(FRotator(-10.f, -135.f, 0.f));
	}

	/** 카메라를 Box에 맞춘다. 창 크기가 아직 없으면(처음 열릴 때) 크기가 생긴 뒤에 맞춘다. */
	void RequestFocus(const FBox& Box)
	{
		PendingFocus = Box;
	}

	virtual void Tick(float DeltaSeconds) override
	{
		FEditorViewportClient::Tick(DeltaSeconds);
		if (!GIntraFrameDebuggingGameThread)
		{
			PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
		}
		if (PendingFocus.IsSet() && Viewport && Viewport->GetSizeXY().X > 0)
		{
			FocusViewportOnBox(PendingFocus.GetValue(), true);
			PendingFocus.Reset();
		}
	}

private:
	TOptional<FBox> PendingFocus;
};

namespace
{
	/** 컴포넌트가 지금 그리는 메시(스켈레탈 또는 스태틱). 없으면 null. */
	const UObject* GetComponentMesh(const UMeshComponent* Component)
	{
		if (const USkeletalMeshComponent* const SkeletalPart = Cast<USkeletalMeshComponent>(Component))
		{
			return SkeletalPart->GetSkeletalMeshAsset();
		}
		if (const UStaticMeshComponent* const StaticPart = Cast<UStaticMeshComponent>(Component))
		{
			return StaticPart->GetStaticMesh();
		}
		return nullptr;
	}
}

void SMechPartsWorkbenchViewport::Construct(const FArguments& InArgs)
{
	// 물리 계산을 켜 둬야 천 시뮬레이션이 돈다 (엔진은 bShouldSimulatePhysics인 장면에서만 천 계산을 만든다).
	// 엔진 스켈레탈 메시 편집기(Persona) 미리보기와 같은 설정. 10-12까지는 꺼져 있어서 워크벤치에서 천이 전혀 움직이지 않았다.
	PreviewScene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues().ShouldSimulatePhysics(true));

	MechComponent = NewObject<USkeletalMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	PreviewScene->AddComponent(MechComponent, FTransform::Identity);

	// MakeEditorViewportClient가 여기서 불린다.
	SEditorViewport::Construct(SEditorViewport::FArguments());

	RefreshMech();
}

SMechPartsWorkbenchViewport::~SMechPartsWorkbenchViewport()
{
	// 엔진 미리보기 창들과 같은 정리 순서: 장면이 먼저 사라지므로 클라이언트가 창을 더 그리지 않게 끊는다.
	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = nullptr;
	}
}

TSharedRef<FEditorViewportClient> SMechPartsWorkbenchViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShared<FMechPartsWorkbenchViewportClient>(*PreviewScene, SharedThis(this));
	return ViewportClient.ToSharedRef();
}

void SMechPartsWorkbenchViewport::SetPartAsset(UObject* InPartAsset)
{
	PartAsset = InPartAsset;
	RefreshPart();
	OnFocusViewportToSelection();
}

void SMechPartsWorkbenchViewport::RemovePartComponent()
{
	if (PartComponent)
	{
		PreviewScene->RemoveComponent(PartComponent);
		PartComponent = nullptr;
	}
}

void SMechPartsWorkbenchViewport::RefreshPart()
{
	UObject* const Asset = PartAsset.Get();
	UObject* const Mesh = Asset ? MechPartReflection::LoadPartMesh(Asset) : nullptr;

	// 메시가 바뀐 경우에만 컴포넌트를 새로 만든다 (숫자를 끌어 바꾸는 동안 매번 새로 만들지 않게).
	if (GetComponentMesh(PartComponent) != Mesh)
	{
		RemovePartComponent();
		PartComponent = CreateMeshComponent(Mesh);
	}

	if (PartComponent)
	{
		if (const FMechPartAttachment* const Attachment = MechPartReflection::FindAttachment(Asset))
		{
			Attachment->Apply(PartComponent, MechComponent);
		}
	}
	// 고른 파츠의 Replace Regions가 바뀌었을 수 있다.
	UpdateMechVisibility();
	if (ViewportClient.IsValid())
	{
		ViewportClient->Invalidate();
	}
}

UMeshComponent* SMechPartsWorkbenchViewport::CreateMeshComponent(UObject* Mesh)
{
	UMeshComponent* Component = nullptr;
	if (USkeletalMesh* const SkeletalMesh = Cast<USkeletalMesh>(Mesh))
	{
		USkeletalMeshComponent* const SkeletalPart = NewObject<USkeletalMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		SkeletalPart->SetSkeletalMeshAsset(SkeletalMesh);
		Component = SkeletalPart;
	}
	else if (UStaticMesh* const StaticMesh = Cast<UStaticMesh>(Mesh))
	{
		UStaticMeshComponent* const StaticPart = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		StaticPart->SetStaticMesh(StaticMesh);
		Component = StaticPart;
	}
	if (Component)
	{
		PreviewScene->AddComponent(Component, FTransform::Identity);
	}
	return Component;
}

void SMechPartsWorkbenchViewport::SetLoadout(const TArray<UObject*>& InParts)
{
	// 이미 있던 파츠는 컴포넌트를 그대로 쓰고, 빠진 파츠의 컴포넌트만 치운다.
	TArray<FLoadoutPart> OldParts = MoveTemp(LoadoutParts);
	LoadoutParts.Reset();
	for (UObject* const Asset : InParts)
	{
		FLoadoutPart& Entry = LoadoutParts.AddDefaulted_GetRef();
		Entry.Asset = Asset;
		if (FLoadoutPart* const Existing = OldParts.FindByPredicate([Asset](const FLoadoutPart& Old) { return Old.Asset.Get() == Asset; }))
		{
			Entry.Component = Existing->Component;
			Existing->Component = nullptr;
		}
	}
	for (FLoadoutPart& Removed : OldParts)
	{
		if (Removed.Component)
		{
			PreviewScene->RemoveComponent(Removed.Component);
		}
	}
	RefreshLoadout();
}

void SMechPartsWorkbenchViewport::RefreshLoadout()
{
	for (FLoadoutPart& Entry : LoadoutParts)
	{
		UObject* const Asset = Entry.Asset.Get();
		UObject* const Mesh = Asset ? MechPartReflection::LoadPartMesh(Asset) : nullptr;
		if (GetComponentMesh(Entry.Component) != Mesh)
		{
			if (Entry.Component)
			{
				PreviewScene->RemoveComponent(Entry.Component);
			}
			Entry.Component = CreateMeshComponent(Mesh);
		}
		const FMechPartAttachment* const Attachment = Asset ? MechPartReflection::FindAttachment(Asset) : nullptr;
		if (Entry.Component && Attachment)
		{
			Attachment->Apply(Entry.Component, MechComponent);
		}
	}
	UpdateMechVisibility();
	if (ViewportClient.IsValid())
	{
		ViewportClient->Invalidate();
	}
}

void SMechPartsWorkbenchViewport::SetHideWholeMech(bool bHide)
{
	bHideWholeMech = bHide;
	UpdateMechVisibility();
	if (ViewportClient.IsValid())
	{
		ViewportClient->Invalidate();
	}
}

void SMechPartsWorkbenchViewport::RefreshBodyRegions()
{
	for (USkeletalMeshComponent* const Region : RegionComponents)
	{
		if (Region)
		{
			PreviewScene->RemoveComponent(Region);
		}
	}
	RegionComponents.Reset();
	RegionFlags.Reset();

	// 게임과 같은 방식: 본체 메시로 나눈 세트가 있으면 부위 메시를 본체 자세에 붙인다.
	if (const UMechBodyRegionSet* const RegionSet = MechBodyRegionSplitter::FindRegionSet(MechComponent->GetSkeletalMeshAsset()))
	{
		for (const FMechBodyRegionMesh& Region : RegionSet->Regions)
		{
			USkeletalMesh* const Mesh = Region.Mesh.LoadSynchronous();
			if (!Mesh)
			{
				continue;
			}
			USkeletalMeshComponent* const Component = NewObject<USkeletalMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
			PreviewScene->AddComponent(Component, FTransform::Identity);
			UMechBodyRegionSet::SetupRegionComponent(Component, Mesh, MechComponent);
			RegionComponents.Add(Component);
			RegionFlags.Add(static_cast<int32>(Region.Region));
		}
	}
	UpdateMechVisibility();
}

void SMechPartsWorkbenchViewport::UpdateMechVisibility()
{
	int32 HiddenRegions = 0;
	auto AddReplacedRegions = [&HiddenRegions](UObject* Asset)
	{
		if (const FMechPartAttachment* const Attachment = Asset ? MechPartReflection::FindAttachment(Asset) : nullptr)
		{
			HiddenRegions |= Attachment->ReplaceRegions;
		}
	};
	AddReplacedRegions(PartAsset.Get());
	for (const FLoadoutPart& Entry : LoadoutParts)
	{
		AddReplacedRegions(Entry.Asset.Get());
	}

	// 부위 메시가 있으면 본체는 항상 숨기고(게임과 같음) 부위 메시를 켜고 끈다. 없으면 '원본 전부 숨기기'일 때만 본체를 숨긴다.
	UMechBodyRegionSet::SetMechBodyHidden(MechComponent, HasBodyRegions() || bHideWholeMech);
	for (int32 Index = 0; Index < RegionComponents.Num(); ++Index)
	{
		if (RegionComponents[Index])
		{
			RegionComponents[Index]->SetVisibility(!bHideWholeMech && (RegionFlags[Index] & HiddenRegions) == 0);
		}
	}
}

void SMechPartsWorkbenchViewport::RecreatePart()
{
	RemovePartComponent();
	RefreshPart();
}

void SMechPartsWorkbenchViewport::RefreshMech()
{
	const UMechPartsWorkbenchSettings* const Settings = GetDefault<UMechPartsWorkbenchSettings>();

	// 파츠가 메카에 붙어 있으므로 먼저 떼고, 메카를 바꾼 뒤 다시 붙인다(착용 미리보기 파츠도 같음).
	RemovePartComponent();
	for (FLoadoutPart& Entry : LoadoutParts)
	{
		if (Entry.Component)
		{
			PreviewScene->RemoveComponent(Entry.Component);
			Entry.Component = nullptr;
		}
	}

	USkeletalMesh* const MechMesh = Settings->PreviewMechMesh.LoadSynchronous();
	const bool bMechMeshChanged = MechComponent->GetSkeletalMeshAsset() != MechMesh;
	MechComponent->SetSkeletalMeshAsset(MechMesh);
	MechComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	UAnimationAsset* const Animation = Settings->PreviewAnimation.LoadSynchronous();
	if (Animation && Settings->bPlayPreviewAnimation)
	{
		// 반복 재생 (2026-10-11).
		MechComponent->PlayAnimation(Animation, true);
	}
	else if (Animation)
	{
		// 정한 시점의 자세에서 멈춰 둔다. 엔진 애니메이션 썸네일과 같은 순서다.
		// 몽타주는 멈춘 상태에서 가중치가 0이라 UpdateMontageWeightForTimeSkip으로 채워야 자세가 보인다
		// (10-11 버그: 이게 빠져서 공격 몽타주가 기본 자세로 보였다).
		MechComponent->SetAnimation(Animation);
		MechComponent->Play(false);
		MechComponent->Stop();
		MechComponent->SetPosition(Settings->PreviewAnimationTime, false);
		if (UAnimSingleNodeInstance* const SingleNode = MechComponent->GetSingleNodeInstance())
		{
			SingleNode->UpdateMontageWeightForTimeSkip(Settings->PreviewAnimationTime);
		}
		MechComponent->RefreshBoneTransforms();
	}
	else
	{
		// 애니메이션이 없으면 메시 기본 자세.
		MechComponent->SetAnimation(nullptr);
	}

	// 메카 메시가 바뀌었으면 그 메시용 부위 메시 세트로 바꾼다.
	if (bMechMeshChanged || RegionComponents.IsEmpty())
	{
		RefreshBodyRegions();
	}
	RefreshPart();
	RefreshLoadout();
	// 자세만 바꿀 때는 보던 카메라를 그대로 두고, 메카 메시가 바뀌었을 때(처음 열 때 포함)만 메카에 맞춘다.
	if (bMechMeshChanged && ViewportClient.IsValid())
	{
		ViewportClient->RequestFocus(MechComponent->CalcBounds(MechComponent->GetComponentTransform()).GetBox());
	}
}

void SMechPartsWorkbenchViewport::OnFocusViewportToSelection()
{
	if (!ViewportClient.IsValid())
	{
		return;
	}
	const USceneComponent* Target = MechComponent;
	if (PartComponent)
	{
		Target = PartComponent;
	}
	// 작은 파츠에 카메라가 너무 붙어 머리와 겹쳐 보이지 않게, 최소 크기(FocusMinSize) 상자로 넓혀서 맞춘다 (10-11).
	const FBox Box = Target->CalcBounds(Target->GetComponentTransform()).GetBox();
	const FVector MinExtent(GetDefault<UMechPartsWorkbenchSettings>()->FocusMinSize * 0.5f);
	ViewportClient->RequestFocus(FBox::BuildAABB(Box.GetCenter(), Box.GetExtent().ComponentMax(MinExtent)));
}
