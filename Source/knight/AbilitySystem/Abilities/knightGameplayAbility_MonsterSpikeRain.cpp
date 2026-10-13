#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterSpikeRain.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
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

FVector UknightGameplayAbility_MonsterSpikeRain::FindGround(const FVector& Location, float FallbackZ) const
{
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(knightMonsterSpikeGround), false, GetAvatarActorFromActorInfo());
	const FVector From(Location.X, Location.Y, Location.Z + 3000.f);
	const FVector To(Location.X, Location.Y, Location.Z - 5000.f);
	// 캐릭터 캡슐에 걸리면 가시가 머리 위에 꽂히므로 지형(WorldStatic)만 본다.
	if (GetWorld() && GetWorld()->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return Hit.ImpactPoint;
	}
	return FVector(Location.X, Location.Y, FallbackZ);
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
	const AActor* const Target = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	const USkeletalMeshComponent* const MonsterMesh = Monster->GetMesh();
	const FVector Back = MonsterMesh && MonsterMesh->DoesSocketExist(LaunchSocket) ? MonsterMesh->GetSocketLocation(LaunchSocket) : Monster->GetActorLocation();
	// 타깃이 없으면 정면 먼 곳에 떨어뜨린다.
	const FVector Center = Target ? Target->GetActorLocation() : Monster->GetActorLocation() + Monster->GetActorForwardVector() * Selection.MaxRange * 0.5f;
	const float GroundZ = Monster->GetActorLocation().Z;

	FknightSpikeLook Look;
	Look.Mesh = SpikeMesh;
	Look.Material = SpikeMaterial;
	Look.Scale = SpikeScale;
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Monster;
	SpawnParams.Instigator = Monster;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (LaunchEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(Monster, LaunchEffect, Back + FVector::UpVector * LaunchHeight * 0.5f, FRotator::ZeroRotator,
			FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
	}

	const TSharedPtr<FknightSpikeVolleyHits> VolleyHits = MakeShared<FknightSpikeVolleyHits>();
	const TWeakObjectPtr<AActor> WeakMonster(Monster);
	for (int32 Index = 0; Index < SpikeCount; ++Index)
	{
		// 올라가는 가시: 등에서 위로 곧게 솟아 화면 밖에서 사라진다.
		const FVector2D Spread = FMath::RandPointInCircle(LaunchSpread);
		if (AknightMonsterSpikeProjectile* const Up = World->SpawnActor<AknightMonsterSpikeProjectile>(
			Back + FVector(Spread.X, Spread.Y, LaunchHeight), FRotator::ZeroRotator, SpawnParams))
		{
			Up->SetLook(Look);
			Up->LaunchUp(AscendSpeed * FMath::FRandRange(0.9f, 1.1f), AscendTime);
		}

		// 첫 가시는 타깃 자리, 나머지는 무작위 방향으로 흩뿌리되 CenterBias만큼 타깃 쪽으로 모은다.
		FVector Landing = Center;
		if (Index > 0)
		{
			const float Angle = FMath::FRandRange(0.f, 2.f * PI);
			Landing += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * ScatterRadius * FMath::Pow(FMath::FRand(), CenterBias);
		}
		FknightSpikeImpact Impact;
		Impact.Landing = FindGround(Landing, GroundZ);
		Impact.HitRadius = HitRadius;
		Impact.PathHeight = PathHeight;
		Impact.Effects = ImpactEffects;
		Impact.StuckTime = StuckTime;
		const float LandTime = FirstLandTime + LandInterval * Index + FMath::FRandRange(0.f, LandJitter);

		// 바닥 경고 원. 위에서 아래로 투영하고, 반지름은 판정과 같다.
		if (WarningDecal && WarningTime > 0.f)
		{
			FTimerHandle WarningHandle;
			const TObjectPtr<UMaterialInterface> Decal = WarningDecal;
			const float Lifetime = FMath::Min(WarningTime, LandTime) + 0.05f;
			const FTimerDelegate ShowWarning = FTimerDelegate::CreateWeakLambda(World, [World, Decal, Impact, Lifetime]()
			{
				UGameplayStatics::SpawnDecalAtLocation(World, Decal, FVector(300.f, Impact.HitRadius, Impact.HitRadius), Impact.Landing,
					FRotator(-90.f, 0.f, 0.f), Lifetime);
			});
			const float WarningDelay = LandTime - WarningTime;
			if (WarningDelay > 0.f)
			{
				World->GetTimerManager().SetTimer(WarningHandle, ShowWarning, WarningDelay, false);
			}
			else
			{
				ShowWarning.Execute();
			}
		}

		// 떨어지는 가시: 착탄 FallTime 전에 착탄점 바로 위에서 새로 만들어 수직으로 꽂는다.
		FTimerHandle FallHandle;
		const FknightMeleeHitParams Params = HitParams;
		const float Height = FallHeight;
		const float Fall = FMath::Min(FallTime, LandTime);
		World->GetTimerManager().SetTimer(FallHandle, FTimerDelegate::CreateWeakLambda(World, [World, WeakMonster, Params, Impact, Look, Height, Fall, VolleyHits]()
		{
			FActorSpawnParameters FallParams;
			FallParams.Owner = WeakMonster.Get();
			FallParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AknightMonsterSpikeProjectile* const Down = World->SpawnActor<AknightMonsterSpikeProjectile>(
				Impact.Landing + FVector::UpVector * Height, FRotator::ZeroRotator, FallParams))
			{
				Down->SetLook(Look);
				Down->LaunchDown(WeakMonster.Get(), Params, Impact, Fall, VolleyHits);
			}
		}), LandTime - Fall, false);
	}
	UE_LOG(LogknightAI, Verbose, TEXT("%s: %s fired %d spikes around %s."), *GetNameSafe(Monster), *GetName(), SpikeCount, *GetNameSafe(Target));
}

void UknightGameplayAbility_MonsterSpikeRain::HandleMontageEnded()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}
