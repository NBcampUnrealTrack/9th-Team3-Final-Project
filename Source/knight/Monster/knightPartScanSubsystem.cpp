#include "Monster/knightPartScanSubsystem.h"

#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Monster/knightMonsterPartShellComponent.h"

namespace
{
	const FName ScanOriginParam(TEXT("ScanOrigin"));
	const FName ScanRadiusParam(TEXT("ScanRadius"));
	const FName ScanAlphaParam(TEXT("ScanAlpha"));

	/** 마지막 이 비율 동안 링이 옅어진다. */
	constexpr float ScanFadeFraction = 0.25f;

	FAutoConsoleCommandWithWorld PartScanCommand(
		TEXT("knight.PartScan"),
		TEXT("플레이어 위치에서 부위 탐지 스캔을 쏜다."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UknightPartScanSubsystem* const Subsystem = World ? World->GetSubsystem<UknightPartScanSubsystem>() : nullptr;
			APlayerController* const PC = World ? World->GetFirstPlayerController() : nullptr;
			UknightMonsterPartShellComponent* const Shell = Subsystem ? Subsystem->GetAnyShell() : nullptr;
			if (Shell && PC && PC->GetPawn())
			{
				Shell->StartScanFrom(PC->GetPawn());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs PartShellLockCommand(
		TEXT("knight.PartShellLock"),
		TEXT("테스트용: 모든 몬스터에서 셸 부위 순서의 N번 부위 셸을 락온한 것처럼 켠다. 인자 없거나 -1이면 끈다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const int32 Index = Args.IsEmpty() ? INDEX_NONE : FCString::Atoi(*Args[0]);
			UknightPartScanSubsystem* const Subsystem = World ? World->GetSubsystem<UknightPartScanSubsystem>() : nullptr;
			if (Subsystem)
			{
				Subsystem->ForEachShell([Index](UknightMonsterPartShellComponent& Shell) { Shell.DebugLockShellPart(Index); });
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs PartShellParamCommand(
		TEXT("knight.PartShellParam"),
		TEXT("테스트용: 모든 몬스터 부위 셸 머티리얼의 스칼라 값을 바꾼다. 예: knight.PartShellParam Style 1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UknightPartScanSubsystem* const Subsystem = World ? World->GetSubsystem<UknightPartScanSubsystem>() : nullptr;
			if (Subsystem && Args.Num() >= 2)
			{
				const FName Name(*Args[0]);
				const float Value = FCString::Atof(*Args[1]);
				Subsystem->ForEachShell([Name, Value](UknightMonsterPartShellComponent& Shell) { Shell.DebugSetMaterialScalar(Name, Value); });
			}
		}));
}

void UknightPartScanSubsystem::RegisterShell(UknightMonsterPartShellComponent* Shell)
{
	Shells.AddUnique(Shell);
}

void UknightPartScanSubsystem::UnregisterShell(UknightMonsterPartShellComponent* Shell)
{
	Shells.Remove(Shell);
	Revealed.Remove(Shell);
}

void UknightPartScanSubsystem::ForEachShell(TFunctionRef<void(UknightMonsterPartShellComponent&)> Function) const
{
	for (const TWeakObjectPtr<UknightMonsterPartShellComponent>& Shell : Shells)
	{
		if (Shell.IsValid())
		{
			Function(*Shell);
		}
	}
}

UknightMonsterPartShellComponent* UknightPartScanSubsystem::GetAnyShell() const
{
	for (const TWeakObjectPtr<UknightMonsterPartShellComponent>& Shell : Shells)
	{
		if (Shell.IsValid())
		{
			return Shell.Get();
		}
	}
	return nullptr;
}

void UknightPartScanSubsystem::StartScan(const FVector& Origin, UMaterialInterface* ScanMaterial, float Speed, float MaxRadius)
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}
	ScanOrigin = Origin;
	ScanRadius = 0.f;
	ScanSpeed = FMath::Max(Speed, 1.f);
	ScanMaxRadius = FMath::Max(MaxRadius, 1.f);
	Revealed.Reset();
	bScanning = true;

	if (ScanMaterial && !Volume)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Volume = World->SpawnActor<APostProcessVolume>(Params);
		if (Volume)
		{
			Volume->bUnbound = true;
			Volume->Priority = 1000.f;
			Volume->BlendWeight = 1.f;
			ScanMID = UMaterialInstanceDynamic::Create(ScanMaterial, this);
			Volume->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, ScanMID));
		}
	}
	if (Volume)
	{
		Volume->bEnabled = true;
	}
	if (ScanMID)
	{
		ScanMID->SetVectorParameterValue(ScanOriginParam, FLinearColor(ScanOrigin));
		ScanMID->SetScalarParameterValue(ScanRadiusParam, 0.f);
		ScanMID->SetScalarParameterValue(ScanAlphaParam, 1.f);
	}
}

void UknightPartScanSubsystem::Tick(float DeltaTime)
{
	if (!bScanning)
	{
		return;
	}
	ScanRadius += ScanSpeed * DeltaTime;

	for (const TWeakObjectPtr<UknightMonsterPartShellComponent>& Shell : Shells)
	{
		const AActor* const Owner = Shell.IsValid() ? Shell->GetOwner() : nullptr;
		if (Owner && !Revealed.Contains(Shell) && FVector::Dist(Owner->GetActorLocation(), ScanOrigin) <= ScanRadius)
		{
			Revealed.Add(Shell);
			Shell->PlayScanReveal();
		}
	}

	if (ScanMID)
	{
		const float FadeStart = ScanMaxRadius * (1.f - ScanFadeFraction);
		const float Alpha = ScanRadius <= FadeStart ? 1.f : FMath::Clamp(1.f - (ScanRadius - FadeStart) / (ScanMaxRadius - FadeStart), 0.f, 1.f);
		ScanMID->SetScalarParameterValue(ScanRadiusParam, ScanRadius);
		ScanMID->SetScalarParameterValue(ScanAlphaParam, Alpha);
	}
	if (ScanRadius >= ScanMaxRadius)
	{
		StopScan();
	}
}

void UknightPartScanSubsystem::StopScan()
{
	bScanning = false;
	if (Volume)
	{
		Volume->bEnabled = false;
	}
}

TStatId UknightPartScanSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UknightPartScanSubsystem, STATGROUP_Tickables);
}

bool UknightPartScanSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UknightPartScanSubsystem::Deinitialize()
{
	StopScan();
	Volume = nullptr;
	ScanMID = nullptr;
	Shells.Reset();
	Revealed.Reset();
	Super::Deinitialize();
}
