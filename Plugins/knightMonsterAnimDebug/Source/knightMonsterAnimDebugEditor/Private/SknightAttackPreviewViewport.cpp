#include "SknightAttackPreviewViewport.h"

#include "PreviewScene.h"
#include "knightAttackPreviewModel.h"

FknightAttackPreviewViewportClient::FknightAttackPreviewViewportClient(const TSharedRef<FknightAttackPreviewModel>& InModel,
	const TSharedRef<SEditorViewport>& InViewport)
	: FEditorViewportClient(nullptr, &InModel->GetScene(), InViewport)
	, Model(InModel)
{
	SetRealtime(true);
	EngineShowFlags.SetGrid(true);
	DrawHelper.bDrawGrid = true;
	DrawHelper.bDrawPivot = false;
	ResetCamera();
}

void FknightAttackPreviewViewportClient::ResetCamera()
{
	const FVector Location(-380.f, -420.f, 260.f);
	const FVector Target(120.f, 0.f, 90.f);
	SetViewLocation(Location);
	SetViewRotation((Target - Location).Rotation());
}

void FknightAttackPreviewViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);
	Model->Tick(DeltaSeconds);
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

TSharedRef<FEditorViewportClient> SknightAttackPreviewViewport::MakeEditorViewportClient()
{
	Client = MakeShared<FknightAttackPreviewViewportClient>(Model.ToSharedRef(), SharedThis(this));
	return Client.ToSharedRef();
}
