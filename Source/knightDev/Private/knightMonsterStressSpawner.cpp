#include "knightMonsterStressSpawner.h"

#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "TimerManager.h"
#include "knightDev.h"
#include "knightPerfTrackerSubsystem.h"

AknightMonsterStressSpawner::AknightMonsterStressSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITORONLY_DATA
	if (UBillboardComponent* const Billboard = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Billboard")))
	{
		Billboard->SetupAttachment(RootComponent);
	}
#endif
}

void AknightMonsterStressSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (bSpawnOnBeginPlay)
	{
		SpawnMonsters(Count);
	}
}

void AknightMonsterStressSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CaptureTimer);
	Super::EndPlay(EndPlayReason);
}

void AknightMonsterStressSpawner::SpawnMonsters(int32 InCount)
{
	DespawnMonsters();
	Count = FMath::Clamp(InCount, 1, 200);
	UWorld* const World = GetWorld();
	if (!World || !MonsterClass)
	{
		UE_LOG(LogknightDev, Warning, TEXT("%s: set MonsterClass to spawn."), *GetName());
		return;
	}
	if (RandomSeed != 0)
	{
		FMath::RandInit(RandomSeed);
		FMath::SRandInit(RandomSeed);
	}

	// 스포너를 중심으로 한 정사각 격자에 배치하고, 스포너의 전방을 바라보게 한다.
	const int32 Columns = FMath::CeilToInt32(FMath::Sqrt(static_cast<float>(Count)));
	const int32 Rows = FMath::DivideAndRoundUp(Count, Columns);
	const FVector GridOrigin(-(Rows - 1) * Spacing * 0.5f, -(Columns - 1) * Spacing * 0.5f, 0.f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector LocalOffset = GridOrigin + FVector((Index / Columns) * Spacing, (Index % Columns) * Spacing, 0.f);
		const FTransform SpawnTransform(GetActorRotation(), GetActorTransform().TransformPosition(LocalOffset));
		if (AknightMonsterCharacterBase* const Monster = World->SpawnActor<AknightMonsterCharacterBase>(MonsterClass, SpawnTransform, Params))
		{
			SpawnedMonsters.Add(Monster);
		}
	}
	UE_LOG(LogknightDev, Log, TEXT("%s spawned %d x %s."), *GetName(), SpawnedMonsters.Num(), *GetNameSafe(MonsterClass));

	if (bAutoCapture)
	{
		if (UknightPerfTrackerSubsystem* const Tracker = UknightPerfTrackerSubsystem::Get(World))
		{
			Tracker->StartCapture(FString::Printf(TEXT("%s_%d"), *CaptureLabel, Count), CaptureDuration, CaptureWarmup);
			// 저장은 트래커가 끝날 때 알아서 한다. 여기서는 잠시 뒤 후속 처리(종료)만 한다.
			GetWorldTimerManager().SetTimer(CaptureTimer, this, &ThisClass::HandleCaptureFinished, CaptureWarmup + CaptureDuration + 1.f, false);
		}
		else
		{
			UE_LOG(LogknightDev, Warning, TEXT("%s: knightPerfTracker subsystem is not available in this world; no capture."), *GetName());
		}
	}
}

void AknightMonsterStressSpawner::DespawnMonsters()
{
	for (AknightMonsterCharacterBase* const Monster : SpawnedMonsters)
	{
		if (IsValid(Monster))
		{
			if (AController* const Controller = Monster->GetController())
			{
				Controller->Destroy();
			}
			Monster->Destroy();
		}
	}
	SpawnedMonsters.Reset();
	GetWorldTimerManager().ClearTimer(CaptureTimer);
}

void AknightMonsterStressSpawner::HandleCaptureFinished()
{
	if (bQuitAfterCapture)
	{
		UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
	}
}

namespace knightMonsterStressSpawner
{
	static void StressSpawnCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		const int32 NewCount = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 1;
		int32 Spawners = 0;
		for (TActorIterator<AknightMonsterStressSpawner> It(World); It; ++It)
		{
			It->SpawnMonsters(NewCount);
			++Spawners;
		}
		if (Spawners == 0)
		{
			UE_LOG(LogknightDev, Warning, TEXT("knight.Monster.StressSpawn: no knightMonsterStressSpawner in this map."));
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs StressSpawnCmd(
		TEXT("knight.Monster.StressSpawn"),
		TEXT("knight.Monster.StressSpawn <Count>. Respawns every monster stress spawner in the map with Count monsters."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StressSpawnCommand),
		ECVF_Cheat);
}
