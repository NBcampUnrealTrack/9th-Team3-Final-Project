#include "GameplayDebuggerCategory_knightMonsterAnim.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "CanvasItem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "knightAnimDebugSettings.h"
#include "knightAnimDebugSubsystem.h"

namespace knightMonsterAnimCategory
{
	constexpr float BarWidth = 420.f;
	constexpr float BarHeight = 14.f;
	constexpr int32 PanelEventCount = 5;
	static const float TimeScaleSteps[] = { 1.f, 0.5f, 0.25f, 0.1f };

	static void DrawRect(FGameplayDebuggerCanvasContext& CanvasContext, float X, float Y, float Width, float Height, const FLinearColor& Color)
	{
		if (Width <= 0.f || Height <= 0.f)
		{
			return;
		}
		FCanvasTileItem Tile(FVector2D(X, Y), FVector2D(Width, Height), Color);
		Tile.BlendMode = SE_BLEND_Translucent;
		CanvasContext.DrawItem(Tile, X, Y);
	}

	static FLinearColor WithAlpha(const FColor& Color, float Alpha)
	{
		FLinearColor Linear(Color);
		Linear.A = Alpha;
		return Linear;
	}

	/** 판정 순간(어빌리티 시작 기준 초). 판정 표시가 있는 구간이 없으면 -1. */
	static float GetHitTime(const FknightAnimDebugAbilityRun& Run)
	{
		float Time = 0.f;
		for (const FknightAnimDebugPhase& Phase : Run.Phases)
		{
			Time += Phase.Duration;
			if (Phase.bHitAtEnd)
			{
				return Time;
			}
		}
		return -1.f;
	}

	/** "Root/Combat/Attack" → "Attack", "State.Monster.Enraged" → "Enraged". */
	static FString LastSegment(const FString& Text, const TCHAR* Separator)
	{
		int32 Index = INDEX_NONE;
		return Text.FindLastChar(*Separator, Index) ? Text.RightChop(Index + 1) : Text;
	}

	static FString FormatAttributes(const UAbilitySystemComponent* ASC)
	{
		FString Text;
		for (const FknightAnimDebugAttribute& Attribute : GetDefault<UknightAnimDebugSettings>()->Attributes)
		{
			float Value = 0.f;
			if (!UknightAnimDebugSubsystem::FindAttributeValue(ASC, Attribute.Name, Value))
			{
				continue;
			}
			float Max = 0.f;
			const bool bHasMax = UknightAnimDebugSubsystem::FindAttributeValue(ASC, Attribute.MaxName, Max);
			Text += FString::Printf(TEXT("%s%s %.0f%s"), Text.IsEmpty() ? TEXT("") : TEXT("  "), *Attribute.Name.ToString(), Value,
				bHasMax ? *FString::Printf(TEXT("/%.0f"), Max) : TEXT(""));
		}
		return Text;
	}

	static void GetShownTags(const UAbilitySystemComponent* ASC, TArray<FString>& OutTags)
	{
		if (!ASC)
		{
			return;
		}
		const TArray<FString>& Prefixes = GetDefault<UknightAnimDebugSettings>()->TagPrefixes;
		for (const FGameplayTag& Tag : ASC->GetOwnedGameplayTags())
		{
			const FString Name = Tag.ToString();
			for (const FString& Prefix : Prefixes)
			{
				if (Name.StartsWith(Prefix))
				{
					OutTags.Add(Name);
					break;
				}
			}
		}
	}
}

FGameplayDebuggerCategory_knightMonsterAnim::FGameplayDebuggerCategory_knightMonsterAnim()
{
	bShowOnlyWithDebugActor = false;
	CollectDataInterval = 0.f;

	using FConfig = FGameplayDebuggerInputHandlerConfig;
	const FGameplayDebuggerInputModifier Alt = FGameplayDebuggerInputModifier::Alt;
	BindKeyPress(FConfig(TEXT("SelectPrevious"), TEXT("One"), Alt), this, &FGameplayDebuggerCategory_knightMonsterAnim::OnSelectPrevious);
	BindKeyPress(FConfig(TEXT("SelectNext"), TEXT("Two"), Alt), this, &FGameplayDebuggerCategory_knightMonsterAnim::OnSelectNext);
	BindKeyPress(FConfig(TEXT("Play"), TEXT("Three"), Alt), this, &FGameplayDebuggerCategory_knightMonsterAnim::OnPlay);
	BindKeyPress(FConfig(TEXT("Repeat"), TEXT("Four"), Alt), this, &FGameplayDebuggerCategory_knightMonsterAnim::OnRepeat);
	BindKeyPress(FConfig(TEXT("FreezeAI"), TEXT("Five"), Alt), this, &FGameplayDebuggerCategory_knightMonsterAnim::OnFreezeAI);
	BindKeyPress(FConfig(TEXT("TimeScale"), TEXT("Six"), Alt), this, &FGameplayDebuggerCategory_knightMonsterAnim::OnTimeScale);
	BindKeyPress(FConfig(TEXT("Pause"), TEXT("Seven"), Alt), this, &FGameplayDebuggerCategory_knightMonsterAnim::OnPause);
	BindKeyPress(FConfig(TEXT("Step"), TEXT("Eight"), Alt), this, &FGameplayDebuggerCategory_knightMonsterAnim::OnStep);
	BindKeyPress(FConfig(TEXT("Record"), TEXT("Nine"), Alt), this, &FGameplayDebuggerCategory_knightMonsterAnim::OnRecord);
	BindKeyPress(FConfig(TEXT("OverheadAll"), TEXT("Zero"), Alt), this, &FGameplayDebuggerCategory_knightMonsterAnim::OnToggleOverheadAll);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_knightMonsterAnim::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_knightMonsterAnim());
}

UknightAnimDebugSubsystem* FGameplayDebuggerCategory_knightMonsterAnim::GetSubsystem() const
{
	const UWorld* const World = CachedWorld.Get();
	return World ? World->GetSubsystem<UknightAnimDebugSubsystem>() : nullptr;
}

FString FGameplayDebuggerCategory_knightMonsterAnim::GetSelectedAbility() const
{
	return AbilityNames.IsValidIndex(SelectedAbility) ? AbilityNames[SelectedAbility] : FString();
}

void FGameplayDebuggerCategory_knightMonsterAnim::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	if (UknightAnimDebugSubsystem* const Subsystem = OwnerPC ? UknightAnimDebugSubsystem::Get(OwnerPC) : nullptr)
	{
		Subsystem->SetFocusedPawn(Cast<APawn>(DebugActor));
	}
}

void FGameplayDebuggerCategory_knightMonsterAnim::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	UWorld* const World = OwnerPC ? OwnerPC->GetWorld() : nullptr;
	CachedWorld = World;
	UknightAnimDebugSubsystem* const Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		CanvasContext.Print(TEXT("{red}게임 월드에서만 동작한다 (Standalone, PIE, 리슨 서버 호스트)."));
		return;
	}
	Subsystem->MarkViewed();

	APawn* const Target = Subsystem->ResolveTarget();
	for (const FknightAnimDebugTrack& Track : Subsystem->GetTracks())
	{
		const bool bIsTarget = Track.Pawn.Get() == Target;
		if (Track.Pawn.IsValid() && (bOverheadForAll || bIsTarget))
		{
			DrawOverhead(CanvasContext, Track, bIsTarget);
		}
	}

	AbilityNames.Reset();
	if (Target)
	{
		Subsystem->GetPlayableAbilityNames(Target, AbilityNames);
		DrawTargetPanel(CanvasContext, *Subsystem, *Target);
		if (const FknightAnimDebugTrack* const Track = Subsystem->FindTrack(Target))
		{
			DrawWorldShapes(World, *Track);
		}
	}
	else
	{
		CanvasContext.Print(TEXT("{grey}추적 중인 몬스터가 없다. 프로젝트 설정 > Knight > Monster Anim Debug의 TrackedPawnClass를 확인한다."));
	}
	SelectedAbility = AbilityNames.IsEmpty() ? 0 : FMath::Clamp(SelectedAbility, 0, AbilityNames.Num() - 1);

	DrawPerf(CanvasContext, *Subsystem);
	DrawControls(CanvasContext, *Subsystem);
}

void FGameplayDebuggerCategory_knightMonsterAnim::DrawOverhead(FGameplayDebuggerCanvasContext& CanvasContext, const FknightAnimDebugTrack& Track, bool bIsTarget) const
{
	using namespace knightMonsterAnimCategory;

	const APawn* const Pawn = Track.Pawn.Get();
	const FVector Location = Pawn->GetActorLocation() + FVector(0.f, 0.f, Pawn->GetSimpleCollisionHalfHeight() + 40.f);
	if (!CanvasContext.IsLocationVisible(Location))
	{
		return;
	}

	TArray<FString> Parts;
	if (!Track.StatePath.IsEmpty())
	{
		Parts.Add(TEXT("[") + LastSegment(Track.StatePath, TEXT("/")) + TEXT("]"));
	}
	if (Track.bHasActiveRun)
	{
		const double Elapsed = CanvasContext.GetWorld()->GetTimeSeconds() - Track.ActiveRun.StartWorldTime;
		const float Total = Track.ActiveRun.GetTotalPhaseTime();
		Parts.Add(Total > 0.f
			? FString::Printf(TEXT("%s %d%%"), *Track.ActiveRun.Name, FMath::RoundToInt(FMath::Clamp(Elapsed / Total, 0.0, 1.0) * 100.0))
			: FString::Printf(TEXT("%s %.1fs"), *Track.ActiveRun.Name, Elapsed));
	}
	const UAnimInstance* const AnimInstance = UknightAnimDebugSubsystem::GetAnimInstance(Pawn);
	if (const UAnimMontage* const Montage = Track.Montage.Get(); Montage && AnimInstance && Montage->GetPlayLength() > 0.f)
	{
		Parts.Add(FString::Printf(TEXT("%s %d%%"), *Montage->GetName(),
			FMath::RoundToInt(AnimInstance->Montage_GetPosition(Montage) / Montage->GetPlayLength() * 100.f)));
	}
	const FString Attributes = FormatAttributes(Track.ASC.Get());
	if (!Attributes.IsEmpty())
	{
		Parts.Add(Attributes);
	}
	TArray<FString> Tags;
	GetShownTags(Track.ASC.Get(), Tags);
	for (const FString& Tag : Tags)
	{
		Parts.Add(LastSegment(Tag, TEXT(".")));
	}

	const FString Text = FString::Join(Parts, TEXT(" | "));
	float Width = 0.f;
	float Height = 0.f;
	CanvasContext.MeasureString(Text, Width, Height);
	const FVector2D ScreenLocation = CanvasContext.ProjectLocation(Location);
	CanvasContext.PrintAt(ScreenLocation.X - Width * 0.5f, ScreenLocation.Y - Height, bIsTarget ? FColor::Yellow : FColor::White, Text);
}

void FGameplayDebuggerCategory_knightMonsterAnim::DrawTargetPanel(FGameplayDebuggerCanvasContext& CanvasContext, UknightAnimDebugSubsystem& Subsystem, APawn& Target) const
{
	using namespace knightMonsterAnimCategory;

	const FknightAnimDebugTrack* const Track = Subsystem.FindTrack(&Target);
	if (!Track)
	{
		return;
	}
	const double WorldTime = CanvasContext.GetWorld()->GetTimeSeconds();

	CanvasContext.Printf(TEXT("{yellow}%s"), *Track->DisplayName);

	CanvasContext.Printf(TEXT("{grey}StateTree: {white}%s {grey}(%.1fs)"),
		Track->StatePath.IsEmpty() ? TEXT("-") : *Track->StatePath, WorldTime - Track->StateEnterWorldTime);
	if (!Track->PreviousStates.IsEmpty())
	{
		CanvasContext.Printf(TEXT("{grey}  이전: %s"), *FString::Join(Track->PreviousStates, TEXT("  ←  ")));
	}

	if (Track->bHasActiveRun)
	{
		CanvasContext.Printf(TEXT("{grey}어빌리티: {cyan}%s {white}%.2fs"), *Track->ActiveRun.Name, WorldTime - Track->ActiveRun.StartWorldTime);
	}
	else
	{
		CanvasContext.Printf(TEXT("{grey}어빌리티: 없음%s"),
			Track->LastRunName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (마지막 %s, %.1fs 전)"), *Track->LastRunName, WorldTime - Track->LastRunEndWorldTime));
	}

	FString Notifies;
	for (const FString& Name : Track->ActiveNotifies)
	{
		Notifies += FString::Printf(TEXT("%s%s "), UknightAnimDebugSubsystem::IsHitWindowNotify(Name) ? TEXT("{red}") : TEXT("{white}"), *Name);
	}
	CanvasContext.Printf(TEXT("{grey}노티파이: %s"), Notifies.IsEmpty() ? TEXT("{white}-") : *Notifies);

	TArray<FString> Tags;
	GetShownTags(Track->ASC.Get(), Tags);
	CanvasContext.Printf(TEXT("{grey}태그: {white}%s"), Tags.IsEmpty() ? TEXT("-") : *FString::Join(Tags, TEXT(", ")));

	const FString Attributes = FormatAttributes(Track->ASC.Get());
	if (!Attributes.IsEmpty())
	{
		CanvasContext.Printf(TEXT("{grey}속성: {white}%s"), *Attributes);
	}

	DrawTimelines(CanvasContext, *Track, UknightAnimDebugSubsystem::GetAnimInstance(&Target));

	if (!Track->RecentEvents.IsEmpty())
	{
		CanvasContext.Print(TEXT("{grey}최근 이벤트"));
		const int32 First = FMath::Max(0, Track->RecentEvents.Num() - PanelEventCount);
		for (int32 Index = First; Index < Track->RecentEvents.Num(); ++Index)
		{
			const FknightAnimDebugEvent& Event = Track->RecentEvents[Index];
			CanvasContext.Printf(TEXT("{grey}  %6.2fs 전  {white}%s"), WorldTime - Event.WorldTime, *Event.Detail);
		}
	}
}

void FGameplayDebuggerCategory_knightMonsterAnim::DrawTimelines(FGameplayDebuggerCanvasContext& CanvasContext, const FknightAnimDebugTrack& Track, UAnimInstance* AnimInstance) const
{
	using namespace knightMonsterAnimCategory;

	const float X = CanvasContext.CursorX;
	float Y = CanvasContext.CursorY + 2.f;
	const float LineHeight = CanvasContext.GetLineHeight();
	const double WorldTime = CanvasContext.GetWorld()->GetTimeSeconds();

	// 어빌리티 구간: 설정의 타이밍 프로퍼티(예고·후딜 등)를 이어 붙이고, 판정 순간을 빨간 선으로 표시한다.
	float AbilityHitTime = -1.f;
	float AbilityTotal = 0.f;
	if (Track.bHasActiveRun && !Track.ActiveRun.Phases.IsEmpty())
	{
		const FknightAnimDebugAbilityRun& Run = Track.ActiveRun;
		AbilityTotal = Run.GetTotalPhaseTime();
		AbilityHitTime = GetHitTime(Run);
		const float Elapsed = static_cast<float>(WorldTime - Run.StartWorldTime);

		CanvasContext.PrintAt(X, Y, FString::Printf(TEXT("{grey}어빌리티 구간 {white}%.2f / %.2fs  {grey}(빨간 선 = 판정)"), Elapsed, AbilityTotal));
		Y += LineHeight;

		if (AbilityTotal > 0.f)
		{
			DrawRect(CanvasContext, X, Y, BarWidth, BarHeight, FLinearColor(0.f, 0.f, 0.f, 0.5f));
			float Start = 0.f;
			for (const FknightAnimDebugPhase& Phase : Run.Phases)
			{
				const float SegmentX = X + Start / AbilityTotal * BarWidth;
				DrawRect(CanvasContext, SegmentX, Y, Phase.Duration / AbilityTotal * BarWidth, BarHeight, WithAlpha(Phase.Color, 0.85f));
				CanvasContext.PrintAt(SegmentX + 2.f, Y + BarHeight + 1.f, FString::Printf(TEXT("{grey}%s %.2f"), *Phase.Label, Phase.Duration));
				Start += Phase.Duration;
			}
			if (AbilityHitTime >= 0.f)
			{
				DrawRect(CanvasContext, X + AbilityHitTime / AbilityTotal * BarWidth - 1.f, Y - 3.f, 3.f, BarHeight + 6.f, FLinearColor::Red);
			}
			DrawRect(CanvasContext, X + FMath::Clamp(Elapsed / AbilityTotal, 0.f, 1.f) * BarWidth - 1.f, Y - 2.f, 2.f, BarHeight + 4.f, FLinearColor::White);
			Y += BarHeight + LineHeight + 4.f;
		}
	}

	// 몽타주: 섹션 경계(흰 선), 노티파이 스테이트 구간(아래 절반, 판정 구간은 빨강), 단발 노티파이(노란 점), 재생 위치.
	const UAnimMontage* const Montage = Track.Montage.Get();
	if (Montage && AnimInstance && Montage->GetPlayLength() > 0.f)
	{
		const float Length = Montage->GetPlayLength();
		const float Position = AnimInstance->Montage_GetPosition(Montage);
		CanvasContext.PrintAt(X, Y, FString::Printf(TEXT("{grey}몽타주 {white}%s [%s] %.2f / %.2fs x%.2f"), *Montage->GetName(),
			*AnimInstance->Montage_GetCurrentSection(Montage).ToString(), Position, Length, AnimInstance->Montage_GetPlayRate(Montage)));
		Y += LineHeight;

		DrawRect(CanvasContext, X, Y, BarWidth, BarHeight, FLinearColor(0.3f, 0.3f, 0.3f, 0.5f));
		float FirstHitStart = -1.f;
		float LabelX = X;
		for (const FAnimNotifyEvent& Notify : Montage->Notifies)
		{
			const float Start = Notify.GetTriggerTime();
			const float NotifyX = X + Start / Length * BarWidth;
			if (!Notify.NotifyStateClass)
			{
				DrawRect(CanvasContext, NotifyX - 1.f, Y, 3.f, BarHeight * 0.4f, FLinearColor::Yellow);
				continue;
			}
			const FString Name = UknightAnimDebugSubsystem::GetNotifyStateName(Notify);
			const bool bHitWindow = UknightAnimDebugSubsystem::IsHitWindowNotify(Name);
			DrawRect(CanvasContext, NotifyX, Y + BarHeight * 0.5f, Notify.GetDuration() / Length * BarWidth, BarHeight * 0.5f,
				bHitWindow ? FLinearColor(1.f, 0.15f, 0.15f, 0.9f) : FLinearColor(0.3f, 0.8f, 1.f, 0.7f));
			if (bHitWindow)
			{
				FirstHitStart = FirstHitStart < 0.f ? Start : FMath::Min(FirstHitStart, Start);
				// 판정 구간 이름은 겹치지 않게 오른쪽으로 밀어 쓴다.
				const FString Label = FString::Printf(TEXT("{red}%s %.2f-%.2f"), *Name, Start, Start + Notify.GetDuration());
				float LabelWidth = 0.f;
				float LabelHeight = 0.f;
				CanvasContext.MeasureString(Label, LabelWidth, LabelHeight);
				LabelX = FMath::Max(LabelX, NotifyX);
				CanvasContext.PrintAt(LabelX, Y + BarHeight + 1.f, Label);
				LabelX += LabelWidth + 8.f;
			}
		}
		for (const FCompositeSection& Section : Montage->CompositeSections)
		{
			DrawRect(CanvasContext, X + Section.GetTime() / Length * BarWidth, Y, 1.f, BarHeight, FLinearColor(1.f, 1.f, 1.f, 0.8f));
		}
		DrawRect(CanvasContext, X + FMath::Clamp(Position / Length, 0.f, 1.f) * BarWidth - 1.f, Y - 2.f, 2.f, BarHeight + 4.f, FLinearColor::White);
		Y += BarHeight + LineHeight + 4.f;

		// 어빌리티가 타이밍을 잡는 공격은 몽타주가 연출용이라, 두 판정 시각이 어긋나면 눈에 보이는 타격과 실제 판정이 다르다.
		if (AbilityHitTime >= 0.f && FirstHitStart >= 0.f)
		{
			const float Difference = FirstHitStart - AbilityHitTime;
			CanvasContext.PrintAt(X, Y, FString::Printf(TEXT("{grey}정렬: 어빌리티 판정 %.2fs / 몽타주 판정 노티파이 %.2fs  %s%+.2fs"),
				AbilityHitTime, FirstHitStart, FMath::Abs(Difference) > 0.1f ? TEXT("{yellow}") : TEXT("{green}"), Difference));
			Y += LineHeight;
		}
		if (AbilityTotal > 0.f)
		{
			CanvasContext.PrintAt(X, Y, FString::Printf(TEXT("{grey}길이: 어빌리티 %.2fs / 몽타주 %.2fs%s"), AbilityTotal, Length,
				Length + 0.05f < AbilityTotal ? TEXT("  {yellow}몽타주가 먼저 끝난다") : TEXT("")));
			Y += LineHeight;
		}
	}

	CanvasContext.CursorX = CanvasContext.DefaultX;
	CanvasContext.CursorY = Y;
}

void FGameplayDebuggerCategory_knightMonsterAnim::DrawWorldShapes(UWorld* World, const FknightAnimDebugTrack& Track) const
{
	const APawn* const Pawn = Track.Pawn.Get();
	if (!World || !Pawn || !Track.bHasActiveRun || !Track.ActiveRun.bHasArea)
	{
		return;
	}

	// 범위 판정 구체: 판정 전에는 주황, 판정 직후 0.25초는 빨강, 그 뒤(후딜)는 회색.
	const FknightAnimDebugAbilityRun& Run = Track.ActiveRun;
	const float Elapsed = static_cast<float>(World->GetTimeSeconds() - Run.StartWorldTime);
	const float HitTime = knightMonsterAnimCategory::GetHitTime(Run);
	FColor Color = FColor(128, 128, 128);
	if (HitTime < 0.f || Elapsed < HitTime)
	{
		Color = FColor::Orange;
	}
	else if (Elapsed < HitTime + 0.25f)
	{
		Color = FColor::Red;
	}
	const FVector Center = Pawn->GetActorTransform().TransformPosition(Run.AreaOffset);
	DrawDebugSphere(World, Center, Run.AreaRadius, 20, Color, false, -1.f, SDPG_World, Color == FColor::Red ? 4.f : 1.5f);
}

void FGameplayDebuggerCategory_knightMonsterAnim::DrawPerf(FGameplayDebuggerCanvasContext& CanvasContext, const UknightAnimDebugSubsystem& Subsystem) const
{
	TArray<FknightAnimDebugPerfValue> Values;
	double AliveCount = 0.0;
	Subsystem.GetPerfValues(Values, AliveCount);

	FString Line = FString::Printf(TEXT("{grey}성능: {white}프레임 %.1fms"), Subsystem.GetFrameMs());
	for (const FknightAnimDebugPerfValue& Value : Values)
	{
		if (Value.bIsTimeMs)
		{
			Line += FString::Printf(TEXT("{grey} | {white}%s %.3fms"), *Value.Name.ToString(), Value.Value);
			if (AliveCount > 0.0)
			{
				Line += FString::Printf(TEXT(" {grey}(평균/마리 %.4f)"), Value.Value / AliveCount);
			}
		}
		else
		{
			Line += FString::Printf(TEXT("{grey} | {white}%s %.0f"), *Value.Name.ToString(), Value.Value);
		}
	}
	CanvasContext.Print(Line);
}

void FGameplayDebuggerCategory_knightMonsterAnim::DrawControls(FGameplayDebuggerCanvasContext& CanvasContext, const UknightAnimDebugSubsystem& Subsystem) const
{
	const FString Selected = GetSelectedAbility();
	CanvasContext.Printf(TEXT("{grey}선택: {white}< %s >  {grey}(%d/%d)"), Selected.IsEmpty() ? TEXT("-") : *Selected,
		AbilityNames.IsEmpty() ? 0 : SelectedAbility + 1, AbilityNames.Num());

	auto OnOff = [](bool bOn) { return bOn ? TEXT("{green}켜짐") : TEXT("{grey}꺼짐"); };
	const FString Recording = Subsystem.IsRecording() ? FString::Printf(TEXT("{red}녹화 중 %.1fs"), Subsystem.GetRecordingSeconds()) : FString(TEXT("{grey}꺼짐"));
	CanvasContext.Printf(TEXT("{white}[Alt+1/2] {grey}선택  {white}[Alt+3] {grey}실행  {white}[Alt+4] {grey}반복 %s  {white}[Alt+5] {grey}AI 정지 %s"),
		OnOff(Subsystem.IsRepeating()), OnOff(Subsystem.IsAIFrozen()));
	CanvasContext.Printf(TEXT("{white}[Alt+6] {grey}배속 {white}x%.2f  {white}[Alt+7] {grey}일시정지 %s  {white}[Alt+8] {grey}한 프레임  {white}[Alt+9] {grey}녹화 %s  {white}[Alt+0] {grey}머리 위: %s"),
		Subsystem.GetTimeScale(), OnOff(Subsystem.IsPaused()), *Recording, bOverheadForAll ? TEXT("{white}전체") : TEXT("{white}대상만"));
}

void FGameplayDebuggerCategory_knightMonsterAnim::OnSelectPrevious()
{
	if (!AbilityNames.IsEmpty())
	{
		SelectedAbility = (SelectedAbility + AbilityNames.Num() - 1) % AbilityNames.Num();
	}
}

void FGameplayDebuggerCategory_knightMonsterAnim::OnSelectNext()
{
	if (!AbilityNames.IsEmpty())
	{
		SelectedAbility = (SelectedAbility + 1) % AbilityNames.Num();
	}
}

void FGameplayDebuggerCategory_knightMonsterAnim::OnPlay()
{
	if (UknightAnimDebugSubsystem* const Subsystem = GetSubsystem())
	{
		Subsystem->PlayAbility(Subsystem->ResolveTarget(), GetSelectedAbility());
	}
}

void FGameplayDebuggerCategory_knightMonsterAnim::OnRepeat()
{
	if (UknightAnimDebugSubsystem* const Subsystem = GetSubsystem())
	{
		if (Subsystem->IsRepeating())
		{
			Subsystem->StopRepeat();
		}
		else
		{
			Subsystem->StartRepeat(Subsystem->ResolveTarget(), GetSelectedAbility(), 1.f);
		}
	}
}

void FGameplayDebuggerCategory_knightMonsterAnim::OnFreezeAI()
{
	if (UknightAnimDebugSubsystem* const Subsystem = GetSubsystem())
	{
		Subsystem->SetAIFrozen(!Subsystem->IsAIFrozen());
	}
}

void FGameplayDebuggerCategory_knightMonsterAnim::OnTimeScale()
{
	using namespace knightMonsterAnimCategory;

	UknightAnimDebugSubsystem* const Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}
	// 지금 배속에 가장 가까운 단계의 다음 단계로 간다.
	const float Current = Subsystem->GetTimeScale();
	int32 Nearest = 0;
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(TimeScaleSteps); ++Index)
	{
		if (FMath::Abs(TimeScaleSteps[Index] - Current) < FMath::Abs(TimeScaleSteps[Nearest] - Current))
		{
			Nearest = Index;
		}
	}
	Subsystem->SetTimeScale(TimeScaleSteps[(Nearest + 1) % UE_ARRAY_COUNT(TimeScaleSteps)]);
}

void FGameplayDebuggerCategory_knightMonsterAnim::OnPause()
{
	if (UknightAnimDebugSubsystem* const Subsystem = GetSubsystem())
	{
		Subsystem->SetPaused(!Subsystem->IsPaused());
	}
}

void FGameplayDebuggerCategory_knightMonsterAnim::OnStep()
{
	if (UknightAnimDebugSubsystem* const Subsystem = GetSubsystem())
	{
		Subsystem->StepFrames(1);
	}
}

void FGameplayDebuggerCategory_knightMonsterAnim::OnRecord()
{
	if (UknightAnimDebugSubsystem* const Subsystem = GetSubsystem())
	{
		if (Subsystem->IsRecording())
		{
			Subsystem->StopRecording();
		}
		else
		{
			Subsystem->StartRecording(TEXT("AnimDebug"));
		}
	}
}

void FGameplayDebuggerCategory_knightMonsterAnim::OnToggleOverheadAll()
{
	bOverheadForAll = !bOverheadForAll;
}

#endif // WITH_GAMEPLAY_DEBUGGER
