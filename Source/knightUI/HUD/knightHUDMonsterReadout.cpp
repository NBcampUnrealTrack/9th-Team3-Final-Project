#include "HUD/knightHUDMonsterReadout.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Font.h"
#include "Engine/LocalPlayer.h"
#include "Fonts/FontMeasure.h"
#include "Foundation/knightUISubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HUD/knightHUDLockOnMarker.h"
#include "HUD/knightHUDMessageLog.h"
#include "Kismet/GameplayStatics.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterDefinition.h"
#include "Monster/knightMonsterPartComponent.h"
#include "Monster/knightMonsterPartData.h"
#include "Styling/CoreStyle.h"
#include "System/knightGameplayTags.h"
#include "UObject/ConstructorHelpers.h"
#include "ViewModels/knightVM_BossStatus.h"

#define LOCTEXT_NAMESPACE "knightHUDMonsterReadout"

DEFINE_LOG_CATEGORY_STATIC(LogknightHUDReadout, Log, All);

namespace
{
	FknightReadoutEntry MakeEntry(EknightReadoutKind Kind, const FText& Label, const FText& Detail, float Duration, int32 Priority)
	{
		FknightReadoutEntry Entry;
		Entry.Kind = Kind;
		Entry.Label = Label;
		Entry.Detail = Detail;
		Entry.Duration = Duration;
		Entry.Priority = Priority;
		return Entry;
	}

	FString ReadoutName(EknightMonsterReadout Type)
	{
		return StaticEnum<EknightMonsterReadout>()->GetNameStringByValue(static_cast<int64>(Type));
	}

	bool IsDown(const UAbilitySystemComponent& ASC)
	{
		// 일어나는 동안(State.Monster.GettingUp)에만 드러나는 약점이 있어 기상까지 다운으로 친다.
		return ASC.HasMatchingGameplayTag(knightGameplayTags::State_HitReact_Knockdown) || ASC.HasMatchingGameplayTag(knightGameplayTags::State_Monster_Collapsed)
			|| ASC.HasMatchingGameplayTag(knightGameplayTags::State_Monster_GettingUp);
	}
}

UknightHUDMonsterReadout::UknightHUDMonsterReadout(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LabelFont = FCoreStyle::GetDefaultFontStyle("Mono", 11);
	LabelFont.LetterSpacing = 150;
	DetailFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	static ConstructorHelpers::FObjectFinder<UFont> KoreanFont(TEXT("/Game/Knight/UI/Style/Fonts/F_Knight_PlexKR.F_Knight_PlexKR"));
	if (KoreanFont.Succeeded())
	{
		DetailFont.FontObject = KoreanFont.Object;
		DetailFont.TypefaceFontName = TEXT("Regular");
		DetailFont.Size = 9;
	}
	DetailPrefix = LOCTEXT("Prefix", "SENSOR · ");

	using EKind = EknightReadoutKind;
	Entries.Add(EknightMonsterReadout::Exhausted, MakeEntry(EKind::Opening, LOCTEXT("Exhausted", "OUTPUT LOW"), LOCTEXT("ExhaustedDetail", "출력 저하"), 2.2f, 3));
	Entries.Add(EknightMonsterReadout::Stunned, MakeEntry(EKind::Opening, LOCTEXT("Stunned", "STUN · WEAK POINT"), LOCTEXT("StunnedDetail", "약점 노출"), 2.4f, 5));
	Entries.Add(EknightMonsterReadout::Staggered, MakeEntry(EKind::Opening, LOCTEXT("Staggered", "STAGGER"), LOCTEXT("StaggeredDetail", "자세 붕괴"), 1.2f, 1));
	Entries.Add(EknightMonsterReadout::Enraged, MakeEntry(EKind::Danger, LOCTEXT("Enraged", "THREAT RISING"), LOCTEXT("EnragedDetail", "위협 상승"), 2.4f, 4));
	Entries.Add(EknightMonsterReadout::Phase2, MakeEntry(EKind::Danger, LOCTEXT("Phase2", "PATTERN SHIFT"), LOCTEXT("Phase2Detail", "행동 패턴 변화"), 2.4f, 4));
	Entries.Add(EknightMonsterReadout::VitalsLow, MakeEntry(EKind::Vitals, LOCTEXT("VitalsLow", "VITALS DROPPING"), LOCTEXT("VitalsLowDetail", "생체 반응 약화"), 2.2f, 2));
	Entries.Add(EknightMonsterReadout::VitalsCritical, MakeEntry(EKind::Vitals, LOCTEXT("VitalsCritical", "VITALS CRITICAL"), LOCTEXT("VitalsCriticalDetail", "생체 반응 위험"), 2.2f, 3));
}

void UknightHUDMonsterReadout::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	const ULocalPlayer* const LocalPlayer = GetOwningLocalPlayer();
	const UknightUISubsystem* const UI = LocalPlayer ? LocalPlayer->GetSubsystem<UknightUISubsystem>() : nullptr;
	UknightVM_BossStatus* const VM = UI ? UI->GetBossStatusViewModel() : nullptr;
	ViewModel = VM;
	if (!VM)
	{
		return;
	}
	ReadoutHandle = VM->OnReadout.AddUObject(this, &ThisClass::HandleReadout);
	using FFields = UknightVM_BossStatus::FFieldNotificationClassDescriptor;
	VM->AddFieldValueChangedDelegate(FFields::HealthPercent, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleViewModelChanged));
	VM->AddFieldValueChangedDelegate(FFields::bHasMonster, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleViewModelChanged));
	VM->AddFieldValueChangedDelegate(FFields::bDefeated, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleViewModelChanged));
	HandleViewModelChanged(VM, FFields::bHasMonster);
}

void UknightHUDMonsterReadout::NativeDestruct()
{
	if (UknightVM_BossStatus* const VM = ViewModel.Get())
	{
		VM->OnReadout.Remove(ReadoutHandle);
		VM->RemoveAllFieldValueChangedDelegates(this);
	}
	ReadoutHandle.Reset();
	ViewModel.Reset();
	Super::NativeDestruct();
}

AknightMonsterCharacterBase* UknightHUDMonsterReadout::GetMonster() const
{
	const UknightVM_BossStatus* const VM = ViewModel.Get();
	return VM ? VM->GetMonster() : nullptr;
}

void UknightHUDMonsterReadout::HandleViewModelChanged(UObject* InViewModel, UE::FieldNotification::FFieldId FieldId)
{
	const UknightVM_BossStatus* const VM = ViewModel.Get();
	AknightMonsterCharacterBase* const Monster = GetMonster();
	if (!VM || !Monster || VM->IsDefeated())
	{
		TrackedMonster.Reset();
		Current.Reset();
		Pending.Reset();
		LastHealth = -1.f;
		return;
	}
	if (TrackedMonster.Get() != Monster)
	{
		// 다른 몬스터를 묶으면 앞 몬스터의 판독과 체력 기준을 버린다.
		TrackedMonster = Monster;
		Current.Reset();
		Pending.Reset();
		LastHealth = -1.f;
	}
	const float Health = VM->GetHealthPercent();
	if (LastHealth >= 0.f)
	{
		// 한 번에 두 기준을 넘으면 더 낮은 쪽만 띄운다.
		if (LastHealth > VitalsCriticalRatio && Health <= VitalsCriticalRatio)
		{
			HandleReadout(EknightMonsterReadout::VitalsCritical);
		}
		else if (LastHealth > VitalsLowRatio && Health <= VitalsLowRatio)
		{
			HandleReadout(EknightMonsterReadout::VitalsLow);
		}
	}
	LastHealth = Health;
}

bool UknightHUDMonsterReadout::MakeShown(EknightMonsterReadout Type, FShown& OutShown) const
{
	const FknightReadoutEntry* const Entry = Entries.Find(Type);
	if (!Entry)
	{
		return false;
	}
	OutShown.Type = Type;
	OutShown.Kind = Entry->Kind;
	OutShown.Label = Entry->Label;
	OutShown.Detail = Entry->Detail;
	OutShown.Duration = Entry->Duration;
	OutShown.Priority = Entry->Priority;
	const AknightMonsterCharacterBase* const Monster = GetMonster();
	const UknightMonsterDefinition* const Definition = Monster ? Monster->GetMonsterDefinition() : nullptr;
	if (const FknightMonsterReadoutText* const Override = Definition ? Definition->ReadoutTexts.Find(Type) : nullptr)
	{
		if (!Override->Label.IsEmpty())
		{
			OutShown.Label = Override->Label;
		}
		if (!Override->Detail.IsEmpty())
		{
			OutShown.Detail = Override->Detail;
		}
	}
	return true;
}

void UknightHUDMonsterReadout::HandleReadout(EknightMonsterReadout Type)
{
	FShown Shown;
	if (!MakeShown(Type, Shown))
	{
		return;
	}
	// 떠 있는 것보다 중요하지 않으면 그것이 끝난 뒤에 띄운다(대기는 하나, 더 중요한 것으로 바뀐다).
	if (Current.IsSet() && Shown.Priority < Current->Priority)
	{
		if (!Pending.IsSet() || Shown.Priority >= Pending->Priority)
		{
			Pending = Shown;
		}
		UE_LOG(LogknightHUDReadout, Verbose, TEXT("monster readout %s queued behind %s"), *ReadoutName(Type), *ReadoutName(Current->Type));
		return;
	}
	Begin(Shown);
}

void UknightHUDMonsterReadout::Begin(const FShown& Shown)
{
	Current = Shown;
	Current->Elapsed = 0.f;
	USoundBase* const Sound = Shown.Kind == EknightReadoutKind::Opening ? OpeningSound.Get()
		: Shown.Kind == EknightReadoutKind::Vitals && VitalsSound ? VitalsSound.Get()
		: DangerSound.Get();
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
	if (UknightHUDMessageLog* const Log = FindMessageLog())
	{
		const AknightMonsterCharacterBase* const Monster = GetMonster();
		Log->AddMonsterReadoutLine(Shown.Type, Monster ? Monster->GetMonsterDefinition() : nullptr);
	}
	UE_LOG(LogknightHUDReadout, Verbose, TEXT("monster readout %s: %s"), *ReadoutName(Shown.Type), *Shown.Label.ToString());
}

void UknightHUDMonsterReadout::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// 실제 시간으로 센다(히트스톱·슬로모션 중에도 같은 속도로 사라진다).
	const float Step = FMath::Min(InDeltaTime, 0.05f);
	Time += Step;
	if (!Current.IsSet())
	{
		return;
	}
	Current->Elapsed += Step;
	if (Current->Elapsed >= Current->Duration)
	{
		Current.Reset();
		if (Pending.IsSet())
		{
			const FShown Next = Pending.GetValue();
			Pending.Reset();
			Begin(Next);
		}
	}
}

FLinearColor UknightHUDMonsterReadout::GetKindColor(EknightReadoutKind Kind) const
{
	return Kind == EknightReadoutKind::Opening ? OpeningColor : Kind == EknightReadoutKind::Danger ? DangerColor : VitalsColor;
}

knightHUDDraw::FScreenLook UknightHUDMonsterReadout::GetScreenLook() const
{
	knightHUDDraw::FScreenLook Look;
	Look.GhostColor = GhostColor;
	Look.GhostOffset = GhostOffset;
	Look.ScanlineSpacing = ScanlineSpacing;
	Look.ScanlineOpacity = ScanlineOpacity;
	Look.SweepPeriod = SweepPeriod;
	return Look;
}

template <typename T>
T* UknightHUDMonsterReadout::FindInLayout() const
{
	// 같은 HUD 레이아웃 안의 위젯을 찾는다.
	const UUserWidget* const Layout = GetTypedOuter<UUserWidget>();
	if (!Layout || !Layout->WidgetTree)
	{
		return nullptr;
	}
	T* Found = nullptr;
	Layout->WidgetTree->ForEachWidget([&Found](UWidget* Widget)
	{
		if (!Found)
		{
			Found = Cast<T>(Widget);
		}
	});
	return Found;
}

UknightHUDLockOnMarker* UknightHUDMonsterReadout::FindLockOnMarker() const
{
	if (!LockOnMarker.IsValid())
	{
		LockOnMarker = FindInLayout<UknightHUDLockOnMarker>();
	}
	return LockOnMarker.Get();
}

UknightHUDMessageLog* UknightHUDMonsterReadout::FindMessageLog() const
{
	if (!MessageLog.IsValid())
	{
		MessageLog = FindInLayout<UknightHUDMessageLog>();
	}
	return MessageLog.Get();
}

bool UknightHUDMonsterReadout::GetTagAnchor(const FVector2D& Size, FVector2D& OutAnchor, bool& bOutFlipped) const
{
	const AknightMonsterCharacterBase* const Monster = GetMonster();
	APlayerController* const PC = GetOwningPlayer();
	if (!Monster || !PC || !PC->PlayerCameraManager)
	{
		return false;
	}
	const AActor* LockedTarget = nullptr;
	FVector2D PlateCorner;
	if (const UknightHUDLockOnMarker* const Marker = FindLockOnMarker();
		Marker && Marker->GetPlateAnchor(LockedTarget, PlateCorner, bOutFlipped) && LockedTarget == Monster)
	{
		OutAnchor = PlateCorner + FVector2D(bOutFlipped ? -PlateGap.X : PlateGap.X, PlateGap.Y);
		return true;
	}

	const USkeletalMeshComponent* const Mesh = Monster->GetMesh();
	const UknightMonsterDefinition* const Definition = Monster->GetMonsterDefinition();
	const FName Socket = Definition ? Definition->FallbackLockOnSocket : NAME_None;
	const FVector Body = Mesh && Mesh->DoesSocketExist(Socket) ? Mesh->GetSocketLocation(Socket) : Monster->GetActorLocation();
	const FVector Local = PC->PlayerCameraManager->GetCameraRotation().UnrotateVector(Body - PC->PlayerCameraManager->GetCameraLocation());
	FVector2D Pixels;
	if (Local.X <= 0.f || !UGameplayStatics::ProjectWorldToScreen(PC, Body, Pixels, true))
	{
		// 카메라 뒤: 화면 위쪽 가운데.
		OutAnchor = FVector2D(Size.X * 0.5f, BehindTop);
		bOutFlipped = false;
		return true;
	}
	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	const FVector2D Center = Pixels / Scale;
	bOutFlipped = Center.X > Size.X * FlipScreenFraction;
	OutAnchor = Center + FVector2D(bOutFlipped ? -BodyOffset.X : BodyOffset.X, BodyOffset.Y);
	OutAnchor.X = FMath::Clamp(OutAnchor.X, SafeMargin.X, Size.X - SafeMargin.X);
	OutAnchor.Y = FMath::Clamp(OutAnchor.Y, SafeMargin.Y, Size.Y - SafeMargin.Y);
	return true;
}

int32 UknightHUDMonsterReadout::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const float Tint = InWidgetStyle.GetColorAndOpacityTint().A;
	PaintWeakPoints(AllottedGeometry, OutDrawElements, LayerId + 1, Tint);
	if (Current.IsSet())
	{
		PaintTag(AllottedGeometry, OutDrawElements, LayerId + 1, FVector2D(AllottedGeometry.GetLocalSize()), Tint);
	}
	return LayerId + 3;
}

void UknightHUDMonsterReadout::PaintTag(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer, const FVector2D& Size, float Tint) const
{
	FVector2D Anchor;
	bool bFlipped = false;
	if (!GetTagAnchor(Size, Anchor, bFlipped))
	{
		return;
	}
	const FShown& Shown = Current.GetValue();
	const float FadeOut = FadeOutTime > 0.f ? FMath::Clamp((Shown.Duration - Shown.Elapsed) / FadeOutTime, 0.f, 1.f) : 1.f;
	const float Envelope = knightHUDDraw::BootFlicker(Shown.Elapsed, BootFlickerTime) * FadeOut * Tint;
	if (Envelope <= 0.f)
	{
		return;
	}
	const knightHUDDraw::FScreenLook Look = GetScreenLook();
	const FLinearColor KindColor = GetKindColor(Shown.Kind);
	const FLinearColor LineColor = knightHUDDraw::WithOpacity(KindColor, BaseOpacity * Envelope);
	const FLinearColor TextColor = knightHUDDraw::WithOpacity(KindColor, Envelope);

	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D LabelSize = Measure->Measure(Shown.Label, LabelFont);
	const FText Detail = FText::Format(LOCTEXT("DetailFormat", "{0}{1}"), DetailPrefix, Shown.Detail);
	const FVector2D DetailSize = Measure->Measure(Detail, DetailFont);

	const float Icon = LabelSize.Y * 0.5f;
	const FVector2D TagSize(TagPadding.X * 2.f + Icon + 6.f + LabelSize.X, TagPadding.Y * 2.f + LabelSize.Y);
	const FVector2D TagPosition(bFlipped ? Anchor.X - TagSize.X : Anchor.X, Anchor.Y);

	knightHUDDraw::Box(Out, Layer, Geometry, TagPosition, TagSize, knightHUDDraw::WithOpacity(PlateColor, Envelope));
	knightHUDDraw::Scanlines(Look, Out, Layer, Geometry, TagPosition, TagSize, KindColor, BaseOpacity * Envelope, Time);
	const FVector2f TL(TagPosition);
	const FVector2f BR = TL + FVector2f(TagSize);
	knightHUDDraw::ScreenLines(Look, Out, Layer, Geometry, { TL, FVector2f(BR.X, TL.Y), BR, FVector2f(TL.X, BR.Y), TL }, LineColor, 1.f);

	// 앞 기호: 반격 기회 ◇, 위험 ▲, 체력 ▽. 글꼴에 없을 수 있어 선으로 그린다.
	const FVector2f C(TagPosition.X + TagPadding.X + Icon * 0.5f, TagPosition.Y + TagSize.Y * 0.5f);
	const float R = Icon * 0.55f;
	TArray<FVector2f> Shape;
	switch (Shown.Kind)
	{
	case EknightReadoutKind::Opening:
		Shape = { C + FVector2f(0.f, -R), C + FVector2f(R, 0.f), C + FVector2f(0.f, R), C + FVector2f(-R, 0.f), C + FVector2f(0.f, -R) };
		break;
	case EknightReadoutKind::Danger:
		Shape = { C + FVector2f(0.f, -R), C + FVector2f(R, R * 0.8f), C + FVector2f(-R, R * 0.8f), C + FVector2f(0.f, -R) };
		break;
	default:
		Shape = { C + FVector2f(-R, -R * 0.8f), C + FVector2f(R, -R * 0.8f), C + FVector2f(0.f, R), C + FVector2f(-R, -R * 0.8f) };
		break;
	}
	knightHUDDraw::ScreenLines(Look, Out, Layer + 1, Geometry, Shape, TextColor, 1.25f);
	knightHUDDraw::ScreenText(Look, Out, Layer + 1, Geometry, FVector2D(TagPosition.X + TagPadding.X + Icon + 6.f, TagPosition.Y + TagPadding.Y), Shown.Label, LabelFont,
		TextColor);

	const FVector2D DetailPosition(bFlipped ? BR.X - DetailSize.X : TL.X + 2.f, BR.Y + 3.f);
	knightHUDDraw::ScreenText(Look, Out, Layer + 1, Geometry, DetailPosition, Detail, DetailFont, knightHUDDraw::WithOpacity(KindColor, Envelope * 0.8f));
}

void UknightHUDMonsterReadout::PaintWeakPoints(const FGeometry& Geometry, FSlateWindowElementList& Out, int32 Layer, float Tint) const
{
	const AknightMonsterCharacterBase* const Monster = GetMonster();
	const UAbilitySystemComponent* const ASC = Monster ? Monster->GetAbilitySystemComponent() : nullptr;
	const UknightMonsterPartComponent* const Parts = Monster ? Monster->GetPartComponent() : nullptr;
	const UknightMonsterDefinition* const Definition = Monster ? Monster->GetMonsterDefinition() : nullptr;
	APlayerController* const PC = GetOwningPlayer();
	if (!ASC || !Parts || !Definition || !Definition->PartData || !PC || !PC->PlayerCameraManager || Monster->IsDead() || !IsDown(*ASC))
	{
		return;
	}
	const USkeletalMeshComponent* const Mesh = Monster->GetMesh();
	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	const FRotator CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
	const float Wave = 0.5f + 0.5f * FMath::Sin(Time * UE_TWO_PI / WeakPointPulsePeriod);
	const FLinearColor Color = knightHUDDraw::WithOpacity(OpeningColor, FMath::Lerp(0.25f, 0.9f, Wave) * Tint);
	const knightHUDDraw::FScreenLook Look = GetScreenLook();
	for (const FknightMonsterPartDefinition& Part : Definition->PartData->Parts)
	{
		if (Parts->GetDamageMultiplier(Part.PartTag) < WeakPointMultiplier
			|| (!Part.ExposedTags.IsEmpty() && !ASC->HasAnyMatchingGameplayTags(Part.ExposedTags)))
		{
			continue;
		}
		FVector Location;
		if (Mesh && !Part.LockOnSocket.IsNone() && Mesh->DoesSocketExist(Part.LockOnSocket))
		{
			Location = Mesh->GetSocketLocation(Part.LockOnSocket);
		}
		else if (!Parts->GetPartLocation(Part.PartTag, Location))
		{
			continue;
		}
		FVector2D Pixels;
		if (CameraRotation.UnrotateVector(Location - CameraLocation).X <= 0.f || !UGameplayStatics::ProjectWorldToScreen(PC, Location, Pixels, true))
		{
			continue;
		}
		const FVector2f C(Pixels / Scale);
		const float R = WeakPointSize * 0.5f * FMath::Lerp(1.15f, 1.f, Wave);
		knightHUDDraw::ScreenLines(Look, Out, Layer, Geometry,
			{ C + FVector2f(0.f, -R), C + FVector2f(R, 0.f), C + FVector2f(0.f, R), C + FVector2f(-R, 0.f), C + FVector2f(0.f, -R) }, Color, 1.25f);
		knightHUDDraw::Box(Out, Layer, Geometry, FVector2D(C) - FVector2D(1.5f), FVector2D(3.f), Color);
	}
}

#undef LOCTEXT_NAMESPACE
