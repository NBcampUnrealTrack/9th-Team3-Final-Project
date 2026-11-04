#include "knightEnhancementPreviewCheckCommandlet.h"
#include "Menu/UI/KnightMaintenanceWidget.h"
#include "Progression/knightProgressionSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Slate/WidgetRenderer.h"
#include "WidgetBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"
#include "RenderingThread.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"

int32 UknightEnhancementPreviewCheckCommandlet::Main(const FString& Params)
{
	FString Out;
	if (!FParse::Value(*Params, TEXT("Out="), Out) || !FParse::Param(FCommandLine::Get(), TEXT("AllowCommandletRendering"))) return 1;
	IFileManager::Get().MakeDirectory(*Out, true);
	if (!FSlateApplication::IsInitialized())
	{
		FSlateApplication::Create();
		FSlateApplication::Get().InitializeRenderer(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
	}
	int32 Failures = 0;
	auto Check = [&Failures](bool bPass, const TCHAR* What)
	{
		if (!bPass) ++Failures;
		UE_LOG(LogTemp, Display, TEXT("EnhancementPreview %s %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), What);
	};
	if (UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenance")))
	{
		UE_LOG(LogTemp, Display, TEXT("EnhancementPreview asset animation count=%d"), BP->Animations.Num());
		TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
			for (UEdGraphNode* Node : Graph->Nodes)
				if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
					UE_LOG(LogTemp, Display, TEXT("EnhancementPreview graph function=%s"), *Call->GetFunctionName().ToString());
	}
	UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Knight/Data/Equipment/Equipment_Icon/T_Category_Weapon"));
	if (!Icon) return 1;
	Icon->SetForceMipLevelsToBeResident(30.f); Icon->WaitForStreaming();
	UKnightMaintenanceWidget* Widget = NewObject<UKnightMaintenanceWidget>();
	Widget->AddToRoot();
	Widget->WidgetTree = NewObject<UWidgetTree>(Widget);
	Widget->Category = 1;
	UWidget* Preview = Widget->BuildEnhancementPreview();
	Widget->WidgetTree->RootWidget = Preview;
	Widget->EnhancementWeaponImage->SetBrushFromTexture(Icon, true);
	const TSharedRef<SWidget> SlatePreview = Preview->TakeWidget();
	FWidgetRenderer* Renderer = new FWidgetRenderer(true);
	UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(FVector2D(640, 180), TF_Bilinear, true);
	Target->AddToRoot();
	auto Draw = [&]() { Renderer->DrawWidget(Target, SlatePreview, FVector2D(640, 180), 0.f); FlushRenderingCommands(); };
	auto Capture = [&](const FString& Name)
	{
		TArray<FColor> Pixels;
		Check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels), TEXT("render target readback"));
		TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(640, 180, Pixels, Png);
		Check(FFileHelper::SaveArrayToFile(Png, *FPaths::Combine(Out, Name + TEXT(".png"))), TEXT("render capture saved"));
	};
	Draw(); Draw(); Capture(TEXT("idle"));
	const FGeometry IdleGeometry = Widget->EnhancementWeaponImage->GetCachedGeometry();
	const FVector2D IdleDesired = SlatePreview->GetDesiredSize();
	const FWidgetTransform IdleTransform = Widget->EnhancementWeaponImage->GetRenderTransform();
	for (bool bSuccess : { true, false, false })
	{
		Widget->StartEnhancementBars(bSuccess ? EKnightWeaponUpgradeOutcome::Success : EKnightWeaponUpgradeOutcome::ChanceFailed);
		Widget->RequestWeaponUpgrade(); // Repeated input must leave the current presentation intact.
		Check(Widget->EnhancementBarPhase == UKnightMaintenanceWidget::EEnhancementBarPhase::Rising, TEXT("repeat request cannot reset active attempt"));
		bool bFixed = true, bContinuous = true;
		for (int32 Frame = 0; Frame < 70; ++Frame)
		{
			const auto PreviousPhase = Widget->EnhancementBarPhase;
			const auto PreviousPositions = Widget->EnhancementBarPositions;
			const auto PreviousOpacities = Widget->EnhancementBarOpacities;
			Widget->TickEnhancementBars(1.f / 60.f); Draw();
			const FGeometry G = Widget->EnhancementWeaponImage->GetCachedGeometry();
			bFixed &= G.GetLocalSize().Equals(IdleGeometry.GetLocalSize(), .01f)
				&& G.LocalToAbsolute(FVector2D::ZeroVector).Equals(IdleGeometry.LocalToAbsolute(FVector2D::ZeroVector), .01f)
				&& G.LocalToAbsolute(G.GetLocalSize()).Equals(IdleGeometry.LocalToAbsolute(IdleGeometry.GetLocalSize()), .01f)
				&& Widget->EnhancementWeaponImage->GetRenderTransform() == IdleTransform
				&& SlatePreview->GetDesiredSize().Equals(IdleDesired, .01f);
			if (PreviousPhase == UKnightMaintenanceWidget::EEnhancementBarPhase::Trembling
				&& PreviousPhase != Widget->EnhancementBarPhase)
				bContinuous &= PreviousPositions == Widget->EnhancementBarPositions && PreviousOpacities == Widget->EnhancementBarOpacities;
			if (Frame == 25 || Frame == 43)
				Capture(FString::Printf(TEXT("%s_%d"), bSuccess ? TEXT("success") : TEXT("failure"), Frame));
		}
		Check(bFixed, TEXT("icon geometry, angle, scale and desired size fixed on every frame"));
		Check(bContinuous, TEXT("result starts at exact displayed bar positions and alpha"));
		Check(Widget->EnhancementBarCanvas->GetVisibility() == ESlateVisibility::Collapsed, TEXT("result cleans up all bars"));
	}
	Widget->StartEnhancementBars(EKnightWeaponUpgradeOutcome::Rejected); Draw();
	Check(Widget->EnhancementBarPhase == UKnightMaintenanceWidget::EEnhancementBarPhase::Idle, TEXT("rejected attempt has no effect"));
	// An authored nonzero angle, offset and scale must survive an interrupted attempt.
	FWidgetTransform Authored = IdleTransform; Authored.Angle = 11.f; Authored.Translation = FVector2D(3, -2); Authored.Scale = FVector2D(.94f, .94f);
	Widget->EnhancementWeaponImage->SetRenderTransform(Authored);
	Widget->CaptureEnhancementIdleTransform();
	Widget->StartEnhancementBars(EKnightWeaponUpgradeOutcome::ChanceFailed);
	Widget->TickEnhancementBars(.15f); Draw();
	Widget->ResetEnhancementBars(); Draw();
	Check(Widget->EnhancementWeaponImage->GetRenderTransform() == Authored, TEXT("interruption preserves authored nonzero idle transform"));
	Check(Widget->EnhancementBarPositions.IsEmpty() && Widget->EnhancementBarResultPositions.IsEmpty(), TEXT("reset discards previous weapon/result state"));
	Widget->EnhancementWeaponImage->SetRenderTransform(IdleTransform); Widget->CaptureEnhancementIdleTransform();
	Draw(); Capture(TEXT("cleaned"));
	UE_LOG(LogTemp, Display, TEXT("EnhancementPreview COMPLETE failures=%d; component rendering only, no gameplay transaction"), Failures);
	BeginCleanup(Renderer); FlushRenderingCommands();
	Target->RemoveFromRoot(); Widget->RemoveFromRoot();
	return Failures ? 1 : 0;
}
