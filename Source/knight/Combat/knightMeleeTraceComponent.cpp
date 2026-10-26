#include "Combat/knightMeleeTraceComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Combat/knightCombatLibrary.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/CombatInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
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
	StartSwingTrail(Trace);

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
	StopSwingTrail(ActiveTraces[Index]);
	ActiveTraces.RemoveAt(Index);
	if (ActiveTraces.IsEmpty())
	{
		StopAllTraces();
	}
}

void UknightMeleeTraceComponent::StartSwingTrail(FActiveTrace& Trace) const
{
	UMeshComponent* const Mesh = Trace.Mesh.Get();
	if (!SwingTrail || !Mesh)
	{
		return;
	}
	const FVector Start = Mesh->GetSocketLocation(Trace.Params.StartSocket);
	const FVector End = Mesh->GetSocketLocation(Trace.Params.EndSocket);
	const FVector Blade = End - Start;
	const float Length = Blade.Size();
	if (Length < 1.f)
	{
		return;
	}
	const FVector BladeDir = Blade / Length;

	UNiagaraSystem* TrailAsset = SwingTrail;
	float WidthMultiplier = 1.f;
	bool bLightOn = bSwingTrailLight;
	FLinearColor ColorHead = SwingTrailColorHead;
	FLinearColor ColorTail = SwingTrailColorTail;

	if (bUseStanceTrailVariation)
	{
		if (const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
		{
			const bool bTwoHanded = ASC->HasMatchingGameplayTag(knightGameplayTags::State_Stance_TwoHanded);
			if (bTwoHanded)
			{
				if (TwoHandedSwingTrail)
				{
					TrailAsset = TwoHandedSwingTrail;
				}
				WidthMultiplier = TwoHandedTrailWidthMultiplier;
				ColorHead = TwoHandedTrailColorHead;
				ColorTail = TwoHandedTrailColorTail;
				bLightOn = bTwoHandedTrailLight;
			}
			else
			{
				WidthMultiplier = OneHandedTrailWidthMultiplier;
				ColorHead = OneHandedTrailColorHead;
				ColorTail = OneHandedTrailColorTail;
			}
		}
	}

	// 시작 소켓에 붙여 칼과 같이 움직이게 하고, 위치·회전은 월드 기준으로 맞춘 뒤 상대값으로 고정된다.
	// 이펙트가 리본 폭을 로컬 -Z(User.AxisOffset)로 펼치므로 -Z를 칼날 방향으로 돌린다.
	UNiagaraComponent* const FX = UNiagaraFunctionLibrary::SpawnSystemAttached(TrailAsset, Mesh, Trace.Params.StartSocket,
		FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, /*bAutoDestroy*/ true, /*bAutoActivate*/ false);
	if (!FX)
	{
		return;
	}
	const FVector Center = (Start + End) * 0.5f + BladeDir * SwingTrailCenterOffset;
	const FRotator TrailRot = FRotationMatrix::MakeFromZ(-BladeDir).Rotator();
	FX->SetWorldLocationAndRotation(Center, TrailRot);
	FX->SetVariableFloat(TEXT("User.Width"), Length * SwingTrailWidthScale * WidthMultiplier);
	FX->SetVariableBool(TEXT("User.LightOn"), bLightOn);
	if (SwingTrailLife > 0.f)
	{
		FX->SetVariableFloat(TEXT("User.Life"), SwingTrailLife);
	}
	if (bOverrideSwingTrailColor || bUseStanceTrailVariation)
	{
		FX->SetVariableLinearColor(TEXT("User.ColorHead"), ColorHead);
		FX->SetVariableLinearColor(TEXT("User.ColorTail"), ColorTail);
		FX->SetVariableLinearColor(TEXT("User.ColorDark"), ColorTail);
	}
	FX->Activate(true);
	Trace.TrailFX = FX;

	if (bEnableRefractionTrail)
	{
		UNiagaraSystem* RefractionAsset = RefractionTrail;
		if (!RefractionAsset)
		{
			RefractionAsset = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/SlashHitVFX/NS/NS_Trail_refraction.NS_Trail_refraction"));
		}
		if (RefractionAsset)
		{
			if (UNiagaraComponent* const RefFX = UNiagaraFunctionLibrary::SpawnSystemAttached(RefractionAsset, Mesh, Trace.Params.StartSocket,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, /*bAutoDestroy*/ true, /*bAutoActivate*/ false))
			{
				RefFX->SetWorldLocationAndRotation(Center, TrailRot);
				RefFX->SetVariableFloat(TEXT("User.Width"), Length * SwingTrailWidthScale * WidthMultiplier);
				RefFX->Activate(true);
				Trace.RefractionFX = RefFX;
			}
		}
	}
}

void UknightMeleeTraceComponent::StopSwingTrail(FActiveTrace& Trace)
{
	// 끄면 이펙트가 남은 꼬리를 서서히 지우고(NMS_infiniteDeactivate) 다 사라지면 스스로 없어진다.
	if (UNiagaraComponent* const FX = Trace.TrailFX.Get())
	{
		FX->Deactivate();
	}
	Trace.TrailFX.Reset();

	if (UNiagaraComponent* const RefFX = Trace.RefractionFX.Get())
	{
		RefFX->Deactivate();
	}
	Trace.RefractionFX.Reset();
}

void UknightMeleeTraceComponent::StopAllTraces()
{
	for (FActiveTrace& Trace : ActiveTraces)
	{
		StopSwingTrail(Trace);
	}
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

void UknightMeleeTraceComponent::HitArea(const FVector& Center, float Radius)
{
	UWorld* const World = GetWorld();
	if (!World || Radius <= 0.f)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : HitObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
	}
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(knightMeleeArea), false, GetOwner());
	TArray<AActor*> AttachedActors;
	GetOwner()->GetAttachedActors(AttachedActors);
	QueryParams.AddIgnoredActors(AttachedActors);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	// 피해 컨텍스트, 큐, 부위 조회는 히트 결과를 필요로 하므로 구체 중심에서 가장 가까운 표면 지점으로 하나씩 만든다.
	TArray<FHitResult> AreaHits;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* const Target = Overlap.GetActor();
		UPrimitiveComponent* const Component = Overlap.GetComponent();
		if (!Target || !Component)
		{
			continue;
		}
		FVector ImpactPoint = Component->GetComponentLocation();
		FVector ClosestPoint;
		if (Component->GetClosestPointOnCollision(Center, ClosestPoint) >= 0.f)
		{
			ImpactPoint = ClosestPoint;
		}
		FHitResult& Hit = AreaHits.Emplace_GetRef(Target, Component, ImpactPoint, (Center - ImpactPoint).GetSafeNormal());
		Hit.bBlockingHit = true;
		Hit.TraceStart = Center;
		Hit.TraceEnd = ImpactPoint;
	}

	TArray<FHitResult> BestHits;
	UknightCombatLibrary::SelectBestHitPerActor(AreaHits, BestHits);
	for (const FHitResult& Hit : BestHits)
	{
		HandleHit(Hit);
	}
	// 열린 판정 구간이 없으면 타격 목록을 비워 다음 판정이 같은 대상을 다시 칠 수 있게 한다.
	if (!IsTracing())
	{
		HitActors.Reset();
	}

	if (knightMeleeTraceCVars::DrawMeleeTraces != 0)
	{
		DrawDebugSphere(World, Center, Radius, 24, BestHits.IsEmpty() ? FColor::Yellow : FColor::Red, false, 1.f, 0, 3.f);
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
