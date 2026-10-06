#include "Combat/knightMeleeTraceComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/CombatInterface.h"
#include "System/knightGameplayTags.h"
#include "knight.h"
#include "knightPerfCounters.h"

namespace knightMeleeTraceCVars
{
	static int32 DrawMeleeTraces = 0;
	static FAutoConsoleVariableRef CVarDrawMeleeTraces(
		TEXT("knight.Combat.DrawMeleeTraces"), DrawMeleeTraces,
		TEXT("Draw melee weapon sweeps. 0: off, 1: on"), ECVF_Cheat);
}

UknightMeleeTraceComponent::UknightMeleeTraceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// 이번 프레임 애니메이션이 메시 포즈를 잡은 뒤에 소켓 위치를 샘플링한다.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	HitObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	HitObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	HitObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Destructible));
}

void UknightMeleeTraceComponent::BeginTrace(const FknightMeleeTraceParams& Params)
{
	const bool bFirst = ActiveTraces.IsEmpty();
	FActiveTrace& Trace = ActiveTraces.AddDefaulted_GetRef();
	Trace.Params = Params;
	Trace.Params.BladeSamples = FMath::Max(2, Params.BladeSamples);
	Trace.Mesh = FindTraceMesh(Params.MeshComponentTag);
	if (!Trace.Mesh.IsValid())
	{
		UE_LOG(LogknightCombat, Warning, TEXT("%s: no mesh with tag '%s' for the melee trace."), *GetNameSafe(GetOwner()), *Params.MeshComponentTag.ToString());
	}
	SampleBlade(Trace, Trace.PreviousPoints);

	if (!bFirst)
	{
		// 겹친 윈도우(양손 동시 공격)는 자기 형태로 따로 훑고, 타격 목록은 첫 윈도우부터 함께 쓴다.
		return;
	}
	HitActors.Reset();
	SetComponentTickEnabled(true);
	if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::Window_MeleeTrace);
	}
}

void UknightMeleeTraceComponent::EndTrace(const FknightMeleeTraceParams& Params)
{
	const int32 Index = ActiveTraces.IndexOfByPredicate([&Params](const FActiveTrace& Trace)
	{
		return Trace.Params.StartSocket == Params.StartSocket && Trace.Params.EndSocket == Params.EndSocket
			&& Trace.Params.MeshComponentTag == Params.MeshComponentTag;
	});
	if (Index == INDEX_NONE)
	{
		return;
	}
	ActiveTraces.RemoveAt(Index);
	if (ActiveTraces.IsEmpty())
	{
		StopAllTraces();
	}
}

void UknightMeleeTraceComponent::StopAllTraces()
{
	ActiveTraces.Reset();
	HitActors.Reset();
	SetComponentTickEnabled(false);

	if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		ASC->RemoveLooseGameplayTag(knightGameplayTags::Window_MeleeTrace);
	}
}

void UknightMeleeTraceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsTracing())
	{
		StopAllTraces();
	}
	Super::EndPlay(EndPlayReason);
}

void UknightMeleeTraceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (IsTracing())
	{
		SweepFrame();
	}
}

UMeshComponent* UknightMeleeTraceComponent::FindTraceMesh(FName ComponentTag) const
{
	const AActor* const Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	if (ComponentTag.IsNone())
	{
		if (const ACharacter* const Character = Cast<ACharacter>(Owner))
		{
			return Character->GetMesh();
		}
		return Owner->FindComponentByClass<UMeshComponent>();
	}
	TArray<UMeshComponent*> Meshes;
	Owner->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* const Mesh : Meshes)
	{
		if (Mesh->ComponentHasTag(ComponentTag))
		{
			return Mesh;
		}
	}
	return nullptr;
}

void UknightMeleeTraceComponent::SampleBlade(const FActiveTrace& Trace, TArray<FVector>& OutPoints)
{
	const int32 Samples = Trace.Params.BladeSamples;
	OutPoints.Reset(Samples);
	const UMeshComponent* const Mesh = Trace.Mesh.Get();
	if (!Mesh)
	{
		return;
	}
	const FVector Start = Mesh->GetSocketLocation(Trace.Params.StartSocket);
	const FVector End = Mesh->GetSocketLocation(Trace.Params.EndSocket);
	for (int32 Index = 0; Index < Samples; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / static_cast<float>(Samples - 1);
		OutPoints.Add(FMath::Lerp(Start, End, Alpha));
	}
}

void UknightMeleeTraceComponent::SweepFrame()
{
	KNIGHT_PERF_SCOPE("Combat.MeleeSweep");
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : HitObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
	}
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(knightMeleeTrace), false, GetOwner());
	QueryParams.bReturnPhysicalMaterial = true;
	TArray<AActor*> AttachedActors;
	GetOwner()->GetAttachedActors(AttachedActors);
	QueryParams.AddIgnoredActors(AttachedActors);

	const bool bDraw = knightMeleeTraceCVars::DrawMeleeTraces != 0;

	// 모든 칼날(겹친 윈도우 포함)의 타격을 먼저 모은 뒤 액터당 하나만 남기며, 이동 캡슐보다 부위 타격을 우선한다.
	// 스윕을 하나씩 처리하면 부위가 잡히기 전에 캡슐이 그 액터를 먼저 차지해 버렸다.
	TArray<FHitResult> FrameHits;
	TArray<FHitResult> Hits;
	TArray<FVector> CurrentPoints;
	for (FActiveTrace& Trace : ActiveTraces)
	{
		SampleBlade(Trace, CurrentPoints);
		if (CurrentPoints.Num() != Trace.PreviousPoints.Num())
		{
			Trace.PreviousPoints = CurrentPoints;
			continue;
		}
		const FCollisionShape Sphere = FCollisionShape::MakeSphere(Trace.Params.Radius);
		for (int32 Index = 0; Index < CurrentPoints.Num(); ++Index)
		{
			Hits.Reset();
			World->SweepMultiByObjectType(Hits, Trace.PreviousPoints[Index], CurrentPoints[Index], FQuat::Identity, ObjectParams, Sphere, QueryParams);
			FrameHits.Append(Hits);
			if (bDraw)
			{
				DrawDebugLine(World, Trace.PreviousPoints[Index], CurrentPoints[Index], Hits.IsEmpty() ? FColor::Green : FColor::Red, false, 1.f);
				DrawDebugSphere(World, CurrentPoints[Index], Trace.Params.Radius, 8, FColor::Yellow, false, 1.f);
			}
		}
		Trace.PreviousPoints = CurrentPoints;
	}

	TArray<FHitResult> BestHits;
	UknightCombatLibrary::SelectBestHitPerActor(FrameHits, BestHits);
	for (const FHitResult& Hit : BestHits)
	{
		HandleHit(Hit);
	}
}

void UknightMeleeTraceComponent::HandleHit(const FHitResult& Hit)
{
	AActor* const Target = Hit.GetActor();
	if (!Target || HitActors.Contains(Target) || !IsValidTarget(Target))
	{
		return;
	}
	HitActors.Add(Target);
	OnMeleeHit.Broadcast(Hit);

	FGameplayEventData Payload;
	Payload.EventTag = knightGameplayTags::Event_Melee_Hit;
	Payload.Instigator = GetOwner();
	Payload.Target = Target;
	Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(Hit);
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), Payload.EventTag, Payload);
}

bool UknightMeleeTraceComponent::IsValidTarget(AActor* Target) const
{
	if (!Target || !Target->GetClass()->ImplementsInterface(UCombatInterface::StaticClass())
		|| !ICombatInterface::Execute_CanReceiveHitBP(Target, GetOwner()))
	{
		return false;
	}
	return FGenericTeamId::GetAttitude(GetOwner(), Target) == ETeamAttitude::Hostile;
}
