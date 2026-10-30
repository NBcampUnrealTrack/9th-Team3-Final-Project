#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterGroundEruption.h"

#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

void UknightGameplayAbility_MonsterGroundEruption::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ScatterPoints.Reset();
	bErupted = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	if (!IsActive() || !Avatar || !World)
	{
		return;
	}
	// 불길이 솟을 자리를 지금 정해 두어 예고 효과와 실제 불길이 같은 곳에 난다.
	PickScatterPoints();
	WarningCenter = FindGround(Avatar->GetActorTransform().TransformPosition(AreaOffset));
	WarningSeed = FMath::FRandRange(0.f, 100.f);
	if (WindupTime <= 0.f)
	{
		return;
	}
	if (WarningDecal)
	{
		// 바닥 데칼은 위에서 아래로 투영한다. 몸 높이와 경사를 덮도록 깊게 둔다.
		// 수명을 WindupTime으로 걸면 타격이 한 프레임만 늦어도 분출 전에 사라지므로, 분출·종료 때 StopWarning이 지운다.
		UDecalComponent* const Decal = UGameplayStatics::SpawnDecalAtLocation(Avatar, WarningDecal, FVector(800.f, AreaRadius, AreaRadius),
			WarningCenter, FRotator(-90.f, 0.f, 0.f), 0.f);
		if (Decal)
		{
			Decal->SetFadeIn(0.f, WindupTime);
			Decal->SetFadeScreenSize(0.f);
			// 머티리얼에 Seed가 있으면 공격마다 무늬를 바꾼다.
			if (UMaterialInstanceDynamic* const Material = Decal->CreateDynamicMaterialInstance())
			{
				Material->SetScalarParameterValue(TEXT("Seed"), WarningSeed);
				static const FName BeatParameters[] = { TEXT("Beat0"), TEXT("Beat1"), TEXT("Beat2") };
				for (int32 Index = 0; Index < UE_ARRAY_COUNT(BeatParameters); ++Index)
				{
					Material->SetScalarParameterValue(BeatParameters[Index], WarningBeats.IsValidIndex(Index) ? WarningBeats[Index] : 2.f);
				}
			}
			WarningDecalComponent = Decal;
		}
	}
	if (WarningEffect)
	{
		for (const FVector& Point : ScatterPoints)
		{
			if (UNiagaraComponent* const Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, WarningEffect, Point,
				FRotator::ZeroRotator, FVector::OneVector, true, true, ENCPoolMethod::AutoRelease))
			{
				WarningEffectComponents.Add(Effect);
			}
		}
	}
	if (WarningSound)
	{
		WarningSoundComponent = UGameplayStatics::SpawnSoundAtLocation(Avatar, WarningSound, WarningCenter);
	}
	// 박자는 데칼 페이드인과 같은 월드 시간으로 잰다(머티리얼의 Beat 시점과 맞춘다).
	for (int32 Index = 0; Index < WarningBeats.Num(); ++Index)
	{
		const float Delay = WindupTime * WarningBeats[Index];
		if (Delay <= 0.f || Delay >= WindupTime)
		{
			continue;
		}
		FTimerHandle& Timer = WarningBeatTimers.AddDefaulted_GetRef();
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::PlayWarningBeat, Index), Delay, false);
	}
}

void UknightGameplayAbility_MonsterGroundEruption::PlayWarningBeat(int32 Index)
{
	UWorld* const World = GetWorld();
	if (!IsActive() || !World || bErupted)
	{
		return;
	}
	if (WarningBeatEffect)
	{
		for (const FVector& Point : ScatterPoints)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, WarningBeatEffect, Point, FRotator::ZeroRotator, FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
		}
	}
	if (WarningBeatSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, WarningBeatSound, WarningCenter, 1.f, 1.f + WarningBeatPitchStep * Index);
	}
}

void UknightGameplayAbility_MonsterGroundEruption::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopWarning();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FVector UknightGameplayAbility_MonsterGroundEruption::FindGround(const FVector& Location) const
{
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	const ACharacter* const Character = Cast<ACharacter>(Avatar);
	const float FootZ = Avatar ? Avatar->GetActorLocation().Z - (Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f) : Location.Z;
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(knightMonsterEruptionGround), false, Avatar);
	const FVector From(Location.X, Location.Y, FootZ + 1500.f);
	const FVector To(Location.X, Location.Y, FootZ - 1500.f);
	if (GetWorld() && GetWorld()->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return Hit.ImpactPoint;
	}
	return FVector(Location.X, Location.Y, FootZ);
}

void UknightGameplayAbility_MonsterGroundEruption::PickScatterPoints()
{
	ScatterPoints.Reset();
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !ScatterEffect)
	{
		return;
	}
	// 몸 둘레부터 범위 끝까지 고르게 흩는다. 방향은 등간격에 조금씩 흔든다.
	const FVector Center = Avatar->GetActorTransform().TransformPosition(AreaOffset);
	const float StartAngle = FMath::FRandRange(0.f, 2.f * PI);
	for (int32 Index = 0; Index < ScatterCount; ++Index)
	{
		const float Angle = StartAngle + 2.f * PI * Index / FMath::Max(ScatterCount, 1) + FMath::FRandRange(-0.2f, 0.2f);
		const float Radius = AreaRadius * FMath::Sqrt(FMath::FRandRange(0.15f, 0.9f));
		ScatterPoints.Add(FindGround(Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius));
	}
}

void UknightGameplayAbility_MonsterGroundEruption::StopWarning()
{
	if (UDecalComponent* const Decal = WarningDecalComponent.Get())
	{
		Decal->DestroyComponent();
	}
	WarningDecalComponent.Reset();
	for (const TWeakObjectPtr<UNiagaraComponent>& Effect : WarningEffectComponents)
	{
		if (Effect.IsValid())
		{
			Effect->Deactivate();
		}
	}
	WarningEffectComponents.Reset();
	if (UAudioComponent* const Sound = WarningSoundComponent.Get())
	{
		Sound->FadeOut(0.05f, 0.f);
	}
	WarningSoundComponent.Reset();
	if (UWorld* const World = GetWorld())
	{
		for (FTimerHandle& Timer : WarningBeatTimers)
		{
			World->GetTimerManager().ClearTimer(Timer);
		}
	}
	WarningBeatTimers.Reset();
}

void UknightGameplayAbility_MonsterGroundEruption::SpawnAfterglow()
{
	// 예고 데칼이 없던 공격(WindupTime 0, 예고 중 데칼 미생성)에는 식는 균열도 없다.
	if (!WarningDecal || AfterglowTime <= 0.f || !WarningDecalComponent.IsValid())
	{
		return;
	}
	UDecalComponent* const Decal = UGameplayStatics::SpawnDecalAtLocation(GetAvatarActorFromActorInfo(), WarningDecal,
		FVector(800.f, AreaRadius, AreaRadius), WarningCenter, FRotator(-90.f, 0.f, 0.f), 0.f);
	if (!Decal)
	{
		return;
	}
	Decal->SetFadeScreenSize(0.f);
	// 페이드아웃의 1 → 0을 머티리얼이 식는 정도로 읽는다. 데칼만 지우고 주인 액터는 남긴다.
	Decal->SetFadeOut(0.f, AfterglowTime, false);
	if (Decal->GetFadeDuration() <= 0.f)
	{
		// r.Decal.FadeDurationScale이 0이면 페이드가 꺼지므로 수명만 따로 건다.
		Decal->SetLifeSpan(AfterglowTime);
	}
	if (UMaterialInstanceDynamic* const Material = Decal->CreateDynamicMaterialInstance())
	{
		Material->SetScalarParameterValue(TEXT("Seed"), WarningSeed);
		Material->SetScalarParameterValue(TEXT("Cooling"), 1.f);
	}
}

void UknightGameplayAbility_MonsterGroundEruption::OnImpact()
{
	Super::OnImpact();

	// ActiveTime 동안 판정은 여러 번 오지만 불길은 첫 타격 한 번만 세운다.
	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	if (bErupted || !Avatar || !World)
	{
		return;
	}
	bErupted = true;
	SpawnAfterglow();
	StopWarning();
	const FVector Ground = FindGround(Avatar->GetActorTransform().TransformPosition(AreaOffset));
	if (EruptionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, EruptionSound, Ground);
	}
	for (UNiagaraSystem* const Effect : ImpactEffects)
	{
		if (Effect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Effect, Ground, FRotator::ZeroRotator, FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
		}
	}
	if (!ScatterEffect)
	{
		return;
	}
	if (ScatterPoints.IsEmpty())
	{
		PickScatterPoints();
	}
	for (const FVector& Point : ScatterPoints)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ScatterEffect, Point, FRotator::ZeroRotator, FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
	}
}
