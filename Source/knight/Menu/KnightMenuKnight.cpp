#include "Menu/KnightMenuKnight.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Item/knightPartsComponent.h"
#include "Progression/knightProgressionSubsystem.h"

AKnightMenuKnight::AKnightMenuKnight(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	Parts = CreateDefaultSubobject<UknightPartsComponent>(TEXT("Parts"));

	// 메뉴에 서 있기만 한다: 충돌·이동 없음.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->GravityScale = 0.f;
	// BP_Knight와 같은 메시 위치(캡슐 바닥에 발, 앞이 +X).
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -98.3f), FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AKnightMenuKnight::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->DisableMovement();
	if (IdleAnimation)
	{
		const bool bFrozen = FrozenPoseTime >= 0.f;
		GetMesh()->PlayAnimation(IdleAnimation, !bFrozen);
		if (bFrozen)
		{
			// 그 시각의 자세에서 멈춘다(반복 재생하지 않음).
			GetMesh()->SetPosition(FMath::Min(FrozenPoseTime, IdleAnimation->GetPlayLength()), false);
			GetMesh()->Stop();
		}
	}
	Parts->ApplyLoadout();
	AttachWeaponToMenuSocket();
	if (UknightProgressionSubsystem* const Progression = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>() : nullptr)
	{
		Progression->OnProgressionChanged.AddDynamic(this, &ThisClass::HandleProgressionChanged);
	}
}

void AKnightMenuKnight::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UknightProgressionSubsystem* const Progression = GetGameInstance() ? GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>() : nullptr)
	{
		Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void AKnightMenuKnight::HandleProgressionChanged()
{
	// 재화·소재 변경에도 불리지만, 장착이 그대로면 아무것도 하지 않는다.
	if (Parts->SyncLoadoutWithSave())
	{
		AttachWeaponToMenuSocket();
	}
}

void AKnightMenuKnight::AttachWeaponToMenuSocket()
{
	USkeletalMeshComponent* const Body = GetMesh();
	if (WeaponSocket.IsNone() || !Body || !Body->DoesSocketExist(WeaponSocket))
	{
		return;
	}
	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* const PartMesh : Meshes)
	{
		if (PartMesh != Body && PartMesh->ComponentHasTag(WeaponComponentTag))
		{
			// 게임 안에서 검을 넣을 때(UknightGameplayAbility_CombatStance)와 같은 방식으로 등 소켓에 붙인다.
			UknightPartsComponent::AttachWeaponToSocket(PartMesh, Body, WeaponSocket);
			return;
		}
	}
}
