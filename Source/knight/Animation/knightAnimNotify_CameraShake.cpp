#include "Animation/knightAnimNotify_CameraShake.h"

#include "Camera/CameraShakeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UknightAnimNotify_CameraShake::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	UWorld* const World = MeshComp ? MeshComp->GetWorld() : nullptr;
	if (!Shake || !World || !World->IsGameWorld())
	{
		return;
	}
	const FVector Epicenter = !SocketName.IsNone() && MeshComp->DoesSocketExist(SocketName) ? MeshComp->GetSocketLocation(SocketName) : MeshComp->GetComponentLocation();
	// 플레이어 카메라 매니저의 StartCameraShake를 거치므로 화면 흔들림 설정이 적용된다.
	UGameplayStatics::PlayWorldCameraShake(World, Shake, Epicenter, InnerRadius, OuterRadius);
}

FString UknightAnimNotify_CameraShake::GetNotifyName_Implementation() const
{
	return TEXT("Camera Shake");
}
