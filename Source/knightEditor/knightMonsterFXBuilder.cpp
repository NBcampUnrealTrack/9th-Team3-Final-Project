#include "knightMonsterFXBuilder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "NiagaraDecalRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "Stateless/Modules/NiagaraStatelessModule_AccelerationForce.h"
#include "Stateless/Modules/NiagaraStatelessModule_AddVelocity.h"
#include "Stateless/Modules/NiagaraStatelessModule_CurlNoiseForce.h"
#include "Stateless/Modules/NiagaraStatelessModule_DecalAttributes.h"
#include "Stateless/Modules/NiagaraStatelessModule_Drag.h"
#include "Stateless/Modules/NiagaraStatelessModule_InitializeParticle.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleColor.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleMeshSize.h"
#include "Stateless/Modules/NiagaraStatelessModule_ScaleSpriteSize.h"
#include "Stateless/Modules/NiagaraStatelessModule_ShapeLocation.h"
#include "Stateless/Modules/NiagaraStatelessModule_SpriteFacingAndAlignment.h"
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

	/** 한 번 터지고 끝나는 이미터. 루프 길이를 수명보다 길게 두어 마지막 입자까지 보이고 나서 시스템이 끝나게 한다. */
	void SetBurst(UNiagaraStatelessEmitter& Emitter, int32 MinCount, int32 MaxCount, float MaxLifetime)
	{
		if (FNiagaraEmitterStateData* const State = GetProtected<FNiagaraEmitterStateData>(Emitter, TEXT("EmitterState")))
		{
			State->LoopBehavior = ENiagaraLoopBehavior::Once;
			State->LoopDuration.InitConstant(MaxLifetime);
		}
		for (int32 Index = 0; Index < Emitter.GetNumSpawnInfos(); ++Index)
		{
			Emitter.GetSpawnInfoByIndex(Index)->bEnabled = false;
		}
		FNiagaraStatelessSpawnInfo& Spawn = *Emitter.GetSpawnInfoByIndex(0);
		Spawn.bEnabled = true;
		Spawn.Type = ENiagaraStatelessSpawnInfoType::Burst;
		Spawn.SpawnTime = 0.f;
		Spawn.Amount.Mode = ENiagaraDistributionMode::UniformRange;
		Spawn.Amount.Min = MinCount;
		Spawn.Amount.Max = MaxCount;
	}

	/** 알파: 빠르게 나타났다가 Peak 뒤로 서서히 사라진다. 색은 그대로. */
	void SetFade(UNiagaraStatelessEmitter& Emitter, float Peak, float PeakAlpha)
	{
		if (UNiagaraStatelessModule_ScaleColor* const Color = Get<UNiagaraStatelessModule_ScaleColor>(Emitter))
		{
			FNiagaraDistributionColor& Curve = Color->ScaleDistribution;
			Curve.Mode = ENiagaraDistributionMode::NonUniformCurve;
			Curve.ChannelCurves.SetNum(4);
			for (int32 Channel = 0; Channel < 3; ++Channel)
			{
				SetCurve(Curve.ChannelCurves[Channel], { {0.f, 1.f}, {1.f, 1.f} });
			}
			SetCurve(Curve.ChannelCurves[3], { {0.f, 0.f}, {Peak, PeakAlpha}, {1.f, 0.f} });
			Curve.UpdateValuesFromDistribution();
		}
	}

	/** 스프라이트 렌더러를 메시 렌더러 하나로 바꾼다(다시 구울 때는 있는 메시 렌더러를 그대로 쓴다). */
	void SetMeshRenderer(UNiagaraStatelessEmitter& Emitter, UStaticMesh* Mesh, UMaterialInterface* Material)
	{
		UNiagaraMeshRendererProperties* MeshRenderer = nullptr;
		for (UNiagaraRendererProperties* const Renderer : TArray<UNiagaraRendererProperties*>(Emitter.GetRenderers()))
		{
			if (UNiagaraMeshRendererProperties* const Existing = Cast<UNiagaraMeshRendererProperties>(Renderer))
			{
				MeshRenderer = Existing;
			}
			else if (Renderer)
			{
				Emitter.RemoveRenderer(Renderer, FGuid());
			}
		}
		if (!MeshRenderer)
		{
			MeshRenderer = NewObject<UNiagaraMeshRendererProperties>(&Emitter, NAME_None, RF_Transactional);
			Emitter.AddRenderer(MeshRenderer, FGuid());
		}
		MeshRenderer->Meshes.SetNum(1);
		MeshRenderer->Meshes[0].Mesh = Mesh;
		MeshRenderer->bOverrideMaterials = true;
		MeshRenderer->OverrideMaterials.SetNum(1);
		MeshRenderer->OverrideMaterials[0].ExplicitMat = Material;
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

namespace knightMonsterFX
{
	/**
	 * 스프라이트 레이어 하나(경량 이미터 하나)의 설정. 거리·속도·힘은 월드 cm(빌더가 OwnerScale로 나눈다), 크기는 cm,
	 * 곡선은 수명 비율(0..1). 속도는 기본으로 로컬 +X 방향(기준 속도 x SpeedScale, 옆 퍼짐 Spread)이고, bUseVelocity면
	 * VelocityMin~Max(월드 cm/s)를 그대로 쓴다. BoxSize가 0이 아니면 그 크기의 상자 안에서 생긴다.
	 */
	struct FSpriteLayer
	{
		FName Name;
		UMaterialInterface* Material = nullptr;
		float Rate = 0.f;
		FVector2f Life;
		FVector2f SizeMin, SizeMax;
		FVector2f SpeedScale;
		float Spread = 0.f;
		float Rise = 0.f;
		float Drag = 0.f;
		float CurlStrength = 0.f;
		float CurlFrequency = 0.f;
		TArray<TPair<float, float>> ColorKeys;
		TArray<TPair<float, float>> AlphaKeys;
		TArray<TPair<float, float>> SizeKeys;
		ENiagaraSpriteAlignment Alignment = ENiagaraSpriteAlignment::Unaligned;
		int32 SortOrder = 0;
		FVector3f BoxSize = FVector3f::ZeroVector;
		bool bUseVelocity = false;
		FVector3f VelocityMin = FVector3f::ZeroVector;
		FVector3f VelocityMax = FVector3f::ZeroVector;
		/** 바닥에 눕힌다(로컬 +Z를 바라봄, 돌지 않음). */
		bool bFaceUp = false;
		float SpawnRadius = 40.f;
		/** 스프라이트 대신 데칼로 바닥(로컬 -Z)에 투영한다. 지형 높낮이를 따라 붙는다. 크기는 DecalSizeMin~Max(월드 cm, 투영 깊이 x 가로 x 세로). */
		bool bDecal = false;
		FVector3f DecalSizeMin = FVector3f::ZeroVector;
		FVector3f DecalSizeMax = FVector3f::ZeroVector;
		/** 데칼 회전 범위(투영 축 기준, 도). 0이면 몬스터 방향으로 정렬한다. */
		float DecalRollRange = 360.f;
		/** 입자 색 RGB를 0~1 무작위로 둔다(머티리얼이 무늬 위치를 고르는 씨앗으로 쓴다). 알파 곡선은 그대로다. */
		bool bRandomColorSeed = false;
	};

	void SetCurve(FRichCurve& Curve, const TArray<TPair<float, float>>& Keys)
	{
		Curve.Reset();
		for (const TPair<float, float>& Key : Keys)
		{
			Curve.AddKey(Key.Key, Key.Value);
		}
	}

	void SetVectorRange(FNiagaraDistributionRangeVector3& Distribution, const FVector3f& Min, const FVector3f& Max)
	{
		Distribution.Mode = ENiagaraDistributionMode::NonUniformRange;
		Distribution.Min = Min;
		Distribution.Max = Max;
		Distribution.ChannelConstantsAndRanges = { Min.X, Min.Y, Min.Z, Max.X, Max.Y, Max.Z };
	}

	void ApplySpriteLayer(UNiagaraStatelessEmitter& Emitter, const FSpriteLayer& Layer, float Speed, float ToLocal)
	{
		EnableOnly(Emitter, { UNiagaraStatelessModule_InitializeParticle::StaticClass(), UNiagaraStatelessModule_ShapeLocation::StaticClass(),
			UNiagaraStatelessModule_AddVelocity::StaticClass(), UNiagaraStatelessModule_AccelerationForce::StaticClass(),
			UNiagaraStatelessModule_Drag::StaticClass(), UNiagaraStatelessModule_CurlNoiseForce::StaticClass(),
			UNiagaraStatelessModule_SpriteRotationRate::StaticClass(), UNiagaraStatelessModule_ScaleColor::StaticClass(),
			UNiagaraStatelessModule_ScaleSpriteSize::StaticClass(), UNiagaraStatelessModule_SpriteFacingAndAlignment::StaticClass(),
			UNiagaraStatelessModule_DecalAttributes::StaticClass() });

		for (int32 Index = 0; Index < Emitter.GetNumSpawnInfos(); ++Index)
		{
			Emitter.GetSpawnInfoByIndex(Index)->bEnabled = false;
		}
		FNiagaraStatelessSpawnInfo& Spawn = *Emitter.GetSpawnInfoByIndex(0);
		Spawn.bEnabled = true;
		Spawn.Type = ENiagaraStatelessSpawnInfoType::Rate;
		Spawn.Rate.InitConstant(Layer.Rate);

		if (UNiagaraStatelessModule_DecalAttributes* const Decal = Get<UNiagaraStatelessModule_DecalAttributes>(Emitter))
		{
			Decal->SetIsModuleEnabled(Layer.bDecal);
			// 데칼은 로컬 X 방향으로 투영하므로 아래(-Z)를 보게 세우고, 투영 축으로 무작위로 돌린다. 크기는 메시 스케일을 받지 않아 월드 cm 그대로 넣는다.
			Decal->bApplyOrientation = true;
			Decal->OrientationCoordinateSpace = ENiagaraCoordinateSpace::Local;
			Decal->Orientation.Mode = ENiagaraDistributionMode::NonUniformRange;
			Decal->Orientation.ChannelConstantsAndRanges = { -90.f, 0.f, -Layer.DecalRollRange * 0.5f, -90.f, 0.f, Layer.DecalRollRange * 0.5f };
			Decal->Orientation.UpdateValuesFromDistribution();
			Decal->bApplySize = true;
			const FVector3f SizeMin = Layer.DecalSizeMin;
			const FVector3f SizeMax = Layer.DecalSizeMax;
			Decal->Size.Mode = ENiagaraDistributionMode::NonUniformRange;
			Decal->Size.ChannelConstantsAndRanges = { SizeMin.X, SizeMin.Y, SizeMin.Z, SizeMax.X, SizeMax.Y, SizeMax.Z };
			Decal->Size.UpdateValuesFromDistribution();
			Decal->bApplyFade = false;
		}

		if (UNiagaraStatelessModule_InitializeParticle* const Init = Get<UNiagaraStatelessModule_InitializeParticle>(Emitter))
		{
			Init->LifetimeDistribution.InitRange(Layer.Life.X, Layer.Life.Y);
			FNiagaraDistributionRangeVector2& Size = Init->SpriteSizeDistribution;
			const bool bSquare = Layer.SizeMin.X == Layer.SizeMin.Y && Layer.SizeMax.X == Layer.SizeMax.Y;
			Size.Mode = bSquare ? ENiagaraDistributionMode::UniformRange : ENiagaraDistributionMode::NonUniformRange;
			Size.Min = Layer.SizeMin;
			Size.Max = Layer.SizeMax;
			Size.ChannelConstantsAndRanges = bSquare ? TArray<float>{ Layer.SizeMin.X, Layer.SizeMax.X }
				: TArray<float>{ Layer.SizeMin.X, Layer.SizeMin.Y, Layer.SizeMax.X, Layer.SizeMax.Y };
			Init->SpriteRotationDistribution.InitRange(-180.f, 180.f);
			Init->ColorDistribution = FNiagaraDistributionColor(FLinearColor::White);
			if (Layer.bRandomColorSeed)
			{
				Init->ColorDistribution.Mode = ENiagaraDistributionMode::NonUniformRange;
				Init->ColorDistribution.ChannelConstantsAndRanges = { 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f, 1.f };
				Init->ColorDistribution.UpdateValuesFromDistribution();
			}
		}
		if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(Emitter))
		{
			if (Layer.BoxSize.IsNearlyZero())
			{
				Shape->ShapePrimitive = ENSM_ShapePrimitive::Sphere;
				Shape->SphereRadius.InitConstant(Layer.SpawnRadius * ToLocal);
			}
			else
			{
				Shape->ShapePrimitive = ENSM_ShapePrimitive::Box;
				Shape->BoxSize.InitConstant(Layer.BoxSize * ToLocal);
			}
		}
		if (UNiagaraStatelessModule_AddVelocity* const Velocity = Get<UNiagaraStatelessModule_AddVelocity>(Emitter))
		{
			FVector3f Min, Max;
			if (Layer.bUseVelocity)
			{
				Min = Layer.VelocityMin * ToLocal;
				Max = Layer.VelocityMax * ToLocal;
			}
			else
			{
				// 로컬 X로 나가는 직선 속도에 옆 퍼짐을 범위로 준다.
				const float Forward = Speed * Layer.SpeedScale.Y;
				Min = FVector3f(Speed * Layer.SpeedScale.X, -Forward * Layer.Spread, -Forward * Layer.Spread);
				Max = FVector3f(Forward, Forward * Layer.Spread, Forward * Layer.Spread);
			}
			Velocity->VelocityType = ENSM_VelocityType::Linear;
			Velocity->CoordinateSpace = ENiagaraCoordinateSpace::Local;
			SetVectorRange(Velocity->LinearVelocityDistribution, Min, Max);
			Velocity->LinearVelocityScale.InitConstant(1.f);
		}
		// 뜨거운 것은 위로 말려 올라간다. 월드 방향 힘도 메시 스케일을 받으므로 로컬 단위로 나눈다.
		if (UNiagaraStatelessModule_AccelerationForce* const Rise = Get<UNiagaraStatelessModule_AccelerationForce>(Emitter))
		{
			Rise->SetIsModuleEnabled(Layer.Rise != 0.f);
			Rise->CoordinateSpace = ENiagaraCoordinateSpace::World;
			Rise->AccelerationDistribution.InitConstant(FVector3f(0.f, 0.f, Layer.Rise * ToLocal));
		}
		if (UNiagaraStatelessModule_Drag* const Drag = Get<UNiagaraStatelessModule_Drag>(Emitter))
		{
			Drag->SetIsModuleEnabled(Layer.Drag > 0.f);
			Drag->DragDistribution.InitConstant(Layer.Drag);
		}
		// 난류: 힘은 로컬 단위로 나누고, 주파수는 로컬 공간 기준이라 메시 스케일을 곱한다(월드에서 같은 크기의 소용돌이).
		if (UNiagaraStatelessModule_CurlNoiseForce* const Curl = Get<UNiagaraStatelessModule_CurlNoiseForce>(Emitter))
		{
			Curl->SetIsModuleEnabled(Layer.CurlStrength > 0.f);
			Curl->NoiseStrength = Layer.CurlStrength * ToLocal;
			Curl->NoiseFrequency = Layer.CurlFrequency / FMath::Max(ToLocal, KINDA_SMALL_NUMBER);
		}
		if (UNiagaraStatelessModule_SpriteRotationRate* const Spin = Get<UNiagaraStatelessModule_SpriteRotationRate>(Emitter))
		{
			Spin->SetIsModuleEnabled(!Layer.bFaceUp);
			Spin->RotationRateDistribution.InitRange(-90.f, 90.f);
		}
		if (UNiagaraStatelessModule_SpriteFacingAndAlignment* const Facing = Get<UNiagaraStatelessModule_SpriteFacingAndAlignment>(Emitter))
		{
			Facing->SetIsModuleEnabled(Layer.bFaceUp);
			Facing->bSpriteFacingEnabled = true;
			Facing->bSpriteAlignmentEnabled = false;
			Facing->SpriteFacing.InitConstant(FVector3f::ZAxisVector);
		}
		if (UNiagaraStatelessModule_ScaleColor* const Color = Get<UNiagaraStatelessModule_ScaleColor>(Emitter))
		{
			FNiagaraDistributionColor& Curve = Color->ScaleDistribution;
			Curve.Mode = ENiagaraDistributionMode::NonUniformCurve;
			Curve.ChannelCurves.SetNum(4);
			for (int32 Channel = 0; Channel < 3; ++Channel)
			{
				SetCurve(Curve.ChannelCurves[Channel], Layer.ColorKeys);
			}
			SetCurve(Curve.ChannelCurves[3], Layer.AlphaKeys);
			Curve.UpdateValuesFromDistribution();
		}
		if (UNiagaraStatelessModule_ScaleSpriteSize* const Size = Get<UNiagaraStatelessModule_ScaleSpriteSize>(Emitter))
		{
			FNiagaraDistributionVector2& Curve = Size->ScaleDistribution;
			Curve.Mode = ENiagaraDistributionMode::NonUniformCurve;
			Curve.ChannelCurves.SetNum(2);
			for (int32 Channel = 0; Channel < 2; ++Channel)
			{
				SetCurve(Curve.ChannelCurves[Channel], Layer.SizeKeys);
			}
			Curve.UpdateValuesFromDistribution();
		}
		if (Layer.bDecal)
		{
			// 스프라이트 렌더러를 데칼 렌더러로 바꾼다(다시 구울 때는 있는 데칼 렌더러를 그대로 쓴다).
			UNiagaraDecalRendererProperties* DecalRenderer = nullptr;
			for (UNiagaraRendererProperties* const Renderer : TArray<UNiagaraRendererProperties*>(Emitter.GetRenderers()))
			{
				if (UNiagaraDecalRendererProperties* const Existing = Cast<UNiagaraDecalRendererProperties>(Renderer))
				{
					DecalRenderer = Existing;
				}
				else if (Renderer)
				{
					Emitter.RemoveRenderer(Renderer, FGuid());
				}
			}
			if (!DecalRenderer)
			{
				DecalRenderer = NewObject<UNiagaraDecalRendererProperties>(&Emitter, NAME_None, RF_Transactional);
				Emitter.AddRenderer(DecalRenderer, FGuid());
			}
			DecalRenderer->Material = Layer.Material;
			DecalRenderer->SortOrderHint = Layer.SortOrder;
			return;
		}
		SetSprite(Emitter, Layer.Material, Layer.Alignment);
		for (UNiagaraRendererProperties* const Renderer : Emitter.GetRenderers())
		{
			if (UNiagaraSpriteRendererProperties* const Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
			{
				Sprite->SortOrderHint = Layer.SortOrder;
				if (Layer.bFaceUp)
				{
					Sprite->FacingMode = ENiagaraSpriteFacingMode::CustomFacingVector;
				}
			}
		}
	}

	/**
	 * 레이어마다 이미터 하나인 경량 시스템을 PackagePath에 만든다(모자란 이미터는 첫 이미터를 복제). 머티리얼이 없는 레이어는 끈다.
	 * Speed는 로컬 +X 기준 속도(월드 cm/s), BoundsMin/Max는 월드 cm의 고정 바운드.
	 */
	UNiagaraSystem* BuildLayeredSystem(const FString& PackagePath, TArrayView<const FSpriteLayer> Layers, float Speed, float OwnerScale,
		const FVector& BoundsMin, const FVector& BoundsMax, float OnceDuration = 0.f)
	{
		bool bCreated = false;
		UNiagaraSystem* const System = LoadOrDuplicateTemplate(PackagePath, bCreated);
		if (!System)
		{
			return nullptr;
		}
		// 위치와 속도는 메시 스케일을 받으므로 로컬 단위로 나눠 넣는다.
		const float ToLocal = 1.f / FMath::Max(OwnerScale, KINDA_SMALL_NUMBER);
		while (System->GetEmitterHandles().Num() < Layers.Num())
		{
			System->DuplicateEmitterHandle(System->GetEmitterHandles()[0], Layers[System->GetEmitterHandles().Num()].Name);
		}
		for (int32 Index = 0; Index < System->GetEmitterHandles().Num(); ++Index)
		{
			FNiagaraEmitterHandle& Handle = System->GetEmitterHandle(Index);
			const FSpriteLayer* const Layer = Layers.IsValidIndex(Index) ? &Layers[Index] : nullptr;
			Handle.SetIsEnabled(Layer && Layer->Material, *System, false);
			UNiagaraStatelessEmitter* const Emitter = Handle.GetStatelessEmitter();
			if (!Layer || !Emitter || !Layer->Material)
			{
				continue;
			}
			Handle.SetName(Layer->Name, *System);
			ApplySpriteLayer(*Emitter, *Layer, Speed * ToLocal, ToLocal);
			// 일회성: OnceDuration 동안만 생성하고 남은 입자가 수명을 다하면 끝난다. 0이면 컴포넌트가 멈출 때까지 계속 생성한다.
			if (FNiagaraEmitterStateData* const State = GetProtected<FNiagaraEmitterStateData>(*Emitter, TEXT("EmitterState")))
			{
				State->LoopBehavior = OnceDuration > 0.f ? ENiagaraLoopBehavior::Once : ENiagaraLoopBehavior::Infinite;
				State->LoopDuration.InitConstant(OnceDuration > 0.f ? OnceDuration : 1.f);
				State->InactiveResponse = ENiagaraEmitterInactiveResponse::Complete;
			}
			if (FBox* const Bounds = GetProtected<FBox>(*Emitter, TEXT("FixedBounds")))
			{
				*Bounds = FBox(BoundsMin * ToLocal, BoundsMax * ToLocal);
			}
			Emitter->PostEditChange();
		}
		return Finish(System, bCreated);
	}
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildBreathSystem(const FString& PackagePath, UMaterialInterface* FireMaterial, UMaterialInterface* CoreMaterial,
	UMaterialInterface* SmokeMaterial, UMaterialInterface* EmberMaterial, float Reach, float OwnerScale)
{
	using namespace knightMonsterFX;
	if (!FireMaterial)
	{
		return nullptr;
	}
	const FSpriteLayer Layers[] = {
		// 연기: 불줄기를 따라 나가다 끝쯤에서 짙어지고, 넓게 퍼지며 천천히 오른다. 쏘기가 끝난 뒤에도 1~2초 남는다.
		// 불 바로 뒤라 처음엔 붉게 달아 있고, 완전한 검정이 아니라 짙은 회갈색이다.
		{ TEXT("Smoke"), SmokeMaterial, 45.f, {1.4f, 2.f}, {250.f, 250.f}, {400.f, 400.f}, {0.75f, 0.95f}, 0.2f, 350.f, 0.6f, 500.f, 0.002f,
			{ {0.f, 10.f}, {0.35f, 5.f}, {0.6f, 4.f}, {1.f, 4.f} }, { {0.f, 0.f}, {0.35f, 0.f}, {0.5f, 0.5f}, {0.8f, 0.3f}, {1.f, 0.f} },
			{ {0.f, 0.5f}, {0.4f, 1.f}, {1.f, 2.5f} }, ENiagaraSpriteAlignment::Unaligned, 0 },
		// 화염: 불줄기 몸통.
		{ TEXT("Fire"), FireMaterial, 160.f, {0.8f, 1.f}, {150.f, 150.f}, {250.f, 250.f}, {0.8f, 1.f}, 0.12f, 500.f, 0.f, 250.f, 0.003f,
			{ {0.f, 1.f}, {0.5f, 0.75f}, {1.f, 0.25f} }, { {0.f, 0.f}, {0.06f, 1.f}, {0.55f, 0.85f}, {1.f, 0.f} },
			{ {0.f, 0.4f}, {0.25f, 1.f}, {1.f, 3.f} }, ENiagaraSpriteAlignment::Unaligned, 1 },
		// 심지: 입 앞 40%쯤까지 가는 흰빛 불. 짧고 밝다.
		{ TEXT("Core"), CoreMaterial, 140.f, {0.3f, 0.45f}, {90.f, 90.f}, {150.f, 150.f}, {0.7f, 0.9f}, 0.06f, 0.f, 0.f, 0.f, 0.f,
			{ {0.f, 1.6f}, {0.5f, 1.2f}, {1.f, 0.6f} }, { {0.f, 0.f}, {0.05f, 1.f}, {0.6f, 0.8f}, {1.f, 0.f} },
			{ {0.f, 0.5f}, {0.3f, 1.f}, {1.f, 1.8f} }, ENiagaraSpriteAlignment::Unaligned, 2 },
		// 불씨: 넓게 튀어 흩날리는 작은 불똥. 속도 방향으로 늘인다.
		{ TEXT("Embers"), EmberMaterial, 70.f, {0.8f, 1.4f}, {8.f, 30.f}, {14.f, 60.f}, {0.6f, 1.1f}, 0.35f, 300.f, 0.5f, 600.f, 0.004f,
			{ {0.f, 1.f}, {1.f, 0.6f} }, { {0.f, 0.f}, {0.05f, 1.f}, {0.7f, 1.f}, {1.f, 0.f} },
			{ {0.f, 1.f}, {1.f, 0.6f} }, ENiagaraSpriteAlignment::VelocityAligned, 3 },
	};
	const float Margin = 900.f;
	return BuildLayeredSystem(PackagePath, Layers, Reach, OwnerScale, FVector(-Margin, -Reach * 0.4f - Margin, -Reach * 0.4f - Margin),
		FVector(Reach * 1.2f + Margin, Reach * 0.4f + Margin, Reach * 0.8f + Margin));
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildBreathChargeSystem(const FString& PackagePath, UMaterialInterface* GlowMaterial,
	UMaterialInterface* SmokeMaterial, UMaterialInterface* EmberMaterial, float OwnerScale)
{
	using namespace knightMonsterFX;
	if (!GlowMaterial)
	{
		return nullptr;
	}
	// 입 안에서 달아오르는 빛, 콧김처럼 새어 나와 오르는 연기, 입가에서 튀는 작은 불씨. 로컬 +X가 입 앞이다.
	FSpriteLayer Glow{ TEXT("Glow"), GlowMaterial, 60.f, {0.25f, 0.4f}, {140.f, 140.f}, {220.f, 220.f}, {0.f, 0.f}, 0.f, 0.f, 0.f, 0.f, 0.f,
		{ {0.f, 0.6f}, {0.5f, 1.f}, {1.f, 0.5f} }, { {0.f, 0.f}, {0.2f, 0.8f}, {1.f, 0.f} }, { {0.f, 0.6f}, {1.f, 1.3f} },
		ENiagaraSpriteAlignment::Unaligned, 1 };
	Glow.bUseVelocity = true;
	Glow.VelocityMin = FVector3f(20.f, -20.f, -10.f);
	Glow.VelocityMax = FVector3f(80.f, 20.f, 20.f);
	Glow.SpawnRadius = 30.f;

	FSpriteLayer Smoke{ TEXT("Smoke"), SmokeMaterial, 25.f, {1.f, 1.5f}, {150.f, 150.f}, {260.f, 260.f}, {0.f, 0.f}, 0.f, 250.f, 0.8f, 120.f, 0.01f,
		{ {0.f, 8.f}, {0.4f, 4.f}, {1.f, 4.f} }, { {0.f, 0.f}, {0.2f, 0.55f}, {1.f, 0.f} }, { {0.f, 0.5f}, {1.f, 2.5f} },
		ENiagaraSpriteAlignment::Unaligned, 0 };
	Smoke.bUseVelocity = true;
	Smoke.VelocityMin = FVector3f(40.f, -60.f, 0.f);
	Smoke.VelocityMax = FVector3f(120.f, 60.f, 60.f);
	Smoke.SpawnRadius = 50.f;

	FSpriteLayer Embers{ TEXT("Embers"), EmberMaterial, 40.f, {0.5f, 0.9f}, {5.f, 14.f}, {9.f, 26.f}, {0.f, 0.f}, 0.f, 200.f, 0.6f, 200.f, 0.01f,
		{ {0.f, 1.f}, {1.f, 0.6f} }, { {0.f, 0.f}, {0.1f, 1.f}, {0.7f, 1.f}, {1.f, 0.f} }, { {0.f, 1.f}, {1.f, 0.5f} },
		ENiagaraSpriteAlignment::VelocityAligned, 2 };
	Embers.bUseVelocity = true;
	Embers.VelocityMin = FVector3f(50.f, -150.f, -50.f);
	Embers.VelocityMax = FVector3f(250.f, 150.f, 200.f);
	Embers.SpawnRadius = 40.f;

	const FSpriteLayer Layers[] = { Smoke, Glow, Embers };
	return BuildLayeredSystem(PackagePath, Layers, 0.f, OwnerScale, FVector(-600.f, -600.f, -400.f), FVector(800.f, 600.f, 900.f));
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildBreathGroundSystem(const FString& PackagePath, UMaterialInterface* FlameMaterial,
	UMaterialInterface* ScorchMaterial, float Length, float Width, float ActiveTime)
{
	using namespace knightMonsterFX;
	if (!FlameMaterial)
	{
		return nullptr;
	}
	// 불줄기가 바닥에 닿는 띠(로컬 X가 앞, Length x Width)에서 짧게 솟는 불길.
	FSpriteLayer Flames{ TEXT("GroundFlames"), FlameMaterial, 70.f, {0.5f, 0.8f}, {160.f, 160.f}, {300.f, 300.f}, {0.f, 0.f}, 0.f, 300.f, 0.f, 200.f, 0.004f,
		{ {0.f, 1.2f}, {0.5f, 0.8f}, {1.f, 0.3f} }, { {0.f, 0.f}, {0.1f, 1.f}, {0.5f, 0.8f}, {1.f, 0.f} }, { {0.f, 0.5f}, {0.3f, 1.f}, {1.f, 2.f} },
		ENiagaraSpriteAlignment::Unaligned, 1 };
	Flames.BoxSize = FVector3f(Length, Width, 20.f);
	Flames.bUseVelocity = true;
	Flames.VelocityMin = FVector3f(-80.f, -120.f, 150.f);
	Flames.VelocityMax = FVector3f(200.f, 120.f, 400.f);

	// 빛나는 잔열 자국: 바닥에 투영한 데칼. 불이 지나간 뒤 5초쯤 남아 노란빛 균열이 붉게 식으며 사라진다.
	FSpriteLayer Scorch{ TEXT("Scorch"), ScorchMaterial, 14.f, {4.5f, 6.f}, {350.f, 350.f}, {600.f, 600.f}, {0.f, 0.f}, 0.f, 0.f, 0.f, 0.f, 0.f,
		{ {0.f, 1.f}, {1.f, 1.f} }, { {0.f, 0.f}, {0.04f, 1.f}, {0.5f, 0.8f}, {1.f, 0.f} }, { {0.f, 0.9f}, {0.1f, 1.f}, {1.f, 1.f} },
		ENiagaraSpriteAlignment::Unaligned, 0 };
	Scorch.BoxSize = FVector3f(Length, Width * 0.8f, 0.f);
	Scorch.bUseVelocity = true;
	Scorch.bDecal = true;
	// 균열 줄무늬가 브레스 방향(로컬 X)을 따라 눕도록 크게 돌리지 않는다.
	Scorch.DecalRollRange = 30.f;
	Scorch.bRandomColorSeed = true;
	Scorch.DecalSizeMin = FVector3f(400.f, 500.f, 500.f);
	Scorch.DecalSizeMax = FVector3f(400.f, 850.f, 850.f);

	const FSpriteLayer Layers[] = { Scorch, Flames };
	const float Margin = 600.f;
	// 몸에 붙이지 않고 바닥에 스폰하므로 스케일 1(cm 그대로)이고, ActiveTime 동안만 생성하고 스스로 끝난다.
	return BuildLayeredSystem(PackagePath, Layers, 0.f, 1.f, FVector(-Length * 0.5f - Margin, -Width * 0.5f - Margin, -300.f),
		FVector(Length * 0.5f + Margin, Width * 0.5f + Margin, 1200.f), ActiveTime);
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildFireEruptionSystem(const FString& PackagePath, UMaterialInterface* FireMaterial,
	UMaterialInterface* CoreMaterial, UMaterialInterface* SmokeMaterial, UMaterialInterface* EmberMaterial, UMaterialInterface* ScorchMaterial,
	float Height, float Radius)
{
	using namespace knightMonsterFX;
	if (!FireMaterial)
	{
		return nullptr;
	}
	// 0.35초 동안 한꺼번에 솟고 끝난다. 속도는 수명 동안 Height까지 오르도록 잡는다.
	const float Burst = 0.35f;
	const float RiseSpeed = Height / 0.8f;
	const FVector3f Base(Radius * 2.f, Radius * 2.f, 20.f);

	// 밑동의 흰빛 심지: 짧고 밝게 터진다.
	FSpriteLayer Core{ TEXT("Core"), CoreMaterial, 90.f, {0.25f, 0.4f}, {140.f, 140.f}, {240.f, 240.f}, {0.f, 0.f}, 0.f, 0.f, 0.f, 0.f, 0.f,
		{ {0.f, 1.6f}, {0.5f, 1.1f}, {1.f, 0.5f} }, { {0.f, 0.f}, {0.05f, 1.f}, {0.6f, 0.8f}, {1.f, 0.f} }, { {0.f, 0.5f}, {0.3f, 1.f}, {1.f, 1.8f} },
		ENiagaraSpriteAlignment::Unaligned, 2 };
	Core.BoxSize = Base;
	Core.bUseVelocity = true;
	Core.VelocityMin = FVector3f(-100.f, -100.f, RiseSpeed * 0.2f);
	Core.VelocityMax = FVector3f(100.f, 100.f, RiseSpeed * 0.5f);

	// 불기둥: 위로 솟으며 커지고, 위쪽은 노이즈로 흔들리며 깎인다.
	FSpriteLayer Fire{ TEXT("Fire"), FireMaterial, 130.f, {0.6f, 0.9f}, {150.f, 150.f}, {260.f, 260.f}, {0.f, 0.f}, 0.f, 300.f, 0.8f, 300.f, 0.003f,
		{ {0.f, 1.f}, {0.5f, 0.75f}, {1.f, 0.25f} }, { {0.f, 0.f}, {0.06f, 1.f}, {0.55f, 0.85f}, {1.f, 0.f} }, { {0.f, 0.4f}, {0.25f, 1.f}, {1.f, 2.4f} },
		ENiagaraSpriteAlignment::Unaligned, 1 };
	Fire.BoxSize = Base;
	Fire.bUseVelocity = true;
	Fire.VelocityMin = FVector3f(-150.f, -150.f, RiseSpeed * 0.6f);
	Fire.VelocityMax = FVector3f(150.f, 150.f, RiseSpeed * 1.3f);

	// 불이 꺼진 뒤 피어오르는 짙은 연기.
	FSpriteLayer Smoke{ TEXT("Smoke"), SmokeMaterial, 30.f, {1.4f, 2.f}, {250.f, 250.f}, {400.f, 400.f}, {0.f, 0.f}, 0.f, 200.f, 0.8f, 300.f, 0.002f,
		{ {0.f, 8.f}, {0.35f, 4.f}, {1.f, 4.f} }, { {0.f, 0.f}, {0.3f, 0.f}, {0.45f, 0.45f}, {0.8f, 0.25f}, {1.f, 0.f} }, { {0.f, 0.5f}, {0.4f, 1.f}, {1.f, 2.5f} },
		ENiagaraSpriteAlignment::Unaligned, 0 };
	Smoke.BoxSize = Base;
	Smoke.bUseVelocity = true;
	Smoke.VelocityMin = FVector3f(-100.f, -100.f, RiseSpeed * 0.5f);
	Smoke.VelocityMax = FVector3f(100.f, 100.f, RiseSpeed * 0.9f);

	FSpriteLayer Embers{ TEXT("Embers"), EmberMaterial, 80.f, {0.8f, 1.4f}, {8.f, 30.f}, {14.f, 60.f}, {0.f, 0.f}, 0.f, 200.f, 0.6f, 500.f, 0.004f,
		{ {0.f, 1.f}, {1.f, 0.6f} }, { {0.f, 0.f}, {0.05f, 1.f}, {0.7f, 1.f}, {1.f, 0.f} }, { {0.f, 1.f}, {1.f, 0.6f} },
		ENiagaraSpriteAlignment::VelocityAligned, 3 };
	Embers.BoxSize = Base;
	Embers.bUseVelocity = true;
	Embers.VelocityMin = FVector3f(-500.f, -500.f, RiseSpeed * 0.6f);
	Embers.VelocityMax = FVector3f(500.f, 500.f, RiseSpeed * 1.6f);

	// 불기둥이 솟은 자리에 남는 잔열 자국. 몇 장을 겹쳐 아무 방향으로 돌린다.
	FSpriteLayer Scorch{ TEXT("Scorch"), ScorchMaterial, 14.f, {5.f, 7.f}, {350.f, 350.f}, {600.f, 600.f}, {0.f, 0.f}, 0.f, 0.f, 0.f, 0.f, 0.f,
		{ {0.f, 1.f}, {1.f, 1.f} }, { {0.f, 0.f}, {0.04f, 1.f}, {0.5f, 0.8f}, {1.f, 0.f} }, { {0.f, 0.9f}, {0.1f, 1.f}, {1.f, 1.f} },
		ENiagaraSpriteAlignment::Unaligned, 0 };
	Scorch.BoxSize = FVector3f(Radius * 2.f, Radius * 2.f, 0.f);
	Scorch.bUseVelocity = true;
	Scorch.bDecal = true;
	Scorch.bRandomColorSeed = true;
	Scorch.DecalSizeMin = FVector3f(400.f, Radius * 4.f, Radius * 4.f);
	Scorch.DecalSizeMax = FVector3f(400.f, Radius * 7.f, Radius * 7.f);

	const FSpriteLayer Layers[] = { Scorch, Smoke, Fire, Core, Embers };
	const float Spread = Radius + 900.f;
	return BuildLayeredSystem(PackagePath, Layers, 0.f, 1.f, FVector(-Spread, -Spread, -300.f), FVector(Spread, Spread, Height * 2.f + 600.f), Burst);
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildBurnSystem(const FString& PackagePath, UMaterialInterface* FireMaterial,
	UMaterialInterface* SmokeMaterial, UMaterialInterface* EmberMaterial, FVector BodyExtent, float FlameScale)
{
	using namespace knightMonsterFX;
	if (!FireMaterial)
	{
		return nullptr;
	}
	const FVector3f Box = FVector3f(BodyExtent) * 2.f;
	// 몸 곳곳에서 붙었다 꺼지는 작은 불꽃. 위로 날리며 금방 사라진다.
	FSpriteLayer Flames{ TEXT("Flames"), FireMaterial, 45.f, {0.35f, 0.6f}, {50.f, 50.f}, {95.f, 95.f}, {0.f, 0.f}, 0.f, 250.f, 0.f, 150.f, 0.02f,
		{ {0.f, 1.2f}, {0.5f, 0.8f}, {1.f, 0.3f} }, { {0.f, 0.f}, {0.1f, 1.f}, {0.6f, 0.8f}, {1.f, 0.f} }, { {0.f, 0.5f}, {0.3f, 1.f}, {1.f, 1.6f} },
		ENiagaraSpriteAlignment::Unaligned, 1 };
	Flames.BoxSize = Box;
	Flames.bUseVelocity = true;
	Flames.VelocityMin = FVector3f(-30.f, -30.f, 60.f);
	Flames.VelocityMax = FVector3f(30.f, 30.f, 160.f);

	// 그을린 연기: 몸 위쪽에서 가늘게 피어오른다.
	FSpriteLayer Smoke{ TEXT("Smoke"), SmokeMaterial, 8.f, {0.9f, 1.4f}, {50.f, 50.f}, {90.f, 90.f}, {0.f, 0.f}, 0.f, 120.f, 0.5f, 60.f, 0.02f,
		{ {0.f, 3.f}, {0.4f, 1.5f}, {1.f, 1.5f} }, { {0.f, 0.f}, {0.2f, 0.35f}, {1.f, 0.f} }, { {0.f, 0.6f}, {1.f, 2.2f} },
		ENiagaraSpriteAlignment::Unaligned, 0 };
	Smoke.BoxSize = FVector3f(Box.X, Box.Y, Box.Z * 0.5f);
	Smoke.bUseVelocity = true;
	Smoke.VelocityMin = FVector3f(-20.f, -20.f, 80.f);
	Smoke.VelocityMax = FVector3f(20.f, 20.f, 160.f);

	FSpriteLayer Embers{ TEXT("Embers"), EmberMaterial, 18.f, {0.5f, 0.9f}, {3.f, 8.f}, {5.f, 14.f}, {0.f, 0.f}, 0.f, 150.f, 0.5f, 150.f, 0.02f,
		{ {0.f, 1.f}, {1.f, 0.6f} }, { {0.f, 0.f}, {0.1f, 1.f}, {0.7f, 1.f}, {1.f, 0.f} }, { {0.f, 1.f}, {1.f, 0.5f} },
		ENiagaraSpriteAlignment::VelocityAligned, 2 };
	Embers.BoxSize = Box;
	Embers.bUseVelocity = true;
	Embers.VelocityMin = FVector3f(-120.f, -120.f, 50.f);
	Embers.VelocityMax = FVector3f(120.f, 120.f, 250.f);

	for (FSpriteLayer* const Layer : { &Flames, &Smoke })
	{
		Layer->SizeMin *= FlameScale;
		Layer->SizeMax *= FlameScale;
		Layer->Rate *= FlameScale;
	}

	const FSpriteLayer Layers[] = { Smoke, Flames, Embers };
	const FVector Margin(200.f, 200.f, 400.f);
	return BuildLayeredSystem(PackagePath, Layers, 0.f, 1.f, -BodyExtent - Margin, BodyExtent + Margin);
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

UNiagaraSystem* UknightMonsterFXBuilder::BuildSonicWaveSystem(const FString& PackagePath, UStaticMesh* WaveMesh, UMaterialInterface* WaveMaterial,
	UMaterialInterface* StreakMaterial, UMaterialInterface* WispMaterial, float WaveRadius, FLinearColor Tint)
{
	using namespace knightMonsterFX;
	if (!WaveMesh || !WaveMaterial)
	{
		return nullptr;
	}
	// 파면 메시(SM_Janhyang_SonicWave)는 테두리 반지름이 약 82cm다.
	const float MeshScale = WaveRadius / 82.f;
	constexpr float WaveLife = 2.f;

	// 첫 레이어는 자리만 잡는다(아래에서 메시 렌더러로 바꾼다). 나머지 둘은 테두리 링에서 나와 뒤로 흐른다.
	FSpriteLayer Layers[3];
	Layers[0].Name = TEXT("Wave");
	Layers[0].Material = WaveMaterial;
	Layers[0].Rate = 1.f;
	Layers[0].Life = FVector2f(WaveLife);
	Layers[0].SizeMin = Layers[0].SizeMax = FVector2f(1.f);
	Layers[0].ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Layers[0].AlphaKeys = { {0.f, 0.f}, {0.03f, 1.f}, {1.f, 1.f} };
	Layers[0].SizeKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Layers[0].SortOrder = 0;

	// 바람결: 가늘고 긴 속도 정렬 줄기가 테두리에서 벗겨져 뒤로 짧게 흐른다.
	Layers[1].Name = TEXT("Streaks");
	Layers[1].Material = StreakMaterial;
	Layers[1].Rate = 150.f;
	Layers[1].Life = FVector2f(0.18f, 0.32f);
	Layers[1].SizeMin = FVector2f(4.f, 110.f);
	Layers[1].SizeMax = FVector2f(8.f, 260.f);
	Layers[1].ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Layers[1].AlphaKeys = { {0.f, 0.f}, {0.2f, 0.3f}, {0.6f, 0.15f}, {1.f, 0.f} };
	Layers[1].SizeKeys = { {0.f, 0.4f}, {0.35f, 1.f}, {1.f, 1.15f} };
	Layers[1].Alignment = ENiagaraSpriteAlignment::VelocityAligned;
	Layers[1].SortOrder = 1;

	// 흩날림: 바람결을 따라 천천히 벗겨지며 커지는 옅은 회색 연기 결(몸 주위 잔상과 같은 톤).
	Layers[2].Name = TEXT("Wisps");
	Layers[2].Material = WispMaterial;
	Layers[2].Rate = 30.f;
	Layers[2].Life = FVector2f(0.6f, 1.1f);
	Layers[2].SizeMin = FVector2f(100.f);
	Layers[2].SizeMax = FVector2f(220.f);
	Layers[2].CurlStrength = 300.f;
	Layers[2].CurlFrequency = 0.004f;
	Layers[2].Drag = 1.5f;
	Layers[2].ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Layers[2].AlphaKeys = { {0.f, 0.f}, {0.25f, 0.12f}, {1.f, 0.f} };
	Layers[2].SizeKeys = { {0.f, 0.6f}, {1.f, 2.5f} };
	Layers[2].SortOrder = 2;

	const FVector BoundsMin(-1500.f, -WaveRadius * 3.f, -WaveRadius * 3.f);
	const FVector BoundsMax(WaveRadius * 2.f, WaveRadius * 3.f, WaveRadius * 3.f);
	UNiagaraSystem* const System = BuildLayeredSystem(PackagePath, Layers, 0.f, 1.f, BoundsMin, BoundsMax);
	if (!System)
	{
		return nullptr;
	}

	for (int32 Index = 0; Index < System->GetEmitterHandles().Num(); ++Index)
	{
		UNiagaraStatelessEmitter* const Emitter = System->GetEmitterHandle(Index).GetStatelessEmitter();
		if (!Emitter || !Layers[FMath::Min(Index, 2)].Material)
		{
			continue;
		}
		if (Index == 0)
		{
			// 파면 하나: 로컬 +X(날아가는 방향)로 볼록하게 붙어 날아가며 조금씩 커진다.
			EnableOnly(*Emitter, { UNiagaraStatelessModule_InitializeParticle::StaticClass(), UNiagaraStatelessModule_ShapeLocation::StaticClass(),
				UNiagaraStatelessModule_ScaleColor::StaticClass(), UNiagaraStatelessModule_ScaleMeshSize::StaticClass() });
			SetBurst(*Emitter, 1, 1, WaveLife);
			if (UNiagaraStatelessModule_InitializeParticle* const Init = Get<UNiagaraStatelessModule_InitializeParticle>(*Emitter))
			{
				Init->MeshScaleDistribution.InitConstant(FVector3f(MeshScale));
			}
			if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
			{
				Shape->ShapePrimitive = ENSM_ShapePrimitive::Sphere;
				Shape->SphereRadius.InitConstant(0.f);
			}
			if (UNiagaraStatelessModule_ScaleMeshSize* const Size = Get<UNiagaraStatelessModule_ScaleMeshSize>(*Emitter))
			{
				FNiagaraDistributionVector3& Curve = Size->ScaleDistribution;
				Curve.Mode = ENiagaraDistributionMode::NonUniformCurve;
				Curve.ChannelCurves.SetNum(3);
				for (int32 Channel = 0; Channel < 3; ++Channel)
				{
					SetCurve(Curve.ChannelCurves[Channel], { {0.f, 0.8f}, {0.3f, 1.f}, {1.f, 1.25f} });
				}
				Curve.UpdateValuesFromDistribution();
			}
			SetMeshRenderer(*Emitter, WaveMesh, WaveMaterial);
		}
		else
		{
			// 테두리 링(로컬 X축이 링의 축)에서 생겨, 파면 앞쪽 한 점에서 멀어지는 방향(뒤로, 조금 바깥으로)으로 흐른다.
			if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
			{
				Shape->ShapePrimitive = ENSM_ShapePrimitive::Ring;
				Shape->RingRadius.InitRange(WaveRadius * 0.9f, WaveRadius * 1.05f);
				Shape->DiscCoverage.InitConstant(0.f);
				Shape->ShapeRotation.InitConstant(FRotator3f(90.f, 0.f, 0.f));
			}
			if (UNiagaraStatelessModule_AddVelocity* const Velocity = Get<UNiagaraStatelessModule_AddVelocity>(*Emitter))
			{
				Velocity->VelocityType = ENSM_VelocityType::FromPoint;
				Velocity->CoordinateSpace = ENiagaraCoordinateSpace::Local;
				Velocity->PointOrigin = FVector3f(WaveRadius * (Index == 1 ? 3.3f : 5.f), 0.f, 0.f);
				Velocity->PointVelocityDistribution.InitRange(Index == 1 ? 1500.f : 500.f, Index == 1 ? 2500.f : 900.f);
				Velocity->PointVelocityScale.InitConstant(1.f);
			}
			if (UNiagaraStatelessModule_InitializeParticle* const Init = Get<UNiagaraStatelessModule_InitializeParticle>(*Emitter))
			{
				Init->ColorDistribution = FNiagaraDistributionColor(Tint);
			}
		}
		Emitter->PostEditChange();
	}
	return Finish(System, false);
}

namespace knightMonsterFX
{
	/** 링(RingRotation으로 돌린 원)에서 생겨 PointOrigin에서 멀어지는 방향으로 나간다. Speed가 음수면 그 점으로 빨려 든다. */
	void SetRingFromPoint(UNiagaraStatelessEmitter& Emitter, FVector2f RingRadius, const FRotator3f& RingRotation, const FVector3f& PointOrigin,
		FVector2f Speed, const FVector3f& Offset = FVector3f::ZeroVector)
	{
		if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(Emitter))
		{
			Shape->ShapePrimitive = ENSM_ShapePrimitive::Ring;
			Shape->RingRadius.InitRange(RingRadius.X, RingRadius.Y);
			Shape->DiscCoverage.InitConstant(0.f);
			Shape->ShapeRotation.InitConstant(RingRotation);
			Shape->TransformOffset = FNiagaraDistributionPosition(Offset);
		}
		if (UNiagaraStatelessModule_AddVelocity* const Velocity = Get<UNiagaraStatelessModule_AddVelocity>(Emitter))
		{
			Velocity->VelocityType = ENSM_VelocityType::FromPoint;
			Velocity->CoordinateSpace = ENiagaraCoordinateSpace::Local;
			Velocity->PointOrigin = PointOrigin;
			Velocity->PointVelocityDistribution.InitRange(Speed.X, Speed.Y);
			Velocity->PointVelocityScale.InitConstant(1.f);
		}
	}

	void SetTint(UNiagaraStatelessEmitter& Emitter, const FLinearColor& Tint)
	{
		if (UNiagaraStatelessModule_InitializeParticle* const Init = Get<UNiagaraStatelessModule_InitializeParticle>(Emitter))
		{
			Init->ColorDistribution = FNiagaraDistributionColor(Tint);
		}
	}
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildSonicChargeSystem(const FString& PackagePath, UMaterialInterface* GlowMaterial,
	UMaterialInterface* StreakMaterial, UMaterialInterface* WispMaterial, float Radius, FLinearColor Tint)
{
	using namespace knightMonsterFX;
	if (!GlowMaterial)
	{
		return nullptr;
	}
	// 빛: 가슴에서 짧게 깜빡이며 겹치는 옅은 빛(발광을 낮게, 하얗게 날아가지 않게).
	FSpriteLayer Glow;
	Glow.Name = TEXT("Glow");
	Glow.Material = GlowMaterial;
	Glow.Rate = 24.f;
	Glow.Life = FVector2f(0.25f, 0.4f);
	Glow.SizeMin = FVector2f(Radius * 0.6f);
	Glow.SizeMax = FVector2f(Radius * 0.9f);
	Glow.ColorKeys = { {0.f, 0.6f}, {1.f, 0.6f} };
	Glow.AlphaKeys = { {0.f, 0.f}, {0.3f, 0.45f}, {1.f, 0.f} };
	Glow.SizeKeys = { {0.f, 0.6f}, {1.f, 1.2f} };
	Glow.SpawnRadius = 15.f;
	Glow.SortOrder = 2;

	// 빨려 드는 바람결: 주위 링에서 생겨 가슴으로 모인다(속도 정렬, 수명 동안 거의 가슴까지 닿음).
	FSpriteLayer Streaks;
	Streaks.Name = TEXT("Inflow");
	Streaks.Material = StreakMaterial;
	Streaks.Rate = 150.f;
	Streaks.Life = FVector2f(0.28f, 0.38f);
	Streaks.SizeMin = FVector2f(5.f, 140.f);
	Streaks.SizeMax = FVector2f(10.f, 280.f);
	Streaks.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Streaks.AlphaKeys = { {0.f, 0.f}, {0.3f, 0.45f}, {0.8f, 0.3f}, {1.f, 0.f} };
	Streaks.SizeKeys = { {0.f, 0.5f}, {0.4f, 1.f}, {1.f, 0.6f} };
	Streaks.Alignment = ENiagaraSpriteAlignment::VelocityAligned;
	Streaks.SortOrder = 1;

	// 빨려 드는 연기 결: 같은 링보다 조금 바깥에서 천천히 모여든다.
	FSpriteLayer Wisps;
	Wisps.Name = TEXT("Wisps");
	Wisps.Material = WispMaterial;
	Wisps.Rate = 18.f;
	Wisps.Life = FVector2f(0.8f, 1.1f);
	Wisps.SizeMin = FVector2f(140.f);
	Wisps.SizeMax = FVector2f(240.f);
	Wisps.CurlStrength = 200.f;
	Wisps.CurlFrequency = 0.004f;
	Wisps.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Wisps.AlphaKeys = { {0.f, 0.f}, {0.4f, 0.14f}, {1.f, 0.f} };
	Wisps.SizeKeys = { {0.f, 1.2f}, {1.f, 0.5f} };
	Wisps.SortOrder = 0;

	const FSpriteLayer Layers[] = { Wisps, Streaks, Glow };
	const FVector Bounds(Radius * 1.6f);
	UNiagaraSystem* const System = BuildLayeredSystem(PackagePath, Layers, 0.f, 1.f, -Bounds, Bounds);
	if (!System)
	{
		return nullptr;
	}
	for (int32 Index = 0; Index < System->GetEmitterHandles().Num(); ++Index)
	{
		UNiagaraStatelessEmitter* const Emitter = System->GetEmitterHandle(Index).GetStatelessEmitter();
		if (!Emitter || !Layers[FMath::Min(Index, 2)].Material)
		{
			continue;
		}
		if (Index == 0)
		{
			// 수평 링 둘레에서 위아래로 조금 흩어진 자리. 수명 동안 링 반지름의 70%쯤 들어온다.
			SetRingFromPoint(*Emitter, FVector2f(Radius * 0.8f, Radius * 1.2f), FRotator3f(0.f, 0.f, 0.f), FVector3f::ZeroVector, FVector2f(-450.f, -650.f));
		}
		else if (Index == 1)
		{
			// 비스듬히 기운 링(가슴을 감싸는 여러 각도)에서 가슴으로 곧장 빨려 든다.
			SetRingFromPoint(*Emitter, FVector2f(Radius * 0.7f, Radius), FRotator3f(0.f, 0.f, 0.f), FVector3f::ZeroVector, FVector2f(-1500.f, -2000.f));
			if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
			{
				FNiagaraDistributionRangeRotator& Tilt = Shape->ShapeRotation;
				Tilt.Mode = ENiagaraDistributionMode::NonUniformRange;
				Tilt.Min = FRotator3f(-60.f, 0.f, -60.f);
				Tilt.Max = FRotator3f(60.f, 180.f, 60.f);
				Tilt.ChannelConstantsAndRanges = { -60.f, 0.f, -60.f, 60.f, 180.f, 60.f };
			}
		}
		SetTint(*Emitter, Tint);
		Emitter->PostEditChange();
	}
	return Finish(System, false);
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildSonicInflowSystem(const FString& PackagePath, UMaterialInterface* StreakMaterial,
	UMaterialInterface* WispMaterial, float Radius, float Height, bool bWater, FLinearColor Tint)
{
	using namespace knightMonsterFX;
	if (!WispMaterial || !StreakMaterial)
	{
		return nullptr;
	}
	// 먼지(물안개): 바닥 링에서 피어 가슴 쪽으로 모이며 오른다. 수명 동안 높이의 절반쯤 올라가고 위로 갈수록 옅어진다.
	FSpriteLayer Lift;
	Lift.Name = TEXT("Lift");
	Lift.Material = WispMaterial;
	Lift.Rate = 60.f;
	Lift.Life = FVector2f(0.9f, 1.3f);
	Lift.SizeMin = FVector2f(Radius * 0.5f);
	Lift.SizeMax = FVector2f(Radius * 0.9f);
	Lift.CurlStrength = 250.f;
	Lift.CurlFrequency = 0.003f;
	Lift.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Lift.AlphaKeys = { {0.f, 0.f}, {0.25f, bWater ? 0.3f : 0.35f}, {0.7f, 0.15f}, {1.f, 0.f} };
	Lift.SizeKeys = { {0.f, 0.7f}, {1.f, 1.4f} };
	Lift.SortOrder = 0;

	// 바람결: 바닥 둘레에서 가슴으로 곧게 빨려 오르는 가는 줄기(속도 정렬).
	FSpriteLayer Streaks;
	Streaks.Name = TEXT("Streaks");
	Streaks.Material = StreakMaterial;
	Streaks.Rate = 90.f;
	Streaks.Life = FVector2f(0.35f, 0.5f);
	Streaks.SizeMin = FVector2f(7.f, 200.f);
	Streaks.SizeMax = FVector2f(12.f, 400.f);
	Streaks.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Streaks.AlphaKeys = { {0.f, 0.f}, {0.3f, 0.3f}, {0.8f, 0.15f}, {1.f, 0.f} };
	Streaks.SizeKeys = { {0.f, 0.5f}, {0.4f, 1.f}, {1.f, 0.7f} };
	Streaks.Alignment = ENiagaraSpriteAlignment::VelocityAligned;
	Streaks.SortOrder = 1;

	// 알갱이(물방울): 바닥이 떨리듯 낮게 튀었다 떨어지는 작은 점들.
	FSpriteLayer Grains;
	Grains.Name = TEXT("Grains");
	Grains.Material = bWater ? StreakMaterial : WispMaterial;
	Grains.Rate = bWater ? 220.f : 90.f;
	Grains.Life = FVector2f(0.25f, 0.4f);
	Grains.SizeMin = bWater ? FVector2f(6.f, 14.f) : FVector2f(35.f);
	Grains.SizeMax = bWater ? FVector2f(12.f, 30.f) : FVector2f(70.f);
	Grains.Rise = -1200.f;
	Grains.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Grains.AlphaKeys = { {0.f, 0.f}, {0.15f, bWater ? 0.55f : 0.3f}, {1.f, 0.f} };
	Grains.SizeKeys = { {0.f, 1.f}, {1.f, bWater ? 0.7f : 1.5f} };
	Grains.Alignment = bWater ? ENiagaraSpriteAlignment::VelocityAligned : ENiagaraSpriteAlignment::Unaligned;
	Grains.SortOrder = 2;

	const FSpriteLayer Layers[] = { Lift, Streaks, Grains };
	const float Margin = Radius * 1.6f;
	UNiagaraSystem* const System = BuildLayeredSystem(PackagePath, Layers, 0.f, 1.f, FVector(-Margin, -Margin, -100.f), FVector(Margin, Margin, Height * 1.2f));
	if (!System)
	{
		return nullptr;
	}
	for (int32 Index = 0; Index < System->GetEmitterHandles().Num(); ++Index)
	{
		UNiagaraStatelessEmitter* const Emitter = System->GetEmitterHandle(Index).GetStatelessEmitter();
		if (!Emitter || !Layers[FMath::Min(Index, 2)].Material)
		{
			continue;
		}
		if (Index == 2)
		{
			// 바닥 원판 안에서 거의 곧장 위로 낮게(최고 50~150cm) 튄다.
			SetRingFromPoint(*Emitter, FVector2f(Radius * 0.1f, Radius), FRotator3f(0.f, 0.f, 0.f), FVector3f(0.f, 0.f, -Radius * 2.f),
				FVector2f(350.f, 600.f), FVector3f(0.f, 0.f, 5.f));
			if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
			{
				Shape->DiscCoverage.InitConstant(1.f);
			}
		}
		else
		{
			// 바닥 링에서 가슴(위 Height) 쪽 한 점으로 빨려 든다. 먼지는 넓게, 바람결은 그 안쪽에서.
			const bool bStreaks = Index == 1;
			SetRingFromPoint(*Emitter, bStreaks ? FVector2f(Radius * 0.5f, Radius * 0.9f) : FVector2f(Radius * 0.6f, Radius * 1.1f), FRotator3f(0.f, 0.f, 0.f),
				FVector3f(0.f, 0.f, Height), bStreaks ? FVector2f(-Height * 1.4f, -Height * 1.9f) : FVector2f(-Height * 0.4f, -Height * 0.55f),
				FVector3f(0.f, 0.f, bStreaks ? 40.f : 60.f));
		}
		SetTint(*Emitter, Tint);
		Emitter->PostEditChange();
	}
	return Finish(System, false);
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildSonicPillarSystem(const FString& PackagePath, UMaterialInterface* StreakMaterial,
	UMaterialInterface* WispMaterial, float Radius, float Length, FLinearColor Tint)
{
	using namespace knightMonsterFX;
	if (!StreakMaterial)
	{
		return nullptr;
	}
	// 바람결: 기둥 안 여러 높이에서 생겨 아래로, 조금 바깥으로 쏟아진다. 수명 동안 기둥 높이의 절반쯤 내려간다.
	FSpriteLayer Streaks;
	Streaks.Name = TEXT("Streaks");
	Streaks.Material = StreakMaterial;
	Streaks.Rate = 160.f;
	Streaks.Life = FVector2f(0.22f, 0.34f);
	Streaks.SizeMin = FVector2f(5.f, 160.f);
	Streaks.SizeMax = FVector2f(9.f, 360.f);
	Streaks.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Streaks.AlphaKeys = { {0.f, 0.f}, {0.2f, 0.14f}, {0.7f, 0.08f}, {1.f, 0.f} };
	Streaks.SizeKeys = { {0.f, 0.4f}, {0.35f, 1.f}, {1.f, 1.2f} };
	Streaks.Alignment = ENiagaraSpriteAlignment::VelocityAligned;
	Streaks.SortOrder = 1;

	// 연기 결: 기둥 둘레에서 천천히 내려오며 커지고 흩어진다.
	FSpriteLayer Wisps;
	Wisps.Name = TEXT("Wisps");
	Wisps.Material = WispMaterial;
	Wisps.Rate = 30.f;
	Wisps.Life = FVector2f(0.6f, 1.f);
	Wisps.SizeMin = FVector2f(Radius * 0.5f);
	Wisps.SizeMax = FVector2f(Radius * 0.9f);
	Wisps.CurlStrength = 300.f;
	Wisps.CurlFrequency = 0.003f;
	Wisps.Drag = 1.f;
	Wisps.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Wisps.AlphaKeys = { {0.f, 0.f}, {0.3f, 0.1f}, {1.f, 0.f} };
	Wisps.SizeKeys = { {0.f, 0.6f}, {1.f, 1.8f} };
	Wisps.SortOrder = 0;

	const FSpriteLayer Layers[] = { Wisps, Streaks };
	const float Margin = Radius * 2.f;
	UNiagaraSystem* const System = BuildLayeredSystem(PackagePath, Layers, 0.f, 1.f, FVector(-Margin, -Margin, -Length - Margin), FVector(Margin, Margin, Margin));
	if (!System)
	{
		return nullptr;
	}
	for (int32 Index = 0; Index < System->GetEmitterHandles().Num(); ++Index)
	{
		UNiagaraStatelessEmitter* const Emitter = System->GetEmitterHandle(Index).GetStatelessEmitter();
		if (!Emitter || !Layers[FMath::Min(Index, 1)].Material)
		{
			continue;
		}
		const bool bStreaks = Index == 1;
		if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
		{
			// 위 끝(0)부터 Length 아래까지의 원기둥 겉면 근처. 아래로 갈수록 기둥이 넓어지는 것은 퍼지는 속도로 맞춘다.
			Shape->ShapePrimitive = ENSM_ShapePrimitive::Cylinder;
			Shape->CylinderHeight.InitConstant(Length * (bStreaks ? 0.85f : 0.9f));
			Shape->CylinderRadius.InitRange(Radius * (bStreaks ? 0.35f : 0.5f), Radius * (bStreaks ? 0.7f : 0.8f));
			Shape->CylinderHeightMidpoint.InitConstant(0.5f);
			Shape->bCylinderSurfaceOnly = false;
			Shape->TransformOffset = FNiagaraDistributionPosition(FVector3f(0.f, 0.f, -Length * (bStreaks ? 0.45f : 0.5f)));
		}
		if (UNiagaraStatelessModule_AddVelocity* const Velocity = Get<UNiagaraStatelessModule_AddVelocity>(*Emitter))
		{
			// 기둥 위쪽 높은 한 점에서 멀어지는 방향: 아래로 쏟아지며 조금 바깥으로 벌어진다.
			Velocity->VelocityType = ENSM_VelocityType::FromPoint;
			Velocity->CoordinateSpace = ENiagaraCoordinateSpace::Local;
			Velocity->PointOrigin = FVector3f(0.f, 0.f, Length * 1.5f);
			Velocity->PointVelocityDistribution.InitRange(bStreaks ? Length * 1.6f : Length * 0.2f, bStreaks ? Length * 2.2f : Length * 0.35f);
			Velocity->PointVelocityScale.InitConstant(1.f);
		}
		SetTint(*Emitter, Tint);
		Emitter->PostEditChange();
	}
	return Finish(System, false);
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildSonicGroundSystem(const FString& PackagePath, UMaterialInterface* StreakMaterial,
	UMaterialInterface* WispMaterial, float Radius, bool bTrail, FLinearColor Tint)
{
	using namespace knightMonsterFX;
	if (!WispMaterial)
	{
		return nullptr;
	}
	// 바람결: 닿는 자리 링에서 바닥을 따라 바깥으로, 아주 조금 위로 쓸려 나간다.
	FSpriteLayer Streaks;
	Streaks.Name = TEXT("Streaks");
	Streaks.Material = bTrail ? nullptr : StreakMaterial;
	Streaks.Rate = 120.f;
	Streaks.Life = FVector2f(0.22f, 0.36f);
	Streaks.SizeMin = FVector2f(5.f, 90.f);
	Streaks.SizeMax = FVector2f(9.f, 200.f);
	Streaks.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Streaks.AlphaKeys = { {0.f, 0.f}, {0.2f, 0.16f}, {0.7f, 0.08f}, {1.f, 0.f} };
	Streaks.SizeKeys = { {0.f, 0.4f}, {0.35f, 1.f}, {1.f, 1.2f} };
	Streaks.Alignment = ENiagaraSpriteAlignment::VelocityAligned;
	Streaks.SortOrder = 1;

	// 연기 결: 바닥에서 피어올라 바깥으로 흩어진다. 자국(bTrail)은 더 오래, 더 옅게 남는다.
	FSpriteLayer Wisps;
	Wisps.Name = TEXT("Wisps");
	Wisps.Material = WispMaterial;
	Wisps.Rate = bTrail ? 60.f : 35.f;
	Wisps.Life = bTrail ? FVector2f(1.2f, 1.8f) : FVector2f(0.6f, 1.f);
	Wisps.SizeMin = FVector2f(Radius * 0.45f);
	Wisps.SizeMax = FVector2f(Radius * 0.8f);
	Wisps.CurlStrength = 300.f;
	Wisps.CurlFrequency = 0.003f;
	Wisps.Drag = 1.5f;
	Wisps.Rise = bTrail ? 60.f : 0.f;
	Wisps.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Wisps.AlphaKeys = { {0.f, 0.f}, {0.2f, bTrail ? 0.06f : 0.1f}, {1.f, 0.f} };
	Wisps.SizeKeys = { {0.f, 0.6f}, {1.f, 2.4f} };
	Wisps.SortOrder = 0;

	const FSpriteLayer Layers[] = { Wisps, Streaks };
	const float Margin = Radius * 3.f;
	UNiagaraSystem* const System = BuildLayeredSystem(PackagePath, Layers, 0.f, 1.f, FVector(-Margin, -Margin, -200.f), FVector(Margin, Margin, Radius * 2.f),
		bTrail ? 0.1f : 0.f);
	if (!System)
	{
		return nullptr;
	}
	for (int32 Index = 0; Index < System->GetEmitterHandles().Num(); ++Index)
	{
		UNiagaraStatelessEmitter* const Emitter = System->GetEmitterHandle(Index).GetStatelessEmitter();
		if (!Emitter || !Layers[FMath::Min(Index, 1)].Material)
		{
			continue;
		}
		const bool bStreaks = Index == 1;
		// 바닥 아래 한 점에서 멀어지는 방향: 바깥으로, 조금 위로.
		SetRingFromPoint(*Emitter, bStreaks ? FVector2f(Radius * 0.4f, Radius * 0.8f) : FVector2f(Radius * 0.3f, Radius * 0.9f), FRotator3f(0.f, 0.f, 0.f),
			FVector3f(0.f, 0.f, bStreaks ? -Radius * 0.25f : -Radius * 0.8f), bStreaks ? FVector2f(1500.f, 2400.f) : FVector2f(350.f, 700.f),
			FVector3f(0.f, 0.f, bStreaks ? 30.f : 60.f));
		SetTint(*Emitter, Tint);
		Emitter->PostEditChange();
	}
	return Finish(System, false);
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildSonicWaterSpraySystem(const FString& PackagePath, UMaterialInterface* SprayMaterial,
	UMaterialInterface* MistMaterial, float Width, FLinearColor Tint)
{
	using namespace knightMonsterFX;
	if (!SprayMaterial)
	{
		return nullptr;
	}
	// 물보라 막: 가는 물줄기(속도 정렬)가 닿는 자리 양옆에서 비스듬히 솟았다가 중력으로 떨어진다. 촘촘해서 막처럼 읽힌다.
	// 부드러운 결(MistMaterial)을 속도 방향으로 길게 늘여 물이 찢겨 나가는 막처럼 보이게 한다.
	FSpriteLayer Sheet;
	Sheet.Name = TEXT("Sheet");
	Sheet.Material = MistMaterial ? MistMaterial : SprayMaterial;
	Sheet.Rate = 160.f;
	Sheet.Life = FVector2f(0.4f, 0.65f);
	Sheet.SizeMin = FVector2f(50.f, 140.f);
	Sheet.SizeMax = FVector2f(90.f, 280.f);
	Sheet.Rise = -1800.f;
	Sheet.Drag = 0.6f;
	Sheet.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Sheet.AlphaKeys = { {0.f, 0.f}, {0.1f, 0.3f}, {0.6f, 0.2f}, {1.f, 0.f} };
	Sheet.SizeKeys = { {0.f, 0.4f}, {0.3f, 1.f}, {1.f, 1.3f} };
	Sheet.Alignment = ENiagaraSpriteAlignment::VelocityAligned;
	Sheet.SortOrder = 1;

	// 물방울: 막보다 높이, 멀리 튀는 작고 짧은 점들.
	FSpriteLayer Drops = Sheet;
	Drops.Name = TEXT("Drops");
	Drops.Material = SprayMaterial;
	Drops.Rate = 90.f;
	Drops.Life = FVector2f(0.5f, 0.8f);
	Drops.SizeMin = FVector2f(5.f, 10.f);
	Drops.SizeMax = FVector2f(9.f, 22.f);
	Drops.AlphaKeys = { {0.f, 0.f}, {0.1f, 0.35f}, {0.7f, 0.25f}, {1.f, 0.f} };
	Drops.SizeKeys = { {0.f, 1.f}, {1.f, 0.7f} };
	Drops.SortOrder = 2;

	// 물안개: 갈라진 물 위로 낮게 퍼지며 남는 옅은 안개.
	FSpriteLayer Mist;
	Mist.Name = TEXT("Mist");
	Mist.Material = MistMaterial;
	Mist.Rate = 40.f;
	Mist.Life = FVector2f(0.8f, 1.3f);
	Mist.SizeMin = FVector2f(Width * 0.5f);
	Mist.SizeMax = FVector2f(Width * 0.9f);
	Mist.CurlStrength = 200.f;
	Mist.CurlFrequency = 0.003f;
	Mist.Drag = 1.2f;
	Mist.Rise = 40.f;
	Mist.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Mist.AlphaKeys = { {0.f, 0.f}, {0.2f, 0.12f}, {1.f, 0.f} };
	Mist.SizeKeys = { {0.f, 0.6f}, {1.f, 2.2f} };
	Mist.SortOrder = 0;

	const FSpriteLayer Layers[] = { Mist, Sheet, Drops };
	const float Margin = Width * 3.f;
	UNiagaraSystem* const System = BuildLayeredSystem(PackagePath, Layers, 0.f, 1.f, FVector(-Margin, -Margin, -200.f), FVector(Margin, Margin, Width * 2.5f));
	if (!System)
	{
		return nullptr;
	}
	for (int32 Index = 0; Index < System->GetEmitterHandles().Num(); ++Index)
	{
		UNiagaraStatelessEmitter* const Emitter = System->GetEmitterHandle(Index).GetStatelessEmitter();
		if (!Emitter || !Layers[FMath::Min(Index, 2)].Material)
		{
			continue;
		}
		const bool bMist = Index == 0;
		if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
		{
			// 지나가는 방향(X)으로 얇고 옆(Y)으로 긴 띠. 가운데 아래 한 점에서 멀어지므로 +Y 쪽은 +Y로, -Y 쪽은 -Y로 갈라진다.
			Shape->ShapePrimitive = ENSM_ShapePrimitive::Box;
			Shape->BoxSize.InitConstant(FVector3f(Width * 0.4f, Width * (bMist ? 1.6f : 1.2f), 10.f));
			Shape->TransformOffset = FNiagaraDistributionPosition(FVector3f(-Width * 0.1f, 0.f, 10.f));
		}
		if (UNiagaraStatelessModule_AddVelocity* const Velocity = Get<UNiagaraStatelessModule_AddVelocity>(*Emitter))
		{
			Velocity->VelocityType = ENSM_VelocityType::FromPoint;
			Velocity->CoordinateSpace = ENiagaraCoordinateSpace::Local;
			// 원점이 깊을수록 위로 곧게 솟고, 얕을수록 옆으로 눕는다.
			Velocity->PointOrigin = FVector3f(Width * 0.3f, 0.f, bMist ? -Width * 0.3f : (Index == 1 ? -Width * 0.55f : -Width * 0.35f));
			Velocity->PointVelocityDistribution.InitRange(bMist ? 300.f : (Index == 1 ? 900.f : 1100.f), bMist ? 600.f : (Index == 1 ? 1500.f : 1900.f));
			Velocity->PointVelocityScale.InitConstant(1.f);
		}
		SetTint(*Emitter, Tint);
		Emitter->PostEditChange();
	}
	return Finish(System, false);
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildTornadoSystem(const FString& PackagePath, UMaterialInterface* SprayMaterial,
	UMaterialInterface* MistMaterial, float Radius, float Height, FLinearColor Tint)
{
	using namespace knightMonsterFX;
	if (!SprayMaterial || !MistMaterial)
	{
		return nullptr;
	}
	// 밑동 물안개: 수면에서 바깥으로 낮게 퍼지며 부푼다. 기둥 밑동을 감싸는 치마처럼 보인다.
	FSpriteLayer Skirt;
	Skirt.Name = TEXT("Skirt");
	Skirt.Material = MistMaterial;
	Skirt.Rate = 60.f;
	Skirt.Life = FVector2f(0.9f, 1.4f);
	Skirt.SizeMin = FVector2f(Radius * 0.7f);
	Skirt.SizeMax = FVector2f(Radius * 1.2f);
	Skirt.CurlStrength = 250.f;
	Skirt.CurlFrequency = 0.003f;
	Skirt.Drag = 1.2f;
	Skirt.Rise = 80.f;
	Skirt.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Skirt.AlphaKeys = { {0.f, 0.f}, {0.2f, 0.4f}, {0.7f, 0.2f}, {1.f, 0.f} };
	Skirt.SizeKeys = { {0.f, 0.6f}, {1.f, 1.8f} };
	Skirt.SortOrder = 1;

	// 물보라 막: 밑동에서 비스듬히 솟았다가 중력으로 떨어지는 물줄기. 부드러운 결을 속도 방향으로 늘여 막처럼 보이게 한다.
	FSpriteLayer Sheet;
	Sheet.Name = TEXT("Sheet");
	Sheet.Material = MistMaterial;
	Sheet.Rate = 180.f;
	Sheet.Life = FVector2f(0.5f, 0.8f);
	Sheet.SizeMin = FVector2f(50.f, 140.f);
	Sheet.SizeMax = FVector2f(90.f, 280.f);
	Sheet.Rise = -1600.f;
	Sheet.Drag = 0.5f;
	Sheet.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Sheet.AlphaKeys = { {0.f, 0.f}, {0.1f, 0.35f}, {0.6f, 0.2f}, {1.f, 0.f} };
	Sheet.SizeKeys = { {0.f, 0.4f}, {0.3f, 1.f}, {1.f, 1.3f} };
	Sheet.Alignment = ENiagaraSpriteAlignment::VelocityAligned;
	Sheet.SortOrder = 2;

	// 물방울: 막보다 높이, 멀리 튀는 작은 점.
	FSpriteLayer Drops = Sheet;
	Drops.Name = TEXT("Drops");
	Drops.Material = SprayMaterial;
	Drops.Rate = 70.f;
	Drops.Life = FVector2f(0.6f, 1.f);
	Drops.SizeMin = FVector2f(4.f, 8.f);
	Drops.SizeMax = FVector2f(7.f, 16.f);
	Drops.AlphaKeys = { {0.f, 0.f}, {0.1f, 0.22f}, {0.7f, 0.14f}, {1.f, 0.f} };
	Drops.SizeKeys = { {0.f, 1.f}, {1.f, 0.7f} };
	Drops.SortOrder = 3;

	// 기둥 물안개: 깔때기 안을 감아 오르는 옅은 물안개(컴포넌트 회전으로 돈다).
	FSpriteLayer Column;
	Column.Name = TEXT("Column");
	Column.Material = MistMaterial;
	Column.Rate = 45.f;
	Column.Life = FVector2f(0.8f, 1.2f);
	Column.SizeMin = FVector2f(Radius * 0.7f);
	Column.SizeMax = FVector2f(Radius * 1.1f);
	Column.CurlStrength = 150.f;
	Column.CurlFrequency = 0.004f;
	Column.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Column.AlphaKeys = { {0.f, 0.f}, {0.3f, 0.22f}, {1.f, 0.f} };
	Column.SizeKeys = { {0.f, 0.7f}, {1.f, 1.3f} };
	Column.SortOrder = 0;

	// 꼭대기 물안개: 깔때기 위 끝에서 바깥으로 천천히 흩어진다.
	FSpriteLayer Top;
	Top.Name = TEXT("Top");
	Top.Material = MistMaterial;
	Top.Rate = 16.f;
	Top.Life = FVector2f(1.2f, 1.8f);
	Top.SizeMin = FVector2f(Radius * 0.9f);
	Top.SizeMax = FVector2f(Radius * 1.5f);
	Top.CurlStrength = 200.f;
	Top.CurlFrequency = 0.002f;
	Top.Drag = 0.8f;
	Top.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Top.AlphaKeys = { {0.f, 0.f}, {0.3f, 0.12f}, {1.f, 0.f} };
	Top.SizeKeys = { {0.f, 0.7f}, {1.f, 1.6f} };
	Top.SortOrder = 0;

	// 감아 오르는 물줄기: 밑동 둘레에서 위로 세차게 치솟는 긴 물줄기(속도 정렬). 컴포넌트 회전으로 기둥을 따라 돈다.
	FSpriteLayer Climb;
	Climb.Name = TEXT("Climb");
	Climb.Material = MistMaterial;
	Climb.Rate = 70.f;
	Climb.Life = FVector2f(0.5f, 0.8f);
	Climb.SizeMin = FVector2f(40.f, 260.f);
	Climb.SizeMax = FVector2f(80.f, 520.f);
	Climb.Rise = -400.f;
	Climb.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Climb.AlphaKeys = { {0.f, 0.f}, {0.15f, 0.3f}, {0.7f, 0.18f}, {1.f, 0.f} };
	Climb.SizeKeys = { {0.f, 0.6f}, {0.3f, 1.f}, {1.f, 1.2f} };
	Climb.Alignment = ENiagaraSpriteAlignment::VelocityAligned;
	Climb.SortOrder = 2;

	// 떨어져 나가는 물: 기둥 겉에서 바깥으로 내던져져 떨어지는 물방울. 힘이 실린 회오리처럼 보이게 한다.
	FSpriteLayer Shed = Drops;
	Shed.Name = TEXT("Shed");
	Shed.Rate = 140.f;
	Shed.Life = FVector2f(0.6f, 1.f);
	Shed.Rise = -1400.f;
	Shed.SizeMin = FVector2f(5.f, 12.f);
	Shed.SizeMax = FVector2f(9.f, 24.f);
	Shed.AlphaKeys = { {0.f, 0.f}, {0.1f, 0.3f}, {0.7f, 0.18f}, {1.f, 0.f} };

	const FSpriteLayer Layers[] = { Skirt, Sheet, Drops, Column, Top, Climb, Shed };
	const float Margin = Radius * 3.f;
	UNiagaraSystem* const System = BuildLayeredSystem(PackagePath, Layers, 0.f, 1.f, FVector(-Margin, -Margin, -200.f), FVector(Margin, Margin, Height * 1.4f));
	if (!System)
	{
		return nullptr;
	}
	for (int32 Index = 0; Index < System->GetEmitterHandles().Num(); ++Index)
	{
		UNiagaraStatelessEmitter* const Emitter = System->GetEmitterHandle(Index).GetStatelessEmitter();
		if (!Emitter || !Layers[FMath::Min(Index, 6)].Material)
		{
			continue;
		}
		switch (Index)
		{
		case 5:
			// 밑동 링에서 아주 깊은 한 점에서 멀어지는 방향: 거의 곧장 위로, 수명 동안 기둥 높이의 40~70%를 오른다.
			SetRingFromPoint(*Emitter, FVector2f(Radius * 0.35f, Radius * 0.6f), FRotator3f::ZeroRotator, FVector3f(0.f, 0.f, -Height * 3.f),
				FVector2f(Height * 0.9f, Height * 1.3f), FVector3f(0.f, 0.f, 40.f));
			break;
		case 6:
			if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
			{
				Shape->ShapePrimitive = ENSM_ShapePrimitive::Cylinder;
				Shape->CylinderHeight.InitConstant(Height * 0.7f);
				Shape->CylinderRadius.InitRange(Radius * 0.45f, Radius * 0.65f);
				Shape->CylinderHeightMidpoint.InitConstant(0.5f);
				Shape->bCylinderSurfaceOnly = false;
				Shape->TransformOffset = FNiagaraDistributionPosition(FVector3f(0.f, 0.f, Height * 0.4f));
			}
			if (UNiagaraStatelessModule_AddVelocity* const Velocity = Get<UNiagaraStatelessModule_AddVelocity>(*Emitter))
			{
				// 기둥 가운데 아래 한 점에서 멀어지는 방향: 바깥으로, 조금 위로 내던져진다.
				Velocity->VelocityType = ENSM_VelocityType::FromPoint;
				Velocity->CoordinateSpace = ENiagaraCoordinateSpace::Local;
				Velocity->PointOrigin = FVector3f(0.f, 0.f, Height * 0.2f);
				Velocity->PointVelocityDistribution.InitRange(700.f, 1200.f);
				Velocity->PointVelocityScale.InitConstant(1.f);
			}
			break;
		case 0:
			// 밑동 링에서 바깥으로, 조금 위로.
			SetRingFromPoint(*Emitter, FVector2f(Radius * 0.4f, Radius * 0.9f), FRotator3f::ZeroRotator, FVector3f(0.f, 0.f, -Radius * 0.6f),
				FVector2f(300.f, 600.f), FVector3f(0.f, 0.f, 30.f));
			break;
		case 1:
		case 2:
			// 원점이 깊을수록 위로 곧게 솟는다. 물방울이 막보다 높고 멀리 간다.
			SetRingFromPoint(*Emitter, FVector2f(Radius * 0.3f, Radius * 0.6f), FRotator3f::ZeroRotator, FVector3f(0.f, 0.f, -Radius * 0.7f),
				Index == 1 ? FVector2f(800.f, 1200.f) : FVector2f(700.f, 1100.f), FVector3f(0.f, 0.f, 20.f));
			break;
		case 3:
			if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
			{
				// 깔때기 아래 70% 높이, 판정 반지름 절반 안쪽.
				Shape->ShapePrimitive = ENSM_ShapePrimitive::Cylinder;
				Shape->CylinderHeight.InitConstant(Height * 0.7f);
				Shape->CylinderRadius.InitRange(Radius * 0.15f, Radius * 0.45f);
				Shape->CylinderHeightMidpoint.InitConstant(0.5f);
				Shape->bCylinderSurfaceOnly = false;
				Shape->TransformOffset = FNiagaraDistributionPosition(FVector3f(0.f, 0.f, Height * 0.4f));
			}
			if (UNiagaraStatelessModule_AddVelocity* const Velocity = Get<UNiagaraStatelessModule_AddVelocity>(*Emitter))
			{
				// 깊은 아래 한 점에서 멀어지는 방향: 거의 위로, 위로 갈수록 조금 벌어진다.
				Velocity->VelocityType = ENSM_VelocityType::FromPoint;
				Velocity->CoordinateSpace = ENiagaraCoordinateSpace::Local;
				Velocity->PointOrigin = FVector3f(0.f, 0.f, -Height);
				Velocity->PointVelocityDistribution.InitRange(Height * 0.3f, Height * 0.5f);
				Velocity->PointVelocityScale.InitConstant(1.f);
			}
			break;
		default:
			SetRingFromPoint(*Emitter, FVector2f(Radius * 0.8f, Radius * 1.3f), FRotator3f::ZeroRotator, FVector3f(0.f, 0.f, Height * 0.8f),
				FVector2f(150.f, 300.f), FVector3f(0.f, 0.f, Height));
			break;
		}
		SetTint(*Emitter, Tint);
		Emitter->PostEditChange();
	}
	return Finish(System, false);
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildTornadoFormSystem(const FString& PackagePath, UMaterialInterface* SprayMaterial,
	UMaterialInterface* MistMaterial, float Radius, FLinearColor Tint)
{
	using namespace knightMonsterFX;
	if (!SprayMaterial || !MistMaterial)
	{
		return nullptr;
	}
	// 물보라 고리: 소용돌이 둘레에서 낮게 솟았다 떨어지는 물 벽(컴포넌트 회전으로 고리가 돈다).
	FSpriteLayer Ring;
	Ring.Name = TEXT("Ring");
	Ring.Material = MistMaterial;
	Ring.Rate = 300.f;
	Ring.Life = FVector2f(0.5f, 0.8f);
	Ring.SizeMin = FVector2f(70.f, 150.f);
	Ring.SizeMax = FVector2f(120.f, 300.f);
	Ring.Rise = -1400.f;
	Ring.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Ring.AlphaKeys = { {0.f, 0.f}, {0.15f, 0.35f}, {0.7f, 0.2f}, {1.f, 0.f} };
	Ring.SizeKeys = { {0.f, 0.5f}, {0.4f, 1.f}, {1.f, 1.2f} };
	Ring.Alignment = ENiagaraSpriteAlignment::VelocityAligned;
	Ring.SortOrder = 1;

	// 물방울: 고리에서 튀는 작은 점.
	FSpriteLayer Drops = Ring;
	Drops.Name = TEXT("Drops");
	Drops.Material = SprayMaterial;
	Drops.Rate = 90.f;
	Drops.SizeMin = FVector2f(4.f, 8.f);
	Drops.SizeMax = FVector2f(7.f, 16.f);
	Drops.AlphaKeys = { {0.f, 0.f}, {0.1f, 0.3f}, {0.7f, 0.15f}, {1.f, 0.f} };
	Drops.SizeKeys = { {0.f, 1.f}, {1.f, 0.7f} };
	Drops.SortOrder = 2;

	// 빨려 드는 물안개: 바깥에서 소용돌이 가운데로 수면을 따라 모인다.
	FSpriteLayer Inflow;
	Inflow.Name = TEXT("Inflow");
	Inflow.Material = MistMaterial;
	Inflow.Rate = 40.f;
	Inflow.Life = FVector2f(0.8f, 1.1f);
	Inflow.SizeMin = FVector2f(Radius * 0.5f);
	Inflow.SizeMax = FVector2f(Radius * 0.9f);
	Inflow.CurlStrength = 150.f;
	Inflow.CurlFrequency = 0.003f;
	Inflow.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Inflow.AlphaKeys = { {0.f, 0.f}, {0.4f, 0.3f}, {1.f, 0.f} };
	Inflow.SizeKeys = { {0.f, 1.2f}, {1.f, 0.6f} };
	Inflow.SortOrder = 0;

	const FSpriteLayer Layers[] = { Inflow, Ring, Drops };
	const float Margin = Radius * 3.f;
	UNiagaraSystem* const System = BuildLayeredSystem(PackagePath, Layers, 0.f, 1.f, FVector(-Margin, -Margin, -200.f), FVector(Margin, Margin, Radius * 2.f));
	if (!System)
	{
		return nullptr;
	}
	for (int32 Index = 0; Index < System->GetEmitterHandles().Num(); ++Index)
	{
		UNiagaraStatelessEmitter* const Emitter = System->GetEmitterHandle(Index).GetStatelessEmitter();
		if (!Emitter || !Layers[FMath::Min(Index, 2)].Material)
		{
			continue;
		}
		if (Index == 0)
		{
			SetRingFromPoint(*Emitter, FVector2f(Radius * 1.3f, Radius * 1.8f), FRotator3f::ZeroRotator, FVector3f(0.f, 0.f, 20.f),
				FVector2f(-Radius * 1.0f, -Radius * 1.4f), FVector3f(0.f, 0.f, 30.f));
		}
		else
		{
			// 고리 둘레의 깊은 한 점에서 멀어지는 방향: 거의 위로 낮게. 물방울은 조금 더 높이.
			SetRingFromPoint(*Emitter, FVector2f(Radius * 0.7f, Radius * 1.0f), FRotator3f::ZeroRotator, FVector3f(0.f, 0.f, -Radius * 1.5f),
				Index == 1 ? FVector2f(450.f, 750.f) : FVector2f(650.f, 1000.f), FVector3f(0.f, 0.f, 10.f));
		}
		SetTint(*Emitter, Tint);
		Emitter->PostEditChange();
	}
	return Finish(System, false);
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildTornadoBurstSystem(const FString& PackagePath, UMaterialInterface* SprayMaterial,
	UMaterialInterface* MistMaterial, float Radius, float Height, bool bCollapse, FLinearColor Tint)
{
	using namespace knightMonsterFX;
	if (!SprayMaterial || !MistMaterial)
	{
		return nullptr;
	}
	// 물 막: 터짐은 밑동에서 바깥·위로 크게 솟구치고, 쏟아짐은 기둥 높이에서 바깥으로 벌어지며 떨어진다.
	FSpriteLayer Sheet;
	Sheet.Name = TEXT("Sheet");
	Sheet.Material = MistMaterial;
	Sheet.Rate = bCollapse ? 300.f : 1000.f;
	Sheet.Life = bCollapse ? FVector2f(0.8f, 1.3f) : FVector2f(0.8f, 1.2f);
	Sheet.SizeMin = FVector2f(80.f, 220.f);
	Sheet.SizeMax = FVector2f(160.f, 480.f);
	Sheet.Rise = -1800.f;
	Sheet.Drag = 0.4f;
	Sheet.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Sheet.AlphaKeys = { {0.f, 0.f}, {0.08f, 0.4f}, {0.6f, 0.22f}, {1.f, 0.f} };
	Sheet.SizeKeys = { {0.f, 0.5f}, {0.3f, 1.f}, {1.f, 1.3f} };
	Sheet.Alignment = ENiagaraSpriteAlignment::VelocityAligned;
	Sheet.SortOrder = 1;

	FSpriteLayer Drops = Sheet;
	Drops.Name = TEXT("Drops");
	Drops.Material = SprayMaterial;
	Drops.Rate = bCollapse ? 500.f : 900.f;
	Drops.SizeMin = FVector2f(6.f, 12.f);
	Drops.SizeMax = FVector2f(10.f, 26.f);
	Drops.AlphaKeys = { {0.f, 0.f}, {0.08f, 0.35f}, {0.7f, 0.2f}, {1.f, 0.f} };
	Drops.SizeKeys = { {0.f, 1.f}, {1.f, 0.7f} };
	Drops.SortOrder = 2;

	// 물안개: 낮게 넓게 퍼지며 오래 남는다.
	FSpriteLayer Mist;
	Mist.Name = TEXT("Mist");
	Mist.Material = MistMaterial;
	Mist.Rate = bCollapse ? 160.f : 260.f;
	Mist.Life = FVector2f(1.6f, 2.4f);
	Mist.SizeMin = FVector2f(Radius * 1.3f);
	Mist.SizeMax = FVector2f(Radius * 2.f);
	Mist.CurlStrength = 200.f;
	Mist.CurlFrequency = 0.002f;
	Mist.Drag = 1.2f;
	Mist.Rise = 40.f;
	Mist.ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Mist.AlphaKeys = { {0.f, 0.f}, {0.15f, 0.25f}, {1.f, 0.f} };
	Mist.SizeKeys = { {0.f, 0.6f}, {1.f, 2.f} };
	Mist.SortOrder = 0;

	const FSpriteLayer Layers[] = { Mist, Sheet, Drops };
	const float Margin = Radius * 5.f;
	UNiagaraSystem* const System = BuildLayeredSystem(PackagePath, Layers, 0.f, 1.f, FVector(-Margin, -Margin, -200.f), FVector(Margin, Margin, Height * 1.3f),
		bCollapse ? 0.35f : 0.1f);
	if (!System)
	{
		return nullptr;
	}
	for (int32 Index = 0; Index < System->GetEmitterHandles().Num(); ++Index)
	{
		UNiagaraStatelessEmitter* const Emitter = System->GetEmitterHandle(Index).GetStatelessEmitter();
		if (!Emitter || !Layers[FMath::Min(Index, 2)].Material)
		{
			continue;
		}
		if (Index == 0)
		{
			SetRingFromPoint(*Emitter, FVector2f(Radius * 0.5f, Radius * 1.2f), FRotator3f::ZeroRotator, FVector3f(0.f, 0.f, -Radius * 0.3f),
				FVector2f(500.f, 900.f), FVector3f(0.f, 0.f, 60.f));
		}
		else if (!bCollapse)
		{
			SetRingFromPoint(*Emitter, FVector2f(Radius * 0.3f, Radius * 0.7f), FRotator3f::ZeroRotator, FVector3f(0.f, 0.f, -Radius * 0.8f),
				Index == 1 ? FVector2f(1800.f, 2800.f) : FVector2f(2000.f, 3200.f), FVector3f(0.f, 0.f, 30.f));
		}
		else
		{
			if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
			{
				// 기둥 높이 전체에서 쏟아진다.
				Shape->ShapePrimitive = ENSM_ShapePrimitive::Cylinder;
				Shape->CylinderHeight.InitConstant(Height * 0.8f);
				Shape->CylinderRadius.InitRange(Radius * 0.3f, Radius * 0.7f);
				Shape->CylinderHeightMidpoint.InitConstant(0.5f);
				Shape->bCylinderSurfaceOnly = false;
				Shape->TransformOffset = FNiagaraDistributionPosition(FVector3f(0.f, 0.f, Height * 0.45f));
			}
			if (UNiagaraStatelessModule_AddVelocity* const Velocity = Get<UNiagaraStatelessModule_AddVelocity>(*Emitter))
			{
				Velocity->VelocityType = ENSM_VelocityType::FromPoint;
				Velocity->CoordinateSpace = ENiagaraCoordinateSpace::Local;
				Velocity->PointOrigin = FVector3f(0.f, 0.f, Height * 0.5f);
				Velocity->PointVelocityDistribution.InitRange(200.f, 500.f);
				Velocity->PointVelocityScale.InitConstant(1.f);
			}
		}
		SetTint(*Emitter, Tint);
		Emitter->PostEditChange();
	}
	return Finish(System, false);
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildSonicImpactSystem(const FString& PackagePath, UStaticMesh* WaveMesh, UMaterialInterface* RingMaterial,
	UMaterialInterface* ShardMaterial, UMaterialInterface* GustMaterial, UMaterialInterface* FlashMaterial, float Radius, int32 ShardCount,
	int32 GustCount, float RingGrow, bool bSideways)
{
	using namespace knightMonsterFX;
	if (!WaveMesh || !RingMaterial)
	{
		return nullptr;
	}
	constexpr float RingLife = 0.35f;

	FSpriteLayer Layers[4];
	// 파면 링: 부딪힌 파면이 납작하게 퍼지며 사라진다(배경을 한 번 크게 일그러뜨리는 충격파).
	Layers[0].Name = TEXT("Ring");
	Layers[0].Material = RingMaterial;
	Layers[0].Life = FVector2f(RingLife);
	Layers[0].SizeMin = Layers[0].SizeMax = FVector2f(1.f);
	Layers[0].ColorKeys = { {0.f, 1.f}, {1.f, 1.f} };
	Layers[0].AlphaKeys = { {0.f, 1.f}, {0.5f, 0.6f}, {1.f, 0.f} };
	Layers[0].SizeKeys = { {0.f, 1.f}, {1.f, 1.f} };

	// 파편: 테두리가 깨져 바깥으로 튀는 짧고 밝은 줄기. 금방 느려지며 사라진다.
	Layers[1].Name = TEXT("Shards");
	Layers[1].Material = ShardCount > 0 ? ShardMaterial : nullptr;
	Layers[1].Life = FVector2f(0.2f, 0.35f);
	Layers[1].SizeMin = FVector2f(10.f, 120.f);
	Layers[1].SizeMax = FVector2f(20.f, 300.f);
	Layers[1].Drag = 6.f;
	Layers[1].ColorKeys = { {0.f, 3.f}, {1.f, 1.5f} };
	Layers[1].AlphaKeys = { {0.f, 1.f}, {0.5f, 0.7f}, {1.f, 0.f} };
	Layers[1].SizeKeys = { {0.f, 1.f}, {1.f, 0.6f} };
	Layers[1].Alignment = ENiagaraSpriteAlignment::VelocityAligned;
	Layers[1].SortOrder = 2;

	// 돌풍: 파편 뒤로 회색 바람이 사방으로 터지며 퍼진다.
	Layers[2].Name = TEXT("Gust");
	Layers[2].Material = GustCount > 0 ? GustMaterial : nullptr;
	Layers[2].Life = FVector2f(0.45f, 0.8f);
	Layers[2].SizeMin = FVector2f(160.f);
	Layers[2].SizeMax = FVector2f(320.f);
	Layers[2].Drag = 3.f;
	Layers[2].CurlStrength = 400.f;
	Layers[2].CurlFrequency = 0.004f;
	Layers[2].ColorKeys = { {0.f, 2.f}, {1.f, 1.5f} };
	Layers[2].AlphaKeys = { {0.f, 0.f}, {0.15f, 0.35f}, {1.f, 0.f} };
	Layers[2].SizeKeys = { {0.f, 0.5f}, {1.f, 2.6f} };
	Layers[2].SortOrder = 1;

	Layers[3].Name = TEXT("Flash");
	Layers[3].Material = FlashMaterial;
	Layers[3].Life = FVector2f(0.1f, 0.12f);
	Layers[3].SizeMin = FVector2f(Radius * 2.2f);
	Layers[3].SizeMax = FVector2f(Radius * 2.6f);
	Layers[3].SpawnRadius = 0.f;
	Layers[3].ColorKeys = { {0.f, 3.f}, {1.f, 3.f} };
	Layers[3].AlphaKeys = { {0.f, 1.f}, {1.f, 0.f} };
	Layers[3].SizeKeys = { {0.f, 0.8f}, {1.f, 1.2f} };
	Layers[3].SortOrder = 3;

	const FVector Extent(Radius * 6.f);
	UNiagaraSystem* const System = BuildLayeredSystem(PackagePath, Layers, 0.f, 1.f, -Extent, Extent, 1.f);
	if (!System)
	{
		return nullptr;
	}
	const int32 Counts[4] = { 1, ShardCount, GustCount, 1 };
	for (int32 Index = 0; Index < FMath::Min(System->GetEmitterHandles().Num(), 4); ++Index)
	{
		UNiagaraStatelessEmitter* const Emitter = System->GetEmitterHandle(Index).GetStatelessEmitter();
		if (!Emitter || !Layers[Index].Material)
		{
			continue;
		}
		SetBurst(*Emitter, Counts[Index], Counts[Index], Layers[Index].Life.Y);
		if (Index == 0)
		{
			EnableOnly(*Emitter, { UNiagaraStatelessModule_InitializeParticle::StaticClass(), UNiagaraStatelessModule_ShapeLocation::StaticClass(),
				UNiagaraStatelessModule_ScaleColor::StaticClass(), UNiagaraStatelessModule_ScaleMeshSize::StaticClass() });
			if (UNiagaraStatelessModule_InitializeParticle* const Init = Get<UNiagaraStatelessModule_InitializeParticle>(*Emitter))
			{
				// 파면 메시(SM_Janhyang_SonicWave)는 테두리 반지름이 약 82cm다.
				Init->MeshScaleDistribution.InitConstant(FVector3f(Radius / 82.f));
			}
			if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
			{
				Shape->ShapePrimitive = ENSM_ShapePrimitive::Sphere;
				Shape->SphereRadius.InitConstant(0.f);
			}
			if (UNiagaraStatelessModule_ScaleMeshSize* const Size = Get<UNiagaraStatelessModule_ScaleMeshSize>(*Emitter))
			{
				// 앞뒤(X)로는 납작해지고 옆으로 빠르게 퍼진다.
				FNiagaraDistributionVector3& Curve = Size->ScaleDistribution;
				Curve.Mode = ENiagaraDistributionMode::NonUniformCurve;
				Curve.ChannelCurves.SetNum(3);
				SetCurve(Curve.ChannelCurves[0], { {0.f, 1.f}, {1.f, 0.35f} });
				for (int32 Channel = 1; Channel < 3; ++Channel)
				{
					SetCurve(Curve.ChannelCurves[Channel], { {0.f, 1.f}, {0.3f, 1.f + (RingGrow - 1.f) * 0.7f}, {1.f, RingGrow} });
				}
				Curve.UpdateValuesFromDistribution();
			}
			SetMeshRenderer(*Emitter, WaveMesh, RingMaterial);
		}
		else if (Index == 1 || Index == 2)
		{
			// 맞은 자리 둘레에서 생겨, 조금 뒤의 한 점에서 멀어지는 방향(바깥으로, 조금 앞으로)으로 튄다.
			// 기본은 날아온 축에 수직인 링(위아래·좌우 모두), bSideways면 좌우로 눌린 링이라 옆으로만 갈라진다.
			if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
			{
				Shape->ShapePrimitive = ENSM_ShapePrimitive::Ring;
				Shape->RingRadius.InitRange(Radius * 0.25f, Radius * 0.45f);
				Shape->DiscCoverage.InitConstant(0.f);
				Shape->ShapeRotation.InitConstant(bSideways ? FRotator3f::ZeroRotator : FRotator3f(90.f, 0.f, 0.f));
				Shape->ShapeScale.InitConstant(bSideways ? FVector3f(0.25f, 1.f, 1.f) : FVector3f::OneVector);
			}
			if (UNiagaraStatelessModule_AddVelocity* const Velocity = Get<UNiagaraStatelessModule_AddVelocity>(*Emitter))
			{
				Velocity->VelocityType = ENSM_VelocityType::FromPoint;
				Velocity->CoordinateSpace = ENiagaraCoordinateSpace::Local;
				Velocity->PointOrigin = FVector3f(-Radius * 0.6f, 0.f, 0.f);
				Velocity->PointVelocityDistribution.InitRange(Index == 1 ? 1800.f : 800.f, Index == 1 ? 3200.f : 1400.f);
				Velocity->PointVelocityScale.InitConstant(1.f);
			}
		}
		Emitter->PostEditChange();
	}
	return Finish(System, false);
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildFireBurstSystem(const FString& PackagePath, UMaterialInterface* Material, float Height)
{
	using namespace knightMonsterFX;
	bool bCreated = false;
	UNiagaraSystem* const System = Material ? LoadOrDuplicateTemplate(PackagePath, bCreated) : nullptr;
	if (!System)
	{
		return nullptr;
	}

	const float MaxLifetime = 0.8f;
	const float Speed = Height / (MaxLifetime * 0.6f);
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		UNiagaraStatelessEmitter* const Emitter = Handle.GetStatelessEmitter();
		if (!Emitter)
		{
			continue;
		}
		EnableOnly(*Emitter, { UNiagaraStatelessModule_InitializeParticle::StaticClass(), UNiagaraStatelessModule_ShapeLocation::StaticClass(),
			UNiagaraStatelessModule_AddVelocity::StaticClass(), UNiagaraStatelessModule_Drag::StaticClass(),
			UNiagaraStatelessModule_SpriteRotationRate::StaticClass(), UNiagaraStatelessModule_ScaleColor::StaticClass(),
			UNiagaraStatelessModule_ScaleSpriteSize::StaticClass() });
		SetBurst(*Emitter, 36, 44, MaxLifetime);

		if (UNiagaraStatelessModule_InitializeParticle* const Init = Get<UNiagaraStatelessModule_InitializeParticle>(*Emitter))
		{
			Init->LifetimeDistribution.InitRange(MaxLifetime * 0.5f, MaxLifetime);
			SetUniformRange(Init->SpriteSizeDistribution, Height * 0.15f, Height * 0.3f);
			Init->SpriteRotationDistribution.InitRange(-180.f, 180.f);
			Init->ColorDistribution = FNiagaraDistributionColor(FLinearColor::White);
		}
		if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
		{
			Shape->ShapePrimitive = ENSM_ShapePrimitive::Sphere;
			Shape->SphereRadius.InitConstant(Height * 0.2f);
		}
		// 위로 솟으며 옆으로 조금 퍼진다. 공기 저항으로 금방 느려진다.
		if (UNiagaraStatelessModule_AddVelocity* const Velocity = Get<UNiagaraStatelessModule_AddVelocity>(*Emitter))
		{
			const FVector3f Min(-Speed * 0.3f, -Speed * 0.3f, Speed * 0.5f);
			const FVector3f Max(Speed * 0.3f, Speed * 0.3f, Speed);
			Velocity->VelocityType = ENSM_VelocityType::Linear;
			Velocity->CoordinateSpace = ENiagaraCoordinateSpace::Local;
			Velocity->LinearVelocityDistribution.Mode = ENiagaraDistributionMode::NonUniformRange;
			Velocity->LinearVelocityDistribution.Min = Min;
			Velocity->LinearVelocityDistribution.Max = Max;
			Velocity->LinearVelocityDistribution.ChannelConstantsAndRanges = { Min.X, Min.Y, Min.Z, Max.X, Max.Y, Max.Z };
			Velocity->LinearVelocityScale.InitConstant(1.f);
		}
		if (UNiagaraStatelessModule_Drag* const Drag = Get<UNiagaraStatelessModule_Drag>(*Emitter))
		{
			Drag->DragDistribution.InitConstant(2.f);
		}
		if (UNiagaraStatelessModule_SpriteRotationRate* const Spin = Get<UNiagaraStatelessModule_SpriteRotationRate>(*Emitter))
		{
			Spin->RotationRateDistribution.InitRange(-120.f, 120.f);
		}
		SetFade(*Emitter, 0.08f, 1.f);
		if (UNiagaraStatelessModule_ScaleSpriteSize* const Size = Get<UNiagaraStatelessModule_ScaleSpriteSize>(*Emitter))
		{
			FNiagaraDistributionVector2& Curve = Size->ScaleDistribution;
			Curve.Mode = ENiagaraDistributionMode::NonUniformCurve;
			Curve.ChannelCurves.SetNum(2);
			for (int32 Channel = 0; Channel < 2; ++Channel)
			{
				SetCurve(Curve.ChannelCurves[Channel], { {0.f, 0.6f}, {0.3f, 1.f}, {1.f, 0.4f} });
			}
			Curve.UpdateValuesFromDistribution();
		}
		SetSprite(*Emitter, Material);

		if (FBox* const Bounds = GetProtected<FBox>(*Emitter, TEXT("FixedBounds"))) *Bounds = FBox(FVector(-Height, -Height, -Height * 0.5f), FVector(Height, Height, Height * 2.f));
		Emitter->PostEditChange();
	}
	return Finish(System, bCreated);
}

UNiagaraSystem* UknightMonsterFXBuilder::BuildShockwaveSystem(const FString& PackagePath, UMaterialInterface* Material, float Radius)
{
	using namespace knightMonsterFX;
	bool bCreated = false;
	UNiagaraSystem* const System = Material ? LoadOrDuplicateTemplate(PackagePath, bCreated) : nullptr;
	if (!System)
	{
		return nullptr;
	}

	const float MaxLifetime = 0.3f;
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		UNiagaraStatelessEmitter* const Emitter = Handle.GetStatelessEmitter();
		if (!Emitter)
		{
			continue;
		}
		EnableOnly(*Emitter, { UNiagaraStatelessModule_InitializeParticle::StaticClass(), UNiagaraStatelessModule_ShapeLocation::StaticClass(),
			UNiagaraStatelessModule_AddVelocity::StaticClass(), UNiagaraStatelessModule_Drag::StaticClass(),
			UNiagaraStatelessModule_ScaleColor::StaticClass(), UNiagaraStatelessModule_ScaleSpriteSize::StaticClass() });
		SetBurst(*Emitter, 26, 32, MaxLifetime);

		if (UNiagaraStatelessModule_InitializeParticle* const Init = Get<UNiagaraStatelessModule_InitializeParticle>(*Emitter))
		{
			Init->LifetimeDistribution.InitRange(MaxLifetime * 0.6f, MaxLifetime);
			// 속도 방향 정렬 스프라이트는 Y가 진행 방향이다.
			Init->SpriteSizeDistribution.Mode = ENiagaraDistributionMode::NonUniformRange;
			Init->SpriteSizeDistribution.Min = FVector2f(Radius * 0.04f, Radius * 0.2f);
			Init->SpriteSizeDistribution.Max = FVector2f(Radius * 0.07f, Radius * 0.35f);
			Init->SpriteSizeDistribution.ChannelConstantsAndRanges = { Radius * 0.04f, Radius * 0.2f, Radius * 0.07f, Radius * 0.35f };
			Init->ColorDistribution = FNiagaraDistributionColor(FLinearColor(1.f, 0.85f, 0.65f, 1.f));
		}
		if (UNiagaraStatelessModule_ShapeLocation* const Shape = Get<UNiagaraStatelessModule_ShapeLocation>(*Emitter))
		{
			Shape->ShapePrimitive = ENSM_ShapePrimitive::Ring;
			Shape->RingRadius.InitRange(Radius * 0.05f, Radius * 0.1f);
		}
		// 착탄점에서 수평으로 빠르게 퍼졌다가 멈춘다.
		if (UNiagaraStatelessModule_AddVelocity* const Velocity = Get<UNiagaraStatelessModule_AddVelocity>(*Emitter))
		{
			Velocity->VelocityType = ENSM_VelocityType::FromPoint;
			Velocity->CoordinateSpace = ENiagaraCoordinateSpace::Local;
			Velocity->PointOrigin = FVector3f(0.f, 0.f, -Radius * 0.02f);
			Velocity->PointVelocityDistribution.InitRange(Radius * 4.f, Radius * 6.f);
		}
		if (UNiagaraStatelessModule_Drag* const Drag = Get<UNiagaraStatelessModule_Drag>(*Emitter))
		{
			Drag->DragDistribution.InitConstant(6.f);
		}
		SetFade(*Emitter, 0.1f, 0.9f);
		if (UNiagaraStatelessModule_ScaleSpriteSize* const Size = Get<UNiagaraStatelessModule_ScaleSpriteSize>(*Emitter))
		{
			FNiagaraDistributionVector2& Curve = Size->ScaleDistribution;
			Curve.Mode = ENiagaraDistributionMode::NonUniformCurve;
			Curve.ChannelCurves.SetNum(2);
			SetCurve(Curve.ChannelCurves[0], { {0.f, 1.f}, {1.f, 0.5f} });
			SetCurve(Curve.ChannelCurves[1], { {0.f, 0.5f}, {0.4f, 1.f}, {1.f, 0.7f} });
			Curve.UpdateValuesFromDistribution();
		}
		SetSprite(*Emitter, Material, ENiagaraSpriteAlignment::VelocityAligned);

		if (FBox* const Bounds = GetProtected<FBox>(*Emitter, TEXT("FixedBounds"))) *Bounds = FBox(FVector(-Radius * 2.f, -Radius * 2.f, -Radius * 0.5f), FVector(Radius * 2.f, Radius * 2.f, Radius * 0.5f));
		Emitter->PostEditChange();
	}
	return Finish(System, bCreated);
}
