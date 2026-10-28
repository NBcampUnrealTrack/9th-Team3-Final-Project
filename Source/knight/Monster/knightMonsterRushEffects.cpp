#include "Monster/knightMonsterRushEffects.h"

#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

void FknightMonsterRushEffects::Start(ACharacter& Character)
{
	if (bActive)
	{
		return;
	}
	bActive = true;
	DustTimer = 0.f;
	if (WindEffect)
	{
		Wind = UNiagaraFunctionLibrary::SpawnSystemAttached(WindEffect, Character.GetRootComponent(), NAME_None, WindOffset, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true, true, ENCPoolMethod::AutoRelease);
	}
	if (LoopSound)
	{
		LoopAudio = UGameplayStatics::SpawnSoundAttached(LoopSound, Character.GetRootComponent());
	}
}

void FknightMonsterRushEffects::Tick(ACharacter& Character, float DeltaTime, bool bGrounded)
{
	if (!bActive || !TrailDust)
	{
		return;
	}
	DustTimer -= DeltaTime;
	if (!bGrounded || DustTimer > 0.f)
	{
		return;
	}
	DustTimer = TrailDustInterval;
	const float HalfHeight = Character.GetCapsuleComponent() ? Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	const FVector Location = Character.GetActorLocation() + Character.GetActorQuat().RotateVector(TrailDustOffset) - FVector(0.f, 0.f, HalfHeight);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(&Character, TrailDust, Location, Character.GetActorRotation(), FVector::OneVector, true, true,
		ENCPoolMethod::AutoRelease);
}

void FknightMonsterRushEffects::Stop()
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	if (UNiagaraComponent* const Component = Wind.Get())
	{
		Component->Deactivate();
	}
	Wind.Reset();
	if (UAudioComponent* const Audio = LoopAudio.Get())
	{
		Audio->FadeOut(LoopSoundFadeOut, 0.f);
	}
	LoopAudio.Reset();
}
