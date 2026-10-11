#include "SMechPartsWorkbenchViewport.h"

#include "AdvancedPreviewScene.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EditorViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
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

void SMechPartsWorkbenchViewport::Construct(const FArguments& InArgs)
{
	PreviewScene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());

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
	const UObject* CurrentMesh = nullptr;
	if (const USkeletalMeshComponent* const SkeletalPart = Cast<USkeletalMeshComponent>(PartComponent))
	{
		CurrentMesh = SkeletalPart->GetSkeletalMeshAsset();
	}
	else if (const UStaticMeshComponent* const StaticPart = Cast<UStaticMeshComponent>(PartComponent))
	{
		CurrentMesh = StaticPart->GetStaticMesh();
	}
	if (CurrentMesh != Mesh)
	{
		RemovePartComponent();
		if (USkeletalMesh* const SkeletalMesh = Cast<USkeletalMesh>(Mesh))
		{
			USkeletalMeshComponent* const Component = NewObject<USkeletalMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
			Component->SetSkeletalMeshAsset(SkeletalMesh);
			PartComponent = Component;
		}
		else if (UStaticMesh* const StaticMesh = Cast<UStaticMesh>(Mesh))
		{
			UStaticMeshComponent* const Component = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
			Component->SetStaticMesh(StaticMesh);
			PartComponent = Component;
		}
		if (PartComponent)
		{
			PreviewScene->AddComponent(PartComponent, FTransform::Identity);
		}
	}

	if (PartComponent)
	{
		if (const FMechPartAttachment* const Attachment = MechPartReflection::FindAttachment(Asset))
		{
			Attachment->Apply(PartComponent, MechComponent);
		}
	}
	if (ViewportClient.IsValid())
	{
		ViewportClient->Invalidate();
	}
}

void SMechPartsWorkbenchViewport::RefreshMech()
{
	const UMechPartsWorkbenchSettings* const Settings = GetDefault<UMechPartsWorkbenchSettings>();

	// 파츠가 메카에 붙어 있으므로 먼저 떼고, 메카를 바꾼 뒤 다시 붙인다.
	RemovePartComponent();

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

	RefreshPart();
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
