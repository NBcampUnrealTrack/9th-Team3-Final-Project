#include "Animation/knightAnimNotifyState_MeleeTrace.h"

#include "Combat/knightMeleeTraceComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UknightAnimNotifyState_MeleeTrace::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	// Editor preview actors have no trace component, so this is a no-op there.
	if (UknightMeleeTraceComponent* const Trace = (MeshComp && MeshComp->GetOwner()) ? MeshComp->GetOwner()->FindComponentByClass<UknightMeleeTraceComponent>() : nullptr)
	{
		Trace->BeginTrace(TraceParams);
	}
}

void UknightAnimNotifyState_MeleeTrace::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (UknightMeleeTraceComponent* const Trace = (MeshComp && MeshComp->GetOwner()) ? MeshComp->GetOwner()->FindComponentByClass<UknightMeleeTraceComponent>() : nullptr)
	{
		Trace->EndTrace();
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UknightAnimNotifyState_MeleeTrace::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Melee Trace (%s-%s)"), *TraceParams.StartSocket.ToString(), *TraceParams.EndSocket.ToString());
}
