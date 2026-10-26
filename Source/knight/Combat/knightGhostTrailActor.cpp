#include "Combat/knightGhostTrailActor.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/knightGameplayAbility_StanceSwitch.h"
#include "TimerManager.h"

#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

AknightGhostTrailActor::AknightGhostTrailActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetActorEnableCollision(false);
	bReplicates = false;

	USceneComponent* const Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

AknightGhostTrailActor* AknightGhostTrailActor::SpawnGhostFromCharacter(
	const ACharacter* Character,
	UMaterialInterface* GhostMaterial,
	const FLinearColor& Color,
	const float InLifetime,
	const float InInitialOpacity)
{
	if (!Character || !Character->GetWorld())
	{
		return nullptr;
	}

	UMaterialInterface* EffectiveMat = GhostMaterial;
	if (!EffectiveMat)
	{
		EffectiveMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Knight/Abilities/Cues/M_Knight_GhostTrail.M_Knight_GhostTrail"));
	}
	if (!EffectiveMat)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	AknightGhostTrailActor* const Ghost = Character->GetWorld()->SpawnActor<AknightGhostTrailActor>(
		AknightGhostTrailActor::StaticClass(), Character->GetActorTransform(), SpawnParams);
	if (Ghost)
	{
		Ghost->CaptureCharacterMeshes(*Character, EffectiveMat, Color, InLifetime, InInitialOpacity);
	}
	return Ghost;
}

void AknightGhostTrailActor::StartGhostTrailForAbility(UGameplayAbility* Ability, const FknightGhostTrailSettings& Settings, float Duration,
	const FVector& StopLocation, TFunction<bool()> ShouldStop)
{
	UWorld* const World = Ability ? Ability->GetWorld() : nullptr;
	const AActor* const Avatar = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
	// 처음부터 기본 공격이 닿는 거리면(몬스터에 붙어 있으면) 잔상을 남기지 않는다.
	if (!World || !Avatar || Duration <= 0.f || FVector::Dist2D(Avatar->GetActorLocation(), StopLocation) <= Settings.InRangeDistance)
	{
		return;
	}
	struct FTrail
	{
		TWeakObjectPtr<UGameplayAbility> WeakAbility;
		FknightGhostTrailSettings Settings;
		FVector StopLocation = FVector::ZeroVector;
		TFunction<bool()> ShouldStop;
		double EndTime = 0.0;
		FTimerHandle Timer;
	};
	const TSharedRef<FTrail> Trail = MakeShared<FTrail>();
	Trail->WeakAbility = Ability;
	Trail->Settings = Settings;
	Trail->StopLocation = StopLocation;
	Trail->ShouldStop = MoveTemp(ShouldStop);
	Trail->EndTime = World->GetTimeSeconds() + Duration;
	const auto SpawnStep = [Trail]()
	{
		UGameplayAbility* const A = Trail->WeakAbility.Get();
		const ACharacter* const Character = A ? Cast<ACharacter>(A->GetAvatarActorFromActorInfo()) : nullptr;
		UWorld* const W = Character ? Character->GetWorld() : nullptr;
		if (!W || !A->IsActive() || W->GetTimeSeconds() > Trail->EndTime
			|| FVector::Dist2D(Character->GetActorLocation(), Trail->StopLocation) <= Trail->Settings.InRangeDistance
			|| (Trail->ShouldStop && Trail->ShouldStop()))
		{
			if (W)
			{
				W->GetTimerManager().ClearTimer(Trail->Timer);
			}
			return;
		}
		const bool bTwoHanded = UknightGameplayAbility_StanceSwitch::IsTwoHanded(A->GetAbilitySystemComponentFromActorInfo());
		SpawnGhostFromCharacter(Character, Trail->Settings.Material, bTwoHanded ? Trail->Settings.TwoHandedColor : Trail->Settings.OneHandedColor,
			Trail->Settings.Lifetime, Trail->Settings.InitialOpacity);
	};
	// 락온 돌진처럼 출발할 때 하나를 바로 남기고, 그 뒤로 간격마다 남긴다.
	SpawnStep();
	World->GetTimerManager().SetTimer(Trail->Timer, FTimerDelegate::CreateLambda(SpawnStep), Settings.SpawnInterval, true);
}

void AknightGhostTrailActor::CaptureCharacterMeshes(
	const ACharacter& Character,
	UMaterialInterface* BaseMaterial,
	const FLinearColor& Color,
	const float InLifetime,
	const float InInitialOpacity)
{
	Lifetime = FMath::Max(InLifetime, 0.05f);
	InitialOpacity = FMath::Clamp(InInitialOpacity, 0.05f, 1.f);
	ElapsedTime = 0.f;

	GhostMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (!GhostMID)
	{
		Destroy();
		return;
	}
	GhostMID->SetVectorParameterValue(TEXT("GhostColor"), Color);
	GhostMID->SetScalarParameterValue(TEXT("GhostOpacity"), InitialOpacity);

	TArray<UMeshComponent*> MeshComponents;
	Character.GetComponents<UMeshComponent>(MeshComponents);

	int32 CopiedCount = 0;
	for (UMeshComponent* const MeshComp : MeshComponents)
	{
		if (!MeshComp || !MeshComp->IsVisible() || MeshComp->bHiddenInGame)
		{
			continue;
		}

		if (USkeletalMeshComponent* const SkelComp = Cast<USkeletalMeshComponent>(MeshComp))
		{
			USkeletalMesh* const SkelAsset = SkelComp->GetSkeletalMeshAsset();
			if (!SkelAsset)
			{
				continue;
			}
			UPoseableMeshComponent* const PoseComp = NewObject<UPoseableMeshComponent>(this);
			PoseComp->SetupAttachment(GetRootComponent());
			PoseComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PoseComp->SetCastShadow(false);
			PoseComp->bReceivesDecals = false;
			PoseComp->SetSkinnedAssetAndUpdate(SkelAsset);
			PoseComp->RegisterComponent();
			PoseComp->SetWorldTransform(SkelComp->GetComponentTransform());
			// (2026-10-26) 본체 포즈를 따라가는 메시(LeaderPose, 예: 흑관 몸통)는 자기 포즈를 계산하지 않아 기본 자세만 남아 있다.
			// 그 메시는 본체(리더)의 포즈를 뼈 이름으로 맞춰 복사한다. 흑관을 입으면 캐릭터 대신 망토 모양 잔상만 남던 문제.
			USkeletalMeshComponent* PoseSource = SkelComp;
			if (USkeletalMeshComponent* const Leader = Cast<USkeletalMeshComponent>(SkelComp->LeaderPoseComponent.Get()))
			{
				PoseSource = Leader;
			}
			PoseComp->CopyPoseFromSkeletalComponent(PoseSource);

			const int32 NumMats = PoseComp->GetNumMaterials();
			for (int32 MatIdx = 0; MatIdx < NumMats; ++MatIdx)
			{
				PoseComp->SetMaterial(MatIdx, GhostMID);
			}
			++CopiedCount;
		}
		else if (UStaticMeshComponent* const StatComp = Cast<UStaticMeshComponent>(MeshComp))
		{
			UStaticMesh* const StatAsset = StatComp->GetStaticMesh();
			if (!StatAsset)
			{
				continue;
			}
			UStaticMeshComponent* const GhostStatComp = NewObject<UStaticMeshComponent>(this);
			GhostStatComp->SetupAttachment(GetRootComponent());
			GhostStatComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GhostStatComp->SetCastShadow(false);
			GhostStatComp->bReceivesDecals = false;
			GhostStatComp->SetStaticMesh(StatAsset);
			GhostStatComp->RegisterComponent();
			GhostStatComp->SetWorldTransform(StatComp->GetComponentTransform());

			const int32 NumMats = GhostStatComp->GetNumMaterials();
			for (int32 MatIdx = 0; MatIdx < NumMats; ++MatIdx)
			{
				GhostStatComp->SetMaterial(MatIdx, GhostMID);
			}
			++CopiedCount;
		}
	}

	if (CopiedCount == 0)
	{
		Destroy();
	}
}

void AknightGhostTrailActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedTime += DeltaSeconds;
	if (ElapsedTime >= Lifetime || !GhostMID)
	{
		Destroy();
		return;
	}

	const float RemainingRatio = FMath::Clamp(1.f - (ElapsedTime / Lifetime), 0.f, 1.f);
	GhostMID->SetScalarParameterValue(TEXT("GhostOpacity"), InitialOpacity * RemainingRatio * RemainingRatio);
}
