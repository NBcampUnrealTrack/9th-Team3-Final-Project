#include "knightMonsterFXBuilder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "Stateless/Modules/NiagaraStatelessModule_AccelerationForce.h"
#include "Stateless/Modules/NiagaraStatelessModule_AddVelocity.h"
#include "Stateless/Modules/NiagaraStatelessModule_Drag.h"
#include "Stateless/Modules/NiagaraStatelessModule_InitializeParticle.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleColor.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleSpriteSize.h"
#include "Stateless/Modules/NiagaraStatelessModule_ShapeLocation.h"
#include "Stateless/Modules/NiagaraStatelessModule_SpriteRotationRate.h"
#include "Stateless/NiagaraStatelessEmitter.h"

namespace knightMonsterFX
{
	/** 경로에 이미 있으면 그 시스템을, 없으면 경량 템플릿 복제본을 돌려준다. */
	UNiagaraSystem* LoadOrDuplicateTemplate(const FString& PackagePath, bool& bOutCreated)
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
		if (UNiagaraSystem* const Existing = LoadObject<UNiagaraSystem>(nullptr, *(PackagePath + TEXT(".") + AssetName)))
		{
			bOutCreated = false;
			return Existing;
		}
		UNiagaraSystem* const Template = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Niagara/DefaultAssets/Templates/Systems/FountainLightweight.FountainLightweight"));
		if (!Template)
		{
			return nullptr;
		}
		UNiagaraSystem* const System = Cast<UNiagaraSystem>(StaticDuplicateObject(Template, CreatePackage(*PackagePath), *AssetName));
		if (System)
		{
			System->SetFlags(RF_Public | RF_Standalone);
			bOutCreated = true;
		}
		return System;
	}

	/** 템플릿의 모듈 중 Enabled에 있는 것만 켠다. */
	void EnableOnly(UNiagaraStatelessEmitter& Emitter, std::initializer_list<UClass*> Enabled)
	{
		for (UNiagaraStatelessModule* const Module : Emitter.GetModules())
		{
			if (!Module)
			{
				continue;
			}
			bool bOn = Module->GetClass()->GetName().Contains(TEXT("SolveVelocitiesAndForces"));
			for (UClass* const Class : Enabled)
			{
				bOn |= Module->IsA(Class);
			}
			Module->SetIsModuleEnabled(bOn);
		}
	}

	/** 이미터의 보호된 UPROPERTY(FixedBounds, EmitterState)를 리플렉션으로 꺼낸다. */
	template <typename ValueType>
	ValueType* GetProtected(UNiagaraStatelessEmitter& Emitter, const TCHAR* Name)
	{
		const FStructProperty* const Property = FindFProperty<FStructProperty>(Emitter.GetClass(), Name);
		return Property && Property->GetElementSize() == sizeof(ValueType) ? Property->ContainerPtrToValuePtr<ValueType>(&Emitter) : nullptr;
	}

	template <typename ModuleType>
	ModuleType* Get(UNiagaraStatelessEmitter& Emitter)
	{
		return Cast<ModuleType>(Emitter.GetModule(ModuleType::StaticClass()));
	}

	/** 수명 비율(0..1)에 따른 곡선. Keys는 (시간, 값) 쌍. */
	void SetCurve(FRichCurve& Curve, std::initializer_list<TPair<float, float>> Keys)
	{
		Curve.Reset();
		for (const TPair<float, float>& Key : Keys)
		{
			Curve.AddKey(Key.Key, Key.Value);
		}
	}

	/** 정사각형 스프라이트 크기를 Min..Max 사이에서 고른다. */
	void SetUniformRange(FNiagaraDistributionRangeVector2& Distribution, float Min, float Max)
	{
		Distribution.Mode = ENiagaraDistributionMode::UniformRange;
		Distribution.Min = FVector2f(Min);
		Distribution.Max = FVector2f(Max);
		Distribution.ChannelConstantsAndRanges = { Min, Max };
	}

	void SetSprite(UNiagaraStatelessEmitter& Emitter, UMaterialInterface* Material, ENiagaraSpriteAlignment Alignment = ENiagaraSpriteAlignment::Unaligned)
	{
		for (UNiagaraRendererProperties* const Renderer : Emitter.GetRenderers())
		{
			if (UNiagaraSpriteRendererProperties* const Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
			{
				Sprite->Material = Material;
				Sprite->Alignment = Alignment;
				Sprite->FacingMode = ENiagaraSpriteFacingMode::FaceCamera;
				Sprite->SortMode = ENiagaraSortMode::ViewDepth;
			}
		}
	}

	UNiagaraSystem* Finish(UNiagaraSystem* System, bool bCreated)
	{
		System->PostEditChange();
		System->RequestCompile(false);
		System->WaitForCompilationComplete();
		System->MarkPackageDirty();
		if (bCreated)
		{
			FAssetRegistryModule::AssetCreated(System);
		}
		return System;
	}
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildBreathSystem(const FString& PackagePath, UMaterialInterface* Material, float Reach, float OwnerScale)
{
	using namespace knightMonsterFX;
	bool bCreated = false;
	UNiagaraSystem* const System = Material ? LoadOrDuplicateTemplate(PackagePath, bCreated) : nullptr;
	if (!System)
	{
		return nullptr;
	}

	// 위치와 속도는 메시 스케일을 받으므로 로컬 단위로 나눠 넣는다.
	const float ToLocal = 1.f / FMath::Max(OwnerScale, KINDA_SMALL_NUMBER);
	const float Lifetime = 1.f;
	const float Speed = Reach / Lifetime * ToLocal;

	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		UNiagaraStatelessEmitter* const Emitter = Handle.GetStatelessEmitter();
		if (!Emitter)
		{
			continue;
		}
		EnableOnly(*Emitter, { UNiagaraStatelessModule_InitializeParticle::StaticClass(), UNiagaraStatelessModule_ShapeLocation::StaticClass(),
			UNiagaraStatelessModule_AddVelocity::StaticClass(), UNiagaraStatelessModule_AccelerationForce::StaticClass(),
			UNiagaraStatelessModule_SpriteRotationRate::StaticClass(), UNiagaraStatelessModule_ScaleColor::StaticClass(),
			UNiagaraStatelessModule_ScaleSpriteSize::StaticClass() });

		for (int32 Index = 0; Index < Emitter->GetNumSpawnInfos(); ++Index)
		{
			Emitter->GetSpawnInfoByIndex(Index)->bEnabled = false;
		}
		FNiagaraStatelessSpawnInfo& Spawn = *Emitter->GetSpawnInfoByIndex(0);
		Spawn.bEnabled = true;
		Spawn.Type = ENiagaraStatelessSpawnInfoType::Rate;
		Spawn.Rate.InitConstant(160.f);

		if (UNiagaraStatelessModule_InitializeParticle* const Init = Get<UNiagaraStatelessModule_InitializeParticle>(*Emitter))
		{
			Init->LifetimeDistribution.InitRange(Lifetime * 0.8f, Lifetime);
			SetUniformRange(Init->SpriteSizeDistribution, 150.f, 250.f);
			Init->SpriteRotationDistribution.InitRange(-180.f, 180.f);
			Init->ColorDistribution = FNiagaraDistributionColor(FLinearColor::White);
		}
		if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
		{
			Shape->ShapePrimitive = ENSM_ShapePrimitive::Sphere;
			Shape->SphereRadius.InitConstant(40.f * ToLocal);
		}
		if (UNiagaraStatelessModule_AddVelocity* const Velocity = Get<UNiagaraStatelessModule_AddVelocity>(*Emitter))
		{
			// 로컬 X로 나가는 직선 속도에 옆 퍼짐을 범위로 준다.
			const FVector3f Min(Speed * 0.8f, -Speed * 0.12f, -Speed * 0.12f);
			const FVector3f Max(Speed, Speed * 0.12f, Speed * 0.12f);
			Velocity->VelocityType = ENSM_VelocityType::Linear;
			Velocity->CoordinateSpace = ENiagaraCoordinateSpace::Local;
			Velocity->LinearVelocityDistribution.Mode = ENiagaraDistributionMode::NonUniformRange;
			Velocity->LinearVelocityDistribution.Min = Min;
			Velocity->LinearVelocityDistribution.Max = Max;
			Velocity->LinearVelocityDistribution.ChannelConstantsAndRanges = { Min.X, Min.Y, Min.Z, Max.X, Max.Y, Max.Z };
			Velocity->LinearVelocityScale.InitConstant(1.f);
		}
		// 뜨거운 불꽃은 끝으로 갈수록 위로 말려 올라간다. 월드 방향 힘도 메시 스케일을 받으므로 로컬 단위로 나눈다.
		if (UNiagaraStatelessModule_AccelerationForce* const Rise = Get<UNiagaraStatelessModule_AccelerationForce>(*Emitter))
		{
			Rise->CoordinateSpace = ENiagaraCoordinateSpace::World;
			Rise->AccelerationDistribution.InitConstant(FVector3f(0.f, 0.f, 600.f * ToLocal));
		}
		if (UNiagaraStatelessModule_SpriteRotationRate* const Spin = Get<UNiagaraStatelessModule_SpriteRotationRate>(*Emitter))
		{
			Spin->RotationRateDistribution.InitRange(-90.f, 90.f);
		}
		if (UNiagaraStatelessModule_ScaleColor* const Color = Get<UNiagaraStatelessModule_ScaleColor>(*Emitter))
		{
			FNiagaraDistributionColor& Curve = Color->ScaleDistribution;
			Curve.Mode = ENiagaraDistributionMode::NonUniformCurve;
			Curve.ChannelCurves.SetNum(4);
			for (int32 Channel = 0; Channel < 3; ++Channel)
			{
				SetCurve(Curve.ChannelCurves[Channel], { {0.f, 1.f}, {0.5f, 0.75f}, {1.f, 0.25f} });
			}
			SetCurve(Curve.ChannelCurves[3], { {0.f, 0.f}, {0.06f, 1.f}, {0.55f, 0.85f}, {1.f, 0.f} });
			Curve.UpdateValuesFromDistribution();
		}
		if (UNiagaraStatelessModule_ScaleSpriteSize* const Size = Get<UNiagaraStatelessModule_ScaleSpriteSize>(*Emitter))
		{
			FNiagaraDistributionVector2& Curve = Size->ScaleDistribution;
			Curve.Mode = ENiagaraDistributionMode::NonUniformCurve;
			Curve.ChannelCurves.SetNum(2);
			for (int32 Channel = 0; Channel < 2; ++Channel)
			{
				SetCurve(Curve.ChannelCurves[Channel], { {0.f, 0.4f}, {0.25f, 1.f}, {1.f, 3.f} });
			}
			Curve.UpdateValuesFromDistribution();
		}
		SetSprite(*Emitter, Material);

		const float Margin = 900.f * ToLocal;
		if (FBox* const Bounds = GetProtected<FBox>(*Emitter, TEXT("FixedBounds"))) *Bounds = FBox(FVector(-Margin, -Reach * 0.4f * ToLocal - Margin, -Reach * 0.4f * ToLocal - Margin),
			FVector(Reach * 1.2f * ToLocal + Margin, Reach * 0.4f * ToLocal + Margin, Reach * 0.6f * ToLocal + Margin));
		Emitter->PostEditChange();
	}
	return Finish(System, bCreated);
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildGroundDustSystem(const FString& PackagePath, UMaterialInterface* Material, float Radius)
{
	using namespace knightMonsterFX;
	bool bCreated = false;
	UNiagaraSystem* const System = Material ? LoadOrDuplicateTemplate(PackagePath, bCreated) : nullptr;
	if (!System)
	{
		return nullptr;
	}

	const float MaxLifetime = 2.f;
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		UNiagaraStatelessEmitter* const Emitter = Handle.GetStatelessEmitter();
		if (!Emitter)
		{
			continue;
		}
		EnableOnly(*Emitter, { UNiagaraStatelessModule_InitializeParticle::StaticClass(), UNiagaraStatelessModule_ShapeLocation::StaticClass(),
			UNiagaraStatelessModule_AddVelocity::StaticClass(), UNiagaraStatelessModule_Drag::StaticClass(),
			UNiagaraStatelessModule_AccelerationForce::StaticClass(), UNiagaraStatelessModule_SpriteRotationRate::StaticClass(),
			UNiagaraStatelessModule_ScaleColor::StaticClass(), UNiagaraStatelessModule_ScaleSpriteSize::StaticClass() });

		// 한 번 터지고 끝난다. 루프 길이를 수명보다 길게 두어 마지막 입자까지 보이고 나서 시스템이 끝나게 한다.
		if (FNiagaraEmitterStateData* const State = GetProtected<FNiagaraEmitterStateData>(*Emitter, TEXT("EmitterState")))
		{
			State->LoopBehavior = ENiagaraLoopBehavior::Once;
			State->LoopDuration.InitConstant(MaxLifetime);
		}
		for (int32 Index = 0; Index < Emitter->GetNumSpawnInfos(); ++Index)
		{
			Emitter->GetSpawnInfoByIndex(Index)->bEnabled = false;
		}
		FNiagaraStatelessSpawnInfo& Spawn = *Emitter->GetSpawnInfoByIndex(0);
		Spawn.bEnabled = true;
		Spawn.Type = ENiagaraStatelessSpawnInfoType::Burst;
		Spawn.SpawnTime = 0.f;
		Spawn.Amount.Mode = ENiagaraDistributionMode::UniformRange;
		Spawn.Amount.Min = 32;
		Spawn.Amount.Max = 40;

		if (UNiagaraStatelessModule_InitializeParticle* const Init = Get<UNiagaraStatelessModule_InitializeParticle>(*Emitter))
		{
			Init->LifetimeDistribution.InitRange(MaxLifetime * 0.6f, MaxLifetime);
			SetUniformRange(Init->SpriteSizeDistribution, Radius * 0.22f, Radius * 0.38f);
			Init->SpriteRotationDistribution.InitRange(-180.f, 180.f);
			Init->ColorDistribution = FNiagaraDistributionColor(FLinearColor(0.42f, 0.38f, 0.33f, 1.f));
		}
		if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
		{
			Shape->ShapePrimitive = ENSM_ShapePrimitive::Ring;
			Shape->RingRadius.InitRange(Radius * 0.1f, Radius * 0.3f);
		}
		if (UNiagaraStatelessModule_AddVelocity* const Velocity = Get<UNiagaraStatelessModule_AddVelocity>(*Emitter))
		{
			Velocity->VelocityType = ENSM_VelocityType::FromPoint;
			Velocity->CoordinateSpace = ENiagaraCoordinateSpace::Local;
			Velocity->PointOrigin = FVector3f(0.f, 0.f, -Radius * 0.15f);
			Velocity->PointVelocityDistribution.InitRange(Radius * 1.6f, Radius * 2.4f);
		}
		// 빠르게 퍼졌다가 금방 멈추고 천천히 피어오른다.
		if (UNiagaraStatelessModule_Drag* const Drag = Get<UNiagaraStatelessModule_Drag>(*Emitter))
		{
			Drag->DragDistribution.InitConstant(3.f);
		}
		if (UNiagaraStatelessModule_AccelerationForce* const Rise = Get<UNiagaraStatelessModule_AccelerationForce>(*Emitter))
		{
			Rise->CoordinateSpace = ENiagaraCoordinateSpace::World;
			Rise->AccelerationDistribution.InitConstant(FVector3f(0.f, 0.f, Radius * 0.15f));
		}
		if (UNiagaraStatelessModule_SpriteRotationRate* const Spin = Get<UNiagaraStatelessModule_SpriteRotationRate>(*Emitter))
		{
			Spin->RotationRateDistribution.InitRange(-30.f, 30.f);
		}
		if (UNiagaraStatelessModule_ScaleColor* const Color = Get<UNiagaraStatelessModule_ScaleColor>(*Emitter))
		{
			FNiagaraDistributionColor& Curve = Color->ScaleDistribution;
			Curve.Mode = ENiagaraDistributionMode::NonUniformCurve;
			Curve.ChannelCurves.SetNum(4);
			for (int32 Channel = 0; Channel < 3; ++Channel)
			{
				SetCurve(Curve.ChannelCurves[Channel], { {0.f, 1.f}, {1.f, 1.f} });
			}
			SetCurve(Curve.ChannelCurves[3], { {0.f, 0.f}, {0.08f, 0.6f}, {0.5f, 0.35f}, {1.f, 0.f} });
			Curve.UpdateValuesFromDistribution();
		}
		if (UNiagaraStatelessModule_ScaleSpriteSize* const Size = Get<UNiagaraStatelessModule_ScaleSpriteSize>(*Emitter))
		{
			FNiagaraDistributionVector2& Curve = Size->ScaleDistribution;
			Curve.Mode = ENiagaraDistributionMode::NonUniformCurve;
			Curve.ChannelCurves.SetNum(2);
			for (int32 Channel = 0; Channel < 2; ++Channel)
			{
				SetCurve(Curve.ChannelCurves[Channel], { {0.f, 0.5f}, {0.3f, 1.f}, {1.f, 2.2f} });
			}
			Curve.UpdateValuesFromDistribution();
		}
		SetSprite(*Emitter, Material);

		if (FBox* const Bounds = GetProtected<FBox>(*Emitter, TEXT("FixedBounds"))) *Bounds = FBox(FVector(-Radius * 2.5f, -Radius * 2.5f, -Radius * 0.5f), FVector(Radius * 2.5f, Radius * 2.5f, Radius * 1.5f));
		Emitter->PostEditChange();
	}
	return Finish(System, bCreated);
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildWindStreakSystem(const FString& PackagePath, UMaterialInterface* Material, FVector BoxCenter, FVector BoxExtent,
	float Speed, float StreakLength)
{
	using namespace knightMonsterFX;
	bool bCreated = false;
	UNiagaraSystem* const System = Material ? LoadOrDuplicateTemplate(PackagePath, bCreated) : nullptr;
	if (!System)
	{
		return nullptr;
	}

	// 줄기 하나가 몸 길이 정도를 지나가고 사라진다.
	const float Lifetime = FMath::Max(BoxExtent.X * 2.f / FMath::Max(Speed, 1.f), 0.1f);
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		UNiagaraStatelessEmitter* const Emitter = Handle.GetStatelessEmitter();
		if (!Emitter)
		{
			continue;
		}
		EnableOnly(*Emitter, { UNiagaraStatelessModule_InitializeParticle::StaticClass(), UNiagaraStatelessModule_ShapeLocation::StaticClass(),
			UNiagaraStatelessModule_AddVelocity::StaticClass(), UNiagaraStatelessModule_ScaleColor::StaticClass(),
			UNiagaraStatelessModule_ScaleSpriteSize::StaticClass() });

		for (int32 Index = 0; Index < Emitter->GetNumSpawnInfos(); ++Index)
		{
			Emitter->GetSpawnInfoByIndex(Index)->bEnabled = false;
		}
		FNiagaraStatelessSpawnInfo& Spawn = *Emitter->GetSpawnInfoByIndex(0);
		Spawn.bEnabled = true;
		Spawn.Type = ENiagaraStatelessSpawnInfoType::Rate;
		Spawn.Rate.InitConstant(120.f);

		if (UNiagaraStatelessModule_InitializeParticle* const Init = Get<UNiagaraStatelessModule_InitializeParticle>(*Emitter))
		{
			Init->LifetimeDistribution.InitRange(Lifetime * 0.6f, Lifetime);
			// 속도 방향 정렬 스프라이트는 Y가 진행 방향이다. 가늘고 길게 둔다.
			Init->SpriteSizeDistribution.Mode = ENiagaraDistributionMode::NonUniformRange;
			Init->SpriteSizeDistribution.Min = FVector2f(16.f, StreakLength * 0.6f);
			Init->SpriteSizeDistribution.Max = FVector2f(30.f, StreakLength);
			Init->SpriteSizeDistribution.ChannelConstantsAndRanges = { 16.f, StreakLength * 0.6f, 30.f, StreakLength };
			Init->ColorDistribution = FNiagaraDistributionColor(FLinearColor(0.9f, 0.92f, 0.95f, 1.f));
		}
		if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
		{
			Shape->ShapePrimitive = ENSM_ShapePrimitive::Box;
			// 몸 속에서 나온 줄기는 메시에 가려지므로 상자 겉면에서만 나오게 한다.
			Shape->BoxSize.InitConstant(FVector3f(BoxExtent * 2.f));
			Shape->bBoxSurfaceOnly = true;
			Shape->TransformOffset = FNiagaraDistributionPosition(FVector3f(BoxCenter));
		}
		if (UNiagaraStatelessModule_AddVelocity* const Velocity = Get<UNiagaraStatelessModule_AddVelocity>(*Emitter))
		{
			const FVector3f Min(-Speed, -Speed * 0.03f, -Speed * 0.02f);
			const FVector3f Max(-Speed * 0.8f, Speed * 0.03f, Speed * 0.02f);
			Velocity->VelocityType = ENSM_VelocityType::Linear;
			Velocity->CoordinateSpace = ENiagaraCoordinateSpace::Local;
			Velocity->LinearVelocityDistribution.Mode = ENiagaraDistributionMode::NonUniformRange;
			Velocity->LinearVelocityDistribution.Min = Min;
			Velocity->LinearVelocityDistribution.Max = Max;
			Velocity->LinearVelocityDistribution.ChannelConstantsAndRanges = { Min.X, Min.Y, Min.Z, Max.X, Max.Y, Max.Z };
			Velocity->LinearVelocityScale.InitConstant(1.f);
		}
		if (UNiagaraStatelessModule_ScaleColor* const Color = Get<UNiagaraStatelessModule_ScaleColor>(*Emitter))
		{
			FNiagaraDistributionColor& Curve = Color->ScaleDistribution;
			Curve.Mode = ENiagaraDistributionMode::NonUniformCurve;
			Curve.ChannelCurves.SetNum(4);
			for (int32 Channel = 0; Channel < 3; ++Channel)
			{
				SetCurve(Curve.ChannelCurves[Channel], { {0.f, 1.f}, {1.f, 1.f} });
			}
			SetCurve(Curve.ChannelCurves[3], { {0.f, 0.f}, {0.2f, 0.6f}, {0.6f, 0.4f}, {1.f, 0.f} });
			Curve.UpdateValuesFromDistribution();
		}
		// 나타날 때 짧았다가 늘어난다.
		if (UNiagaraStatelessModule_ScaleSpriteSize* const Size = Get<UNiagaraStatelessModule_ScaleSpriteSize>(*Emitter))
		{
			FNiagaraDistributionVector2& Curve = Size->ScaleDistribution;
			Curve.Mode = ENiagaraDistributionMode::NonUniformCurve;
			Curve.ChannelCurves.SetNum(2);
			SetCurve(Curve.ChannelCurves[0], { {0.f, 1.f}, {1.f, 0.6f} });
			SetCurve(Curve.ChannelCurves[1], { {0.f, 0.3f}, {0.3f, 1.f}, {1.f, 1.2f} });
			Curve.UpdateValuesFromDistribution();
		}
		SetSprite(*Emitter, Material, ENiagaraSpriteAlignment::VelocityAligned);

		const FVector Margin(Speed * Lifetime + StreakLength, StreakLength, StreakLength);
		if (FBox* const Bounds = GetProtected<FBox>(*Emitter, TEXT("FixedBounds"))) *Bounds = FBox(BoxCenter - BoxExtent - Margin, BoxCenter + BoxExtent + FVector(StreakLength));
		Emitter->PostEditChange();
	}
	return Finish(System, bCreated);
}
