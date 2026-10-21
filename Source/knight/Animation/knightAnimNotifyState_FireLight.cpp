#include "Animation/knightAnimNotifyState_FireLight.h"

#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"

UknightAnimNotifyState_FireLight::UknightAnimNotifyState_FireLight()
{
#if WITH_EDITORONLY_DATA
	bShouldFireInEditor = true;
#endif
}

void UknightAnimNotifyState_FireLight::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp || !MeshComp->GetWorld())
	{
		return;
	}
	// 앞 구간이 끊긴 채 다시 시작하면 남은 라이트부터 치운다.
	RemoveLight(MeshComp);

	UPointLightComponent* const Light = NewObject<UPointLightComponent>(MeshComp->GetOwner() ? static_cast<UObject*>(MeshComp->GetOwner()) : MeshComp);
	Light->SetMobility(EComponentMobility::Movable);
	Light->bUseInverseSquaredFalloff = true;
	Light->IntensityUnits = ELightUnits::Candelas;
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(AttenuationRadius);
	Light->SetCastShadows(bCastShadows);
	Light->SetIntensity(0.f);

	// 소켓 공간의 거리는 메시 스케일을 받으므로 월드 cm인 Distance를 스케일로 나눈다.
	const FName Socket = MeshComp->DoesSocketExist(SocketName) ? SocketName : NAME_None;
	const float Scale = FMath::Max(MeshComp->GetSocketTransform(Socket).GetScale3D().X, KINDA_SMALL_NUMBER);
	Light->SetupAttachment(MeshComp, Socket);
	Light->SetRelativeLocation(LocationOffset + RotationOffset.RotateVector(FVector(Distance / Scale, 0.f, 0.f)));
	Light->RegisterComponent();

	FActiveLight& Active = ActiveLights.Add(MeshComp);
	Active.Light = Light;
	Active.Duration = TotalDuration;
	UpdateLight(Active);
}

void UknightAnimNotifyState_FireLight::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	if (FActiveLight* const Active = ActiveLights.Find(MeshComp))
	{
		Active->Elapsed += FrameDeltaTime;
		UpdateLight(*Active);
	}
}

void UknightAnimNotifyState_FireLight::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	RemoveLight(MeshComp);
}

void UknightAnimNotifyState_FireLight::RemoveLight(USkeletalMeshComponent* MeshComp)
{
	FActiveLight Active;
	if (ActiveLights.RemoveAndCopyValue(MeshComp, Active) && Active.Light.IsValid())
	{
		Active.Light->DestroyComponent();
	}
	// 메시가 먼저 사라진 항목도 정리한다.
	for (auto It = ActiveLights.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void UknightAnimNotifyState_FireLight::UpdateLight(const FActiveLight& Active) const
{
	UPointLightComponent* const Light = Active.Light.Get();
	if (!Light)
	{
		return;
	}
	const float In = FadeIn > 0.f ? FMath::Clamp(Active.Elapsed / FadeIn, 0.f, 1.f) : 1.f;
	const float Out = FadeOut > 0.f ? FMath::Clamp((Active.Duration - Active.Elapsed) / FadeOut, 0.f, 1.f) : 1.f;
	const float Noise = 0.5f + 0.5f * FMath::PerlinNoise1D(Active.Elapsed * FlickerSpeed);
	Light->SetIntensity(Intensity * FMath::Min(In, Out) * (1.f - Flicker * Noise));
}

FString UknightAnimNotifyState_FireLight::GetNotifyName_Implementation() const
{
	return TEXT("Fire Light");
}
