#include "AbilitySystem/Abilities/knightGameplayAbility_MonsterGroundEruption.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

void UknightGameplayAbility_MonsterGroundEruption::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	if (!IsActive() || !Avatar || !WarningDecal || WindupTime <= 0.f)
	{
		return;
	}
	// 바닥 원은 위에서 아래로 투영한다. 몸 높이와 경사를 덮도록 깊게 둔다.
	UGameplayStatics::SpawnDecalAtLocation(Avatar, WarningDecal, FVector(800.f, AreaRadius, AreaRadius),
		FindGround(Avatar->GetActorTransform().TransformPosition(AreaOffset)), FRotator(-90.f, 0.f, 0.f), WindupTime);
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

void UknightGameplayAbility_MonsterGroundEruption::OnImpact()
{
	Super::OnImpact();

	const AActor* const Avatar = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	if (!Avatar || !World)
	{
		return;
	}
	const FVector Center = Avatar->GetActorTransform().TransformPosition(AreaOffset);
	const FVector Ground = FindGround(Center);
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
	// 몸 둘레부터 범위 끝까지 고르게 흩어 불길을 세운다. 방향은 등간격에 조금씩 흔든다.
	const float StartAngle = FMath::FRandRange(0.f, 2.f * PI);
	for (int32 Index = 0; Index < ScatterCount; ++Index)
	{
		const float Angle = StartAngle + 2.f * PI * Index / FMath::Max(ScatterCount, 1) + FMath::FRandRange(-0.2f, 0.2f);
		const float Radius = AreaRadius * FMath::Sqrt(FMath::FRandRange(0.15f, 0.9f));
		const FVector Point = FindGround(Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ScatterEffect, Point, FRotator::ZeroRotator, FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
	}
}
