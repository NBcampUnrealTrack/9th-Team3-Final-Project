#include "Raid/knightRaidFinishPresentation.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/knightPlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Environment/knightWaterRippleSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterHoverComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Raid/knightRaidGameState.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "TimerManager.h"
#include "knight.h"

namespace knightRaidFinish
{
	/** 슬로모션 동안 늘어뜨리는 효과음 클래스(하위 클래스 포함). */
	const TCHAR* const SFXClassPath = TEXT("/Game/Knight/Audio/Settings/SC_Knight_SFX.SC_Knight_SFX");

	/** 몸이 땅에 닿을 것으로 본 시간보다 이만큼(게임 초) 지나도 착지를 못 보면 그냥 쿵을 낸다. */
	constexpr float ImpactTimeout = 3.f;
}

bool UknightRaidFinishPresentation::Start(AknightMonsterCharacterBase& InMonster, APlayerController& InPlayerController, AknightRaidGameState& InRaidState,
	const FknightRaidFinishSettings& InSettings)
{
	UWorld* const World = InMonster.GetWorld();
	if (!World || !InSettings.bEnabled || !InPlayerController.GetPawn() || InSettings.Shots.IsEmpty())
	{
		return false;
	}
	Settings = InSettings;
	Monster = &InMonster;
	PlayerController = &InPlayerController;
	RaidState = &InRaidState;
	DeathGameTime = World->GetTimeSeconds();

	const USkeletalMeshComponent* const Mesh = InMonster.GetMesh();
	const FBoxSphereBounds Bounds = Mesh ? Mesh->Bounds : FBoxSphereBounds(InMonster.GetActorLocation(), FVector(300.f), 300.f);
	BodyRadius = FMath::Clamp(static_cast<float>(FMath::Max3(Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z)), 300.f, 4000.f);
	BodyHalfHeight = FMath::Max(static_cast<float>(Bounds.BoxExtent.Z), 100.f);
	MonsterYaw = InMonster.GetActorRotation().Yaw;
	Focus = Bounds.Origin;

	// 공중에서 죽으면 떨어지는 시간이 사망 몽타주의 착지보다 길 수 있다.
	ExpectedImpactTime = Settings.ImpactTime;
	if (const UknightMonsterHoverComponent* const Hover = InMonster.GetHoverComponent(); Hover && Hover->IsFallingToGround())
	{
		ExpectedImpactTime = FMath::Max(ExpectedImpactTime, Hover->GetFallDuration());
	}

	// 땅에 닿는 순간 = 슬로모션(배율 × 길이) + 복귀(평균 배율 × 길이) + 원래 속도 NormalSpeedLead.
	const float Return = Settings.ReturnTime;
	const float Lead = Settings.NormalSpeedLead;
	SlowDuration = Settings.SlowTime;
	SlowDilation = (ExpectedImpactTime - Return * 0.5f - Lead) / (SlowDuration + Return * 0.5f);
	if (SlowDilation < Settings.MinSlowDilation)
	{
		// 너무 빨리 쓰러진다: 배율을 지키고 슬로모션을 줄인다.
		SlowDilation = Settings.MinSlowDilation;
		SlowDuration = FMath::Clamp((ExpectedImpactTime - Return * (SlowDilation + 1.f) * 0.5f - Lead) / SlowDilation, 0.5f, Settings.SlowTime);
	}
	SlowDilation = FMath::Min(SlowDilation, Settings.MaxSlowDilation);

	// 막타 히트스톱과 흰 섬광. 카메라 페이드는 게임 시간으로 흐르므로 이어질 슬로모션 배율로 길이를 맞춘다.
	UGameplayStatics::SetGlobalTimeDilation(World, Settings.HitStopDilation);
	bHitStop = true;
	if (APlayerCameraManager* const CameraManager = InPlayerController.PlayerCameraManager; CameraManager && Settings.FlashAlpha > 0.f)
	{
		const float FlashGameTime = Settings.HitStopTime * Settings.HitStopDilation + FMath::Max(Settings.FlashTime - Settings.HitStopTime, 0.f) * SlowDilation;
		CameraManager->StartCameraFade(Settings.FlashAlpha, 0.f, FMath::Max(FlashGameTime, 0.01f), FLinearColor::White, false, false);
	}
	InPlayerController.SetIgnoreMoveInput(true);
	InPlayerController.SetIgnoreLookInput(true);
	bInputIgnored = true;

	bWasFalling = !HasLanded();
	LastWaterSplashTime = -1.0;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick));
	SetStage(EknightRaidFinishStage::Slow);
	UE_LOG(Logknight, Log, TEXT("Finish presentation: %s down, impact expected %.2fs after death, slow x%.2f for %.1fs, body radius %.0f."),
		*InMonster.GetName(), ExpectedImpactTime, SlowDilation, SlowDuration, BodyRadius);
	return true;
}

void UknightRaidFinishPresentation::SetStage(EknightRaidFinishStage NewStage)
{
	Stage = NewStage;
	StageTime = 0.f;
	if (AknightRaidGameState* const State = RaidState.Get())
	{
		State->SetFinishStage(NewStage);
	}
	UE_LOG(Logknight, Log, TEXT("Finish presentation: stage %s."), *UEnum::GetValueAsString(NewStage));
}

bool UknightRaidFinishPresentation::Tick(float DeltaTime)
{
	UWorld* const World = GetWorld() ? GetWorld() : (Monster.IsValid() ? Monster->GetWorld() : nullptr);
	if (!World || !PlayerController.IsValid())
	{
		Stop();
		return false;
	}
	if (World->IsPaused())
	{
		return true;
	}
	StageTime += DeltaTime;
	const float SinceDeath = static_cast<float>(World->GetTimeSeconds() - DeathGameTime);
	// 공중에서 떨어진 몸이 물에 닿는 순간. 쿵(사망 몽타주의 쓰러짐)은 따로 BeginImpact에서 낸다.
	if (bWasFalling && HasLanded())
	{
		bWasFalling = false;
		SplashOnWater(Settings.FallWaterSplashScale);
	}

	switch (Stage)
	{
	case EknightRaidFinishStage::Slow:
		if (bHitStop && StageTime >= Settings.HitStopTime)
		{
			bHitStop = false;
			BeginSlow();
		}
		if (!bHitStop)
		{
			UpdateShot();
			if (StageTime >= Settings.HitStopTime + SlowDuration)
			{
				BeginReturn();
			}
		}
		break;
	case EknightRaidFinishStage::Return:
	{
		const float Alpha = FMath::SmoothStep(0.f, 1.f, FMath::Min(StageTime / Settings.ReturnTime, 1.f));
		UGameplayStatics::SetGlobalTimeDilation(World, FMath::Lerp(SlowDilation, 1.f, Alpha));
		if (Alpha >= 1.f && ((SinceDeath >= ExpectedImpactTime && HasLanded()) || SinceDeath >= ExpectedImpactTime + knightRaidFinish::ImpactTimeout))
		{
			BeginImpact();
		}
		break;
	}
	case EknightRaidFinishStage::Impact:
		if (StageTime >= Settings.SettleTime)
		{
			SetStage(EknightRaidFinishStage::Banner);
		}
		break;
	case EknightRaidFinishStage::Banner:
		if (StageTime >= Settings.BannerTime + Settings.LingerTime)
		{
			SetStage(EknightRaidFinishStage::Done);
			Stop();
			if (OnDone)
			{
				OnDone();
			}
			return false;
		}
		break;
	default:
		break;
	}
	return true;
}

void UknightRaidFinishPresentation::BeginSlow()
{
	APlayerController* const PC = PlayerController.Get();
	UWorld* const World = PC ? PC->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	UGameplayStatics::SetGlobalTimeDilation(World, SlowDilation);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	Camera = World->SpawnActor<ACameraActor>(Focus, FRotator::ZeroRotator, Params);
	if (Camera)
	{
		UCameraComponent* const View = Camera->GetCameraComponent();
		View->bConstrainAspectRatio = Settings.LetterboxAspectRatio > 0.f;
		View->AspectRatio = Settings.LetterboxAspectRatio > 0.f ? Settings.LetterboxAspectRatio : View->AspectRatio;
		View->PostProcessSettings = Settings.SlowPostProcess;
		View->PostProcessBlendWeight = 1.f;
		ShotIndex = INDEX_NONE;
		UpdateShot();
		PC->SetViewTargetWithBlend(Camera, 0.f);
	}
	PushSlowSound();
}

void UknightRaidFinishPresentation::UpdateShot()
{
	const AknightMonsterCharacterBase* const Target = Monster.Get();
	if (!Camera || !Target || Settings.Shots.IsEmpty())
	{
		return;
	}
	const float SlowTime = FMath::Max(StageTime - Settings.HitStopTime, 0.f);
	const float ShotLength = FMath::Max(SlowDuration / Settings.Shots.Num(), 0.01f);
	const int32 Index = FMath::Min(FMath::FloorToInt32(SlowTime / ShotLength), Settings.Shots.Num() - 1);
	const float Alpha = FMath::Clamp((SlowTime - Index * ShotLength) / ShotLength, 0.f, 1.f);
	const FknightRaidFinishShot& Shot = Settings.Shots[Index];

	// 컷이 바뀌면 바라보는 점을 바로 옮기고, 컷 안에서는 떨어지는 몸을 부드럽게 따라간다.
	const FVector Wanted = GetBodyCenter() + FVector::UpVector * Shot.FocusHeight * BodyHalfHeight;
	const float RealDelta = FMath::Max(FApp::GetDeltaTime(), 0.f);
	Focus = Index != ShotIndex ? Wanted : FMath::VInterpTo(Focus, Wanted, RealDelta, 6.f);
	ShotIndex = Index;

	const FVector Direction = FRotator(Shot.Pitch, MonsterYaw + Shot.Yaw + Shot.DriftYaw * Alpha, 0.f).Vector();
	FVector Location = Focus + Direction * BodyRadius * Shot.Distance * (1.f - Shot.DriftIn * Alpha);
	// 벽·지형에 가리면 그 앞으로 당긴다.
	if (UWorld* const World = Target->GetWorld())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(knightFinishCamera), false, Target);
		if (const APlayerController* const PC = PlayerController.Get())
		{
			Params.AddIgnoredActor(PC->GetPawn());
		}
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Focus, Location, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(20.f), Params))
		{
			Location = Hit.Location;
		}
	}
	Camera->SetActorLocationAndRotation(Location, (Focus - Location).Rotation());
	Camera->GetCameraComponent()->SetFieldOfView(Shot.FieldOfView);
}

void UknightRaidFinishPresentation::BeginReturn()
{
	SetStage(EknightRaidFinishStage::Return);
	PopSlowSound();
	APlayerController* const PC = PlayerController.Get();
	APawn* const Pawn = PC ? PC->GetPawn() : nullptr;
	if (!PC || !Pawn)
	{
		return;
	}
	// 레터박스는 돌아오기 시작할 때 걷는다(끝에서 갑자기 바뀌지 않게).
	if (Camera)
	{
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	// 돌아온 시점이 몬스터를 보게: 플레이어 카메라는 컨트롤 회전을 따른다.
	const FVector ToMonster = GetBodyCenter() - Pawn->GetActorLocation();
	FRotator Facing = ToMonster.Rotation();
	Facing.Pitch = FMath::Clamp(Facing.Pitch, -20.f, 10.f);
	Facing.Roll = 0.f;
	PC->SetControlRotation(Facing);
	// 블렌드는 게임 시간으로 흐른다. 복귀 동안 배율의 평균만큼 짧게 준다.
	const float BlendGameTime = Settings.ReturnTime * (SlowDilation + 1.f) * 0.5f;
	PC->SetViewTargetWithBlend(Pawn, BlendGameTime, VTBlend_EaseInOut, 2.f);
}

void UknightRaidFinishPresentation::BeginImpact()
{
	SetStage(EknightRaidFinishStage::Impact);
	UWorld* const World = PlayerController.IsValid() ? PlayerController->GetWorld() : nullptr;
	if (World)
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
	}
	if (AknightPlayerCameraManager* const CameraManager = PlayerController.IsValid() ? Cast<AknightPlayerCameraManager>(PlayerController->PlayerCameraManager) : nullptr)
	{
		CameraManager->PlayHeavyImpactShake(Settings.ImpactShake);
	}
	if (const AknightMonsterCharacterBase* const Target = Monster.Get(); Target && World)
	{
		const UCapsuleComponent* const Capsule = Target->GetCapsuleComponent();
		const FVector Feet = Target->GetActorLocation() - FVector(0.f, 0.f, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f);
		const FVector Ground(GetBodyCenter().X, GetBodyCenter().Y, Feet.Z);
		SplashOnWater(Settings.ImpactWaterSplashScale);
		if (UNiagaraSystem* const Effect = Settings.ImpactEffect.LoadSynchronous())
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Effect, Ground, FRotator::ZeroRotator, FVector(Settings.ImpactEffectScale), true, true,
				ENCPoolMethod::AutoRelease);
		}
		if (USoundBase* const Sound = Settings.ImpactSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(World, Sound, Ground);
		}
	}
	RestoreInput();
	if (OnImpact)
	{
		OnImpact();
	}
}

void UknightRaidFinishPresentation::SplashOnWater(float Scale)
{
	const AknightMonsterCharacterBase* const Target = Monster.Get();
	UWorld* const World = Target ? Target->GetWorld() : nullptr;
	UNiagaraSystem* const Splash = Scale > 0.f ? Settings.WaterSplash.LoadSynchronous() : nullptr;
	// 떨어져 닿자마자 쿵이 오면 같은 물보라가 겹친다.
	if (!World || !Splash || (LastWaterSplashTime >= 0.0 && World->GetRealTimeSeconds() - LastWaterSplashTime < 0.5))
	{
		return;
	}
	const FVector Center = GetBodyCenter();
	float SurfaceZ = 0.f;
	const UCapsuleComponent* const Capsule = Target->GetCapsuleComponent();
	const float FeetZ = Target->GetActorLocation().Z - (Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f);
	// 수면이 발보다 한참 아래면(물 위 다리, 높은 지형) 물에 쓰러진 게 아니다.
	if (!UknightWaterRippleSubsystem::FindWaterSurface(World, Center, SurfaceZ) || FeetZ > SurfaceZ + 100.f)
	{
		return;
	}
	const FVector Location(Center.X, Center.Y, SurfaceZ);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Splash, Location, FRotator::ZeroRotator, FVector(Scale), true, true, ENCPoolMethod::AutoRelease);
	if (UNiagaraSystem* const Fall = Settings.WaterSplashFall.LoadSynchronous())
	{
		// 솟구친 물이 쏟아지는 때. 슬로모션이면 같이 늦어지게 게임 시간 타이머로 건다.
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(World, [World, Fall, Location, Scale]
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Fall, Location, FRotator::ZeroRotator, FVector(Scale), true, true, ENCPoolMethod::AutoRelease);
		}), FMath::Max(Settings.WaterSplashFallDelay, 0.01f), false);
	}
	if (UknightWaterRippleSubsystem* const Ripples = World->GetSubsystem<UknightWaterRippleSubsystem>(); Ripples && Settings.WaterRippleDepth > 0.f)
	{
		Ripples->AddImpulse(Location, Settings.WaterRippleRadius * Scale, Settings.WaterRippleDepth);
	}
	LastWaterSplashTime = World->GetRealTimeSeconds();
	UE_LOG(Logknight, Log, TEXT("Finish presentation: %s water splash x%.2f (stage %s)."), *Target->GetName(), Scale, *UEnum::GetValueAsString(Stage));
}

FVector UknightRaidFinishPresentation::GetBodyCenter() const
{
	const AknightMonsterCharacterBase* const Target = Monster.Get();
	const USkeletalMeshComponent* const Mesh = Target ? Target->GetMesh() : nullptr;
	return Mesh ? FVector(Mesh->Bounds.Origin) : (Target ? Target->GetActorLocation() : Focus);
}

bool UknightRaidFinishPresentation::HasLanded() const
{
	const AknightMonsterCharacterBase* const Target = Monster.Get();
	if (!Target)
	{
		return true;
	}
	const UknightMonsterHoverComponent* const Hover = Target->GetHoverComponent();
	return !Target->IsFallingToDeath() && !(Hover && Hover->IsFallingToGround());
}

void UknightRaidFinishPresentation::PushSlowSound()
{
	USoundClass* const SFX = LoadObject<USoundClass>(nullptr, knightRaidFinish::SFXClassPath);
	if (!SFX || bSoundPushed)
	{
		return;
	}
	if (!SlowSoundMix)
	{
		SlowSoundMix = NewObject<USoundMix>(this, TEXT("FinishSlowMix"));
		SlowSoundMix->FadeInTime = 0.f;
		SlowSoundMix->FadeOutTime = 0.3f;
	}
	UGameplayStatics::SetSoundMixClassOverride(this, SlowSoundMix, SFX, Settings.SlowSFXVolume, Settings.SlowSFXPitch, 0.15f, true);
	UGameplayStatics::PushSoundMixModifier(this, SlowSoundMix);
	bSoundPushed = true;
}

void UknightRaidFinishPresentation::PopSlowSound()
{
	if (bSoundPushed && SlowSoundMix)
	{
		UGameplayStatics::PopSoundMixModifier(this, SlowSoundMix);
	}
	bSoundPushed = false;
}

void UknightRaidFinishPresentation::RestoreInput()
{
	if (bInputIgnored && PlayerController.IsValid())
	{
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->SetIgnoreLookInput(false);
	}
	bInputIgnored = false;
}

void UknightRaidFinishPresentation::Stop()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	APlayerController* const PC = PlayerController.Get();
	if (UWorld* const World = PC ? PC->GetWorld() : nullptr)
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
	}
	if (PC && Camera && PC->GetViewTarget() == Camera && PC->GetPawn())
	{
		PC->SetViewTarget(PC->GetPawn());
	}
	PopSlowSound();
	RestoreInput();
	if (Camera)
	{
		Camera->Destroy();
		Camera = nullptr;
	}
}
