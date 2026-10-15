#include "Environment/knightWaterRippleSubsystem.h"

#include "knight.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/knightWaterRippleSettings.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Sound/SoundBase.h"
#include "WaterBodyComponent.h"
#include "WaterBodyManager.h"

namespace knightWaterRipple
{
	/** M_WaterRipple_Step의 Impulse0..15와 맞춘다. */
	constexpr int32 MaxImpulses = 16;
	constexpr int32 MaxStepsPerFrame = 2;
	constexpr double CharacterRefreshInterval = 1.0;

	static TAutoConsoleVariable<int32> CVarDebug(TEXT("knight.WaterRipple.Debug"), 0,
		TEXT("1: 물결 계산 범위, 캐릭터별 물 판정, 누른 자리를 그린다. 2: 같은 내용을 1초마다 로그로도 남긴다. 3: 결과 텍스처와 MPC 값도 읽어 남긴다(느림)."));
	static TAutoConsoleVariable<float> CVarTestPulse(TEXT("knight.WaterRipple.TestPulse"), 0.f,
		TEXT("0보다 크면 이 간격(초)마다 플레이어 앞 6m를 반지름 2m, 높이 10cm로 솟게 한다(읽기 확인이 쉽게 양수)."));
	static TAutoConsoleVariable<int32> CVarForceMaterial(TEXT("knight.WaterRipple.ForceMaterial"), 0,
		TEXT("1: 게임 중에만 모든 물에 PreviewWaterMaterial을 씌운다. 맵 파일은 바뀌지 않는다."));

	static TAutoConsoleVariable<int32> CVarPause(TEXT("knight.WaterRipple.Pause"), 0,
		TEXT("1: 계산을 멈추고 결과를 비운다. 물결 없는 화면과 비교할 때 쓴다."));

	static const FName NormalAndHeightName(TEXT("NormalAndHeight"));
	static const TCHAR* WaterCollectionPath = TEXT("/Water/Materials/MPC/MPC_Water.MPC_Water");

	static const FName& ImpulseName(int32 Index)
	{
		static const TArray<FName> Names = []
		{
			TArray<FName> Result;
			for (int32 i = 0; i < MaxImpulses; ++i)
			{
				Result.Add(FName(*FString::Printf(TEXT("Impulse%d"), i)));
			}
			return Result;
		}();
		return Names[Index];
	}
}

bool UknightWaterRippleSubsystem::FindWaterSurface(const UWorld* World, const FVector& Location, float& OutSurfaceZ)
{
	bool bFound = false;
	FWaterBodyManager::ForEachWaterBodyComponent(World, [&](UWaterBodyComponent* Body)
	{
		if (!Body)
		{
			return true;
		}
		const FBox Box = Body->Bounds.GetBox();
		if (Location.X < Box.Min.X || Location.X > Box.Max.X || Location.Y < Box.Min.Y || Location.Y > Box.Max.Y)
		{
			return true;
		}

		const TValueOrError<FWaterBodyQueryResult, EWaterBodyQueryError> Query = Body->TryQueryWaterInfoClosestToWorldLocation(Location, EWaterBodyQueryFlags::ComputeLocation);
		if (!Query.HasValue() || Query.GetValue().IsInExclusionVolume())
		{
			return true;
		}
		const float SurfaceZ = Query.GetValue().GetWaterSurfaceLocation().Z;

		// 호수 질의는 경계 밖에서도 가장 가까운 수면을 돌려주므로, 수면 바로 아래 점이 물 충돌 모양 안에 있는지 따로 본다.
		const TArray<UPrimitiveComponent*> Collisions = Body->GetCollisionComponents();
		if (!Collisions.IsEmpty())
		{
			const FVector Probe(Location.X, Location.Y, SurfaceZ - 1.f);
			const bool bInside = Collisions.ContainsByPredicate([&Probe](const UPrimitiveComponent* Collision)
			{
				return Collision && Collision->OverlapComponent(Probe, FQuat::Identity, FCollisionShape::MakeSphere(1.f));
			});
			if (!bInside)
			{
				return true;
			}
		}

		OutSurfaceZ = SurfaceZ;
		bFound = true;
		return false;
	});
	return bFound;
}

void UknightWaterRippleSubsystem::AddImpulse(const FVector& Location, float Radius, float Depth)
{
	const FVector2D Location2D(Location);
	if (!bActive || !bHasCenter || Radius <= 0.f || Depth == 0.f || !IsInsideSimulation(Location2D, Radius))
	{
		return;
	}
	PendingImpulses.Add({ Location2D, Radius, Depth, static_cast<float>(Location.Z) });
}

bool UknightWaterRippleSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* const World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UknightWaterRippleSubsystem::Deinitialize()
{
	// PIE가 끝난 뒤 에디터 화면의 물에 마지막 물결이 남지 않게 지운다.
	if (bActive)
	{
		ClearTargets();
	}
	bActive = false;
	Characters.Reset();
	PendingImpulses.Reset();
	Super::Deinitialize();
}

TStatId UknightWaterRippleSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UknightWaterRippleSubsystem, STATGROUP_Tickables);
}

void UknightWaterRippleSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TRACE_CPUPROFILER_EVENT_SCOPE(UknightWaterRippleSubsystem::Tick);

	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}
	const UknightWaterRippleSettings* const Settings = GetDefault<UknightWaterRippleSettings>();
	const double Now = World->GetTimeSeconds();

	if (!bActive)
	{
		if (Now < NextActivationCheck)
		{
			return;
		}
		NextActivationCheck = Now + Settings->InactiveCheckInterval;
		if (!TryActivate())
		{
			return;
		}
		LastSourceTime = Now;
	}

	if (knightWaterRipple::CVarPause.GetValueOnGameThread() != 0)
	{
		ClearTargets();
		return;
	}

	// 고정 간격으로 돌려야 프레임이 달라도 물결이 같은 빠르기로 퍼진다. 밀린 단계는 버린다.
	const float StepDelta = 1.f / Settings->StepsPerSecond;
	StepAccumulator += DeltaTime;
	int32 Steps = FMath::FloorToInt32(StepAccumulator / StepDelta);
	StepAccumulator -= Steps * StepDelta;
	Steps = FMath::Min(Steps, knightWaterRipple::MaxStepsPerFrame);
	if (Steps <= 0)
	{
		return;
	}

	const APawn* const Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Pawn)
	{
		return;
	}

	// 중심을 한 칸 단위로 맞춰 옮겨야 기존 물결을 정확히 한 칸씩 밀어 이어 쓸 수 있다.
	const float Size = Settings->SimulationSize;
	const double Cell = Size / Resolution;
	const FVector2D Focus(Pawn->GetActorLocation());
	const FVector2D NewCenter(FMath::RoundToDouble(Focus.X / Cell) * Cell, FMath::RoundToDouble(Focus.Y / Cell) * Cell);
	const FVector2D Shift = bHasCenter ? (NewCenter - Center) / Size : FVector2D::ZeroVector;
	Center = NewCenter;
	bHasCenter = true;

	if (Now >= NextCharacterRefresh)
	{
		NextCharacterRefresh = Now + knightWaterRipple::CharacterRefreshInterval;
		RefreshCharacters();
	}
	const float Elapsed = FMath::Max(static_cast<float>(Now - LastSourceTime), StepDelta);
	LastSourceTime = Now;
	for (FTrackedCharacter& Tracked : Characters)
	{
		UpdateCharacter(Tracked, Now, Elapsed);
	}

	const float PulseInterval = knightWaterRipple::CVarTestPulse.GetValueOnGameThread();
	if (PulseInterval > 0.f && Now >= NextTestPulse)
	{
		NextTestPulse = Now + PulseInterval;
		AddImpulse(Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 600.f, 200.f, -10.f);
	}
	const int32 DebugMode = knightWaterRipple::CVarDebug.GetValueOnGameThread();
	if (DebugMode != 0)
	{
		DrawDebug(*Pawn);
	}
	if (DebugMode >= 2 && Now >= NextDebugLog)
	{
		NextDebugLog = Now + 1.0;
		for (const FTrackedCharacter& Tracked : Characters)
		{
			UE_LOG(Logknight, Log, TEXT("Water ripple: %s found=%d in=%d surface=%.0f bottom=%.0f bodies=%d maxdown=%.0f"), *GetNameSafe(Tracked.Character.Get()),
				Tracked.bWaterFound, Tracked.bInWater, Tracked.SurfaceZ, Tracked.BottomZ, Tracked.Contacts.Num(), Tracked.MaxDownSpeed);
		}
		for (FTrackedCharacter& Tracked : Characters)
		{
			Tracked.MaxDownSpeed = 0.f;
		}
		UE_LOG(Logknight, Log, TEXT("Water ripple: center (%.0f, %.0f) pending %d"), Center.X, Center.Y, PendingImpulses.Num());
		if (DebugMode >= 3)
		{
			// 읽기는 0~1로 잘리므로 노멀 X/Y는 양수 쪽만, 높이(B)는 1cm 이상이면 1로 보인다.
			const FVector2D PulseUV = (FVector2D(Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 600.f) - Center) / Settings->SimulationSize + FVector2D(0.5, 0.5);
			for (const FVector2D& UV : { FVector2D(0.5, 0.5), PulseUV, PulseUV - FVector2D(0.02, 0.0), PulseUV + FVector2D(0.02, 0.0), PulseUV - FVector2D(0.0, 0.02) })
			{
				const FLinearColor Value = UKismetRenderingLibrary::ReadRenderTargetRawUV(World, OutputTarget, UV.X, UV.Y);
				UE_LOG(Logknight, Log, TEXT("Water ripple: output at (%.3f, %.3f) = %s"), UV.X, UV.Y, *Value.ToString());
			}
			const FLinearColor SimLocation = UKismetMaterialLibrary::GetVectorParameterValue(World, WaterCollection, TEXT("SimLocation"));
			const float SimSize = UKismetMaterialLibrary::GetScalarParameterValue(World, WaterCollection, TEXT("FluidSimSize"));
			UE_LOG(Logknight, Log, TEXT("Water ripple: MPC SimLocation %s FluidSimSize %.0f"), *SimLocation.ToString(), SimSize);
		}
	}

	for (int32 Step = 0; Step < Steps; ++Step)
	{
		RunStep(Step == 0 ? Shift : FVector2D::ZeroVector, Step == 0);
	}
	PendingImpulses.Reset();

	NormalMID->SetTextureParameterValue(TEXT("State"), StateTargets[CurrentState]);
	NormalMID->SetScalarParameterValue(TEXT("Res"), Resolution);
	NormalMID->SetScalarParameterValue(TEXT("CellSize"), Cell);
	NormalMID->SetScalarParameterValue(TEXT("NormalScale"), Settings->NormalScale);
	NormalMID->SetScalarParameterValue(TEXT("HeightScale"), Settings->HeightScale);
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, OutputTarget, NormalMID);

	UKismetMaterialLibrary::SetVectorParameterValue(World, WaterCollection, TEXT("SimLocation"), FLinearColor(Center.X, Center.Y, 0.f, 0.f));
	UKismetMaterialLibrary::SetScalarParameterValue(World, WaterCollection, TEXT("FluidSimSize"), Size);
}

bool UknightWaterRippleSubsystem::TryActivate()
{
	UWorld* const World = GetWorld();
	const UknightWaterRippleSettings* const Settings = GetDefault<UknightWaterRippleSettings>();

	if (knightWaterRipple::CVarForceMaterial.GetValueOnGameThread() != 0)
	{
		if (UMaterialInterface* const Preview = Settings->PreviewWaterMaterial.LoadSynchronous())
		{
			FWaterBodyManager::ForEachWaterBodyComponent(World, [Preview](UWaterBodyComponent* Body)
			{
				if (Body)
				{
					Body->SetWaterMaterial(Preview);
				}
				return true;
			});
		}
	}

	// 물 머티리얼이 결과 텍스처를 읽을 때만 켠다. 물이 없거나 일반 물 머티리얼인 레벨에서는 계산해도 보이지 않는다.
	bool bUsed = false;
	FWaterBodyManager::ForEachWaterBodyComponent(World, [&bUsed, Settings](UWaterBodyComponent* Body)
	{
		const UMaterialInterface* const Material = Body ? Body->GetWaterMaterial() : nullptr;
		UTexture* Texture = nullptr;
		if (Material && Material->GetTextureParameterValue(FHashedMaterialParameterInfo(knightWaterRipple::NormalAndHeightName), Texture)
			&& Texture && FSoftObjectPath(Texture) == Settings->OutputTarget.ToSoftObjectPath())
		{
			bUsed = true;
		}
		return !bUsed;
	});
	if (!bUsed)
	{
		return false;
	}

	OutputTarget = Settings->OutputTarget.LoadSynchronous();
	UMaterialInterface* const StepMaterial = Settings->StepMaterial.LoadSynchronous();
	UMaterialInterface* const NormalMaterial = Settings->NormalMaterial.LoadSynchronous();
	WaterCollection = LoadObject<UMaterialParameterCollection>(nullptr, knightWaterRipple::WaterCollectionPath);
	if (!OutputTarget || !StepMaterial || !NormalMaterial || !WaterCollection)
	{
		UE_LOG(Logknight, Warning, TEXT("Water ripple: missing assets (target %d, step %d, normal %d, MPC %d). Check Project Settings > Knight Water Ripple."),
			OutputTarget != nullptr, StepMaterial != nullptr, NormalMaterial != nullptr, WaterCollection != nullptr);
		return false;
	}

	Resolution = FMath::Max(OutputTarget->SizeX, 16);
	for (TObjectPtr<UTextureRenderTarget2D>& State : StateTargets)
	{
		State = UKismetRenderingLibrary::CreateRenderTarget2D(World, Resolution, Resolution, RTF_RG16f, FLinearColor::Black);
		State->AddressX = TA_Clamp;
		State->AddressY = TA_Clamp;
		State->UpdateResourceImmediate(true);
	}
	SplashSystem = Settings->SplashSystem.LoadSynchronous();
	SplashHeavySystem = Settings->SplashHeavySystem.LoadSynchronous();
	SplashSound = Settings->SplashSound.LoadSynchronous();
	StepMID = UMaterialInstanceDynamic::Create(StepMaterial, this);
	NormalMID = UMaterialInstanceDynamic::Create(NormalMaterial, this);

	ClearTargets();
	CurrentState = 0;
	StepAccumulator = 0.f;
	bHasCenter = false;
	bActive = true;
	UE_LOG(Logknight, Log, TEXT("Water ripple: active (%dx%d, %.0f m)"), Resolution, Resolution, Settings->SimulationSize / 100.f);
	return true;
}

void UknightWaterRippleSubsystem::RefreshCharacters()
{
	Characters.RemoveAll([](const FTrackedCharacter& Tracked) { return !Tracked.Character.IsValid(); });

	const double Now = GetWorld()->GetTimeSeconds();
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* const Character = *It;
		FTrackedCharacter* Tracked = Characters.FindByPredicate([Character](const FTrackedCharacter& Each) { return Each.Character.Get() == Character; });
		const USkeletalMeshComponent* const Mesh = Character->GetMesh();
		UPhysicsAsset* const PhysicsAsset = Mesh ? Mesh->GetPhysicsAsset() : nullptr;
		if (Tracked && Tracked->PhysicsAsset.Get() == PhysicsAsset)
		{
			continue;
		}
		if (!Tracked)
		{
			Tracked = &Characters.AddDefaulted_GetRef();
			Tracked->Character = Character;
		}

		// 메시나 피직스 에셋이 바뀌면 바디 목록을 다시 만든다.
		Tracked->PhysicsAsset = PhysicsAsset;
		Tracked->Contacts.Reset();
		Tracked->bHasPrev = false;
		if (!PhysicsAsset)
		{
			continue;
		}
		for (int32 Index = 0; Index < PhysicsAsset->SkeletalBodySetups.Num(); ++Index)
		{
			const USkeletalBodySetup* const Setup = PhysicsAsset->SkeletalBodySetups[Index];
			const int32 BoneIndex = Setup ? Mesh->GetBoneIndex(Setup->BoneName) : INDEX_NONE;
			if (BoneIndex == INDEX_NONE || Setup->AggGeom.GetElementCount() == 0)
			{
				continue;
			}
			FBodyContact& Contact = Tracked->Contacts.AddDefaulted_GetRef();
			Contact.BodySetupIndex = Index;
			Contact.BoneIndex = BoneIndex;
			// 막 생긴 캐릭터는 바닥에 내려앉는 움직임이 부딪힘으로 잡히므로 잠깐 첨벙을 쉰다.
			Contact.NextSplashTime = Now + 0.5;
			Contact.NextWakeTime = Now + 0.5;
		}
	}
}

void UknightWaterRippleSubsystem::UpdateCharacter(FTrackedCharacter& Tracked, double Now, float Elapsed)
{
	const ACharacter* const Character = Tracked.Character.Get();
	const UCapsuleComponent* const Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	const USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Capsule || !Mesh)
	{
		Tracked.bHasPrev = false;
		return;
	}
	const UknightWaterRippleSettings* const Settings = GetDefault<UknightWaterRippleSettings>();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FVector Location = Character->GetActorLocation();
	const FVector Bottom = Location - FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight());

	// 계산 범위 밖이면 수면도 찾지 않는다.
	if (!IsInsideSimulation(FVector2D(Location), Radius))
	{
		Tracked.bInWater = false;
		Tracked.bHasPrev = false;
		return;
	}

	const float Margin = FMath::Max(Radius * Settings->SurfaceMarginScale, 20.f);
	const float MinSpeed = Settings->MinSpeed + Radius * Settings->MinSpeedPerRadius;
	// 격자 몇 칸보다 작게 누르면 물결이 생기기 전에 뭉개진다.
	const float MinRadius = Settings->SimulationSize / FMath::Max(Resolution, 1) * 2.5f;
	Tracked.BottomZ = Bottom.Z;
	if (Now >= Tracked.NextQueryTime)
	{
		Tracked.NextQueryTime = Now + Settings->WaterQueryInterval;
		Tracked.bWaterFound = FindWaterSurface(GetWorld(), Location, Tracked.SurfaceZ);
		Tracked.bInWater = Tracked.bWaterFound && Bottom.Z < Tracked.SurfaceZ + Margin;
	}
	if (!Tracked.bInWater)
	{
		Tracked.bHasPrev = false;
		return;
	}

	// 캡슐은 캐릭터 전체가 물을 밀어내는 큰 물결만 만든다. 첨벙은 바디가 맡는다.
	if (Tracked.bHasPrev)
	{
		AddMovementImpulse(Bottom, Tracked.PrevBottom, Tracked.SurfaceZ, Margin, FMath::Max(Radius, MinRadius), MinSpeed, Elapsed);
	}
	Tracked.PrevBottom = Bottom;
	Tracked.CapsuleRadius = Radius;

	for (FBodyContact& Contact : Tracked.Contacts)
	{
		UpdateContact(Tracked, Contact, *Mesh, MinRadius, MinSpeed, Now, Elapsed);
	}
	Tracked.bHasPrev = true;
}

void UknightWaterRippleSubsystem::UpdateContact(FTrackedCharacter& Tracked, FBodyContact& Contact, const USkeletalMeshComponent& Mesh, float MinRadius, float MinSpeed, double Now, float Elapsed)
{
	const UPhysicsAsset* const PhysicsAsset = Tracked.PhysicsAsset.Get();
	const USkeletalBodySetup* const Setup = PhysicsAsset && PhysicsAsset->SkeletalBodySetups.IsValidIndex(Contact.BodySetupIndex) ? PhysicsAsset->SkeletalBodySetups[Contact.BodySetupIndex].Get() : nullptr;
	if (!Setup)
	{
		return;
	}
	const UknightWaterRippleSettings* const Settings = GetDefault<UknightWaterRippleSettings>();

	// 뼈는 몸 한가운데를 지나므로 몸이 바닥에 닿아도 몸 두께만큼 떠 있다. 바디 경계 상자의 가장 낮은 면으로 본다.
	// 크기는 상자의 짧은 쪽 반폭이다. 비스듬히 누운 캡슐은 긴 쪽이 길이까지 포함해 부풀기 때문이다.
	const FBox Box = Setup->AggGeom.CalcAABB(Mesh.GetBoneTransform(Contact.BoneIndex));
	const FVector Lowest(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z);
	const float HalfWidth = static_cast<float>(FMath::Min(Box.GetExtent().X, Box.GetExtent().Y));
	const FVector Prev = Contact.PrevLowest;
	Contact.PrevLowest = Lowest;
	if (!Tracked.bHasPrev)
	{
		Contact.PeakDownSpeed = 0.f;
		return;
	}

	const float SurfaceZ = Tracked.SurfaceZ;
	const bool bSubmerged = Lowest.Z <= SurfaceZ;
	AddMovementImpulse(Lowest, Prev, SurfaceZ, 0.f, FMath::Max(HalfWidth * Settings->BodyRadiusScale, MinRadius), MinSpeed, Elapsed);

	const float SizeRatio = HalfWidth / Settings->SplashReferenceBodySize;
	const float MergeRadius = HalfWidth * 3.f;
	const FVector Surface(Lowest.X, Lowest.Y, SurfaceZ);
	const float DownSpeed = static_cast<float>(Prev.Z - Lowest.Z) / Elapsed;
	Tracked.MaxDownSpeed = FMath::Max(Tracked.MaxDownSpeed, DownSpeed);
	const float StrikeSpeed = DetectStrike(Contact, DownSpeed, Now);
	if (StrikeSpeed > 0.f && bSubmerged && ClaimSplash(Tracked, Surface, MergeRadius, Now))
	{
		const float SpeedScale = FMath::Clamp(StrikeSpeed / Settings->SplashReferenceDownSpeed, Settings->SplashSpeedScaleRange.X, Settings->SplashSpeedScaleRange.Y);
		SpawnSplash(Surface, SizeRatio * SpeedScale);
		return;
	}

	// 물에 잠긴 채 옆으로 쓸고 지나가는 바디는 물을 튀기며 지나간다.
	const float SideSpeed = static_cast<float>(FVector2D::Distance(FVector2D(Lowest), FVector2D(Prev))) / Elapsed;
	const float WakeSpeed = Settings->SplashWakeSpeed + Tracked.CapsuleRadius * Settings->SplashWakeSpeedPerRadius;
	if (bSubmerged && SideSpeed >= WakeSpeed && Now >= Contact.NextWakeTime && ClaimSplash(Tracked, Surface, MergeRadius, Now))
	{
		Contact.NextWakeTime = Now + Settings->SplashWakeInterval;
		const float SpeedScale = FMath::Clamp(SideSpeed / (WakeSpeed * 2.f), Settings->SplashSpeedScaleRange.X, Settings->SplashSpeedScaleRange.Y);
		SpawnSplash(Surface, SizeRatio * SpeedScale);
	}
}

void UknightWaterRippleSubsystem::AddMovementImpulse(const FVector& Location, const FVector& Prev, float SurfaceZ, float Margin, float Radius, float MinSpeed, float Elapsed)
{
	if (Location.Z > SurfaceZ + Margin)
	{
		return;
	}
	const UknightWaterRippleSettings* const Settings = GetDefault<UknightWaterRippleSettings>();
	// 내려딛는 발과 착지도 물을 밀어내므로 수직 속도까지 넣는다.
	const float Speed = FVector::Dist(Location, Prev) / Elapsed;
	if (Speed < MinSpeed)
	{
		return;
	}
	const float SpeedRatio = FMath::Min(Speed / FMath::Max(Settings->ReferenceSpeed, 1.f), 2.f);
	AddImpulse(Location, Radius, Settings->StrengthPerSecond * SpeedRatio * Elapsed);
}

float UknightWaterRippleSubsystem::DetectStrike(FBodyContact& Contact, float DownSpeed, double Now) const
{
	// 발이나 몸은 빠르게 내려오다 바닥에 닿는 순간 거의 멈춘다. 내려오던 바디의 속도가 문턱의 1/4 아래로 떨어진 프레임을 부딪힌 순간으로 본다.
	const UknightWaterRippleSettings* const Settings = GetDefault<UknightWaterRippleSettings>();
	const float Threshold = Settings->SplashMinDownSpeed;
	if (DownSpeed >= Threshold)
	{
		Contact.PeakDownSpeed = FMath::Max(Contact.PeakDownSpeed, DownSpeed);
		return 0.f;
	}
	if (Contact.PeakDownSpeed <= 0.f || DownSpeed > Threshold * 0.25f)
	{
		return 0.f;
	}
	const float StrikeSpeed = Contact.PeakDownSpeed;
	Contact.PeakDownSpeed = 0.f;
	if (Now < Contact.NextSplashTime)
	{
		return 0.f;
	}
	Contact.NextSplashTime = Now + Settings->SplashCooldown;
	return StrikeSpeed;
}

void UknightWaterRippleSubsystem::SpawnSplash(const FVector& Location, float SizeScale) const
{
	UWorld* const World = GetWorld();
	const UknightWaterRippleSettings* const Settings = GetDefault<UknightWaterRippleSettings>();
	UNiagaraSystem* const System = SizeScale >= Settings->SplashHeavyScale && SplashHeavySystem ? SplashHeavySystem.Get() : SplashSystem.Get();
	if (System)
	{
		const float Scale = Settings->SplashScale * FMath::Min(SizeScale, Settings->SplashMaxScale);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location, FRotator::ZeroRotator, FVector(Scale), true, true, ENCPoolMethod::AutoRelease);
	}
	if (SplashSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, SplashSound, Location, Settings->SplashVolume);
	}
	const int32 DebugMode = knightWaterRipple::CVarDebug.GetValueOnGameThread();
	if (DebugMode != 0)
	{
		DrawDebugSphere(World, Location, 20.f, 8, System == SplashHeavySystem ? FColor::Purple : FColor::Blue, false, 0.5f);
	}
	if (DebugMode >= 2)
	{
		UE_LOG(Logknight, Log, TEXT("Water splash at (%.0f, %.0f, %.0f) scale %.2f"), Location.X, Location.Y, Location.Z, SizeScale);
	}
}

bool UknightWaterRippleSubsystem::ClaimSplash(FTrackedCharacter& Tracked, const FVector& Location, float Radius, double Now) const
{
	const double MergeTime = GetDefault<UknightWaterRippleSettings>()->SplashMergeTime;
	Tracked.RecentSplashes.RemoveAll([Now, MergeTime](const FRecentSplash& Recent) { return Now - Recent.Time > MergeTime; });
	const FVector2D Location2D(Location);
	const bool bNear = Tracked.RecentSplashes.ContainsByPredicate([&Location2D, Radius](const FRecentSplash& Recent)
	{
		return FVector2D::DistSquared(Recent.Location, Location2D) < FMath::Square(FMath::Max(Recent.Radius, Radius));
	});
	if (bNear)
	{
		return false;
	}
	Tracked.RecentSplashes.Add({ Location2D, Radius, Now });
	return true;
}

float UknightWaterRippleSubsystem::GetBodyHalfWidth(const USkeletalMeshComponent* Mesh, FName Bone)
{
	const UPhysicsAsset* const PhysicsAsset = Mesh ? Mesh->GetPhysicsAsset() : nullptr;
	const int32 BodyIndex = PhysicsAsset && !Bone.IsNone() ? PhysicsAsset->FindBodyIndex(Bone) : INDEX_NONE;
	const int32 BoneIndex = BodyIndex != INDEX_NONE ? Mesh->GetBoneIndex(Bone) : INDEX_NONE;
	if (BoneIndex == INDEX_NONE)
	{
		return 0.f;
	}
	const FBox Box = PhysicsAsset->SkeletalBodySetups[BodyIndex]->AggGeom.CalcAABB(Mesh->GetBoneTransform(BoneIndex));
	return static_cast<float>(FMath::Min(Box.GetExtent().X, Box.GetExtent().Y));
}

bool UknightWaterRippleSubsystem::IsInsideSimulation(const FVector2D& Location, float Radius) const
{
	const float Half = GetDefault<UknightWaterRippleSettings>()->SimulationSize * 0.5f + Radius;
	return FMath::Abs(Location.X - Center.X) <= Half && FMath::Abs(Location.Y - Center.Y) <= Half;
}

void UknightWaterRippleSubsystem::RunStep(const FVector2D& Shift, bool bWithImpulses)
{
	const UknightWaterRippleSettings* const Settings = GetDefault<UknightWaterRippleSettings>();
	const float Size = Settings->SimulationSize;

	StepMID->SetTextureParameterValue(TEXT("State"), StateTargets[CurrentState]);
	StepMID->SetScalarParameterValue(TEXT("Res"), Resolution);
	StepMID->SetScalarParameterValue(TEXT("WaveSpeed"), Settings->WaveSpeed);
	StepMID->SetScalarParameterValue(TEXT("Damping"), Settings->Damping);
	StepMID->SetScalarParameterValue(TEXT("MaxHeight"), Settings->MaxHeight);
	StepMID->SetVectorParameterValue(TEXT("Offset"), FLinearColor(Shift.X, Shift.Y, 0.f, 0.f));

	// 한 단계에 넣을 수 있는 수가 정해져 있어서, 넘치면 크고 센 것부터 남긴다.
	if (bWithImpulses && PendingImpulses.Num() > knightWaterRipple::MaxImpulses)
	{
		PendingImpulses.Sort([](const FPendingImpulse& A, const FPendingImpulse& B) { return A.Radius * FMath::Abs(A.Depth) > B.Radius * FMath::Abs(B.Depth); });
	}
	for (int32 Index = 0; Index < knightWaterRipple::MaxImpulses; ++Index)
	{
		FLinearColor Value(0.f, 0.f, 0.f, 0.f);
		if (bWithImpulses && PendingImpulses.IsValidIndex(Index))
		{
			const FPendingImpulse& Impulse = PendingImpulses[Index];
			const FVector2D UV = (Impulse.Location - Center) / Size + FVector2D(0.5, 0.5);
			Value = FLinearColor(UV.X, UV.Y, Impulse.Radius / Size, Impulse.Depth);
		}
		StepMID->SetVectorParameterValue(knightWaterRipple::ImpulseName(Index), Value);
	}

	const int32 Next = 1 - CurrentState;
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(GetWorld(), StateTargets[Next], StepMID);
	CurrentState = Next;
}

void UknightWaterRippleSubsystem::DrawDebug(const APawn& Pawn) const
{
	UWorld* const World = GetWorld();
	const float Half = GetDefault<UknightWaterRippleSettings>()->SimulationSize * 0.5f;
	constexpr float Life = 0.05f;
	DrawDebugBox(World, FVector(Center.X, Center.Y, Pawn.GetActorLocation().Z), FVector(Half, Half, 1.f), FColor::Cyan, false, Life, 0, 8.f);

	for (const FTrackedCharacter& Tracked : Characters)
	{
		const ACharacter* const Character = Tracked.Character.Get();
		if (!Character)
		{
			continue;
		}
		const TCHAR* const State = Tracked.bInWater ? TEXT("IN WATER") : Tracked.bWaterFound ? TEXT("ABOVE WATER") : TEXT("NO WATER");
		const FString Text = FString::Printf(TEXT("%s  surface %.0f  bottom %.0f  bodies %d"), State, Tracked.SurfaceZ, Tracked.BottomZ, Tracked.Contacts.Num());
		DrawDebugString(World, Character->GetActorLocation() + FVector(0.f, 0.f, 120.f), Text, nullptr, Tracked.bInWater ? FColor::Green : FColor::Red, Life, true);
	}

	float MaxDepth = 0.f;
	for (const FPendingImpulse& Impulse : PendingImpulses)
	{
		MaxDepth = FMath::Max(MaxDepth, FMath::Abs(Impulse.Depth));
		DrawDebugCircle(World, FVector(Impulse.Location, Impulse.Z), Impulse.Radius, 24, FColor::Orange, false, Life, 0, 3.f, FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this), 0.1f, FColor::Cyan,
			FString::Printf(TEXT("Water ripple: %d characters, %d impulses (max %.2f cm)"), Characters.Num(), PendingImpulses.Num(), MaxDepth));
	}
}

void UknightWaterRippleSubsystem::ClearTargets()
{
	UWorld* const World = GetWorld();
	for (UTextureRenderTarget2D* const State : StateTargets)
	{
		if (State)
		{
			UKismetRenderingLibrary::ClearRenderTarget2D(World, State, FLinearColor::Black);
		}
	}
	if (OutputTarget)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(World, OutputTarget, FLinearColor::Black);
	}
}
