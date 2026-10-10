#include "Animation/knightAnimNotifyState_RoarShockwave.h"

#include "Camera/knightPlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	template <typename FunctionType>
	void ForEachLocalCamera(const USkeletalMeshComponent* MeshComp, FunctionType&& Function)
	{
		UWorld* const World = MeshComp ? MeshComp->GetWorld() : nullptr;
		if (!World || !World->IsGameWorld())
		{
			return;
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* const PC = It->Get();
			if (AknightPlayerCameraManager* const Camera = PC && PC->IsLocalController() ? Cast<AknightPlayerCameraManager>(PC->PlayerCameraManager) : nullptr)
			{
				Function(*Camera);
			}
		}
	}
}

void UknightAnimNotifyState_RoarShockwave::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	ForEachLocalCamera(MeshComp, [MeshComp, this](AknightPlayerCameraManager& Camera) { Camera.StartRoarShockwave(MeshComp, Socket); });
}

void UknightAnimNotifyState_RoarShockwave::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	ForEachLocalCamera(MeshComp, [MeshComp](AknightPlayerCameraManager& Camera) { Camera.StopRoarShockwave(MeshComp); });
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UknightAnimNotifyState_RoarShockwave::GetNotifyName_Implementation() const
{
	return TEXT("Roar Shockwave");
}
