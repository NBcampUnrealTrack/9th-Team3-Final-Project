#include "Monster/knightMonsterPartShellComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDefinition.h"
#include "Monster/knightMonsterPartData.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "Monster/knightPartScanSubsystem.h"

namespace
{
	/** 머티리얼이 받는 부위 수. 셸 굽기도 이만큼만 넣는다. */
	constexpr int32 MaxShellParts = 8;

	const FName LineWidthParam(TEXT("LineWidth"));
	const FName FillOpacityParam(TEXT("FillOpacity"));
	const FName SurfaceOffsetParam(TEXT("SurfaceOffset"));

	FName GetColorParam(int32 Index)
	{
		static const FName Names[MaxShellParts] = {TEXT("Color0"), TEXT("Color1"), TEXT("Color2"), TEXT("Color3"),
			TEXT("Color4"), TEXT("Color5"), TEXT("Color6"), TEXT("Color7")};
		return Names[Index];
	}

	FName GetOpacityParam(int32 Index)
	{
		static const FName Names[MaxShellParts] = {TEXT("Opacity0"), TEXT("Opacity1"), TEXT("Opacity2"), TEXT("Opacity3"),
			TEXT("Opacity4"), TEXT("Opacity5"), TEXT("Opacity6"), TEXT("Opacity7")};
		return Names[Index];
	}
}

UknightMonsterPartShellComponent::UknightMonsterPartShellComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	ShellMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Knight/Creatures/Shared/PartShell/M_PartShell.M_PartShell")));
	ShellXRayMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Knight/Creatures/Shared/PartShell/M_PartShell_XRay.M_PartShell_XRay")));
	ScanMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Knight/Creatures/Shared/PartShell/M_PartScan.M_PartScan")));
}

void UknightMonsterPartShellComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UknightMonsterTargetingComponent* const Targeting = GetOwner()->FindComponentByClass<UknightMonsterTargetingComponent>())
	{
		Targeting->OnTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleTargetChanged);
	}
	if (UknightPartScanSubsystem* const Scan = GetWorld()->GetSubsystem<UknightPartScanSubsystem>())
	{
		Scan->RegisterShell(this);
	}
}

void UknightMonsterPartShellComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UknightMonsterTargetingComponent* const Targeting = GetOwner()->FindComponentByClass<UknightMonsterTargetingComponent>())
	{
		Targeting->OnTargetChanged.RemoveDynamic(this, &ThisClass::HandleTargetChanged);
	}
	if (UknightMonsterPartComponent* const PartComponent = GetOwner()->FindComponentByClass<UknightMonsterPartComponent>())
	{
		PartComponent->OnPartConditionChanged.RemoveDynamic(this, &ThisClass::HandlePartConditionChanged);
	}
	if (UWorld* const World = GetWorld())
	{
		if (UknightPartScanSubsystem* const Scan = World->GetSubsystem<UknightPartScanSubsystem>())
		{
			Scan->UnregisterShell(this);
		}
	}
	if (ShellMesh)
	{
		ShellMesh->DestroyComponent();
		ShellMesh = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UknightMonsterPartShellComponent::HandleTargetChanged(AActor* OldTarget, AActor* NewTarget)
{
	// 첫 조우 한 번만. 플레이어를 잡았을 때 그 자리에서 쏜다.
	const APawn* const Pawn = Cast<APawn>(NewTarget);
	if (!bScanOnEngage || bScanned || !Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}
	bScanned = true;
	StartScanFrom(Pawn);
}

void UknightMonsterPartShellComponent::StartScanFrom(const AActor* Origin)
{
	UknightPartScanSubsystem* const Scan = GetWorld() ? GetWorld()->GetSubsystem<UknightPartScanSubsystem>() : nullptr;
	if (Scan && Origin)
	{
		Scan->StartScan(Origin->GetActorLocation(), ScanMaterial.LoadSynchronous(), ScanSpeed, ScanMaxRadius);
	}
}

void UknightMonsterPartShellComponent::SetLockedPart(FGameplayTag PartTag)
{
	if (LockedPart == PartTag)
	{
		return;
	}
	const bool bHasShell = PartTag.IsValid() && EnsureBuilt()
		&& Parts.ContainsByPredicate([&PartTag](const FShellPart& Each) { return Each.Tag == PartTag; });
	// 락온을 옮기거나 풀면 이전 부위는 바로 옅어지기 시작한다.
	for (FShellPart& Part : Parts)
	{
		if (Part.Tag == LockedPart)
		{
			Part.ShowRemaining = FMath::Min(Part.ShowRemaining, FadeOutTime);
		}
	}
	LockedPart = bHasShell ? PartTag : FGameplayTag();
	ShowPart(LockedPart, LockShowTime);
}

void UknightMonsterPartShellComponent::ShowPart(FGameplayTag Tag, float Duration)
{
	if (!Tag.IsValid() || Duration <= 0.f)
	{
		return;
	}
	for (FShellPart& Part : Parts)
	{
		if (Part.Tag == Tag)
		{
			Part.ShowRemaining = FMath::Max(Part.ShowRemaining, Duration);
			SetComponentTickEnabled(true);
		}
	}
}

void UknightMonsterPartShellComponent::DebugLockShellPart(int32 Index)
{
	EnsureBuilt();
	SetLockedPart(Parts.IsValidIndex(Index) ? Parts[Index].Tag : FGameplayTag());
}

void UknightMonsterPartShellComponent::DebugSetMaterialScalar(FName Name, float Value)
{
	EnsureBuilt();
	for (UMaterialInstanceDynamic* const MID : {Material.Get(), XRayMaterial.Get()})
	{
		if (MID)
		{
			MID->SetScalarParameterValue(Name, Value);
		}
	}
}

void UknightMonsterPartShellComponent::PlayScanReveal()
{
	if (!EnsureBuilt())
	{
		return;
	}
	RevealRemaining = RevealTime;
	SetXRay(true);
	SetComponentTickEnabled(true);
}

bool UknightMonsterPartShellComponent::EnsureBuilt()
{
	if (bBuilt)
	{
		return ShellMesh != nullptr;
	}
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetOwner());
	const UknightMonsterDefinition* const Definition = Monster ? Monster->GetMonsterDefinition() : nullptr;
	const UknightMonsterPartData* const PartData = Definition ? Definition->PartData.Get() : nullptr;
	USkeletalMeshComponent* const LeaderMesh = Monster ? Monster->GetMesh() : nullptr;
	bBuilt = true;
	USkeletalMesh* const Asset = PartData ? PartData->PartShellMesh.LoadSynchronous() : nullptr;
	if (!Asset || !LeaderMesh || PartData->PartShellParts.IsEmpty())
	{
		return false;
	}

	UknightMonsterPartComponent* const PartComponent = GetOwner()->FindComponentByClass<UknightMonsterPartComponent>();
	for (int32 Index = 0; Index < FMath::Min(PartData->PartShellParts.Num(), MaxShellParts); ++Index)
	{
		FShellPart& Part = Parts.AddDefaulted_GetRef();
		Part.Tag = PartData->PartShellParts[Index];
		Part.Condition = PartComponent ? PartComponent->GetPartCondition(Part.Tag) : EknightPartCondition::Normal;
	}
	if (PartComponent)
	{
		PartComponent->OnPartConditionChanged.AddUniqueDynamic(this, &ThisClass::HandlePartConditionChanged);
	}

	auto MakeMaterial = [this](UMaterialInterface* Source) -> UMaterialInstanceDynamic*
	{
		if (!Source)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* const MID = UMaterialInstanceDynamic::Create(Source, this);
		MID->SetScalarParameterValue(LineWidthParam, LineWidth);
		MID->SetScalarParameterValue(FillOpacityParam, FillOpacity);
		MID->SetScalarParameterValue(SurfaceOffsetParam, SurfaceOffset);
		for (int32 Index = 0; Index < MaxShellParts; ++Index)
		{
			MID->SetScalarParameterValue(GetOpacityParam(Index), 0.f);
		}
		return MID;
	};
	UMaterialInterface* const Base = ShellMaterial.LoadSynchronous();
	UMaterialInterface* const XRay = ShellXRayMaterial.LoadSynchronous();
	Material = MakeMaterial(Base);
	XRayMaterial = MakeMaterial(XRay ? XRay : Base);
	ApplyColors();

	// 몬스터 메시의 포즈를 그대로 받는다. 셸 메시는 같은 뼈 가중치를 갖고 있다.
	ShellMesh = NewObject<USkeletalMeshComponent>(GetOwner(), TEXT("PartShellMesh"), RF_Transient);
	ShellMesh->SetSkeletalMesh(Asset);
	ShellMesh->SetupAttachment(LeaderMesh);
	ShellMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShellMesh->SetGenerateOverlapEvents(false);
	ShellMesh->SetCastShadow(false);
	ShellMesh->bReceivesDecals = false;
	ShellMesh->bUseBoundsFromLeaderPoseComponent = true;
	ShellMesh->SetTranslucentSortPriority(10);
	ShellMesh->SetMaterial(0, Material);
	ShellMesh->SetVisibility(false);
	ShellMesh->RegisterComponent();
	ShellMesh->SetLeaderPoseComponent(LeaderMesh);
	return true;
}

void UknightMonsterPartShellComponent::SetXRay(bool bInXRay)
{
	if (bXRay == bInXRay || !ShellMesh)
	{
		return;
	}
	bXRay = bInXRay;
	ShellMesh->SetMaterial(0, GetActiveMaterial());
}

UMaterialInstanceDynamic* UknightMonsterPartShellComponent::GetActiveMaterial() const
{
	return bXRay && XRayMaterial ? XRayMaterial.Get() : Material.Get();
}

void UknightMonsterPartShellComponent::HandlePartConditionChanged(FGameplayTag PartTag, EknightPartCondition NewCondition)
{
	bool bChanged = false;
	for (FShellPart& Part : Parts)
	{
		if (Part.Tag == PartTag && Part.Condition != NewCondition)
		{
			Part.Condition = NewCondition;
			bChanged = true;
		}
	}
	if (bChanged)
	{
		ApplyColors();
		ShowPart(PartTag, StateChangeShowTime);
	}
}

void UknightMonsterPartShellComponent::ApplyColors()
{
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		const FLinearColor Color = GetConditionColor(Parts[Index].Condition);
		for (UMaterialInstanceDynamic* const MID : {Material.Get(), XRayMaterial.Get()})
		{
			if (MID)
			{
				MID->SetVectorParameterValue(GetColorParam(Index), Color);
			}
		}
	}
}

FLinearColor UknightMonsterPartShellComponent::GetConditionColor(EknightPartCondition Condition) const
{
	switch (Condition)
	{
	case EknightPartCondition::Critical: return CriticalColor;
	case EknightPartCondition::Broken: return BrokenColor;
	default: return NormalColor;
	}
}

void UknightMonsterPartShellComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!ShellMesh)
	{
		SetComponentTickEnabled(false);
		return;
	}

	BlinkTime += DeltaTime;
	const bool bRevealing = RevealRemaining > 0.f;
	RevealRemaining = FMath::Max(RevealRemaining - DeltaTime, 0.f);
	// 스캔 셸은 남은 시간의 마지막 FadeOutTime 동안 옅어진다.
	const float RevealOpacity = bRevealing ? FMath::Clamp(RevealRemaining / FadeOutTime, 0.f, 1.f) : 0.f;
	if (RevealRemaining <= 0.f)
	{
		// 스캔이 끝나면 락온 셸도 다시 몸에 가려진다.
		SetXRay(false);
	}

	float Opacities[MaxShellParts] = {};
	bool bAnyVisible = false;
	bool bAnyShowing = false;
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		FShellPart& Part = Parts[Index];
		// 락온·상태 변화 표시도 남은 시간의 마지막 FadeOutTime 동안 옅어진다.
		const bool bShowing = Part.ShowRemaining > 0.f;
		Part.ShowRemaining = FMath::Max(Part.ShowRemaining - DeltaTime, 0.f);
		bAnyShowing |= bShowing;
		const float ShowOpacity = bShowing ? FMath::Clamp(Part.ShowRemaining / FadeOutTime, 0.f, 1.f) : 0.f;
		const float Target = FMath::Max(ShowOpacity, RevealOpacity);
		Part.Opacity = Target > Part.Opacity
			? FMath::Min(Target, Part.Opacity + DeltaTime / FadeInTime)
			: FMath::Max(Target, Part.Opacity - DeltaTime / FadeOutTime);
		float Blink = 1.f;
		if (Part.Condition == EknightPartCondition::Critical)
		{
			const float Wave = 0.5f + 0.5f * FMath::Cos(UE_TWO_PI * BlinkTime / CriticalBlinkPeriod);
			Blink = FMath::Lerp(CriticalBlinkMinOpacity, 1.f, Wave);
		}
		Opacities[Index] = Part.Opacity * Blink;
		bAnyVisible |= Part.Opacity > KINDA_SMALL_NUMBER;
	}
	if (UMaterialInstanceDynamic* const MID = GetActiveMaterial())
	{
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			MID->SetScalarParameterValue(GetOpacityParam(Index), Opacities[Index]);
		}
	}
	if (ShellMesh->IsVisible() != bAnyVisible)
	{
		ShellMesh->SetVisibility(bAnyVisible);
	}
	if (!bAnyVisible && RevealRemaining <= 0.f && !bAnyShowing)
	{
		SetComponentTickEnabled(false);
	}
}
