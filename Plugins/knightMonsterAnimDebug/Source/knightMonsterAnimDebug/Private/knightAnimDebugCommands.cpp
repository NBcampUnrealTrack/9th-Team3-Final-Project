// 몬스터 애니메이션 디버거 콘솔 명령. Gameplay Debugger 키와 같은 일을 하며, 자동화 스크립트에서도 쓴다.
//
// 대상은 Gameplay Debugger가 고른 몬스터이고, 없으면 플레이어에게 가장 가까운 몬스터다.
//
//   knight.AnimDebug.List                         대상의 실행 가능한 어빌리티 목록
//   knight.AnimDebug.Play <Name>                  어빌리티 강제 실행 (이름 일부만 써도 된다)
//   knight.AnimDebug.Repeat <Name> [Delay=1]      끝날 때마다 Delay초 뒤 다시 실행. 인자가 없으면 반복을 끈다
//   knight.AnimDebug.FreezeAI [0|1]               추적 중인 몬스터의 AI 정지·재개. 인자가 없으면 토글
//   knight.AnimDebug.TimeScale <Scale>            전체 배속 (0.01~4)
//   knight.AnimDebug.Pause [0|1]                  일시정지(배속을 최저로 내린다). 인자가 없으면 토글
//   knight.AnimDebug.Step [Frames=1]              일시정지 상태에서 몇 프레임만 진행
//   knight.AnimDebug.Record [Label]               녹화 시작. 녹화 중이면 멈추고 저장한다

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "knightAnimDebugModule.h"
#include "knightAnimDebugSubsystem.h"

namespace knightAnimDebugCommands
{
	static UknightAnimDebugSubsystem* GetSubsystem(UWorld* World)
	{
		UknightAnimDebugSubsystem* const Subsystem = World ? World->GetSubsystem<UknightAnimDebugSubsystem>() : nullptr;
		if (!Subsystem)
		{
			UE_LOG(LogknightAnimDebug, Warning, TEXT("게임 월드(PIE 또는 Standalone)에서만 쓸 수 있다."));
		}
		return Subsystem;
	}

	static APawn* GetTarget(UknightAnimDebugSubsystem* Subsystem)
	{
		APawn* const Target = Subsystem ? Subsystem->ResolveTarget() : nullptr;
		if (Subsystem && !Target)
		{
			UE_LOG(LogknightAnimDebug, Warning, TEXT("추적 중인 몬스터가 없다. 설정의 TrackedPawnClass를 확인한다."));
		}
		return Target;
	}

	/** "0"/"1"이면 그 값, 없으면 Current를 뒤집는다. */
	static bool ParseToggle(const TArray<FString>& Args, bool Current)
	{
		return Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) != 0 : !Current;
	}

	static void List(const TArray<FString>& Args, UWorld* World)
	{
		UknightAnimDebugSubsystem* const Subsystem = GetSubsystem(World);
		const APawn* const Target = GetTarget(Subsystem);
		if (!Target)
		{
			return;
		}
		TArray<FString> Names;
		Subsystem->GetPlayableAbilityNames(Target, Names);
		UE_LOG(LogknightAnimDebug, Display, TEXT("%s: 실행 가능한 어빌리티 %d개"), *Target->GetActorNameOrLabel(), Names.Num());
		for (const FString& Name : Names)
		{
			UE_LOG(LogknightAnimDebug, Display, TEXT("  %s"), *Name);
		}
	}

	static void Play(const TArray<FString>& Args, UWorld* World)
	{
		UknightAnimDebugSubsystem* const Subsystem = GetSubsystem(World);
		if (APawn* const Target = GetTarget(Subsystem))
		{
			Subsystem->PlayAbility(Target, Args.IsValidIndex(0) ? Args[0] : FString());
		}
	}

	static void Repeat(const TArray<FString>& Args, UWorld* World)
	{
		UknightAnimDebugSubsystem* const Subsystem = GetSubsystem(World);
		if (!Subsystem)
		{
			return;
		}
		if (!Args.IsValidIndex(0))
		{
			Subsystem->StopRepeat();
			return;
		}
		if (APawn* const Target = GetTarget(Subsystem))
		{
			Subsystem->StartRepeat(Target, Args[0], Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : 1.f);
		}
	}

	static void FreezeAI(const TArray<FString>& Args, UWorld* World)
	{
		if (UknightAnimDebugSubsystem* const Subsystem = GetSubsystem(World))
		{
			Subsystem->SetAIFrozen(ParseToggle(Args, Subsystem->IsAIFrozen()));
		}
	}

	static void TimeScale(const TArray<FString>& Args, UWorld* World)
	{
		if (UknightAnimDebugSubsystem* const Subsystem = GetSubsystem(World))
		{
			Subsystem->SetTimeScale(Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 1.f);
		}
	}

	static void Pause(const TArray<FString>& Args, UWorld* World)
	{
		if (UknightAnimDebugSubsystem* const Subsystem = GetSubsystem(World))
		{
			Subsystem->SetPaused(ParseToggle(Args, Subsystem->IsPaused()));
		}
	}

	static void Step(const TArray<FString>& Args, UWorld* World)
	{
		if (UknightAnimDebugSubsystem* const Subsystem = GetSubsystem(World))
		{
			Subsystem->StepFrames(Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 1);
		}
	}

	static void Record(const TArray<FString>& Args, UWorld* World)
	{
		UknightAnimDebugSubsystem* const Subsystem = GetSubsystem(World);
		if (!Subsystem)
		{
			return;
		}
		if (Subsystem->IsRecording())
		{
			Subsystem->StopRecording();
		}
		else
		{
			Subsystem->StartRecording(Args.IsValidIndex(0) ? Args[0] : FString());
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs ListCmd(TEXT("knight.AnimDebug.List"),
		TEXT("knight.AnimDebug.List. Lists the target monster's playable abilities."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&List));
	static FAutoConsoleCommandWithWorldAndArgs PlayCmd(TEXT("knight.AnimDebug.Play"),
		TEXT("knight.AnimDebug.Play <Name>. Activates an ability on the target monster; a partial name works."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Play), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs RepeatCmd(TEXT("knight.AnimDebug.Repeat"),
		TEXT("knight.AnimDebug.Repeat <Name> [Delay=1]. Re-activates the ability Delay seconds after it ends; no argument stops."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Repeat), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs FreezeAICmd(TEXT("knight.AnimDebug.FreezeAI"),
		TEXT("knight.AnimDebug.FreezeAI [0|1]. Pauses or resumes the AI of tracked monsters; no argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FreezeAI), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs TimeScaleCmd(TEXT("knight.AnimDebug.TimeScale"),
		TEXT("knight.AnimDebug.TimeScale <Scale>. Global time dilation, 0.01 to 4."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TimeScale), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs PauseCmd(TEXT("knight.AnimDebug.Pause"),
		TEXT("knight.AnimDebug.Pause [0|1]. Pauses the game; no argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Pause), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs StepCmd(TEXT("knight.AnimDebug.Step"),
		TEXT("knight.AnimDebug.Step [Frames=1]. While paused, advances a few frames and pauses again."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Step), ECVF_Cheat);
	static FAutoConsoleCommandWithWorldAndArgs RecordCmd(TEXT("knight.AnimDebug.Record"),
		TEXT("knight.AnimDebug.Record [Label]. Starts recording; while recording, stops and writes Markdown and CSV to Saved/Profiling/knightAnimDebug."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Record));
}
