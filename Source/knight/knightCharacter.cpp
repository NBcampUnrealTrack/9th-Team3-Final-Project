// Copyright Epic Games, Inc. All Rights Reserved.

#include "knightCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Camera/knightCameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "AbilitySystem/Attributes/knightAttributeSet.h"
#include "AbilitySystem/Attributes/knightCoreAttributeSet.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayEffect.h"
#include "Character/knightMovementComponent.h"
#include "Combat/knightGroggyFeedbackComponent.h"
#include "Combat/knightHitReactionComponent.h"
#include "Combat/knightHitStopComponent.h"
#include "Combat/knightMeleeTraceComponent.h"
#include "Combat/knightTargetLockComponent.h"
#include "MotionWarpingComponent.h"
#include "Input/knightInputBufferComponent.h"
#include "Item/knightDamageVisualComponent.h"
#include "Item/knightPartsComponent.h"
#include "Character/knightPartHealthComponent.h"
#include "Input/knightInputComponent.h"
#include "Settings/knightSettingsSubsystem.h"
#include "Settings/knightSharedSettings.h"
#include "Progression/knightProgressionSubsystem.h"
#include "System/knightGameplayTags.h"
#include "System/knightTeams.h"
#include "knightGameMode.h"
#include "knight.h"

AknightCharacter::AknightCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UknightMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// 충돌 캡슐 크기
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// 컨트롤러가 돌아도 캐릭터는 돌지 않는다. 컨트롤러 회전은 카메라에만 쓴다.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 이동 수치는 UknightMovementComponent에 있고, 메카별로 블루프린트에서 덮어쓴다.

	// 카메라 붐 (충돌하면 플레이어 쪽으로 당겨진다)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// 따라오는 카메라
	// 카메라 모드로 움직이는 카메라. DefaultCameraMode를 지정하기 전에는 붐 위의 템플릿 카메라처럼 동작한다.
	FollowCamera = CreateDefaultSubobject<UknightCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	AbilitySystemComponent = CreateDefaultSubobject<UknightAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// ASC 소유자의 하위 오브젝트로 만든 속성 세트는 ASC가 자동으로 등록한다.
	CoreAttributeSet = CreateDefaultSubobject<UknightCoreAttributeSet>(TEXT("CoreAttributeSet"));
	MechAttributeSet = CreateDefaultSubobject<UknightAttributeSet>(TEXT("MechAttributeSet"));

	InputBuffer = CreateDefaultSubobject<UknightInputBufferComponent>(TEXT("InputBuffer"));
	MeleeTrace = CreateDefaultSubobject<UknightMeleeTraceComponent>(TEXT("MeleeTrace"));
	HitStop = CreateDefaultSubobject<UknightHitStopComponent>(TEXT("HitStop"));
	HitReaction = CreateDefaultSubobject<UknightHitReactionComponent>(TEXT("HitReaction"));
	GroggyFeedback = CreateDefaultSubobject<UknightGroggyFeedbackComponent>(TEXT("GroggyFeedback"));
	TargetLock = CreateDefaultSubobject<UknightTargetLockComponent>(TEXT("TargetLock"));
	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));
	Parts = CreateDefaultSubobject<UknightPartsComponent>(TEXT("Parts"));
	DamageVisual = CreateDefaultSubobject<UknightDamageVisualComponent>(TEXT("DamageVisual"));
	PartHealth = CreateDefaultSubobject<UknightPartHealthComponent>(TEXT("PartHealth"));

	// 참고: Mesh 컴포넌트(Character에서 상속)의 스켈레탈 메시와 애님 블루프린트는
	// 파생 블루프린트 에셋(BP_Knight)에서 지정한다 (C++에서 콘텐츠를 직접 참조하지 않기 위해)
}

UAbilitySystemComponent* AknightCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UknightMovementComponent* AknightCharacter::GetKnightMovementComponent() const
{
	return Cast<UknightMovementComponent>(GetCharacterMovement());
}

bool AknightCharacter::CanReceiveHit(const AActor* Attacker) const
{
	return !IsDead() && Attacker != this;
}

bool AknightCharacter::IsDead() const
{
	return bIsDying || (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(knightGameplayTags::State_Dead));
}

FGenericTeamId AknightCharacter::GetGenericTeamId() const
{
	return knightTeams::ToTeamId(EknightTeam::Player);
}

void AknightCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (UGameInstance* const GI = GetGameInstance())
		if (UknightProgressionSubsystem* const Progression = GI->GetSubsystem<UknightProgressionSubsystem>())
			Progression->OnProgressionChanged.AddUniqueDynamic(this, &ThisClass::HandleSocketProgressionChanged);

	CoreAttributeSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);

	// 바닥 데칼(잔열 자국 등)이 위에 선 몸과 무기에 덧씌워지지 않게 한다.
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* const Primitive : Primitives)
	{
		Primitive->SetReceivesDecals(false);
	}
}

void AknightCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* const GI = GetGameInstance())
		if (UknightProgressionSubsystem* const Progression = GI->GetSubsystem<UknightProgressionSubsystem>())
			Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleSocketProgressionChanged);
	if (AbilitySystemComponent && SocketRepairCapacityHandle.IsValid())
		AbilitySystemComponent->RemoveActiveGameplayEffect(SocketRepairCapacityHandle);
	SocketRepairCapacityHandle.Invalidate();
	SocketRepairCapacityEffect = nullptr;
	for (FActiveGameplayEffectHandle* const Handle : { &SocketStatHandle, &SocketFullHealthAttackHandle })
	{
		if (AbilitySystemComponent && Handle->IsValid())
			AbilitySystemComponent->RemoveActiveGameplayEffect(*Handle);
		Handle->Invalidate();
	}
	SocketStatEffect = nullptr;
	SocketFullHealthAttackEffect = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AknightCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystem();
}

void AknightCharacter::InitializeAbilitySystem()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	if (bAbilitySystemInitialized)
	{
		return;
	}
	bAbilitySystemInitialized = true;

	AbilitySystemComponent->SetTagRelationshipMapping(TagRelationshipMapping);
	for (const UknightAbilitySet* const AbilitySet : DefaultAbilitySets)
	{
		if (AbilitySet)
		{
			AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &DefaultAbilityHandles, this);
		}
	}

	// (2026-10-20) 기본 메카 스탯(속성 세트 기본값과 다른 것). 파츠 능력치는 이 위에 더해진다.
	AbilitySystemComponent->SetNumericAttributeBase(UknightCoreAttributeSet::GetPoiseRecoveryRateAttribute(), BasePoiseRecoveryRate);
	AbilitySystemComponent->SetNumericAttributeBase(UknightCoreAttributeSet::GetGuardEfficiencyAttribute(), BaseGuardEfficiency);
	AbilitySystemComponent->SetNumericAttributeBase(UknightCoreAttributeSet::GetCriticalDamageMultiplierAttribute(), BaseCriticalDamageMultiplier);

	// (2026-10-22) 완전 출력 소켓: 체력·최대 체력이 바뀔 때마다 '가득 찼는지'를 다시 본다(아래 출격 시 가득 채우기도 여기로 들어온다).
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthChangedForSocket);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UknightCoreAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthChangedForSocket);

	// 파츠는 ASC를 통해 스탯을 주므로, ASC가 준비된 뒤에 장착한다.
	Parts->ApplyLoadout();
	RefreshSocketEffects();

	// 부위 체력은 파츠가 부위 최대 체력을 정한 뒤에 연결해서 MaxHealth 합산이 맞게 한다.
	PartHealth->InitializeWithAbilitySystem(AbilitySystemComponent);

	// 출격 = 이 캐릭터가 생겨 파츠를 모두 입은 직후 (이 함수는 캐릭터마다 한 번만 끝까지 온다). (2026-10-07)
	// 파츠로 오른 최대치에 맞춰 체력·그로기를 가득 채운다. 최대치가 바뀔 때 현재 체력·그로기를 따라 올리지 않는 것은
	// 속성 세트의 규칙(전투 중 파츠 교체로 회복 방지)이라 그대로 두고, 출격 시점에만 채운다.
	AbilitySystemComponent->SetNumericAttributeBase(UknightCoreAttributeSet::GetHealthAttribute(), CoreAttributeSet->GetMaxHealth());
	AbilitySystemComponent->SetNumericAttributeBase(UknightCoreAttributeSet::GetPoiseAttribute(), CoreAttributeSet->GetMaxPoise());
	// New raid pawn starts with its equipped repair capacity; a later socket toggle never refills it.
	if (AppliedSocketRepairBonus > 0)
		AbilitySystemComponent->SetNumericAttributeBase(UknightAttributeSet::GetRepairKitCountAttribute(), MechAttributeSet->GetMaxRepairKitCount());

	// 수리 키트 게이지: 퍼펙트 가드 성공 이벤트(knightCombatLibrary::ApplyMeleeHit이 막은 쪽에 보냄)를 받는다. (2026-10-16)
	static const FGameplayTag ParrySucceededTag = FGameplayTag::RequestGameplayTag(TEXT("Event.ParrySucceeded"), false);
	if (ParrySucceededTag.IsValid())
	{
		AbilitySystemComponent->GenericGameplayEventCallbacks.FindOrAdd(ParrySucceededTag).AddUObject(this, &ThisClass::HandlePerfectGuard);
	}
}

void AknightCharacter::HandleSocketProgressionChanged()
{
	RefreshSocketEffects();
}

void AknightCharacter::RefreshSocketEffects()
{
	if (!bAbilitySystemInitialized || !AbilitySystemComponent || !MechAttributeSet) return;
	const UknightProgressionSubsystem* const Progression = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>() : nullptr;
	const int32 Bonus = Progression ? Progression->GetRepairKitCapacityBonus() : 0;
	// (2026-10-22) 수리 키트 보너스가 그대로여도 아래 비율 소켓은 따로 갱신해야 해서, 바로 끝내지 않고 이 블록만 건너뛴다.
	if (Bonus != AppliedSocketRepairBonus)
	{
		if (SocketRepairCapacityHandle.IsValid())
			AbilitySystemComponent->RemoveActiveGameplayEffect(SocketRepairCapacityHandle);
		SocketRepairCapacityHandle.Invalidate();
		SocketRepairCapacityEffect = nullptr;
		AppliedSocketRepairBonus = Bonus;
		if (Bonus > 0)
		{
			SocketRepairCapacityEffect = NewObject<UGameplayEffect>(this, NAME_None, RF_Transient);
			SocketRepairCapacityEffect->DurationPolicy = EGameplayEffectDurationType::Infinite;
			FGameplayModifierInfo& Modifier = SocketRepairCapacityEffect->Modifiers.AddDefaulted_GetRef();
			Modifier.Attribute = UknightAttributeSet::GetMaxRepairKitCountAttribute();
			Modifier.ModifierOp = EGameplayModOp::AddBase;
			Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Bonus));
			SocketRepairCapacityHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(
				SocketRepairCapacityEffect, 1.f, AbilitySystemComponent->MakeEffectContext());
		}
		if (MechAttributeSet->GetRepairKitCount() > MechAttributeSet->GetMaxRepairKitCount())
			AbilitySystemComponent->SetNumericAttributeBase(UknightAttributeSet::GetRepairKitCountAttribute(), MechAttributeSet->GetMaxRepairKitCount());
	}
	RefreshSocketStatEffects(Progression);
}

void AknightCharacter::RefreshSocketStatEffects(const UknightProgressionSubsystem* Progression)
{
	// 적재 보강(LoadCapacity)·냉각 순환(StaminaRecovery): 최종 값 x(1 + 끼운 값 합). MultiplyAdditive는 크기 1.2가 x1.2다.
	const float LoadBonus = Progression ? Progression->GetSocketEffectTotal(EKnightSocketEffect::LoadCapacity) : 0.f;
	const float StaminaBonus = Progression ? Progression->GetSocketEffectTotal(EKnightSocketEffect::StaminaRecovery) : 0.f;
	if (LoadBonus != AppliedSocketLoadBonus || StaminaBonus != AppliedSocketStaminaBonus)
	{
		if (SocketStatHandle.IsValid())
			AbilitySystemComponent->RemoveActiveGameplayEffect(SocketStatHandle);
		SocketStatHandle.Invalidate();
		SocketStatEffect = nullptr;
		AppliedSocketLoadBonus = LoadBonus;
		AppliedSocketStaminaBonus = StaminaBonus;
		if (LoadBonus > 0.f || StaminaBonus > 0.f)
		{
			SocketStatEffect = NewObject<UGameplayEffect>(this, NAME_None, RF_Transient);
			SocketStatEffect->DurationPolicy = EGameplayEffectDurationType::Infinite;
			const auto AddMultiplier = [this](const FGameplayAttribute& Attribute, float SocketBonus)
			{
				if (SocketBonus <= 0.f) return;
				FGameplayModifierInfo& Modifier = SocketStatEffect->Modifiers.AddDefaulted_GetRef();
				Modifier.Attribute = Attribute;
				Modifier.ModifierOp = EGameplayModOp::MultiplyAdditive;
				Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.f + SocketBonus));
			};
			AddMultiplier(UknightAttributeSet::GetLoadCapacityAttribute(), LoadBonus);
			AddMultiplier(UknightAttributeSet::GetBoostRecoveryRateAttribute(), StaminaBonus);
			SocketStatHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(SocketStatEffect, 1.f, AbilitySystemComponent->MakeEffectContext());
		}
		UE_LOG(Logknight, Log, TEXT("%s: socket stats -> load capacity x%.2f (%.0f), stamina recovery x%.2f (%.1f/s)."), *GetNameSafe(this),
			1.f + LoadBonus, MechAttributeSet->GetLoadCapacity(), 1.f + StaminaBonus, MechAttributeSet->GetBoostRecoveryRate());
	}

	// 완전 출력(FullHealthAttack): 값만 기억해 두고, 실제로 거는 건 체력에 따라 UpdateFullHealthAttackBonus가 한다.
	SocketFullHealthAttackBonus = Progression ? Progression->GetSocketEffectTotal(EKnightSocketEffect::FullHealthAttack) : 0.f;
	UpdateFullHealthAttackBonus();
}

void AknightCharacter::HandleHealthChangedForSocket(const FOnAttributeChangeData& Data)
{
	UpdateFullHealthAttackBonus();
}

void AknightCharacter::UpdateFullHealthAttackBonus()
{
	if (!AbilitySystemComponent || !CoreAttributeSet) return;
	// 현재 체력이 최대 체력과 같을 때만(부동소수 오차 허용). 한 대라도 맞으면 꺼지고, 수리 키트로 가득 차면 다시 켜진다.
	const float MaxHealth = CoreAttributeSet->GetMaxHealth();
	const bool bWant = SocketFullHealthAttackBonus > 0.f && !IsDead() && MaxHealth > 0.f
		&& CoreAttributeSet->GetHealth() >= MaxHealth - KINDA_SMALL_NUMBER;
	const bool bApplied = SocketFullHealthAttackHandle.IsValid();
	if (bWant == bApplied && (!bWant || AppliedFullHealthAttackBonus == SocketFullHealthAttackBonus)) return;

	if (bApplied)
		AbilitySystemComponent->RemoveActiveGameplayEffect(SocketFullHealthAttackHandle);
	SocketFullHealthAttackHandle.Invalidate();
	AppliedFullHealthAttackBonus = 0.f;
	if (bWant)
	{
		SocketFullHealthAttackEffect = NewObject<UGameplayEffect>(this, NAME_None, RF_Transient);
		SocketFullHealthAttackEffect->DurationPolicy = EGameplayEffectDurationType::Infinite;
		FGameplayModifierInfo& Modifier = SocketFullHealthAttackEffect->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = UknightCoreAttributeSet::GetAttackPowerAttribute();
		Modifier.ModifierOp = EGameplayModOp::MultiplyAdditive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.f + SocketFullHealthAttackBonus));
		SocketFullHealthAttackHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(
			SocketFullHealthAttackEffect, 1.f, AbilitySystemComponent->MakeEffectContext());
		AppliedFullHealthAttackBonus = SocketFullHealthAttackBonus;
	}
	UE_LOG(Logknight, Log, TEXT("%s: full health attack socket %s (health %.1f / %.1f, attack power %.1f)."), *GetNameSafe(this),
		bWant ? TEXT("on") : TEXT("off"), CoreAttributeSet->GetHealth(), MaxHealth, CoreAttributeSet->GetAttackPower());
}

void AknightCharacter::HandlePerfectGuard(const FGameplayEventData* Payload)
{
	const float KitCount = MechAttributeSet->GetRepairKitCount();
	const float MaxKitCount = MechAttributeSet->GetMaxRepairKitCount();
	// 키트가 가득이면 게이지가 오르지 않는다(사용자 결정).
	if (IsDead() || RepairKitGaugePerPerfectGuard <= 0.f || KitCount >= MaxKitCount)
	{
		return;
	}
	// (2026-10-22 사용자 결정) 충전 보조 소켓(SocketGaugeCharge) 1개마다 키트 하나에 필요한 퍼펙트 가드 수 -1 (기본 5번 → 4번).
	// 소켓의 EffectValue(배율)는 쓰지 않고 끼운 개수만 센다. 최소 1번.
	const UknightProgressionSubsystem* const Progression = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UknightProgressionSubsystem>() : nullptr;
	const int32 ChargeSockets = Progression ? Progression->GetSocketEffectCount(EKnightSocketEffect::SocketGaugeCharge) : 0;
	const int32 BaseGuardsPerKit = FMath::CeilToInt(UknightAttributeSet::RepairKitGaugeFull / RepairKitGaugePerPerfectGuard);
	const int32 GuardsPerKit = FMath::Max(1, BaseGuardsPerKit - ChargeSockets);
	float Gauge = MechAttributeSet->GetRepairKitGauge() + UknightAttributeSet::RepairKitGaugeFull / GuardsPerKit;
	float NewKitCount = KitCount;
	if (Gauge >= UknightAttributeSet::RepairKitGaugeFull)
	{
		// 꽉 차면 키트 +1, 넘친 만큼은 다음 게이지로. 그래서 최대 개수가 되면 0부터.
		NewKitCount = FMath::Min(KitCount + 1.f, MaxKitCount);
		Gauge = NewKitCount >= MaxKitCount ? 0.f : Gauge - UknightAttributeSet::RepairKitGaugeFull;
		AbilitySystemComponent->SetNumericAttributeBase(UknightAttributeSet::GetRepairKitCountAttribute(), NewKitCount);
	}
	AbilitySystemComponent->SetNumericAttributeBase(UknightAttributeSet::GetRepairKitGaugeAttribute(), Gauge);
	UE_LOG(Logknight, Log, TEXT("%s: perfect guard -> repair kit gauge %.0f / %.0f (%d guards per kit), kits %.0f / %.0f."), *GetNameSafe(this),
		Gauge, UknightAttributeSet::RepairKitGaugeFull, GuardsPerKit, NewKitCount, MaxKitCount);
}

FGameplayTag AknightCharacter::ResolveHitPart(const FHitResult& Hit) const
{
	return PartHealth->ResolveHitPartTag(Hit);
}

float AknightCharacter::GetPartDamageMultiplier(const FGameplayTag& PartTag) const
{
	return PartHealth->GetDamageMultiplier(PartTag);
}

void AknightCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (InputConfig)
	{
		UknightInputComponent* const KnightInput = Cast<UknightInputComponent>(PlayerInputComponent);
		if (!KnightInput)
		{
			UE_LOG(Logknight, Error, TEXT("'%s' has an InputConfig but the input component is not knightInputComponent. Check DefaultInputComponentClass in DefaultInput.ini."), *GetNameSafe(this));
			return;
		}
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Move);
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_Move_Vertical, ETriggerEvent::Triggered, this, &ThisClass::MoveVertical, false);
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ThisClass::Look);
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_Look_Mouse, ETriggerEvent::Triggered, this, &ThisClass::LookMouse, false);
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_LockOn, ETriggerEvent::Started, this, &ThisClass::Input_LockOn, false);
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_LockOn_SwitchLeft, ETriggerEvent::Started, this, &ThisClass::Input_LockOnSwitchLeft, false);
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_LockOn_SwitchRight, ETriggerEvent::Started, this, &ThisClass::Input_LockOnSwitchRight, false);

		// 달리기. IA_Run(LeftShift) → InputTag.Run (DA_Knight_InputConfig). 못 찾아도 경고를 남기지 않는다(마지막 인자 false).
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_Run, ETriggerEvent::Started, this, &ThisClass::Input_RunPressed, false);
		KnightInput->BindNativeAction(InputConfig, knightGameplayTags::InputTag_Run, ETriggerEvent::Completed, this, &ThisClass::Input_RunReleased, false);

		TArray<uint32> BindHandles;
		KnightInput->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, BindHandles);
		return;
	}

	// 템플릿 입력 연결 (InputConfig가 없을 때)
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		// 점프
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// 이동
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AknightCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AknightCharacter::LookMouse);

		// 시점
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AknightCharacter::Look);
	}
	else
	{
		UE_LOG(Logknight, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AknightCharacter::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	AbilitySystemComponent->AbilityInputTagPressed(InputTag);
	InputBuffer->RecordInput(InputTag);
}

void AknightCharacter::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	AbilitySystemComponent->AbilityInputTagReleased(InputTag);
}

void AknightCharacter::Input_LockOn()
{
	if (!IsDead())
	{
		// (2026-10-15) 락온 중에 누르면 가장 가까운 부위로 다시 잡고, 같은 부위·두 번 누르기면 푼다.
		TargetLock->PressLockOn();
	}
}

void AknightCharacter::Input_LockOnSwitchLeft()
{
	TargetLock->SwitchTarget(false);
}

void AknightCharacter::Input_LockOnSwitchRight()
{
	TargetLock->SwitchTarget(true);
}

void AknightCharacter::Move(const FInputActionValue& Value)
{
	// 입력은 Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// 입력 전달
	DoMove(MovementVector.X, MovementVector.Y);
}

void AknightCharacter::MoveVertical(const FInputActionValue& Value)
{
	if (!IsDead())
	{
		if (UknightMovementComponent* const KnightMovement = GetKnightMovementComponent())
		{
			KnightMovement->AddFlightVerticalInput(Value.Get<float>());
		}
	}
}

void AknightCharacter::Look(const FInputActionValue& Value)
{
	ApplyLookInput(Value.Get<FVector2D>(), false);
}

void AknightCharacter::LookMouse(const FInputActionValue& Value)
{
	ApplyLookInput(Value.Get<FVector2D>(), true);
}

void AknightCharacter::ApplyLookInput(FVector2D LookAxisVector, bool bFromMouse)
{
	// 패드의 락온 대상 전환은 감도와 상관없이 스틱 기울기로 본다(Y축 반전만 따름). (2026-10-14)
	FVector2D StickTilt = LookAxisVector;

	// 개인 설정: 장치별 감도와 Y축 반전.
	const APlayerController* const PC = Cast<APlayerController>(GetController());
	const ULocalPlayer* const LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UknightSettingsSubsystem* const Settings = LocalPlayer ? LocalPlayer->GetSubsystem<UknightSettingsSubsystem>() : nullptr;
	if (const UknightSharedSettings* const Shared = Settings ? Settings->GetSharedSettings() : nullptr)
	{
		if (bFromMouse && Shared->bMouseAcceleration)
		{
			const float Seconds = FMath::Max(GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f, 0.001f);
			const float Speed = LookAxisVector.Size() / Seconds;
			LookAxisVector *= 1.f + 0.75f * FMath::Clamp(Speed / 900.f, 0.f, 1.f);
		}
		LookAxisVector *= bFromMouse ? Shared->MouseSensitivity : Shared->GamepadSensitivity;
		if (Shared->bInvertLookY)
		{
			LookAxisVector.Y = -LookAxisVector.Y;
			StickTilt.Y = -StickTilt.Y;
		}
	}

	// 락온 중에는 시점 입력을 휙 움직인 방향의 다른 대상(부위)으로 바꾸는 데 쓴다(다크소울 방식, knightTargetLockComponent).
	// 락온 직전부터 움직이던 입력을 거르려고 락온이 아닐 때도 넘긴다.
	TargetLock->AddSwitchLookInput(bFromMouse ? LookAxisVector : StickTilt, !bFromMouse);

	// 입력 전달
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AknightCharacter::Input_RunPressed()
{
	if (!IsDead())
	{
		SetRunning(true);
	}
}

void AknightCharacter::Input_RunReleased()
{
	SetRunning(false);
}

void AknightCharacter::SetRunning(bool bRun)
{
	if (UknightMovementComponent* const KnightMovement = GetKnightMovementComponent())
	{
		KnightMovement->SetWantsToRun(bRun);
	}
	// 예전 Sprint 어빌리티가 붙이던 태그. 달리기 공격(GA_Knight_RunAttack)의 발동 조건이라 같이 맞춘다.
	AbilitySystemComponent->SetLooseGameplayTagCount(knightGameplayTags::State_Sprinting, bRun ? 1 : 0);
}

void AknightCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr && !IsDead())
	{
		// 어느 쪽이 앞인지 구한다
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// 앞 방향 벡터
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// 오른쪽 방향 벡터
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// 이동 입력 추가
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AknightCharacter::DoLook(float Yaw, float Pitch)
{
	// 락온 중에는 락온이 시점을 정한다. 대상 전환은 전용 입력(휠)과 시점 입력(ApplyLookInput → AddSwitchLookInput)으로 한다.
	if (GetController() != nullptr && !TargetLock->IsLockedOn())
	{
		// 컨트롤러에 좌우·상하 회전 입력 추가
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AknightCharacter::DoJumpStart()
{
	// 캐릭터에 점프를 알린다
	Jump();
}

void AknightCharacter::DoJumpEnd()
{
	// 캐릭터에 점프 그만을 알린다
	StopJumping();
}

void AknightCharacter::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec,
	float DamageMagnitude, float OldValue, float NewValue)
{
	if (bIsDying)
	{
		return;
	}
	bIsDying = true;

	AbilitySystemComponent->AddLooseGameplayTag(knightGameplayTags::State_Dead);
	AbilitySystemComponent->AddLooseGameplayTag(knightGameplayTags::State_InputBlocked);
	AbilitySystemComponent->ClearAbilityInput();
	InputBuffer->ClearBuffer();
	TargetLock->ClearLockOn();
	GetCharacterMovement()->StopMovementImmediately();

	// Event.Death로 발동하는 사망 어빌리티가 연출을 맡는다. 나머지 어빌리티는 모두 취소한다.
	FGameplayEventData Payload;
	Payload.EventTag = knightGameplayTags::Event_Death;
	Payload.Instigator = DamageInstigator;
	Payload.Target = this;
	Payload.EventMagnitude = DamageMagnitude;
	if (DamageEffectSpec)
	{
		Payload.ContextHandle = DamageEffectSpec->GetEffectContext();
	}
	AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);

	const FGameplayTagContainer KeepAbilities(knightGameplayTags::Ability_Death);
	AbilitySystemComponent->CancelAbilities(nullptr, &KeepAbilities);

	UE_LOG(Logknight, Log, TEXT("%s was destroyed by %s."), *GetNameSafe(this), *GetNameSafe(DamageInstigator));
	OnDeathStarted.Broadcast(this);

	if (AknightGameMode* const GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AknightGameMode>() : nullptr)
	{
		GameMode->HandlePlayerDeath(GetController());
	}
}
