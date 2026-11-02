#include "SknightFeedbackTuning.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AttributeSet.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "knightAnimDebugSettings.h"
#include "knightAnimDebugSubsystem.h"

namespace knightFeedbackTuningTab
{
	static const FLinearColor ChangedColor(1.f, 0.8f, 0.25f);
	static const FLinearColor DimColor(0.55f, 0.55f, 0.55f);

	static void Notify(const FString& Message, bool bSuccess)
	{
		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = 5.f;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	/** 설정의 첫 속성 쌍(기본 Health/MaxHealth)으로 현재 값을 최대로 채운다. */
	static void FillHealth(APawn* Pawn)
	{
		UAbilitySystemComponent* const ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		const TArray<FknightAnimDebugAttribute>& Attributes = GetDefault<UknightAnimDebugSettings>()->Attributes;
		if (!ASC || Attributes.IsEmpty())
		{
			return;
		}
		FGameplayAttribute Value;
		FGameplayAttribute Max;
		for (const UAttributeSet* const Set : ASC->GetSpawnedAttributes())
		{
			if (!Set)
			{
				continue;
			}
			if (FProperty* const Property = FindFProperty<FProperty>(Set->GetClass(), Attributes[0].Name))
			{
				Value = FGameplayAttribute(Property);
			}
			if (FProperty* const Property = FindFProperty<FProperty>(Set->GetClass(), Attributes[0].MaxName))
			{
				Max = FGameplayAttribute(Property);
			}
		}
		if (Value.IsValid() && Max.IsValid())
		{
			const float MaxValue = ASC->GetNumericAttribute(Max);
			if (ASC->GetNumericAttribute(Value) < MaxValue)
			{
				ASC->SetNumericAttributeBase(Value, MaxValue);
			}
		}
	}
}

void SknightFeedbackTuning::Construct(const FArguments& InArgs)
{
	using namespace knightFeedbackTuningTab;

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &SknightFeedbackTuning::TickLocks), 0.1f);
	Model.OnReset.AddSP(this, &SknightFeedbackTuning::HandleReset);
	Model.OnCollected.AddSP(this, &SknightFeedbackTuning::HandleCollected);
	FEditorDelegates::PostPIEStarted.AddSP(this, &SknightFeedbackTuning::HandlePIEStarted);

	auto Check = [](const TCHAR* Label, const TCHAR* Tip, bool* Flag, TFunction<void(bool)> OnChanged = nullptr)
	{
		return SNew(SCheckBox)
			.ToolTipText(FText::FromString(Tip))
			.IsChecked_Lambda([Flag]() { return *Flag ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Flag, OnChanged](ECheckBoxState State)
			{
				*Flag = State == ECheckBoxState::Checked;
				if (OnChanged)
				{
					OnChanged(*Flag);
				}
			})
			[
				SNew(STextBlock).Text(FText::FromString(Label))
			];
	};

	auto Button = [](const TCHAR* Label, const TCHAR* Tip, FOnClicked OnClicked)
	{
		return SNew(SButton)
			.Text(FText::FromString(Label))
			.ToolTipText(FText::FromString(Tip))
			.OnClicked(OnClicked);
	};

	ChildSlot
	[
		SNew(SVerticalBox)

		// 1줄: 대상과 공격 실행
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 6.f, 6.f, 2.f)
		[
			SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(0.f, 4.f))
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				Button(TEXT("다시 읽기"), TEXT("PIE의 몬스터(디버거가 고른 것, 없으면 가장 가까운 것), 카메라, 플레이어에서 값을 다시 찾는다. 저장하지 않은 값은 되돌린다. PIE를 시작하면 1.5초 뒤 자동으로 한 번 읽는다."),
					FOnClicked::CreateLambda([this]() { Collect(); return FReply::Handled(); }))
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SAssignNew(AbilityCombo, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&AbilityNames)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(FText::FromString(*Item)); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type) { SelectedAbility = Item; })
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						return FText::FromString(SelectedAbility.IsValid() ? *SelectedAbility : TEXT("공격 선택"));
					})
				]
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				Button(TEXT("다시 실행"), TEXT("고른 공격을 몬스터에게 강제로 실행한다. 배치를 켜면 플레이어를 먼저 몬스터 정면에 세운다."),
					FOnClicked::CreateSP(this, &SknightFeedbackTuning::OnRunClicked))
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				Check(TEXT("반복"), TEXT("공격이 끝나고 잠깐 뒤 다시 실행하기를 계속한다."), &bRepeat, [this](bool bOn)
				{
					UknightAnimDebugSubsystem* const Subsystem = UknightAnimDebugSubsystem::Get(GetPlayWorld());
					if (!Subsystem)
					{
						return;
					}
					if (bOn && SelectedAbility.IsValid())
					{
						Subsystem->StartRepeat(GetMonster(), *SelectedAbility, RepeatDelay);
					}
					else
					{
						Subsystem->StopRepeat();
					}
				})
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SBox).WidthOverride(60.f)
				[
					SNew(SSpinBox<float>)
					.ToolTipText(FText::FromString(TEXT("반복 간격(초)")))
					.MinValue(0.f).MaxValue(10.f).Delta(0.1f)
					.Value_Lambda([this]() { return RepeatDelay; })
					.OnValueChanged_Lambda([this](float Value) { RepeatDelay = Value; })
				]
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				Check(TEXT("실행 전 배치"), TEXT("다시 실행할 때마다 플레이어를 몬스터 정면에 세운다(설정의 FeedbackPlaceDistance)."), &bPlaceBeforeRun)
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center)
			[
				Button(TEXT("지금 배치"), TEXT("플레이어를 몬스터 정면에 세우고 몬스터를 바라보게 한다."),
					FOnClicked::CreateSP(this, &SknightFeedbackTuning::OnPlaceClicked))
			]
		]

		// 1-2줄: 플레이어 공격 실행
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 2.f)
		[
			SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(0.f, 4.f))
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("플레이어")))
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SAssignNew(PlayerAttackCombo, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&PlayerAttackNames)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(FText::FromString(*Item)); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type) { SelectedPlayerAttack = Item; })
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						return FText::FromString(SelectedPlayerAttack.IsValid() ? *SelectedPlayerAttack : TEXT("플레이어 공격 선택"));
					})
				]
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center)
			[
				Button(TEXT("플레이어 공격"), TEXT("고른 플레이어 공격을 강제로 실행한다. 실행 전 배치를 켜면 몬스터에 닿는 거리(몸통 반지름 + FeedbackPlayerAttackGap)에 먼저 세운다."),
					FOnClicked::CreateSP(this, &SknightFeedbackTuning::OnPlayerAttackClicked))
			]
		]

		// 1-3줄: 카메라 반응
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 2.f)
		[
			SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(0.f, 4.f))
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("카메라 반응")))
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SAssignNew(ImpactCombo, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&ImpactMoments)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(FText::FromString(*Item)); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type) { SelectImpactMoment(Item); })
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						return FText::FromString(SelectedImpactMoment.IsValid() ? *SelectedImpactMoment : TEXT("순간 선택"));
					})
				]
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center)
			[
				Button(TEXT("미리보기"), TEXT("고른 순간의 카메라 반응을 공격 없이 최대 세기로 재생한다. 값을 바꾸고 다시 누르면 바로 비교된다."),
					FOnClicked::CreateSP(this, &SknightFeedbackTuning::OnImpactPreviewClicked))
			]
		]

		// 2줄: 상황 고정
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 2.f)
		[
			SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(0.f, 4.f))
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SCheckBox)
				.ToolTipText(FText::FromString(TEXT("몬스터 AI를 멈춘다. 강제로 실행한 공격만 나온다.")))
				.IsChecked_Lambda([this]()
				{
					const UknightAnimDebugSubsystem* const Subsystem = UknightAnimDebugSubsystem::Get(GetPlayWorld());
					return Subsystem && Subsystem->IsAIFrozen() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					if (UknightAnimDebugSubsystem* const Subsystem = UknightAnimDebugSubsystem::Get(GetPlayWorld()))
					{
						Subsystem->SetAIFrozen(State == ECheckBoxState::Checked);
					}
				})
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("AI 정지")))
				]
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
			[
				Check(TEXT("플레이어 체력 고정"), TEXT("맞아도 체력을 바로 채운다. 피격 반응은 그대로 나온다(무적과 다름)."), &bLockPlayerHealth)
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
			[
				Check(TEXT("몬스터 체력 고정"), TEXT("몬스터가 죽지 않게 체력을 바로 채운다."), &bLockMonsterHealth)
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("배속")))
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(70.f)
				[
					SNew(SSpinBox<float>)
					.MinValue(0.05f).MaxValue(2.f).Delta(0.05f)
					.Value_Lambda([this]()
					{
						const UknightAnimDebugSubsystem* const Subsystem = UknightAnimDebugSubsystem::Get(GetPlayWorld());
						return Subsystem ? Subsystem->GetTimeScale() : 1.f;
					})
					.OnValueChanged_Lambda([this](float Value)
					{
						if (UknightAnimDebugSubsystem* const Subsystem = UknightAnimDebugSubsystem::Get(GetPlayWorld()))
						{
							Subsystem->SetTimeScale(Value);
						}
					})
				]
			]
		]

		// 3줄: 찾기와 저장
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 2.f, 6.f, 4.f)
		[
			SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(0.f, 4.f))
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SBox).WidthOverride(380.f)
				[
					SAssignNew(SearchBox, SSearchBox)
					.HintText(FText::FromString(TEXT("출처나 값 이름으로 찾기, 낱말 여러 개는 모두 포함 (예: Slam HitStop)")))
					.OnTextChanged_Lambda([this](const FText& Text) { Filter = Text.ToString(); RebuildRows(); })
				]
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				Check(TEXT("몬스터"), TEXT("몬스터 묶음을 보인다."), &bShowMonster, [this](bool) { RebuildRows(); })
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				Check(TEXT("플레이어"), TEXT("플레이어 묶음을 보인다(저장 잠금)."), &bShowPlayer, [this](bool) { RebuildRows(); })
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
			[
				Check(TEXT("카메라"), TEXT("카메라 매니저, 카메라 컴포넌트, 카메라 모드·모디파이어 기본값을 보인다(저장 잠금)."), &bShowCamera, [this](bool) { RebuildRows(); })
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				Check(TEXT("바뀐 값만"), TEXT("원래 값과 다른 값만 보인다."), &bChangedOnly, [this](bool) { RebuildRows(); })
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				Button(TEXT("저장"), TEXT("바뀐 몬스터 값을 에셋에 저장한다. 플레이어·카메라 값은 저장하지 않는다."),
					FOnClicked::CreateSP(this, &SknightFeedbackTuning::OnSaveClicked))
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				Button(TEXT("되돌리기"), TEXT("저장하지 않은 값을 마지막 저장값으로 돌린다."),
					FOnClicked::CreateSP(this, &SknightFeedbackTuning::OnRevertClicked))
			]
			+ SWrapBox::Slot().VAlign(VAlign_Center)
			[
				Button(TEXT("바뀐 값 복사"), TEXT("바뀐 값을 '출처 · 경로: 전 → 후' 줄로 복사한다. 플레이어 값은 담당자에게 넘길 때 쓴다."),
					FOnClicked::CreateSP(this, &SknightFeedbackTuning::OnCopyClicked))
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 4.f)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor(DimColor))
			.Text(this, &SknightFeedbackTuning::GetStatusText)
		]

		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(RowBox, SVerticalBox)
				]
			]
		]
	];

	// 콘솔 명령으로 이미 모았으면 그 값(바꾼 값 포함)을 그대로 보인다. 다시 모으면 바꾼 값이 되돌아간다.
	if (Model.IsCollected(GetPlayWorld()))
	{
		HandleCollected();
	}
	else
	{
		Collect();
	}
}

SknightFeedbackTuning::~SknightFeedbackTuning()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	Model.OnReset.RemoveAll(this);
	Model.OnCollected.RemoveAll(this);
	FEditorDelegates::PostPIEStarted.RemoveAll(this);
}

UWorld* SknightFeedbackTuning::GetPlayWorld() const
{
	return GEditor ? GEditor->PlayWorld.Get() : nullptr;
}

APawn* SknightFeedbackTuning::GetMonster() const
{
	if (APawn* const Collected = Model.GetMonster())
	{
		return Collected;
	}
	UknightAnimDebugSubsystem* const Subsystem = UknightAnimDebugSubsystem::Get(GetPlayWorld());
	return Subsystem ? Subsystem->ResolveTarget() : nullptr;
}

APawn* SknightFeedbackTuning::GetPlayer() const
{
	const UWorld* const World = GetPlayWorld();
	const APlayerController* const PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? PC->GetPawn() : nullptr;
}

void SknightFeedbackTuning::Collect()
{
	AutoCollectTime = 0.0;
	if (UWorld* const World = GetPlayWorld())
	{
		Model.Collect(World);
	}
	else
	{
		HandleCollected();
	}
}

void SknightFeedbackTuning::HandlePIEStarted(bool bIsSimulating)
{
	AutoCollectTime = FPlatformTime::Seconds() + 1.5;
}

void SknightFeedbackTuning::HandleCollected()
{
	UknightAnimDebugSubsystem* const Subsystem = UknightAnimDebugSubsystem::Get(GetPlayWorld());
	AbilityNames.Reset();
	if (APawn* const Monster = Model.GetMonster(); Monster && Subsystem)
	{
		TArray<FString> Names;
		Subsystem->GetPlayableAbilityNames(Monster, Names);
		Names.Sort();
		for (const FString& Name : Names)
		{
			AbilityNames.Add(MakeShared<FString>(Name));
		}
	}
	if (SelectedAbility.IsValid())
	{
		const TSharedPtr<FString>* const Same = AbilityNames.FindByPredicate([this](const TSharedPtr<FString>& Name) { return *Name == *SelectedAbility; });
		SelectedAbility = Same ? *Same : nullptr;
	}

	ImpactMoments.Reset();
	TArray<FString> Moments;
	Model.GetImpactMoments(Moments, ImpactSourceLabel);
	for (const FString& Moment : Moments)
	{
		ImpactMoments.Add(MakeShared<FString>(Moment));
	}
	if (SelectedImpactMoment.IsValid())
	{
		const TSharedPtr<FString>* const Same = ImpactMoments.FindByPredicate([this](const TSharedPtr<FString>& Name) { return *Name == *SelectedImpactMoment; });
		SelectedImpactMoment = Same ? *Same : nullptr;
	}

	PlayerAttackNames.Reset();
	TArray<FString> PlayerNames;
	Model.GetPlayerAttackNames(PlayerNames);
	for (const FString& Name : PlayerNames)
	{
		PlayerAttackNames.Add(MakeShared<FString>(Name));
	}
	if (SelectedPlayerAttack.IsValid())
	{
		const TSharedPtr<FString>* const Same = PlayerAttackNames.FindByPredicate([this](const TSharedPtr<FString>& Name) { return *Name == *SelectedPlayerAttack; });
		SelectedPlayerAttack = Same ? *Same : nullptr;
	}

	RefreshCombos();
	LastMessage.Reset();
	RebuildRows();
}

void SknightFeedbackTuning::RefreshCombos()
{
	// 콤보 상자는 목록 배열을 바꿔도 스스로 다시 읽지 않는다. 탭이 PIE보다 먼저 열려 있으면 빈 목록이 남는다.
	if (AbilityCombo.IsValid())
	{
		AbilityCombo->RefreshOptions();
		AbilityCombo->SetSelectedItem(SelectedAbility);
	}
	if (PlayerAttackCombo.IsValid())
	{
		PlayerAttackCombo->RefreshOptions();
		PlayerAttackCombo->SetSelectedItem(SelectedPlayerAttack);
	}
	if (ImpactCombo.IsValid())
	{
		ImpactCombo->RefreshOptions();
		ImpactCombo->SetSelectedItem(SelectedImpactMoment);
	}
}

void SknightFeedbackTuning::RebuildRows()
{
	using namespace knightFeedbackTuningTab;
	if (!RowBox.IsValid())
	{
		return;
	}
	RowBox->ClearChildren();

	const TArray<FknightFeedbackSource>& Sources = Model.GetSources();
	const TArray<FknightFeedbackEntry>& Entries = Model.GetEntries();
	int32 LastSource = INDEX_NONE;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FknightFeedbackEntry& Entry = Entries[Index];
		const FknightFeedbackSource& Source = Sources[Entry.SourceIndex];
		const bool bGroupShown = (Source.Group == EknightFeedbackGroup::Monster && bShowMonster)
			|| (Source.Group == EknightFeedbackGroup::Player && bShowPlayer)
			|| (Source.Group == EknightFeedbackGroup::Camera && bShowCamera);
		if (!bGroupShown || !Model.MatchesFilter(Index, Filter))
		{
			continue;
		}
		if (bChangedOnly && FMath::IsNearlyEqual(Model.GetValue(Index), Entry.Original, 1e-6))
		{
			continue;
		}

		if (Entry.SourceIndex != LastSource)
		{
			LastSource = Entry.SourceIndex;
			FString Header = FString(FknightFeedbackTuningModel::GroupLabel(Source.Group)) + TEXT(" ") + Source.Label;
			if (Model.IsSaveLocked(Entry.SourceIndex))
			{
				Header += Source.bNative ? TEXT("   (C++ 기본값, 바뀐 값 복사로 헤더에 옮김)") : TEXT("   (저장 잠금, 바뀐 값 복사로 인계)");
			}
			RowBox->AddSlot().AutoHeight().Padding(6.f, 8.f, 6.f, 2.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Header))
				.ToolTipText(FText::FromString(Source.Object.IsValid() ? Source.Object->GetPathName() : FString()))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			];
		}

		RowBox->AddSlot().AutoHeight().Padding(20.f, 1.f, 6.f, 1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(Entry.Path))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f)
			[
				SNew(SBox).WidthOverride(110.f)
				[
					SNew(SSpinBox<double>)
					.Delta(Entry.bInteger ? 1.0 : 0.0)
					.MinFractionalDigits(Entry.bInteger ? 0 : 2)
					.MaxFractionalDigits(Entry.bInteger ? 0 : 4)
					.Value_Lambda([this, Index]() { return Model.GetValue(Index); })
					.OnValueChanged_Lambda([this, Index](double Value) { Model.SetValue(Index, Value); })
					.OnValueCommitted_Lambda([this, Index](double Value, ETextCommit::Type) { Model.SetValue(Index, Value); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SBox).WidthOverride(90.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this, Index]()
					{
						const FknightFeedbackEntry& Item = Model.GetEntries()[Index];
						return FMath::IsNearlyEqual(Model.GetValue(Index), Item.Original, 1e-6)
							? FText::GetEmpty()
							: FText::FromString(FString::Printf(TEXT("원래 %s"), *FString::SanitizeFloat(Item.Original, 0)));
					})
					.ColorAndOpacity_Lambda([this, Index]()
					{
						return FSlateColor(Model.IsChanged(Index) ? ChangedColor : DimColor);
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("원래대로")))
				.ToolTipText(FText::FromString(TEXT("이번 세션을 시작할 때의 값으로 돌린다.")))
				.IsEnabled_Lambda([this, Index]() { return !FMath::IsNearlyEqual(Model.GetValue(Index), Model.GetEntries()[Index].Original, 1e-6); })
				.OnClicked_Lambda([this, Index]() { Model.SetValue(Index, Model.GetEntries()[Index].Original); return FReply::Handled(); })
			]
		];
	}

	if (Entries.IsEmpty())
	{
		RowBox->AddSlot().AutoHeight().Padding(8.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(FText::FromString(GetPlayWorld()
				? TEXT("값을 찾지 못했어요. 몬스터가 스폰된 뒤 '다시 읽기'를 누르세요.")
				: TEXT("PIE를 시작하면 몬스터, 플레이어, 카메라의 피드백 값을 자동으로 읽어요.")))
		];
	}
}

bool SknightFeedbackTuning::TickLocks(float DeltaTime)
{
	if (!GetPlayWorld())
	{
		return true;
	}
	if (AutoCollectTime > 0.0 && FPlatformTime::Seconds() >= AutoCollectTime)
	{
		Collect();
	}
	if (bLockPlayerHealth)
	{
		knightFeedbackTuningTab::FillHealth(GetPlayer());
	}
	if (bLockMonsterHealth)
	{
		knightFeedbackTuningTab::FillHealth(GetMonster());
	}
	return true;
}

void SknightFeedbackTuning::HandleReset(int32 Reverted)
{
	AbilityNames.Reset();
	SelectedAbility.Reset();
	PlayerAttackNames.Reset();
	SelectedPlayerAttack.Reset();
	ImpactMoments.Reset();
	SelectedImpactMoment.Reset();
	RefreshCombos();
	bRepeat = false;
	LastMessage = Reverted > 0 ? FString::Printf(TEXT("PIE가 끝나 저장하지 않은 값 %d개를 원래대로 돌렸어요."), Reverted) : FString();
	RebuildRows();
}

FReply SknightFeedbackTuning::OnRunClicked()
{
	UknightAnimDebugSubsystem* const Subsystem = UknightAnimDebugSubsystem::Get(GetPlayWorld());
	APawn* const Monster = GetMonster();
	if (!Subsystem || !Monster || !SelectedAbility.IsValid())
	{
		knightFeedbackTuningTab::Notify(TEXT("PIE 중에 공격을 고른 뒤 실행하세요."), false);
		return FReply::Handled();
	}
	if (bPlaceBeforeRun)
	{
		Model.PlacePlayer(GetDefault<UknightAnimDebugSettings>()->FeedbackPlaceDistance);
	}
	Subsystem->PlayAbility(Monster, *SelectedAbility);
	return FReply::Handled();
}

FReply SknightFeedbackTuning::OnPlayerAttackClicked()
{
	if (!GetPlayWorld() || !SelectedPlayerAttack.IsValid())
	{
		knightFeedbackTuningTab::Notify(TEXT("PIE 중에 플레이어 공격을 고른 뒤 실행하세요."), false);
		return FReply::Handled();
	}
	if (!Model.PlayPlayerAttack(*SelectedPlayerAttack, bPlaceBeforeRun))
	{
		LastMessage = FString::Printf(TEXT("%s를 실행하지 못했어요(태그·쿨다운 조건이나 다른 동작 중)."), **SelectedPlayerAttack);
	}
	return FReply::Handled();
}

void SknightFeedbackTuning::SelectImpactMoment(TSharedPtr<FString> Moment)
{
	SelectedImpactMoment = Moment;
	if (!Moment.IsValid() || ImpactSourceLabel.IsEmpty())
	{
		return;
	}
	// 그 순간의 값만 보이게 찾기 칸을 채운다. 마침표까지 넣어 PlayerHit이 PlayerHeavyHit와 섞이지 않게 한다.
	// 출처 이름은 "카메라 매니저 › 에셋" 꼴이라 마지막 조각(에셋 이름)만 쓴다.
	FString AssetName = ImpactSourceLabel;
	int32 Separator = INDEX_NONE;
	if (AssetName.FindLastChar(TEXT(' '), Separator))
	{
		AssetName.RightChopInline(Separator + 1);
	}
	Filter = FString::Printf(TEXT("%s %s."), *AssetName, **Moment);
	if (SearchBox.IsValid())
	{
		SearchBox->SetText(FText::FromString(Filter));
	}
	RebuildRows();
}

FReply SknightFeedbackTuning::OnImpactPreviewClicked()
{
	if (!GetPlayWorld() || !SelectedImpactMoment.IsValid())
	{
		knightFeedbackTuningTab::Notify(TEXT("PIE 중에 카메라 반응 순간을 고른 뒤 미리보기를 누르세요."), false);
		return FReply::Handled();
	}
	if (!Model.PreviewImpactMoment(*SelectedImpactMoment))
	{
		LastMessage = TEXT("카메라 반응 미리보기를 재생하지 못했어요(카메라 매니저 확인).");
	}
	return FReply::Handled();
}

FReply SknightFeedbackTuning::OnPlaceClicked()
{
	Model.PlacePlayer(GetDefault<UknightAnimDebugSettings>()->FeedbackPlaceDistance);
	return FReply::Handled();
}

FReply SknightFeedbackTuning::OnSaveClicked()
{
	FString Message;
	const int32 Saved = Model.SaveChanged(Message);
	LastMessage = Message;
	knightFeedbackTuningTab::Notify(Message, Saved > 0);
	return FReply::Handled();
}

FReply SknightFeedbackTuning::OnRevertClicked()
{
	const int32 Reverted = Model.RevertUnsaved();
	LastMessage = FString::Printf(TEXT("값 %d개를 되돌렸어요."), Reverted);
	return FReply::Handled();
}

FReply SknightFeedbackTuning::OnCopyClicked()
{
	const FString Text = Model.DescribeChanges();
	FPlatformApplicationMisc::ClipboardCopy(*Text);
	LastMessage = Text.IsEmpty() ? TEXT("바뀐 값이 없어요.") : TEXT("바뀐 값을 클립보드에 복사했어요.");
	return FReply::Handled();
}

FText SknightFeedbackTuning::GetStatusText() const
{
	if (!GetPlayWorld())
	{
		return FText::FromString(LastMessage.IsEmpty() ? TEXT("PIE 중에만 동작해요.") : LastMessage);
	}
	const APawn* const Monster = Model.GetMonster();
	FString Status = FString::Printf(TEXT("대상 %s · 값 %d개 · 저장 안 한 값 %d개"),
		Monster ? *Monster->GetName() : TEXT("없음"), Model.GetEntries().Num(), Model.CountChanged());
	if (!LastMessage.IsEmpty())
	{
		Status += TEXT(" · ") + LastMessage;
	}
	return FText::FromString(Status);
}
