#include "knightAttackPreviewModel.h"

#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "PreviewScene.h"
#include "PrimitiveDrawingUtils.h"
#include "SceneManagement.h"
#include "knightAnimDebugSettings.h"

namespace knightAttackPreview
{
	/** 궤적 표본 간격(어빌리티 시간). 60fps 게임의 프레임 간격과 같다. */
	constexpr float TrailSampleInterval = 1.f / 60.f;
	/** 표본 사이를 몇 번 더 나눠 더미와 비교할지. 빠른 휘두르기가 표본 사이로 빠져나가지 않게 한다. */
	constexpr int32 HitSubSteps = 4;
	constexpr float StepFrameSeconds = 1.f / 30.f;
	constexpr float AreaFlashSeconds = 0.15f;
}

FknightAttackPreviewModel::FknightAttackPreviewModel(const TSharedRef<FPreviewScene>& InScene)
	: Scene(InScene)
{
	const UknightAnimDebugSettings* const Settings = GetDefault<UknightAnimDebugSettings>();
	DummyRadius = Settings->DummyRadius;
	DummyHalfHeight = Settings->DummyHalfHeight;
	RefreshMonsters();
}

FknightAttackPreviewModel::~FknightAttackPreviewModel()
{
	if (MeshComponent)
	{
		Scene->RemoveComponent(MeshComponent);
	}
}

void FknightAttackPreviewModel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(MeshComponent);
	Collector.AddReferencedObject(Montage);
	Collector.AddReferencedObjects(MonsterClasses);
	Collector.AddReferencedObjects(AbilityClassRefs);
}

void FknightAttackPreviewModel::RefreshMonsters()
{
	Notice.Reset();
	const TArray<UClass*> Found = knightAnimAbilityInfo::FindMonsterClasses(Notice);
	MonsterClasses.Reset();
	for (UClass* const Class : Found)
	{
		MonsterClasses.Add(Class);
	}
	SelectMonster(MonsterClasses.IsEmpty() ? INDEX_NONE : FMath::Clamp(MonsterIndex, 0, MonsterClasses.Num() - 1));
}

FString FknightAttackPreviewModel::GetMonsterName(int32 Index) const
{
	return MonsterClasses.IsValidIndex(Index) ? knightAnimAbilityInfo::CleanName(MonsterClasses[Index]) : FString();
}

void FknightAttackPreviewModel::SelectMonster(int32 Index)
{
	MonsterIndex = MonsterClasses.IsValidIndex(Index) ? Index : INDEX_NONE;
	// 공백 지도는 몬스터의 공격 목록에 따라 달라지므로 몬스터가 바뀌면 지운다.
	ClearGapMap();
	Abilities.Reset();
	AbilityClassRefs.Reset();
	MeshInfo = FknightMonsterMeshInfo();
	if (MonsterIndex != INDEX_NONE)
	{
		knightAnimAbilityInfo::FindPlayableAbilities(MonsterClasses[MonsterIndex], Abilities);
		for (const FknightAbilityAnimInfo& Ability : Abilities)
		{
			AbilityClassRefs.Add(Ability.AbilityClass);
		}
		MeshInfo = knightAnimAbilityInfo::GetMonsterMeshInfo(MonsterClasses[MonsterIndex]);
	}

	// 공격을 먼저 보여준다.
	int32 FirstAttack = Abilities.IndexOfByPredicate([](const FknightAbilityAnimInfo& Ability) { return Ability.bIsAttack; });
	AbilityIndex = FirstAttack != INDEX_NONE ? FirstAttack : (Abilities.IsEmpty() ? INDEX_NONE : 0);
	Rebuild();
}

void FknightAttackPreviewModel::SelectAbility(int32 Index)
{
	AbilityIndex = Abilities.IsValidIndex(Index) ? Index : INDEX_NONE;
	Rebuild();
}

bool FknightAttackPreviewModel::Select(const UClass* MonsterClass, const UClass* AbilityClass)
{
	const int32 NewMonster = MonsterClasses.IndexOfByKey(MonsterClass);
	if (NewMonster == INDEX_NONE)
	{
		return false;
	}
	SelectMonster(NewMonster);
	const int32 NewAbility = Abilities.IndexOfByPredicate([AbilityClass](const FknightAbilityAnimInfo& Ability) { return Ability.AbilityClass == AbilityClass; });
	if (NewAbility != INDEX_NONE)
	{
		SelectAbility(NewAbility);
	}
	return true;
}

void FknightAttackPreviewModel::Rebuild()
{
	const FknightAbilityAnimInfo* const Ability = GetAbility();
	Montage = Ability ? Ability->GetPrimaryMontage() : nullptr;
	MontageRate = knightAnimAbilityInfo::GetMontageRate(Montage);
	Windows.Reset();
	knightAnimAbilityInfo::GetHitWindows(Montage, Windows);
	Time = 0.f;
	bPlaying = false;

	// 메시는 몬스터가 바뀔 때마다 다시 만든다. 어빌리티만 바뀌면 애니메이션만 바꾼다.
	if (MeshComponent && MeshComponent->GetSkeletalMeshAsset() != MeshInfo.Mesh)
	{
		Scene->RemoveComponent(MeshComponent);
		MeshComponent = nullptr;
	}
	if (!MeshComponent && MeshInfo.Mesh)
	{
		MeshComponent = NewObject<USkeletalMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		MeshComponent->SetSkeletalMeshAsset(MeshInfo.Mesh);
		MeshComponent->bEnableUpdateRateOptimizations = false;
		MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		MeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Scene->AddComponent(MeshComponent, MeshInfo.MeshRelativeTransform * GetActorTransform());
	}
	if (MeshComponent)
	{
		MeshComponent->SetAnimation(Montage);
		MeshComponent->Stop();
	}

	BuildTrail();
	ApplyPose(Time);
	RecomputeHits();
}

FknightSelectionQuery FknightAttackPreviewModel::MakeQuery(float Distance, float Angle) const
{
	FknightSelectionQuery Query;
	Query.Distance = Distance;
	Query.AngleToTarget = FRotator::NormalizeAxis(Angle);
	Query.bTargetAirborne = bTargetAirborne;
	Query.MonsterTags = SimulatedTags;
	return Query;
}

void FknightAttackPreviewModel::RecomputeSelection()
{
	SelectionRows.Reset();
	const TSharedPtr<IknightAttackSelectionSimulator> Simulator = knightAnimDebugEditor::GetAttackSelectionSimulator();
	if (!Simulator.IsValid())
	{
		return;
	}
	const FknightSelectionQuery Query = MakeQuery(DummyDistance, DummyAngle);
	float TotalWeight = 0.f;
	for (int32 Index = 0; Index < Abilities.Num(); ++Index)
	{
		const FknightAbilityAnimInfo& Ability = Abilities[Index];
		if (!Ability.AbilityCDO || !Simulator->Supports(Ability.AbilityClass))
		{
			continue;
		}
		FknightSelectionRow& Row = SelectionRows.AddDefaulted_GetRef();
		Row.AbilityIndex = Index;
		Row.Result = Simulator->Evaluate(*Ability.AbilityCDO, Query);
		TotalWeight += Row.Result.bAvailable ? Row.Result.Weight : 0.f;
	}
	for (FknightSelectionRow& Row : SelectionRows)
	{
		Row.Probability = Row.Result.bAvailable && TotalWeight > 0.f ? Row.Result.Weight / TotalWeight : 0.f;
	}
}

void FknightAttackPreviewModel::ClearGapMap()
{
	GapCells.Reset();
	GapSummary.Reset();
}

void FknightAttackPreviewModel::BuildGapMap()
{
	ClearGapMap();
	const TSharedPtr<IknightAttackSelectionSimulator> Simulator = knightAnimDebugEditor::GetAttackSelectionSimulator();
	if (!Simulator.IsValid())
	{
		return;
	}

	TArray<const FknightAbilityAnimInfo*> Attacks;
	float MaxRange = 0.f;
	for (const FknightAbilityAnimInfo& Ability : Abilities)
	{
		if (Ability.AbilityCDO && Simulator->Supports(Ability.AbilityClass))
		{
			Attacks.Add(&Ability);
			MaxRange = FMath::Max(MaxRange, Simulator->GetShape(*Ability.AbilityCDO).MaxRange);
		}
	}
	if (Attacks.IsEmpty())
	{
		GapSummary = TEXT("공격 어빌리티가 없다.");
		return;
	}

	// 가장 먼 사거리(또는 진입 거리)보다 조금 더 바깥까지, 거리 약 60칸 × 각도 5도 간격으로 훑는다.
	const float Extent = FMath::Clamp(FMath::Max(MaxRange, EngageDistance) * 1.15f, 300.f, 5000.f);
	const float DistanceStep = FMath::Max(20.f, Extent / 60.f);
	constexpr float AngleStep = 5.f;
	int32 EmptyCells = 0;
	int32 EngageCells = 0;
	int32 EngageEmptyCells = 0;
	TArray<FVector2f> FrontGaps;
	for (float Distance = DistanceStep * 0.5f; Distance <= Extent; Distance += DistanceStep)
	{
		for (float Angle = -180.f; Angle < 180.f; Angle += AngleStep)
		{
			const FknightSelectionQuery Query = MakeQuery(Distance, Angle);
			int32 Count = 0;
			for (const FknightAbilityAnimInfo* const Attack : Attacks)
			{
				Count += Simulator->Evaluate(*Attack->AbilityCDO, Query).bAvailable ? 1 : 0;
			}
			FknightGapCell& Cell = GapCells.AddDefaulted_GetRef();
			Cell.Location = FRotator(0.f, Angle, 0.f).RotateVector(FVector::ForwardVector) * Distance + FVector(0.f, 0.f, 2.f);
			Cell.AvailableCount = Count;
			EmptyCells += Count == 0 ? 1 : 0;
			if (EngageDistance > 0.f && Distance <= EngageDistance)
			{
				++EngageCells;
				EngageEmptyCells += Count == 0 ? 1 : 0;
			}

			// 정면(0도)의 빈 거리 구간을 모아 요약에 쓴다.
			if (FMath::IsNearlyZero(Angle) && Count == 0)
			{
				if (!FrontGaps.IsEmpty() && FMath::IsNearlyEqual(FrontGaps.Last().Y, Distance - DistanceStep * 0.5f, 1.f))
				{
					FrontGaps.Last().Y = Distance + DistanceStep * 0.5f;
				}
				else
				{
					FrontGaps.Add(FVector2f(FMath::Max(0.f, Distance - DistanceStep * 0.5f), Distance + DistanceStep * 0.5f));
				}
			}
		}
	}

	TArray<FString> GapTexts;
	TArray<FString> EngageGapTexts;
	for (const FVector2f& Gap : FrontGaps)
	{
		GapTexts.Add(Gap.Y >= Extent ? FString::Printf(TEXT("%.0fcm 이상"), Gap.X) : FString::Printf(TEXT("%.0f~%.0fcm"), Gap.X, Gap.Y));
		// 진입 거리 안쪽만 잘라 낸 구간: 몬스터가 공격 상태에 들어왔는데 멈추는 거리다.
		if (EngageDistance > 0.f && Gap.X < EngageDistance)
		{
			EngageGapTexts.Add(FString::Printf(TEXT("%.0f~%.0fcm"), Gap.X, FMath::Min(Gap.Y, EngageDistance)));
		}
	}
	GapSummary = FString::Printf(TEXT("반경 %.0fcm 안 %.0f%%가 공백(빨강).\n정면 공백: %s"), Extent,
		100.f * EmptyCells / FMath::Max(1, GapCells.Num()), GapTexts.IsEmpty() ? TEXT("없음") : *FString::Join(GapTexts, TEXT(", ")));
	if (EngageDistance > 0.f)
	{
		GapSummary += FString::Printf(TEXT("\n\n진입 거리 %.0fcm 안(하늘색 원):\n정면 공백: %s\n전체의 %.0f%%가 공백 (옆·뒤는 Face 회전으로 대부분 해소된다)"),
			EngageDistance, EngageGapTexts.IsEmpty() ? TEXT("없음 (멈춤이 생기지 않는다)") : *FString::Join(EngageGapTexts, TEXT(", ")),
			100.f * EngageEmptyCells / FMath::Max(1, EngageCells));
	}
}

float FknightAttackPreviewModel::GetDuration() const
{
	const FknightAbilityAnimInfo* const Ability = GetAbility();
	const float AbilityLength = Ability ? Ability->PhaseTotal : 0.f;
	const float MontageLength = Montage ? Montage->GetPlayLength() / MontageRate : 0.f;
	const float Duration = FMath::Max(AbilityLength, MontageLength);
	return Duration > 0.f ? Duration : 1.f;
}

void FknightAttackPreviewModel::SetTime(float NewTime)
{
	Time = FMath::Clamp(NewTime, 0.f, GetDuration());
	ApplyPose(Time);
}

void FknightAttackPreviewModel::StepFrame(int32 Direction)
{
	bPlaying = false;
	SetTime(Time + Direction * knightAttackPreview::StepFrameSeconds);
}

void FknightAttackPreviewModel::Tick(float DeltaSeconds)
{
	if (!bPlaying)
	{
		return;
	}
	const float Duration = GetDuration();
	float NewTime = Time + DeltaSeconds * Speed;
	if (NewTime > Duration)
	{
		if (bLoop)
		{
			NewTime = FMath::Fmod(NewTime, Duration);
		}
		else
		{
			NewTime = Duration;
			bPlaying = false;
		}
	}
	SetTime(NewTime);
}

void FknightAttackPreviewModel::ApplyPose(float AbilityTime)
{
	if (!MeshComponent || !Montage)
	{
		return;
	}
	// 틱 함수 없이 RefreshBoneTransforms를 부르면 병렬 평가 없이 바로 포즈를 계산한다. 궤적 표본이 이 동작에 기댄다.
	MeshComponent->SetPosition(FMath::Clamp(AbilityTime * MontageRate, 0.f, Montage->GetPlayLength()), false);
	MeshComponent->TickAnimation(0.f, false);
	MeshComponent->RefreshBoneTransforms();
}

void FknightAttackPreviewModel::BuildTrail()
{
	Trail.Reset();
	if (!MeshComponent || !Montage)
	{
		return;
	}
	for (int32 WindowIndex = 0; WindowIndex < Windows.Num(); ++WindowIndex)
	{
		const FknightHitWindow& Window = Windows[WindowIndex];
		if (!Window.HasTraceShape())
		{
			continue;
		}
		if (!MeshComponent->DoesSocketExist(Window.StartSocket) || !MeshComponent->DoesSocketExist(Window.EndSocket))
		{
			Notice = FString::Printf(TEXT("%s의 소켓 %s/%s가 메시에 없어 궤적을 그리지 못했다."), *Window.Name, *Window.StartSocket.ToString(), *Window.EndSocket.ToString());
			continue;
		}
		const float StartTime = Window.Start / MontageRate;
		const float EndTime = Window.End / MontageRate;
		for (float SampleTime = StartTime; ; SampleTime += knightAttackPreview::TrailSampleInterval)
		{
			const float Clamped = FMath::Min(SampleTime, EndTime);
			ApplyPose(Clamped);
			FknightTrailSample& Sample = Trail.AddDefaulted_GetRef();
			Sample.Time = Clamped;
			Sample.Start = MeshComponent->GetSocketLocation(Window.StartSocket);
			Sample.End = MeshComponent->GetSocketLocation(Window.EndSocket);
			Sample.Radius = Window.Radius;
			Sample.Window = WindowIndex;
			if (Clamped >= EndTime)
			{
				break;
			}
		}
	}
}

FTransform FknightAttackPreviewModel::GetActorTransform() const
{
	// 캐릭터 원점은 캡슐 중심이므로 바닥(Z=0)에서 캡슐 반 높이만큼 올린다.
	return FTransform(FVector(0.f, 0.f, MeshInfo.CapsuleHalfHeight));
}

FVector FknightAttackPreviewModel::GetDummyCenter() const
{
	const FVector Direction = FRotator(0.f, DummyAngle, 0.f).RotateVector(FVector::ForwardVector);
	return Direction * DummyDistance + FVector(0.f, 0.f, DummyHalfHeight);
}

void FknightAttackPreviewModel::GetDummySegment(FVector& OutA, FVector& OutB) const
{
	const FVector Center = GetDummyCenter();
	const float SegmentHalf = FMath::Max(0.f, DummyHalfHeight - DummyRadius);
	OutA = Center - FVector(0.f, 0.f, SegmentHalf);
	OutB = Center + FVector(0.f, 0.f, SegmentHalf);
}

void FknightAttackPreviewModel::RecomputeHits()
{
	MeleeHitTime = -1.f;
	bAreaHit = false;

	FVector DummyA;
	FVector DummyB;
	GetDummySegment(DummyA, DummyB);

	// 근접: 표본 사이를 보간한 칼날 선분과 더미 캡슐 축 사이의 거리가 두 반지름의 합 이하이면 닿은 것이다.
	for (int32 Index = 0; Index < Trail.Num() && MeleeHitTime < 0.f; ++Index)
	{
		const FknightTrailSample& Current = Trail[Index];
		const bool bHasNext = Trail.IsValidIndex(Index + 1) && Trail[Index + 1].Window == Current.Window;
		const int32 Steps = bHasNext ? knightAttackPreview::HitSubSteps : 1;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float Alpha = static_cast<float>(Step) / Steps;
			const FknightTrailSample& Next = bHasNext ? Trail[Index + 1] : Current;
			const FVector BladeStart = FMath::Lerp(Current.Start, Next.Start, Alpha);
			const FVector BladeEnd = FMath::Lerp(Current.End, Next.End, Alpha);
			FVector OnBlade;
			FVector OnDummy;
			FMath::SegmentDistToSegmentSafe(BladeStart, BladeEnd, DummyA, DummyB, OnBlade, OnDummy);
			if (FVector::Dist(OnBlade, OnDummy) <= Current.Radius + DummyRadius)
			{
				MeleeHitTime = FMath::Lerp(Current.Time, Next.Time, Alpha);
				break;
			}
		}
	}

	// 범위: 구체 중심과 더미 캡슐 축 사이의 거리.
	const FknightAbilityAnimInfo* const Ability = GetAbility();
	if (Ability && Ability->bHasArea)
	{
		const FVector Center = GetActorTransform().TransformPosition(Ability->AreaOffset);
		bAreaHit = FMath::PointDistToSegment(Center, DummyA, DummyB) <= Ability->AreaRadius + DummyRadius;
	}

	RecomputeSelection();
}

FString FknightAttackPreviewModel::GetHitSummary() const
{
	const FknightAbilityAnimInfo* const Ability = GetAbility();
	if (!Ability)
	{
		return TEXT("어빌리티를 고른다.");
	}
	TArray<FString> Lines;
	if (!Trail.IsEmpty())
	{
		Lines.Add(MeleeHitTime >= 0.f ? FString::Printf(TEXT("근접 궤적: %.2fs에 명중"), MeleeHitTime) : FString(TEXT("근접 궤적: 빗나감")));
	}
	else if (!Windows.IsEmpty())
	{
		Lines.Add(TEXT("근접 궤적: 소켓을 읽지 못해 계산하지 못했다"));
	}
	if (Ability->bHasArea)
	{
		const FString HitText = Ability->HitEndTime > Ability->HitTime
			? FString::Printf(TEXT("%.2f~%.2fs"), Ability->HitTime, Ability->HitEndTime) : FString::Printf(TEXT("%.2fs"), Ability->HitTime);
		Lines.Add(bAreaHit ? FString::Printf(TEXT("범위 구체: 명중 (판정 %s)"), *HitText) : FString(TEXT("범위 구체: 빗나감")));
	}
	if (Lines.IsEmpty())
	{
		Lines.Add(TEXT("판정 정보가 없는 어빌리티다."));
	}
	return FString::Join(Lines, TEXT("\n"));
}

FString FknightAttackPreviewModel::DescribeForLog() const
{
	const FknightAbilityAnimInfo* const Ability = GetAbility();
	FString Text = FString::Printf(TEXT("%s / %s: mesh=%s montage=%s rate=%.2f duration=%.2f windows=%d trail=%d"),
		*GetMonsterName(MonsterIndex), Ability ? *Ability->Name : TEXT("-"), MeshComponent ? *GetNameSafe(MeshComponent->GetSkeletalMeshAsset()) : TEXT("-"),
		*GetNameSafe(Montage), MontageRate, GetDuration(), Windows.Num(), Trail.Num());
	if (!Trail.IsEmpty())
	{
		Text += FString::Printf(TEXT(" first(t=%.3f %s→%s) last(t=%.3f %s→%s)"), Trail[0].Time, *Trail[0].Start.ToCompactString(), *Trail[0].End.ToCompactString(),
			Trail.Last().Time, *Trail.Last().Start.ToCompactString(), *Trail.Last().End.ToCompactString());
	}
	Text += FString::Printf(TEXT(" dummy(d=%.0f a=%.0f) melee=%.3f area=%d"), DummyDistance, DummyAngle, MeleeHitTime, bAreaHit ? 1 : 0);
	for (const FknightSelectionRow& Row : SelectionRows)
	{
		Text += FString::Printf(TEXT(" sel[%s %s %.0f%%%s]"), *Abilities[Row.AbilityIndex].Name, Row.Result.bAvailable ? TEXT("ok") : TEXT("no"),
			Row.Probability * 100.f, Row.Result.Reason.IsEmpty() ? TEXT("") : *(TEXT(" ") + Row.Result.Reason));
	}
	if (!Notice.IsEmpty())
	{
		Text += TEXT(" notice=") + Notice;
	}
	return Text;
}

void FknightAttackPreviewModel::Draw(FPrimitiveDrawInterface* PDI) const
{
	const uint8 Depth = SDPG_Foreground;

	// 몬스터 정면 표시.
	PDI->DrawLine(FVector(0.f, 0.f, 2.f), FVector(150.f, 0.f, 2.f), FLinearColor(0.2f, 0.6f, 1.f), Depth, 2.f);

	if (bShowDummy)
	{
		const bool bHit = MeleeHitTime >= 0.f || bAreaHit;
		DrawWireCapsule(PDI, GetDummyCenter(), FVector::ForwardVector, FVector::RightVector, FVector::UpVector,
			bHit ? FLinearColor::Red : FLinearColor::Green, DummyRadius, DummyHalfHeight, 16, Depth, 1.5f);
	}

	if (bShowTrail && !Trail.IsEmpty())
	{
		// 지나간 표본은 진하게, 아직 안 온 표본은 옅게 그린다. 윈도우마다 현재 표본을 판정 캡슐로 그린다(양손 동시 판정).
		TMap<int32, const FknightTrailSample*> CurrentSamples;
		for (int32 Index = 0; Index < Trail.Num(); ++Index)
		{
			const FknightTrailSample& Sample = Trail[Index];
			const bool bPast = Sample.Time <= Time;
			const FLinearColor Color = bPast ? FLinearColor(1.f, 0.25f, 0.2f, 1.f) : FLinearColor(1.f, 0.6f, 0.5f, 0.35f);
			PDI->DrawLine(Sample.Start, Sample.End, Color, Depth, bPast ? 1.5f : 0.5f);
			if (Trail.IsValidIndex(Index + 1) && Trail[Index + 1].Window == Sample.Window)
			{
				PDI->DrawLine(Sample.Start, Trail[Index + 1].Start, Color, Depth, 0.5f);
				PDI->DrawLine(Sample.End, Trail[Index + 1].End, Color, Depth, 0.5f);
			}
			if (bPast && Time - Sample.Time < knightAttackPreview::TrailSampleInterval * 1.5f)
			{
				const FknightTrailSample*& Current = CurrentSamples.FindOrAdd(Sample.Window);
				if (!Current || Sample.Time >= Current->Time)
				{
					Current = &Sample;
				}
			}
		}
		for (const TPair<int32, const FknightTrailSample*>& Pair : CurrentSamples)
		{
			const FknightTrailSample* const CurrentSample = Pair.Value;
			const FVector Axis = CurrentSample->End - CurrentSample->Start;
			const FVector Z = Axis.GetSafeNormal();
			FVector X;
			FVector Y;
			Z.FindBestAxisVectors(X, Y);
			DrawWireCapsule(PDI, (CurrentSample->Start + CurrentSample->End) * 0.5f, X, Y, Z, FLinearColor::Yellow,
				CurrentSample->Radius, Axis.Size() * 0.5f + CurrentSample->Radius, 12, Depth, 1.5f);
		}
	}

	const FknightAbilityAnimInfo* const Ability = GetAbility();

	// 공격 진입 거리: StateTree가 Attack으로 넘어가는 거리(직접 입력).
	if (EngageDistance > 0.f)
	{
		DrawCircle(PDI, FVector(0.f, 0.f, 4.f), FVector::ForwardVector, FVector::RightVector, FLinearColor(0.2f, 0.8f, 1.f), EngageDistance, 64, Depth, 2.5f);
	}

	// 공백 지도: 고를 수 있는 공격이 없으면 빨강, 있으면 초록(많을수록 진하게).
	for (const FknightGapCell& Cell : GapCells)
	{
		const FLinearColor Color = Cell.AvailableCount == 0 ? FLinearColor(1.f, 0.1f, 0.1f)
			: FLinearColor(0.1f, 0.4f + 0.2f * FMath::Min(Cell.AvailableCount, 3), 0.15f);
		PDI->DrawPoint(Cell.Location, Color, Cell.AvailableCount == 0 ? 6.f : 4.f, SDPG_World);
	}

	// 고른 공격의 선택 범위: 최소·최대 사거리 호와 좌우 각도 경계. 더미가 범위 안이면 초록, 밖이면 노랑.
	const TSharedPtr<IknightAttackSelectionSimulator> Simulator = knightAnimDebugEditor::GetAttackSelectionSimulator();
	if (bShowSelectionShape && Simulator.IsValid() && Ability && Ability->AbilityCDO && Simulator->Supports(Ability->AbilityClass))
	{
		const FknightSelectionShape Shape = Simulator->GetShape(*Ability->AbilityCDO);
		const FknightSelectionRow* const Row = SelectionRows.FindByPredicate([this](const FknightSelectionRow& Candidate) { return Candidate.AbilityIndex == AbilityIndex; });
		const FLinearColor Color = Row && Row->Result.bAvailable ? FLinearColor(0.2f, 1.f, 0.4f) : FLinearColor(1.f, 0.85f, 0.2f);
		const float HalfAngle = FMath::Clamp(Shape.MaxAngle, 0.f, 180.f);
		const float InnerAngle = FMath::Clamp(Shape.MinAngle, 0.f, HalfAngle);
		auto ArcPoint = [](float Radius, float Angle) { return FRotator(0.f, Angle, 0.f).RotateVector(FVector::ForwardVector) * Radius + FVector(0.f, 0.f, 3.f); };
		// 허용 각도 구간: MinAngle이 0이면 [-Max, Max] 하나, 아니면 좌우 [Min, Max] 두 개.
		TArray<TPair<float, float>> Spans;
		if (InnerAngle <= 0.f)
		{
			Spans.Emplace(-HalfAngle, HalfAngle);
		}
		else
		{
			Spans.Emplace(-HalfAngle, -InnerAngle);
			Spans.Emplace(InnerAngle, HalfAngle);
		}
		for (const TPair<float, float>& Span : Spans)
		{
			const int32 Segments = FMath::Max(4, FMath::RoundToInt((Span.Value - Span.Key) / 5.f));
			for (const float Radius : { Shape.MinRange, Shape.MaxRange })
			{
				if (Radius <= 0.f)
				{
					continue;
				}
				for (int32 Segment = 0; Segment < Segments; ++Segment)
				{
					const float A0 = Span.Key + (Span.Value - Span.Key) * Segment / Segments;
					const float A1 = Span.Key + (Span.Value - Span.Key) * (Segment + 1) / Segments;
					PDI->DrawLine(ArcPoint(Radius, A0), ArcPoint(Radius, A1), Color, Depth, 2.f);
				}
			}
			for (const float Angle : { Span.Key, Span.Value })
			{
				// 전 방향(±180)의 뒤쪽 끝은 같은 선이라 그리지 않는다.
				if (FMath::Abs(Angle) < 180.f || InnerAngle > 0.f)
				{
					PDI->DrawLine(ArcPoint(Shape.MinRange, Angle), ArcPoint(Shape.MaxRange, Angle), Color, Depth, 2.f);
				}
			}
		}
	}

	if (bShowArea && Ability && Ability->bHasArea)
	{
		// 판정 전에는 주황, 판정 중(지속 판정 포함, 최소 잠깐)은 빨강, 그 뒤(후딜)는 회색.
		const FVector Center = GetActorTransform().TransformPosition(Ability->AreaOffset);
		FLinearColor Color(0.5f, 0.5f, 0.5f);
		float Thickness = 1.f;
		if (Ability->HitTime < 0.f || Time < Ability->HitTime)
		{
			Color = FLinearColor(1.f, 0.55f, 0.1f);
		}
		else if (Time < FMath::Max(Ability->HitTime + knightAttackPreview::AreaFlashSeconds, Ability->HitEndTime))
		{
			Color = FLinearColor::Red;
			Thickness = 3.f;
		}
		DrawWireSphere(PDI, Center, Color, Ability->AreaRadius, 24, Depth, Thickness);
		DrawCircle(PDI, FVector(Center.X, Center.Y, 1.f), FVector::ForwardVector, FVector::RightVector, Color, Ability->AreaRadius, 32, Depth, Thickness);
	}
}
