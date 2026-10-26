// Explicit isolated presentation check. No attack, hit, poise event or save mutation.
#if WITH_EDITOR
#include "Combat/knightGroggyFeedbackComponent.h"
#include "Camera/knightPlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "knightCharacter.h"
#include "knightPlayerController.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Settings/knightSettingsSubsystem.h"
#include "Settings/knightSharedSettings.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"

namespace
{
class FPullObserver : public FSceneViewExtensionBase
{
public:
	FPullObserver(const FAutoRegister& Auto) : FSceneViewExtensionBase(Auto) {}
	virtual int32 GetPriority() const override { return -100; }
	FVector4 Gain, Gamma;
	FIntRect Rect;
	virtual void SetupView(FSceneViewFamily&, FSceneView& View) override
	{ Gain=View.FinalPostProcessSettings.ColorGain; Gamma=View.FinalPostProcessSettings.ColorGamma; Rect=View.UnscaledViewRect; }
};
struct FPullCheck
{
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UknightGroggyFeedbackComponent> Feedback;
	TWeakObjectPtr<APlayerController> PC;
	TWeakObjectPtr<AknightMonsterCharacterBase> Boss;
	TWeakObjectPtr<ACameraActor> TestCamera;
	TSharedPtr<FPullObserver, ESPMode::ThreadSafe> Observer;
	FString Directory;
	double Next=0, Deadline=0;
	int32 Step=0, Failures=0, Assertions=0;
	float BaseFOV=0, BaseIntensity=1, BaseScreenPercentage=100;
	float BaseBlur=0, BasePullFOV=0;
	void SetPresentationFloat(const TCHAR* Name, float Value)
	{ auto* Property=FindFProperty<FFloatProperty>(Feedback->GetClass(),Name); check(Property); Property->SetPropertyValue_InContainer(Feedback.Get(),Value); }
	FVector4 BaseGain, BaseGamma;
	void Check(bool Value,const TCHAR* Label)
	{ ++Assertions; if(!Value) ++Failures; UE_LOG(LogTemp,Display,TEXT("RadialPullCheck %s %s"),Value?TEXT("PASS"):TEXT("FAIL"),Label); }
	void Capture(const TCHAR* Name)
	{
		UE_LOG(LogTemp,Display,TEXT("RadialPullCheck capture=%s weight=%.4f FOV=%.4f rect=%s gain=%s gamma=%s"),Name,
			Feedback->GetRadialPullWeight(),PC->PlayerCameraManager->GetFOVAngle(),*Observer->Rect.ToString(),*Observer->Gain.ToString(),*Observer->Gamma.ToString());
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory,FString(Name)+TEXT(".png")),true,false);
	}
	bool Tick(float)
	{
		const double Now=FPlatformTime::Seconds();
		if(!World.IsValid() || Now>Deadline) { UE_LOG(LogTemp,Error,TEXT("RadialPullCheck TIMEOUT")); FPlatformMisc::RequestExit(false); return false; }
		if(Now<Next || FScreenshotRequest::IsScreenshotRequested()) return true;
		if(Step==0)
		{
			PC=World->GetFirstPlayerController();
			auto* Player=PC.IsValid()?Cast<AknightCharacter>(PC->GetPawn()):nullptr;
			Feedback=Player?Player->FindComponentByClass<UknightGroggyFeedbackComponent>():nullptr;
			if(!Feedback.IsValid()) return true;
			for(TActorIterator<AknightMonsterCharacterBase> It(World.Get());It;++It) { if(!It->IsDead()) { Boss=*It; break; } }
			if(!Boss.IsValid()) return true;
			// Prevent the map's automatic engage roar from contaminating this presentation-only comparison.
			Boss->GetAbilitySystemComponent()->CancelAllAbilities();
			CastChecked<AknightPlayerCameraManager>(PC->PlayerCameraManager)->StopRoarShockwave(Boss->GetMesh());
			BaseBlur=FindFProperty<FFloatProperty>(Feedback->GetClass(),TEXT("RadialPullBlurStrength"))->GetPropertyValue_InContainer(Feedback.Get());
			BasePullFOV=FindFProperty<FFloatProperty>(Feedback->GetClass(),TEXT("RadialPullFOVDegrees"))->GetPropertyValue_InContainer(Feedback.Get());
			for(TActorIterator<ACharacter> It(World.Get());It;++It)
			{
				if(auto* AI=Cast<AAIController>(It->GetController())) { if(AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Isolated presentation check")); AI->StopMovement(); }
				It->GetCharacterMovement()->DisableMovement(); It->CustomTimeDilation=0.f;
			}
			PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
			Observer=FSceneViewExtensions::NewExtension<FPullObserver>();
			Next=Now+10; ++Step; return true;
		}
		auto* Settings=PC->GetLocalPlayer()->GetSubsystem<UknightSettingsSubsystem>()->GetSharedSettings();
		switch(Step)
		{
		case 1:
			BaseFOV=PC->PlayerCameraManager->GetFOVAngle(); BaseGain=Observer->Gain; BaseGamma=Observer->Gamma;
			BaseIntensity=Settings->CameraShakeScale; Settings->CameraShakeScale=1.f;
			Capture(TEXT("00_before")); Next=Now+.4; break;
		case 2:
			Feedback->PlayRadialPull(Boss.Get(),Boss->GetTargetLocation(PC->GetPawn())); Next=Now+.025; break;
		case 3:
			Check(Feedback->GetRadialPullWeight()>0.f,TEXT("actual short pulse active at capture"));
			Capture(TEXT("01_during_pull")); Next=Now+.5; break;
		case 4:
			Check(Feedback->GetRadialPullWeight()==0.f,TEXT("pulse ends on real time while actors frozen"));
			Check(FMath::IsNearlyEqual(BaseFOV,PC->PlayerCameraManager->GetFOVAngle(),.001f),TEXT("underlying FOV restored"));
			Check(BaseGain.Equals(Observer->Gain,.0001f)&&BaseGamma.Equals(Observer->Gamma,.0001f),TEXT("brightness gamma unchanged"));
			Capture(TEXT("02_after")); Next=Now+.4; break;
		case 5:
			Feedback->PlayRadialPull(Boss.Get(),Boss->GetActorLocation()); Next=Now+.03; break;
		case 6:
		{
			const float Before=Feedback->GetRadialPullWeight();
			Feedback->PlayRadialPull(Boss.Get(),Boss->GetActorLocation());
			Check(FMath::Abs(Before-Feedback->GetRadialPullWeight())<.01f,TEXT("retrigger continuous, not additive stacking"));
			CastChecked<AknightPlayerController>(PC.Get())->CancelKnockdownPresentation();
			PC->SetPause(true); Check(Feedback->GetRadialPullWeight()==0.f,TEXT("settings pause cancels"));
			Check(FMath::IsNearlyEqual(BaseFOV,PC->PlayerCameraManager->GetFOVAngle(),.001f),TEXT("settings pause retains underlying FOV"));
			PC->SetPause(false); Check(Feedback->GetRadialPullWeight()==0.f,TEXT("resume does not replay pulse"));
			Settings->CameraShakeScale=0;
			Feedback->PlayRadialPull(Boss.Get(),Boss->GetActorLocation()); Next=Now+.04; break;
		}
		case 7:
			Check(Feedback->GetRadialPullFOVOffset()==0.f,TEXT("effect intensity zero respects setting"));
			Settings->CameraShakeScale=1; Feedback->CancelRadialPull();
			TestCamera=World->SpawnActor<ACameraActor>(PC->PlayerCameraManager->GetCameraLocation(),PC->PlayerCameraManager->GetCameraRotation());
			TestCamera->GetCameraComponent()->bConstrainAspectRatio=true;
			TestCamera->GetCameraComponent()->AspectRatio=4.f/3.f;
			TestCamera->GetCameraComponent()->FieldOfView=BaseFOV;
			Feedback->PlayRadialPull(Boss.Get(),Boss->GetActorLocation());
			PC->SetViewTarget(TestCamera.Get()); Check(Feedback->GetRadialPullWeight()==0.f,TEXT("camera switch cancels"));
			BaseScreenPercentage=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))->GetFloat();
			IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))->Set(66.f,ECVF_SetByCode);
			Next=Now+4; break;
		case 8: Capture(TEXT("03_constrained_before")); Next=Now+.4; break;
		case 9:
			Feedback->PlayRadialPull(Boss.Get(),Boss->GetTargetLocation(PC->GetPawn())); Next=Now+.025; break;
		case 10:
			Check(Observer->Rect.Width()<1280,TEXT("constrained view excludes side bars"));
			Capture(TEXT("04_constrained_pull")); Next=Now+.5; break;
		case 11:
			Capture(TEXT("05_constrained_after")); Next=Now+.4; break;
		case 12:
			PC->SetViewTarget(PC->GetPawn());
			IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))->Set(BaseScreenPercentage,ECVF_SetByCode);
			SetPresentationFloat(TEXT("RadialPullBlurStrength"),0.f); Next=Now+2; break;
		case 13: Feedback->PlayRadialPull(Boss.Get(),Boss->GetTargetLocation(PC->GetPawn())); Next=Now+.025; break;
		case 14: Capture(TEXT("06_fov_only")); Next=Now+.5; break;
		case 15:
			SetPresentationFloat(TEXT("RadialPullBlurStrength"),BaseBlur);
			SetPresentationFloat(TEXT("RadialPullFOVDegrees"),0.f);
			Feedback->PlayRadialPull(Boss.Get(),Boss->GetTargetLocation(PC->GetPawn())); Next=Now+.025; break;
		case 16: Capture(TEXT("07_blur_only")); Next=Now+.5; break;
		default:
			Feedback->CancelRadialPull(); Settings->CameraShakeScale=BaseIntensity;
			SetPresentationFloat(TEXT("RadialPullBlurStrength"),BaseBlur);
			SetPresentationFloat(TEXT("RadialPullFOVDegrees"),BasePullFOV);
			IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))->Set(BaseScreenPercentage,ECVF_SetByCode);
			PC->SetViewTarget(PC->GetPawn()); if(TestCamera.IsValid()) TestCamera->Destroy();
			Observer.Reset();
			UE_LOG(LogTemp,Display,TEXT("RadialPullCheck COMPLETE assertions=%d failures=%d; presentation API, no combat/input automation"),Assertions,Failures);
			FPlatformMisc::RequestExit(false); return false;
		}
		++Step; return true;
	}
};
void StartPullCheck(const TArray<FString>& Args,UWorld* World)
{
	FString UserDir, Namespace;
	if(!World || Args.Num()!=1 || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir) || !UserDir.Contains(TEXT("KnightRadial"))
		|| !FParse::Value(FCommandLine::Get(),TEXT("KnightSaveTestNamespace="),Namespace) || !Namespace.StartsWith(TEXT("Radial")))
	{ UE_LOG(LogTemp,Error,TEXT("RadialPullCheck requires isolated KnightRadial UserDir and Radial namespace")); return; }
	auto Run=MakeShared<FPullCheck>(); Run->World=World; Run->Directory=Args[0]; Run->Deadline=FPlatformTime::Seconds()+180;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta){return Run->Tick(Delta);}));
}
FAutoConsoleCommandWithWorldAndArgs Command(TEXT("knight.RadialPullCheck"),TEXT("Isolated real-render presentation check."),FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartPullCheck),ECVF_Cheat);
}
#endif
