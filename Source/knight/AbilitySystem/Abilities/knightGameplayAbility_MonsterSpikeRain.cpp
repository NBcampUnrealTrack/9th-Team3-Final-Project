#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterSpikeRain.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterSpikeProjectile.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "knight.h"

UknightGameplayAbility_MonsterSpikeRain::UknightGameplayAbility_MonsterSpikeRain(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Selection.MinRange = 1000.f;
	Selection.MaxRange = 4000.f;
	Selection.MaxAngle = 60.f;
}

void UknightGameplayAbility_MonsterSpikeRain::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!Montage)
	{
		UE_LOG(LogknightAbility, Error, TEXT("%s has no Montage."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayMontageAndWait* const MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageEnded);
	MontageTask->ReadyForActivation();

	UAbilityTask_WaitDelay* const FireTask = UAbilityTask_WaitDelay::WaitDelay(this, FireTime);
	FireTask->OnFinish.AddDynamic(this, &ThisClass::HandleFire);
	FireTask->ReadyForActivation();
}

namespace
{
	/** 가시 하나를 예약할 때 필요한 값. 어빌리티가 먼저 끝나도 가시는 떨어지므로 값으로 들고 간다. */
	struct FSpikeVolley
	{
		TWeakObjectPtr<AActor> Monster;
		FknightMeleeHitParams HitParams;
		FknightSpikeLook Look;
		float FallHeight = 0.f;
		float FallTime = 0.f;
		TObjectPtr<UMaterialInterface> WarningDecal;
		float WarningTime = 0.f;
		TSharedPtr<FknightSpikeVolleyHits> Hits;
	};

	/** XY 위치 아래의 지형(캐릭터는 무시). 지형을 못 찾으면 FallbackZ. */
	FVector FindGround(const UWorld* World, const AActor* Ignore, const FVector& Location, float FallbackZ)
	{
		FHitResult Hit;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(knightMonsterSpikeGround), false, Ignore);
		const FVector From(Location.X, Location.Y, Location.Z + 3000.f);
		const FVector To(Location.X, Location.Y, Location.Z - 5000.f);
		// 캐릭터 캡슐에 걸리면 가시가 머리 위에 꽂히므로 지형(WorldStatic)만 본다.
		if (World && World->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			return Hit.ImpactPoint;
		}
		return FVector(Location.X, Location.Y, FallbackZ);
	}

	/** 지금부터 LandDelay초 뒤에 Impact.Landing에 꽂힐 가시 하나를 예약한다(바닥 경고 + 떨어지는 가시). */
	void ScheduleSpike(UWorld* World, const FSpikeVolley& Volley, const FknightSpikeImpact& Impact, float LandDelay)
	{
		// 바닥 경고 원. 위에서 아래로 투영하고, 반지름은 판정과 같다. 경고 시간 동안 페이드인한다.
		if (Volley.WarningDecal && Volley.WarningTime > 0.f)
		{
			const TObjectPtr<UMaterialInterface> Decal = Volley.WarningDecal;
			const float FadeIn = FMath::Min(Volley.WarningTime, LandDelay);
			const FTimerDelegate ShowWarning = FTimerDelegate::CreateWeakLambda(World, [World, Decal, Impact, FadeIn]()
			{
				if (UDecalComponent* const Warning = UGameplayStatics::SpawnDecalAtLocation(World, Decal,
					FVector(300.f, Impact.HitRadius, Impact.HitRadius), Impact.Landing, FRotator(-90.f, 0.f, 0.f), FadeIn + 0.05f))
				{
					Warning->SetFadeIn(0.f, FadeIn);
				}
			});
			const float WarningDelay = LandDelay - Volley.WarningTime;
			if (WarningDelay > 0.f)
			{
				FTimerHandle WarningHandle;
				World->GetTimerManager().SetTimer(WarningHandle, ShowWarning, WarningDelay, false);
			}
			else
			{
				ShowWarning.Execute();
			}
		}

		// 떨어지는 가시: 착탄 FallTime 전에 착탄점 바로 위에서 새로 만들어 수직으로 꽂는다.
		const float Fall = FMath::Min(Volley.FallTime, LandDelay);
		const FTimerDelegate Drop = FTimerDelegate::CreateWeakLambda(World, [World, Volley, Impact, Fall]()
		{
			FActorSpawnParameters FallParams;
			FallParams.Owner = Volley.Monster.Get();
			FallParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AknightMonsterSpikeProjectile* const Down = World->SpawnActor<AknightMonsterSpikeProjectile>(
				Impact.Landing + FVector::UpVector * Volley.FallHeight, FRotator::ZeroRotator, FallParams))
			{
				Down->SetLook(Volley.Look);
				Down->LaunchDown(Volley.Monster.Get(), Volley.HitParams, Impact, Fall, Volley.Hits);
			}
		});
		if (LandDelay - Fall > 0.f)
		{
			FTimerHandle FallHandle;
			World->GetTimerManager().SetTimer(FallHandle, Drop, LandDelay - Fall, false);
		}
		else
		{
			Drop.Execute();
		}
	}
}

void UknightGameplayAbility_MonsterSpikeRain::HandleFire()
{
	AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetAvatarActorFromActorInfo());
	UWorld* const World = GetWorld();
	if (!Monster || !World)
	{
		return;
	}
	const UknightMonsterTargetingComponent* const Targeting = Monster->GetTargetingComponent();
	AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	const USkeletalMeshComponent* const MonsterMesh = Monster->GetMesh();
	const FVector Back = MonsterMesh && MonsterMesh->DoesSocketExist(LaunchSocket) ? MonsterMesh->GetSocketLocation(LaunchSocket) : Monster->GetActorLocation();
	// 타깃이 없으면 정면 먼 곳에 떨어뜨린다.
	const FVector Center = Target ? Target->GetActorLocation() : Monster->GetActorLocation() + Monster->GetActorForwardVector() * Selection.MaxRange * 0.5f;
	const float GroundZ = Monster->GetActorLocation().Z;

	FSpikeVolley Volley;
	Volley.Monster = Monster;
	Volley.HitParams = HitParams;
	Volley.Look.Mesh = SpikeMesh;
	Volley.Look.Material = SpikeMaterial;
	Volley.Look.Scale = SpikeScale;
	Volley.Look.Effect = SpikeEffect;
	Volley.FallHeight = FallHeight;
	Volley.FallTime = FallTime;
	Volley.WarningDecal = WarningDecal;
	Volley.WarningTime = WarningTime;
	Volley.Hits = MakeShared<FknightSpikeVolleyHits>();
	Volley.Hits->RehitInterval = RehitInterval;

	FknightSpikeImpact ImpactTemplate;
	ImpactTemplate.HitRadius = HitRadius;
	ImpactTemplate.PathHeight = PathHeight;
	ImpactTemplate.Effects = ImpactEffects;
	ImpactTemplate.StuckTime = StuckTime;

	const int32 Count = SpikeCount;

	if (LaunchEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(Monster, LaunchEffect, Back + FVector::UpVector * LaunchHeight * 0.5f, FRotator::ZeroRotator,
			FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
	}
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Monster;
	SpawnParams.Instigator = Monster;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// 올라가는 가시: 등에서 위로 곧게 솟아 화면 밖에서 사라진다.
		const FVector2D Spread = FMath::RandPointInCircle(LaunchSpread);
		if (AknightMonsterSpikeProjectile* const Up = World->SpawnActor<AknightMonsterSpikeProjectile>(
			Back + FVector(Spread.X, Spread.Y, LaunchHeight), FRotator::ZeroRotator, SpawnParams))
		{
			Up->SetLook(Volley.Look);
			Up->LaunchUp(AscendSpeed * FMath::FRandRange(0.9f, 1.1f), AscendTime);
		}
	}

	if (Pattern == EknightSpikeRainPattern::Scatter)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			// 첫 가시는 타깃 자리, 나머지는 무작위 방향으로 흩뿌리되 CenterBias만큼 타깃 쪽으로 모은다.
			FVector Landing = Center;
			if (Index > 0)
			{
				const float Angle = FMath::FRandRange(0.f, 2.f * PI);
				Landing += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * ScatterRadius * FMath::Pow(FMath::FRand(), CenterBias);
			}
			FknightSpikeImpact Impact = ImpactTemplate;
			Impact.Landing = FindGround(World, Monster, Landing, GroundZ);
			ScheduleSpike(World, Volley, Impact, FirstLandTime + LandInterval * Index + FMath::FRandRange(0.f, LandJitter));
		}
		UE_LOG(LogknightAI, Verbose, TEXT("%s: %s fired %d spikes around %s."), *GetNameSafe(Monster), *GetName(), Count, *GetNameSafe(Target));
		return;
	}

	// Track: 각 가시는 경고가 뜨는 순간의 타깃 자리를 노린다. 타깃이 없으면 처음 자리에 떨어진다.
	const TWeakObjectPtr<AActor> WeakTarget(Target);
	const float Jitter = TrackJitter;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float LandTime = FirstLandTime + LandInterval * Index + FMath::FRandRange(0.f, LandJitter);
		const float PlaceTime = FMath::Max(LandTime - WarningTime, 0.f);
		const float LandDelay = LandTime - PlaceTime;
		const FTimerDelegate Place = FTimerDelegate::CreateWeakLambda(World, [World, Volley, ImpactTemplate, WeakTarget, Center, Jitter, GroundZ, LandDelay]()
		{
			const AActor* const Goal = WeakTarget.Get();
			const FVector2D Offset = FMath::RandPointInCircle(Jitter);
			FknightSpikeImpact Impact = ImpactTemplate;
			Impact.Landing = FindGround(World, Volley.Monster.Get(), (Goal ? Goal->GetActorLocation() : Center) + FVector(Offset.X, Offset.Y, 0.f), GroundZ);
			ScheduleSpike(World, Volley, Impact, LandDelay);
		});
		if (PlaceTime > 0.f)
		{
			FTimerHandle PlaceHandle;
			World->GetTimerManager().SetTimer(PlaceHandle, Place, PlaceTime, false);
		}
		else
		{
			Place.Execute();
		}
	}
	UE_LOG(LogknightAI, Verbose, TEXT("%s: %s fired %d spikes tracking %s."), *GetNameSafe(Monster), *GetName(), Count, *GetNameSafe(Target));
}

void UknightGameplayAbility_MonsterSpikeRain::HandleMontageEnded()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}
