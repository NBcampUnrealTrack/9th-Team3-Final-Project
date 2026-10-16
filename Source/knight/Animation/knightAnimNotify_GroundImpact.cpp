#include "Animation/knightAnimNotify_GroundImpact.h"

#include "Camera/knightPlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Environment/knightWaterRippleSettings.h"
#include "Environment/knightWaterRippleSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraFunctionLibrary.h"

void UknightAnimNotify_GroundImpact::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	UWorld* const World = MeshComp ? MeshComp->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const FVector Source = !Bone.IsNone() && MeshComp->DoesSocketExist(Bone) ? MeshComp->GetSocketLocation(Bone) : MeshComp->GetComponentLocation();
	FVector Epicenter = Source;

	// 발은 바닥에 거의 닿아 있고 가슴은 높이 떠 있으니, 바로 아래 지형을 찾아 그 위에 먼지를 놓는다.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(knightGroundImpact), false, MeshComp->GetOwner());
	const bool bGround = World->LineTraceSingleByObjectType(Hit, Source + FVector(0.f, 0.f, 300.f), Source - FVector(0.f, 0.f, MaxGroundDistance),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params);

	// 얕은 물 아래 바닥을 찍었으면 수면에서 터뜨리고 물결을 낸다.
	float SurfaceZ = 0.f;
	if (UknightWaterRippleSubsystem::FindWaterSurface(World, Source, SurfaceZ) && Source.Z - SurfaceZ <= MaxGroundDistance
		&& (!bGround || Hit.ImpactPoint.Z <= SurfaceZ + 1.f))
	{
		Epicenter = FVector(Source.X, Source.Y, SurfaceZ);
		if (WaterSplash)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, WaterSplash, Epicenter, FRotator::ZeroRotator, FVector(WaterSplashScale), true, true, ENCPoolMethod::AutoRelease);
		}
		else if (UNiagaraSystem* const Heavy = GetDefault<UknightWaterRippleSettings>()->SplashHeavySystem.LoadSynchronous())
		{
			// 발 첨벙과 같은 이펙트를 찍은 부위 크기에 맞춰, 가장 세게 부딪힌 배율에 WaterSplashScale을 곱해 쓴다.
			// 내려찍기는 큰 동작이라 움직임 첨벙을 줄이는 SplashScale은 곱하지 않는다.
			// 부위에 바디가 없으면 캡슐 반지름의 1/3 정도를 부위 크기로 본다.
			const UknightWaterRippleSettings* const Settings = GetDefault<UknightWaterRippleSettings>();
			float Size = UknightWaterRippleSubsystem::GetBodyHalfWidth(MeshComp, Bone);
			if (Size <= 0.f)
			{
				const ACharacter* const Character = Cast<ACharacter>(MeshComp->GetOwner());
				Size = Character ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.35f : Settings->SplashReferenceBodySize;
			}
			const float Scale = FMath::Min(Size / Settings->SplashReferenceBodySize * Settings->SplashSpeedScaleRange.Y, Settings->SplashMaxScale) * WaterSplashScale;
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Heavy, Epicenter, FRotator::ZeroRotator, FVector(Scale), true, true, ENCPoolMethod::AutoRelease);
		}
		else if (Dust)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Dust, Epicenter, FRotator::ZeroRotator, FVector(DustScale), true, true, ENCPoolMethod::AutoRelease);
		}
		if (UknightWaterRippleSubsystem* const Ripples = World->GetSubsystem<UknightWaterRippleSubsystem>())
		{
			Ripples->AddImpulse(Epicenter, RippleRadius, RippleDepth);
		}
	}
	else if (bGround)
	{
		Epicenter = Hit.ImpactPoint;
		if (Dust)
		{
			// 내려찍기마다 컴포넌트를 새로 만들지 않게 풀에서 꺼내 쓴다.
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Dust, Epicenter, FRotator::ZeroRotator, FVector(DustScale), true, true, ENCPoolMethod::AutoRelease);
		}
	}

	if (!Shake || ShakeScale <= 0.f || !World->IsGameWorld())
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* const PC = It->Get();
		AknightPlayerCameraManager* const Camera = PC && PC->IsLocalController() ? Cast<AknightPlayerCameraManager>(PC->PlayerCameraManager) : nullptr;
		if (!Camera)
		{
			continue;
		}
		const float Distance = FVector::Dist(Camera->GetCameraLocation(), Epicenter);
		const float Falloff = Distance <= InnerRadius ? 1.f : 1.f - FMath::Clamp((Distance - InnerRadius) / FMath::Max(OuterRadius - InnerRadius, 1.f), 0.f, 1.f);
		if (Falloff > 0.f)
		{
			Camera->StartCameraShake(Shake, ShakeScale * Falloff);
		}
	}
}

FString UknightAnimNotify_GroundImpact::GetNotifyName_Implementation() const
{
	return TEXT("Ground Impact");
}
