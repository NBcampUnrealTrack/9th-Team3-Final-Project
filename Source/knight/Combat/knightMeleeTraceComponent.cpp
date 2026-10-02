#include "Combat/knightMeleeTraceComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
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
	// Sample sockets after animation has posed the mesh this frame.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	HitObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	HitObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	HitObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Destructible));
}

void UknightMeleeTraceComponent::BeginTrace(const FknightMeleeTraceParams& Params)
{
	++TraceDepth;
	if (TraceDepth > 1)
	{
		// A nested window keeps the first window's shape and hit list.
		return;
	}

	ActiveParams = Params;
	ActiveParams.BladeSamples = FMath::Max(2, ActiveParams.BladeSamples);
	TraceMesh = FindTraceMesh(Params.MeshComponentTag);
	HitActors.Reset();
	PreviousPoints.Reset();
	if (!TraceMesh.IsValid())
	{
		UE_LOG(LogknightCombat, Warning, TEXT("%s: no mesh with tag '%s' for the melee trace."), *GetNameSafe(GetOwner()), *Params.MeshComponentTag.ToString());
		return;
	}
	SampleBlade(PreviousPoints);
	SetComponentTickEnabled(true);

	if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		ASC->AddLooseGameplayTag(knightGameplayTags::Window_MeleeTrace);
	}
}

void UknightMeleeTraceComponent::EndTrace()
{
	if (TraceDepth <= 0)
	{
		return;
	}
	--TraceDepth;
	if (TraceDepth > 0)
	{
		return;
	}

	SetComponentTickEnabled(false);
	TraceMesh.Reset();
	PreviousPoints.Reset();
	HitActors.Reset();

	if (UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		ASC->RemoveLooseGameplayTag(knightGameplayTags::Window_MeleeTrace);
	}
}

void UknightMeleeTraceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TraceDepth = FMath::Min(TraceDepth, 1);
	EndTrace();
	Super::EndPlay(EndPlayReason);
}

void UknightMeleeTraceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (IsTracing() && TraceMesh.IsValid())
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

void UknightMeleeTraceComponent::SampleBlade(TArray<FVector>& OutPoints) const
{
	OutPoints.Reset(ActiveParams.BladeSamples);
	const UMeshComponent* const Mesh = TraceMesh.Get();
	if (!Mesh)
	{
		return;
	}
	const FVector Start = Mesh->GetSocketLocation(ActiveParams.StartSocket);
	const FVector End = Mesh->GetSocketLocation(ActiveParams.EndSocket);
	for (int32 Index = 0; Index < ActiveParams.BladeSamples; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / static_cast<float>(ActiveParams.BladeSamples - 1);
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

	TArray<FVector> CurrentPoints;
	SampleBlade(CurrentPoints);
	if (CurrentPoints.Num() != PreviousPoints.Num())
	{
		PreviousPoints = CurrentPoints;
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

	const FCollisionShape Sphere = FCollisionShape::MakeSphere(ActiveParams.Radius);
	const bool bDraw = knightMeleeTraceCVars::DrawMeleeTraces != 0;

	TArray<FHitResult> Hits;
	for (int32 Index = 0; Index < CurrentPoints.Num(); ++Index)
	{
		Hits.Reset();
		World->SweepMultiByObjectType(Hits, PreviousPoints[Index], CurrentPoints[Index], FQuat::Identity, ObjectParams, Sphere, QueryParams);
		for (const FHitResult& Hit : Hits)
		{
			HandleHit(Hit);
		}
		if (bDraw)
		{
			DrawDebugLine(World, PreviousPoints[Index], CurrentPoints[Index], Hits.IsEmpty() ? FColor::Green : FColor::Red, false, 1.f);
			DrawDebugSphere(World, CurrentPoints[Index], ActiveParams.Radius, 8, FColor::Yellow, false, 1.f);
		}
	}
	PreviousPoints = MoveTemp(CurrentPoints);
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
	const ICombatInterface* const Combat = Cast<ICombatInterface>(Target);
	if (!Combat || !Combat->CanReceiveHit(GetOwner()))
	{
		return false;
	}
	return FGenericTeamId::GetAttitude(GetOwner(), Target) == ETeamAttitude::Hostile;
}
