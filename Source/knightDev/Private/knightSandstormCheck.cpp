// Explicit opt-in rendered comparison. No OS input, combat actions, profile creation or save writes.
#if WITH_EDITOR
#include "Environment/KnightSandstorm.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Containers/Ticker.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
struct FSandstormRenderCheck
{
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<AKnightSandstorm> Storm;
	FString Directory;
	int32 Step=0;
	double Next=0, Deadline=0;
	float PreviousLimit=0; int32 PreviousVSync=0;
	void Capture(const TCHAR* Name)
	{
		APlayerController* PC=World->GetFirstPlayerController(); FVector2D Pixels;
		World->GetGameViewport()->GetViewportSize(Pixels);
		UExponentialHeightFogComponent* Fog=Storm->TargetFog->GetComponent();
		UE_LOG(LogTemp,Display,TEXT("SandstormCheck CAPTURE %s viewport=%.0fx%.0f camera=%s rotation=%s fov=%.2f density=%.5f start=%.0f volStart=%.0f volFade=%.0f volView=%.0f"),
			Name,Pixels.X,Pixels.Y,*PC->PlayerCameraManager->GetCameraLocation().ToString(),*PC->PlayerCameraManager->GetCameraRotation().ToString(),
			PC->PlayerCameraManager->GetFOVAngle(),Fog->FogDensity,Fog->StartDistance,Fog->VolumetricFogStartDistance,Fog->VolumetricFogNearFadeInDistance,Fog->VolumetricFogDistance);
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory,FString(Name)+TEXT(".png")),true,false);
		for (const TCHAR* Key : {TEXT("r.NGX.DLSS.Enable"), TEXT("r.ScreenPercentage"), TEXT("sg.EffectsQuality"), TEXT("sg.ShadowQuality")})
			if (auto* V=IConsoleManager::Get().FindConsoleVariable(Key)) UE_LOG(LogTemp,Display,TEXT("SandstormCheck %s=%s"),Key,*V->GetString());
		// Terrain samples measure the actual view rather than inferring distance from screen size.
		for (const FVector2D UV : {FVector2D(.5,.6),FVector2D(.25,.35),FVector2D(.75,.25)})
		{
			FVector Origin,Direction; FHitResult Hit;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(SandstormViewDistance),true,PC->GetPawn());
			if (PC->DeprojectScreenPositionToWorld(UV.X*Pixels.X,UV.Y*Pixels.Y,Origin,Direction)
				&& World->LineTraceSingleByObjectType(Hit,Origin,Origin+Direction*100000,FCollisionObjectQueryParams(ECC_WorldStatic),Query))
				UE_LOG(LogTemp,Display,TEXT("SandstormCheck terrain uv=%.2f,%.2f distance=%.0fcm actor=%s"),UV.X,UV.Y,Hit.Distance,*GetNameSafe(Hit.GetActor()));
		}
	}
	void Finish()
	{
		if (Storm.IsValid()) { Storm->bLightDust=false; Storm->SetSandstormEnabled(true); }
		if (auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))) V->Set(PreviousLimit,ECVF_SetByConsole);
		if (auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))) V->Set(PreviousVSync,ECVF_SetByConsole);
		UE_LOG(LogTemp,Display,TEXT("SandstormCheck restored runtime t.MaxFPS=%.1f VSync=%d; no settings saved"),PreviousLimit,PreviousVSync);
		FPlatformMisc::RequestExit(false);
	}
	bool Tick(float)
	{
		const double Now=FPlatformTime::Seconds();
		if (!World.IsValid() || Now>Deadline) { UE_LOG(LogTemp,Error,TEXT("SandstormCheck TIMEOUT step=%d"),Step); Finish(); return false; }
		if (Now<Next || FScreenshotRequest::IsScreenshotRequested()) return true;
		APlayerController* PC=World->GetFirstPlayerController();
		if (!PC || !PC->GetPawn() || !World->GetGameViewport()) return true;
		switch(Step)
		{
		case 0:
		{
			for (TActorIterator<AKnightSandstorm> I(World.Get());I;++I) Storm=*I;
			if (!Storm.IsValid() || !Storm->TargetFog) { UE_LOG(LogTemp,Error,TEXT("SandstormCheck missing placed actor/fog")); Finish(); return false; }
			FVector Boss=FVector::ZeroVector;
			for (TActorIterator<ACharacter> I(World.Get());I;++I)
			{
				if (I->GetClass()->GetName().Contains(TEXT("Janyeol"))) Boss=I->GetActorLocation();
				if (AAIController* AI=Cast<AAIController>(I->GetController()))
				{ if (AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Isolated sandstorm render")); AI->StopMovement(); }
				I->GetCharacterMovement()->DisableMovement();
			}
			if (!Boss.IsNearlyZero()) { FRotator Look=(Boss-PC->GetPawn()->GetActorLocation()).Rotation(); Look.Pitch=-7; PC->SetControlRotation(Look); }
			PC->SetIgnoreLookInput(true); PC->SetIgnoreMoveInput(true);
			UE_LOG(LogTemp,Display,TEXT("SandstormCheck actual player=%s boss=%s separation=%.0fcm"),*PC->GetPawn()->GetActorLocation().ToString(),*Boss.ToString(),FVector::Dist(Boss,PC->GetPawn()->GetActorLocation()));
			PreviousLimit=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))->GetFloat();
			PreviousVSync=IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))->GetInt();
			PC->ConsoleCommand(TEXT("t.MaxFPS 0"),false); PC->ConsoleCommand(TEXT("r.VSync 0"),false);
			Storm->SetSandstormEnabled(false); Next=Now+15.; break;
		}
		case 1: PC->ConsoleCommand(TEXT("csvprofile start"),false); Next=Now+8.; break;
		case 2: PC->ConsoleCommand(TEXT("csvprofile stop"),false); Capture(TEXT("01_off")); Next=Now+2.; break;
		case 3: Storm->SetSandstormEnabled(true); Next=Now+12.; break;
		case 4: PC->ConsoleCommand(TEXT("csvprofile start"),false); Next=Now+8.; break;
		case 5: PC->ConsoleCommand(TEXT("csvprofile stop"),false); Capture(TEXT("02_on")); Next=Now+2.; break;
		case 6: Storm->bLightDust=true; Storm->ApplySettings(); Next=Now+8.; break;
		case 7: PC->ConsoleCommand(TEXT("csvprofile start"),false); Next=Now+8.; break;
		case 8: PC->ConsoleCommand(TEXT("csvprofile stop"),false); Capture(TEXT("03_light")); Next=Now+2.; break;
		default: UE_LOG(LogTemp,Display,TEXT("SandstormCheck COMPLETE: rendered comparison only, combat untested")); Finish(); return false;
		}
		++Step; return true;
	}
};
void StartSandstormCheck(const TArray<FString>& Args,UWorld* World)
{
	FString Namespace;
	if (!World || Args.Num()!=1 || !FParse::Value(FCommandLine::Get(),TEXT("KnightSaveTestNamespace="),Namespace) || !Namespace.StartsWith(TEXT("Sandstorm")))
	{ UE_LOG(LogTemp,Error,TEXT("SandstormCheck requires isolated Sandstorm save namespace and output directory")); return; }
	auto Run=MakeShared<FSandstormRenderCheck>(); Run->World=World; Run->Directory=FPaths::ConvertRelativePathToFull(Args[0]); Run->Deadline=FPlatformTime::Seconds()+180.;
	IFileManager::Get().MakeDirectory(*Run->Directory,true);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta){return Run->Tick(Delta);}));
}
FAutoConsoleCommandWithWorldAndArgs Command(TEXT("knight.SandstormCheck"),TEXT("Isolated OFF/ON render comparison; no desktop input."),FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartSandstormCheck),ECVF_Cheat);
}
#endif
