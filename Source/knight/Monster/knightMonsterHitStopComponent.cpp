#include "Monster/knightMonsterHitStopComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

namespace knightMonsterHitStop
{
	// 몬스터 몸은 히트스톱으로 거의 멈추므로 떨림은 월드 시간 타이머로 움직인다.
	constexpr float ShakeInterval = 1.f / 60.f;
}

void UknightMonsterHitStopComponent::ApplyHitStop(float Duration)
{
	const bool bHit = Duration > 0.f && IsHitThisFrame();
	const float Scaled = bHit ? Duration * TargetDurationScale : Duration;
	Super::ApplyHitStop(Scaled);
	if (bHit && IsInHitStop())
	{
		StartShake(Scaled);
	}
}

bool UknightMonsterHitStopComponent::IsHitThisFrame() const
{
	const UWorld* const World = GetWorld();
	const UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	const UknightCoreAttributeSet* const CoreSet = ASC ? ASC->GetSet<UknightCoreAttributeSet>() : nullptr;
	return World && CoreSet && CoreSet->GetLastHitTime() == static_cast<float>(World->GetTimeSeconds());
}

void UknightMonsterHitStopComponent::StartShake(float Duration)
{
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	UWorld* const World = GetWorld();
	if (!Character || !Character->GetMesh() || !World || ShakeAmplitudeRatio <= 0.f)
	{
		return;
	}
	const float Radius = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 100.f;
	ShakeAmplitude = FMath::Min(Radius * ShakeAmplitudeRatio, ShakeMaxAmplitude);
	ShakeStartTime = World->GetTimeSeconds();
	// 이어서 맞으면 새로 시작한다. 이미 떨고 있던 오프셋은 UpdateShake가 이어서 덮는다.
	ShakeEndTime = ShakeStartTime + Duration;
	if (!World->GetTimerManager().IsTimerActive(ShakeTimer))
	{
		World->GetTimerManager().SetTimer(ShakeTimer, this, &ThisClass::UpdateShake, knightMonsterHitStop::ShakeInterval, true);
	}
	UpdateShake();
}

void UknightMonsterHitStopComponent::UpdateShake()
{
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	const UWorld* const World = GetWorld();
	if (!Mesh || !World || !IsInHitStop() || World->GetTimeSeconds() >= ShakeEndTime)
	{
		StopShake();
		return;
	}
	const float Elapsed = static_cast<float>(World->GetTimeSeconds() - ShakeStartTime);
	const float Fade = 1.f - FMath::Clamp(Elapsed / FMath::Max(static_cast<float>(ShakeEndTime - ShakeStartTime), UE_KINDA_SMALL_NUMBER), 0.f, 1.f);
	// 메시의 부모(캡슐) 공간 Y = 몸의 좌우.
	const FVector Offset(0.f, ShakeAmplitude * Fade * FMath::Sin(UE_TWO_PI * ShakeFrequency * Elapsed), 0.f);
	Mesh->AddRelativeLocation(Offset - AppliedOffset);
	AppliedOffset = Offset;
}

void UknightMonsterHitStopComponent::StopShake()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShakeTimer);
	}
	const ACharacter* const Character = Cast<ACharacter>(GetOwner());
	if (USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr; Mesh && !AppliedOffset.IsZero())
	{
		Mesh->AddRelativeLocation(-AppliedOffset);
	}
	AppliedOffset = FVector::ZeroVector;
}

void UknightMonsterHitStopComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopShake();
	Super::EndPlay(EndPlayReason);
}
