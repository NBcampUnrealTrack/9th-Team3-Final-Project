#include "SknightAttackPreviewViewport.h"

#include "PreviewScene.h"
#include "knightAttackPreviewModel.h"

namespace knightAttackPreviewViewport
{
	/** 이 크기(경계 구 반지름, cm)일 때 카메라 이동 속도를 그대로 쓴다. 사람 크기 몬스터 기준. */
	constexpr float ReferenceRadius = 150.f;
	constexpr float MaxSpeedScale = 30.f;
}

FknightAttackPreviewViewportClient::FknightAttackPreviewViewportClient(const TSharedRef<FknightAttackPreviewModel>& InModel,
	const TSharedRef<SEditorViewport>& InViewport)
	: FEditorViewportClient(nullptr, &InModel->GetScene(), InViewport)
	, Model(InModel)
{
	SetRealtime(true);
	EngineShowFlags.SetGrid(true);
	DrawHelper.bDrawGrid = true;
	DrawHelper.bDrawPivot = false;
	BaseSpeedSettings = GetCameraSpeedSettings();
	ResetCamera();
}

void FknightAttackPreviewViewportClient::SetCameraPreset(EknightPreviewCamera InPreset)
{
	Preset = InPreset;
	ResetCamera();
}

void FknightAttackPreviewViewportClient::ResetCamera()
{
	using namespace knightAttackPreviewViewport;
	FramedMeshRevision = Model->GetMeshRevision();

	// 몬스터는 원점에서 +X를 바라본다. 모든 거리를 메시 경계 반지름 R에서 정한다.
	const FBoxSphereBounds Bounds = Model->GetFocusBounds();
	const float Radius = FMath::Max(Bounds.SphereRadius, 100.f);
	// 공격은 앞쪽에서 일어나므로 몸 중심보다 조금 앞, 조금 낮은 곳을 본다.
	const FVector Focus(Bounds.Origin.X + Radius * 0.3f, Bounds.Origin.Y, Bounds.Origin.Z * 0.6f);

	FVector Location;
	FRotator Rotation;
	switch (Preset)
	{
	case EknightPreviewCamera::Top:
	{
		// 기본 시야각 90도에서 화면 반폭 ≈ 높이이므로, 사거리 끝까지 담기는 높이로 올린다.
		const float Height = FMath::Max(Radius * 1.6f, Model->GetMaxReach() * 1.15f);
		Location = FVector(Bounds.Origin.X, Bounds.Origin.Y, Height);
		Rotation = FRotator(-89.9f, 0.f, 0.f);
		break;
	}
	case EknightPreviewCamera::Side:
		Location = Focus + FVector(0.f, -Radius * 2.8f, 0.f);
		Rotation = (Focus - Location).Rotation();
		break;
	default:
		Location = Focus + FVector(-0.74f, -0.62f, 0.25f).GetSafeNormal() * Radius * 2.6f;
		Rotation = (Focus - Location).Rotation();
		break;
	}
	SetViewLocation(Location);
	SetViewRotation(Rotation);

	// WASD 이동 속도도 몬스터 크기에 맞춘다(8배 몬스터 주위를 사람 속도로 돌면 너무 느리다).
	const float SpeedScale = FMath::Clamp(Radius / ReferenceRadius, 1.f, MaxSpeedScale);
	FEditorViewportCameraSpeedSettings SpeedSettings = BaseSpeedSettings;
	SpeedSettings.SetAbsoluteSpeedRange(BaseSpeedSettings.GetAbsoluteMinSpeed(), BaseSpeedSettings.GetAbsoluteMaxSpeed() * SpeedScale);
	SpeedSettings.SetCurrentSpeed(BaseSpeedSettings.GetCurrentSpeed() * SpeedScale);
	SetCameraSpeedSettings(SpeedSettings);
}

void FknightAttackPreviewViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);
	Model->Tick(DeltaSeconds);
	if (FramedMeshRevision != Model->GetMeshRevision())
	{
		ResetCamera();
	}
}

void FknightAttackPreviewViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);
	Model->Draw(PDI);
}

void SknightAttackPreviewViewport::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

void SknightAttackPreviewViewport::ResetCamera()
{
	if (Client.IsValid())
	{
		Client->ResetCamera();
	}
}

void SknightAttackPreviewViewport::SetCameraPreset(EknightPreviewCamera Preset)
{
	if (Client.IsValid())
	{
		Client->SetCameraPreset(Preset);
	}
}

EknightPreviewCamera SknightAttackPreviewViewport::GetCameraPreset() const
{
	return Client.IsValid() ? Client->GetCameraPreset() : EknightPreviewCamera::Overview;
}

TSharedRef<FEditorViewportClient> SknightAttackPreviewViewport::MakeEditorViewportClient()
{
	Client = MakeShared<FknightAttackPreviewViewportClient>(Model.ToSharedRef(), SharedThis(this));
	return Client.ToSharedRef();
}
