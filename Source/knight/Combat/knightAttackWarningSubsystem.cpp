#include "Combat/knightAttackWarningSubsystem.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "knight.h"

namespace knightAttackWarning
{
	UNiagaraComponent* SpawnOnSocket(UNiagaraSystem* System, USceneComponent& Component, const FknightAttackWarningSettings& Settings, bool bAutoDestroy)
	{
		if (!System)
		{
			return nullptr;
		}
		UNiagaraComponent* const Niagara = UNiagaraFunctionLibrary::SpawnSystemAttached(System, &Component, Settings.Socket, FVector::ZeroVector,
			FRotator::ZeroRotator, EAttachLocation::SnapToTarget, bAutoDestroy, true, bAutoDestroy ? ENCPoolMethod::AutoRelease : ENCPoolMethod::None);
		if (Niagara)
		{
			// 몸 스케일을 받지 않고 cm 그대로 쓴다. 오프셋은 월드 방향이라 위치만 옮긴다.
			Niagara->SetUsingAbsoluteScale(true);
			Niagara->SetWorldScale3D(FVector(Settings.EmberScale));
			if (!Settings.Offset.IsNearlyZero())
			{
				Niagara->SetWorldLocation(Niagara->GetComponentLocation() + Settings.Offset);
			}
		}
		return Niagara;
	}
}

FVector FknightActiveAttackWarning::GetLocation() const
{
	const USceneComponent* const Scene = Component.Get();
	if (!Scene)
	{
		return Offset;
	}
	return (Socket.IsNone() ? Scene->GetComponentLocation() : Scene->GetSocketLocation(Socket)) + Offset;
}

int32 UknightAttackWarningSubsystem::Start(USceneComponent* Component, const FknightAttackWarningSettings& Settings, float Duration)
{
	if (!Component || !Settings.bEnabled || Duration <= 0.f)
	{
		return 0;
	}
	FknightActiveAttackWarning& Warning = Warnings.AddDefaulted_GetRef();
	Warning.Id = NextId++;
	Warning.Component = Component;
	Warning.Owner = Component->GetOwner();
	Warning.Socket = Component->DoesSocketExist(Settings.Socket) ? Settings.Socket : NAME_None;
	Warning.Offset = Settings.Offset;
	Warning.Duration = Duration;

	const UknightAttackWarningConfig* const Config = GetDefault<UknightAttackWarningConfig>();
	FknightAttackWarningSettings Placed = Settings;
	Placed.Socket = Warning.Socket;
	knightAttackWarning::SpawnOnSocket(Config->GlintEffect.LoadSynchronous(), *Component, Placed, true);
	Warning.Embers = knightAttackWarning::SpawnOnSocket(Config->EmberEffect.LoadSynchronous(), *Component, Placed, false);
	UE_LOG(LogknightAbility, Verbose, TEXT("%s: attack warning %d on %s for %.2fs."), *GetNameSafe(Warning.Owner.Get()), Warning.Id, *Warning.Socket.ToString(), Duration);
	return Warning.Id;
}

void UknightAttackWarningSubsystem::Stop(int32 Id)
{
	const int32 Index = Warnings.IndexOfByPredicate([Id](const FknightActiveAttackWarning& Warning) { return Warning.Id == Id; });
	if (Index != INDEX_NONE)
	{
		Remove(Index);
	}
}

void UknightAttackWarningSubsystem::Tick(float DeltaTime)
{
	for (int32 Index = Warnings.Num() - 1; Index >= 0; --Index)
	{
		FknightActiveAttackWarning& Warning = Warnings[Index];
		const AActor* const Owner = Warning.Owner.Get();
		if (!Owner || !Warning.Component.IsValid())
		{
			Remove(Index);
			continue;
		}
		// 어빌리티 시계와 같이 몬스터 시간으로 센다. 히트스톱 동안은 예고도 멈춘다.
		Warning.Elapsed += DeltaTime * FMath::Max(Owner->CustomTimeDilation, 0.f);
		if (Warning.Elapsed >= Warning.Duration)
		{
			Remove(Index);
		}
	}
}

void UknightAttackWarningSubsystem::Remove(int32 Index)
{
	if (UNiagaraComponent* const Embers = Warnings[Index].Embers.Get())
	{
		// 남은 불씨는 수명까지 떨어지고 사라진다.
		Embers->SetAutoDestroy(true);
		Embers->Deactivate();
	}
	Warnings.RemoveAt(Index);
}

TStatId UknightAttackWarningSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UknightAttackWarningSubsystem, STATGROUP_Tickables);
}

bool UknightAttackWarningSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UknightAttackWarningSubsystem::Deinitialize()
{
	for (int32 Index = Warnings.Num() - 1; Index >= 0; --Index)
	{
		Remove(Index);
	}
	Super::Deinitialize();
}
