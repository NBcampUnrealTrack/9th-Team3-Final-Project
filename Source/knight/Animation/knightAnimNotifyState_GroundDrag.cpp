#include "Animation/knightAnimNotifyState_GroundDrag.h"

#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

void UknightAnimNotifyState_GroundDrag::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp)
	{
		return;
	}
	// 앞 구간이 끊긴 채 다시 시작하면 이전 발 자취와 잇지 않는다.
	RemoveTracks(MeshComp);
	TArray<FFootTrack>& Feet = Tracks.Add(MeshComp);
	Feet.SetNum(GetTrackCount());
	for (int32 Index = 0; Index < Feet.Num(); ++Index)
	{
		UpdateTrack(MeshComp, Index, Feet[Index]);
	}
}

void UknightAnimNotifyState_GroundDrag::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	if (TArray<FFootTrack>* const Feet = Tracks.Find(MeshComp))
	{
		for (int32 Index = 0; Index < Feet->Num(); ++Index)
		{
			UpdateTrack(MeshComp, Index, (*Feet)[Index]);
		}
	}
}

void UknightAnimNotifyState_GroundDrag::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	RemoveTracks(MeshComp);
}

int32 UknightAnimNotifyState_GroundDrag::GetTrackCount() const
{
	return ActorTrailOffsets.Num() > 0 ? ActorTrailOffsets.Num() : FootSockets.Num();
}

void UknightAnimNotifyState_GroundDrag::UpdateTrack(USkeletalMeshComponent* MeshComp, const int32 Index, FFootTrack& Track) const
{
	UWorld* const World = MeshComp ? MeshComp->GetWorld() : nullptr;
	AActor* const Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!World || !Owner)
	{
		return;
	}
	// 찾기 시작점과 아래로 찾는 거리. 그 안에 바닥이 없으면 떠 있는 것이라 자취를 끊는다.
	FVector Start;
	float Reach;
	if (ActorTrailOffsets.Num() > 0)
	{
		// 몸 아래: 액터 위치(캡슐 가운데)에서 옆으로 띄우고 캡슐 바닥에서 ContactHeight 아래까지.
		const ACharacter* const Character = Cast<ACharacter>(Owner);
		const float HalfHeight = Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
		Start = Owner->GetActorLocation() + Owner->GetActorRightVector() * ActorTrailOffsets[Index];
		Reach = HalfHeight + ContactHeight;
	}
	else if (FootSockets.IsValidIndex(Index) && MeshComp->DoesSocketExist(FootSockets[Index]))
	{
		// 발: 조금 위에서 ContactHeight 아래까지.
		Start = MeshComp->GetSocketLocation(FootSockets[Index]) + FVector(0.f, 0.f, 50.f);
		Reach = 50.f + ContactHeight;
	}
	else
	{
		return;
	}
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(knightGroundDrag), false, Owner);
	if (!World->LineTraceSingleByObjectType(Hit, Start, Start - FVector(0.f, 0.f, Reach), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Track.bHasLast = false;
		return;
	}
	const FVector Point = Hit.ImpactPoint;
	if (!Track.bHasLast)
	{
		Track.bHasLast = true;
		Track.Last = Point;
		Track.DustDistance = 0.f;
		return;
	}
	const float Distance = FVector::Dist2D(Track.Last, Point);
	if (Distance > MaxSegmentLength)
	{
		Track.Last = Point;
		return;
	}
	if (Distance < MarkSpacing)
	{
		return;
	}
	SpawnMark(World, Track.Last, Point);
	Track.DustDistance += Distance;
	if (DustEffect && Track.DustDistance >= DustSpacing)
	{
		Track.DustDistance = 0.f;
		// 먼지는 끌려온 반대쪽(뒤)을 보게 뿌린다.
		const FRotator Back(0.f, (Track.Last - Point).Rotation().Yaw, 0.f);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, DustEffect, Point, Back, FVector(DustScale), true, true, ENCPoolMethod::AutoRelease);
	}
	Track.Last = Point;
}

void UknightAnimNotifyState_GroundDrag::SpawnMark(UWorld* World, const FVector& From, const FVector& To) const
{
	if (!MarkMaterial)
	{
		return;
	}
	// 데칼은 +X로 비춘다. 아래(Pitch -90)를 보게 돌리면 +Z가 Yaw 방향(길이), +Y가 옆(너비)이 된다.
	const FVector Delta = To - From;
	const FRotator Rotation(-90.f, Delta.Rotation().Yaw, 0.f);
	const FVector Size(MarkDepth, MarkWidth * 0.5f, Delta.Size2D() * 0.5f + MarkOverlap);
	if (UDecalComponent* const Decal = UGameplayStatics::SpawnDecalAtLocation(World, MarkMaterial, Size, (From + To) * 0.5f, Rotation,
		MarkLifeTime + MarkFadeTime))
	{
		// 카메라가 멀어져 화면에서 작아져도 숨기지 않는다(기본값이면 작은 조각이 먼저 사라진다).
		Decal->SetFadeScreenSize(0.f);
		Decal->SetFadeOut(MarkLifeTime, MarkFadeTime, false);
	}
}

void UknightAnimNotifyState_GroundDrag::RemoveTracks(USkeletalMeshComponent* MeshComp)
{
	Tracks.Remove(MeshComp);
	// 메시가 먼저 사라진 항목도 정리한다.
	for (auto It = Tracks.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

FString UknightAnimNotifyState_GroundDrag::GetNotifyName_Implementation() const
{
	return TEXT("Ground Drag");
}
