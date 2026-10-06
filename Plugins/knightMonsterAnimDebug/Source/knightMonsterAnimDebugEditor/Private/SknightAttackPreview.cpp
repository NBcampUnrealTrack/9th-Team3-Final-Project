#include "SknightAttackPreview.h"

#include "Abilities/GameplayAbility.h"
#include "AdvancedPreviewScene.h"
#include "FileHelpers.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SknightAttackPreviewViewport.h"
#include "SknightAttackTimeline.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "knightAttackPreviewModel.h"
#include "knightAttackSelectionSimulator.h"

namespace knightAttackPreviewWidget
{
	static FText Str(const TCHAR* Text)
	{
		return FText::FromString(Text);
	}

	static TSharedRef<SWidget> Heading(const TCHAR* Text)
	{
		return SNew(STextBlock)
			.Text(Str(Text))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10));
	}

	/** "이름  [숫자]" 한 줄. */
	static TSharedRef<SWidget> NumberRow(const TCHAR* Label, TFunction<float()> Get, TFunction<void(float)> Set, float Min, float Max, float Delta)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.45f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Str(Label))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.55f)
			[
				SNew(SSpinBox<float>)
				.MinValue(Min)
				.MaxValue(Max)
				.Delta(Delta)
				.Value_Lambda(MoveTemp(Get))
				.OnValueChanged_Lambda(MoveTemp(Set))
			];
	}

	/** 켜기·끄기 체크박스. OnChanged가 있으면 바뀐 뒤에 부른다. */
	static TSharedRef<SWidget> Toggle(const FString& Label, TFunction<bool()> Get, TFunction<void(bool)> Set)
	{
		return SNew(SCheckBox)
			.IsChecked_Lambda([Get]() { return Get() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Set](ECheckBoxState State) { Set(State == ECheckBoxState::Checked); })
			[
				SNew(STextBlock).Text(FText::FromString(Label))
			];
	}

	static TSharedRef<SWidget> Toggle(const TCHAR* Label, bool* Flag)
	{
		return Toggle(Label, [Flag]() { return *Flag; }, [Flag](bool bValue) { *Flag = bValue; });
	}
}

void SknightAttackPreview::Construct(const FArguments& InArgs)
{
	Model = MakeShared<FknightAttackPreviewModel>(MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues()));

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bShowOptions = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	SelectionDetails = PropertyEditor.CreateDetailView(DetailsArgs);
	// 어빌리티 기본 객체 중 선택 조건 구조체만 보여준다. 고치면 어빌리티 Blueprint에 바로 반영된다(저장은 따로).
	SelectionDetails->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& PropertyAndParent)
	{
		const TSharedPtr<IknightAttackSelectionSimulator> Simulator = knightAnimDebugEditor::GetAttackSelectionSimulator();
		if (!Simulator.IsValid())
		{
			return false;
		}
		const FName SelectionName = Simulator->GetSelectionPropertyName();
		if (PropertyAndParent.Property.GetFName() == SelectionName)
		{
			return true;
		}
		return PropertyAndParent.ParentProperties.ContainsByPredicate([SelectionName](const FProperty* Parent) { return Parent && Parent->GetFName() == SelectionName; });
	}));
	SelectionDetails->OnFinishedChangingProperties().AddSP(this, &SknightAttackPreview::OnSelectionEdited);

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Horizontal)
		+ SSplitter::Slot()
		.Value(0.72f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SAssignNew(Viewport, SknightAttackPreviewViewport)
				.Model(Model)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(SknightAttackTimeline).Model(Model)
			]
		]
		+ SSplitter::Slot()
		.Value(0.28f)
		[
			MakeSidePanel()
		]
	];

	RebuildMonsterOptions();
}

TSharedRef<SWidget> SknightAttackPreview::MakeSidePanel()
{
	using namespace knightAttackPreviewWidget;
	FknightAttackPreviewModel* const M = Model.Get();

	auto MakeComboText = [this](bool bMonster)
	{
		return SNew(STextBlock).Text_Lambda([this, bMonster]()
		{
			if (bMonster)
			{
				return FText::FromString(Model->GetNumMonsters() > 0 ? Model->GetMonsterName(Model->GetMonsterIndex()) : TEXT("몬스터 없음"));
			}
			const FknightAbilityAnimInfo* const Ability = Model->GetAbility();
			return FText::FromString(Ability ? Ability->Name : TEXT("어빌리티 없음"));
		});
	};

	return SNew(SBorder)
		.Padding(8.f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				Heading(TEXT("선택"))
			]
			+ SScrollBox::Slot()
			.Padding(0.f, 2.f)
			[
				SAssignNew(MonsterCombo, SComboBox<TSharedPtr<int32>>)
				.OptionsSource(&MonsterOptions)
				.OnGenerateWidget_Lambda([this](TSharedPtr<int32> Item) { return SNew(STextBlock).Text(FText::FromString(Model->GetMonsterName(*Item))); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<int32> Item, ESelectInfo::Type)
				{
					if (Item.IsValid() && *Item != Model->GetMonsterIndex())
					{
						Model->SelectMonster(*Item);
						RebuildAbilityOptions();
					}
				})
				[
					MakeComboText(true)
				]
			]
			+ SScrollBox::Slot()
			.Padding(0.f, 2.f)
			[
				SAssignNew(AbilityCombo, SComboBox<TSharedPtr<int32>>)
				.OptionsSource(&AbilityOptions)
				.OnGenerateWidget_Lambda([this](TSharedPtr<int32> Item)
				{
					const FknightAbilityAnimInfo& Ability = Model->GetAbilities()[*Item];
					return SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s (%s)"), *Ability.Name, *Ability.Kind)));
				})
				.OnSelectionChanged_Lambda([this](TSharedPtr<int32> Item, ESelectInfo::Type)
				{
					if (Item.IsValid() && *Item != Model->GetAbilityIndex())
					{
						Model->SelectAbility(*Item);
						OnAbilityChanged();
					}
				})
				[
					MakeComboText(false)
				]
			]
			+ SScrollBox::Slot()
			.Padding(0.f, 2.f, 0.f, 10.f)
			[
				SNew(SButton)
				.Text(Str(TEXT("몬스터 목록 새로고침")))
				.OnClicked_Lambda([this]()
				{
					Model->RefreshMonsters();
					RebuildMonsterOptions();
					return FReply::Handled();
				})
			]

			+ SScrollBox::Slot()
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				Heading(TEXT("재생"))
			]
			+ SScrollBox::Slot()
			.Padding(0.f, 2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(SButton).Text(Str(TEXT("처음"))).OnClicked_Lambda([M]() { M->bPlaying = false; M->SetTime(0.f); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(SButton).Text(Str(TEXT("<"))).ToolTipText(Str(TEXT("1/30초 뒤로"))).OnClicked_Lambda([M]() { M->StepFrame(-1); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(SButton)
					.Text_Lambda([M]() { return Str(M->bPlaying ? TEXT("정지") : TEXT("재생")); })
					.OnClicked_Lambda([M]()
					{
						if (!M->bPlaying && M->GetTime() >= M->GetDuration())
						{
							M->SetTime(0.f);
						}
						M->bPlaying = !M->bPlaying;
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(Str(TEXT(">"))).ToolTipText(Str(TEXT("1/30초 앞으로"))).OnClicked_Lambda([M]() { M->StepFrame(1); return FReply::Handled(); })
				]
			]
			+ SScrollBox::Slot()
			.Padding(0.f, 2.f)
			[
				SNew(STextBlock).Text_Lambda([M]() { return FText::FromString(FString::Printf(TEXT("%.2f / %.2f s"), M->GetTime(), M->GetDuration())); })
			]
			+ SScrollBox::Slot()
			.Padding(0.f, 2.f)
			[
				NumberRow(TEXT("배속"), [M]() { return M->Speed; }, [M](float Value) { M->Speed = Value; }, 0.05f, 2.f, 0.05f)
			]
			+ SScrollBox::Slot()
			.Padding(0.f, 2.f, 0.f, 10.f)
			[
				Toggle(TEXT("반복 재생"), &M->bLoop)
			]

			+ SScrollBox::Slot()
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				Heading(TEXT("플레이어 더미"))
			]
			+ SScrollBox::Slot()
			.Padding(0.f, 2.f)
			[
				NumberRow(TEXT("거리(cm)"), [M]() { return M->DummyDistance; }, [M](float Value) { M->DummyDistance = Value; M->RecomputeHits(); }, 0.f, 5000.f, 10.f)
			]
			+ SScrollBox::Slot()
			.Padding(0.f, 2.f)
			[
				NumberRow(TEXT("각도(도, 오른쪽 +)"), [M]() { return M->DummyAngle; }, [M](float Value) { M->DummyAngle = Value; M->RecomputeHits(); }, -180.f, 180.f, 5.f)
			]
			+ SScrollBox::Slot()
			.Padding(0.f, 2.f)
			[
				NumberRow(TEXT("반지름(cm)"), [M]() { return M->DummyRadius; }, [M](float Value) { M->DummyRadius = Value; M->RecomputeHits(); }, 1.f, 300.f, 1.f)
			]
			+ SScrollBox::Slot()
			.Padding(0.f, 2.f, 0.f, 10.f)
			[
				NumberRow(TEXT("반 높이(cm)"), [M]() { return M->DummyHalfHeight; }, [M](float Value) { M->DummyHalfHeight = Value; M->RecomputeHits(); }, 1.f, 500.f, 1.f)
			]

			+ SScrollBox::Slot()
			.Padding(0.f, 0.f, 0.f, 10.f)
			[
				MakeSelectionSection()
			]

			+ SScrollBox::Slot()
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				Heading(TEXT("표시"))
			]
			+ SScrollBox::Slot().Padding(0.f, 1.f) [ Toggle(TEXT("근접 판정 궤적"), &M->bShowTrail) ]
			+ SScrollBox::Slot().Padding(0.f, 1.f) [ Toggle(TEXT("범위 공격 구체"), &M->bShowArea) ]
			+ SScrollBox::Slot().Padding(0.f, 1.f) [ Toggle(TEXT("플레이어 더미"), &M->bShowDummy) ]
			+ SScrollBox::Slot().Padding(0.f, 1.f) [ Toggle(TEXT("선택 범위(사거리·각도)"), &M->bShowSelectionShape) ]
			+ SScrollBox::Slot()
			.Padding(0.f, 2.f, 0.f, 10.f)
			[
				SNew(SButton)
				.Text(Str(TEXT("카메라 초기화")))
				.OnClicked_Lambda([this]() { Viewport->ResetCamera(); return FReply::Handled(); })
			]

			+ SScrollBox::Slot()
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				Heading(TEXT("정보"))
			]
			+ SScrollBox::Slot()
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(this, &SknightAttackPreview::GetInfoText)
			]
		];
}

TSharedRef<SWidget> SknightAttackPreview::MakeSelectionSection()
{
	using namespace knightAttackPreviewWidget;
	FknightAttackPreviewModel* const M = Model.Get();
	const TSharedPtr<IknightAttackSelectionSimulator> Simulator = knightAnimDebugEditor::GetAttackSelectionSimulator();

	TSharedRef<SVerticalBox> Section = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 4.f)
		[
			Heading(TEXT("공격 선택"))
		];

	if (!Simulator.IsValid())
	{
		Section->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(Str(TEXT("공격 선택 시뮬레이터가 등록되지 않았다. 게임 쪽 에디터 모듈(knightEditor)이 등록한다.")))
		];
		return Section;
	}

	// 상황: 더미 위치(위 패널)에 더해 타깃 공중 여부와 몬스터 상태 태그.
	Section->AddSlot()
	.AutoHeight()
	.Padding(0.f, 1.f)
	[
		Toggle(TEXT("타깃이 공중"), [M]() { return M->bTargetAirborne; }, [M](bool bValue) { M->bTargetAirborne = bValue; M->RecomputeSelection(); })
	];
	for (const FGameplayTag& StateTag : Simulator->GetSimulatedStateTags())
	{
		FString Label = StateTag.ToString();
		int32 DotIndex = INDEX_NONE;
		if (Label.FindLastChar(TEXT('.'), DotIndex))
		{
			Label = TEXT("몬스터 상태: ") + Label.RightChop(DotIndex + 1);
		}
		Section->AddSlot()
		.AutoHeight()
		.Padding(0.f, 1.f)
		[
			Toggle(Label,
				[M, StateTag]() { return M->SimulatedTags.HasTagExact(StateTag); },
				[M, StateTag](bool bValue)
				{
					if (bValue)
					{
						M->SimulatedTags.AddTag(StateTag);
					}
					else
					{
						M->SimulatedTags.RemoveTag(StateTag);
					}
					M->RecomputeSelection();
				})
		];
	}

	// 공격 진입 거리: StateTree의 Chase → Attack 전환 거리를 직접 넣는다(StateTree는 연동하지 않는다).
	Section->AddSlot()
	.AutoHeight()
	.Padding(0.f, 4.f, 0.f, 1.f)
	[
		NumberRow(TEXT("공격 진입 거리(cm)"), [M]() { return M->EngageDistance; },
			[M](float Value)
			{
				M->EngageDistance = Value;
				if (M->HasGapMap())
				{
					M->BuildGapMap();
				}
			}, 0.f, 5000.f, 10.f)
	];
	Section->AddSlot()
	.AutoHeight()
	[
		SNew(STextBlock)
		.AutoWrapText(true)
		.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		.Text(Str(TEXT("StateTree에서 Chase가 Attack으로 넘어가는 거리. 0이면 표시하지 않는다.")))
	];

	// 결과 표: 공격마다 한 줄(클릭하면 그 공격을 미리보기로 고른다).
	Section->AddSlot()
	.AutoHeight()
	.Padding(0.f, 6.f, 0.f, 2.f)
	[
		SNew(STextBlock).Text(Str(TEXT("현재 더미 위치에서 (클릭: 미리보기)")))
	];
	Section->AddSlot()
	.AutoHeight()
	[
		SAssignNew(SelectionRowsBox, SVerticalBox)
	];

	// 공백 지도.
	Section->AddSlot()
	.AutoHeight()
	.Padding(0.f, 6.f, 0.f, 2.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
		[
			SNew(SButton)
			.Text(Str(TEXT("공백 지도 만들기")))
			.ToolTipText(Str(TEXT("더미를 몬스터 주변 격자로 옮겨 가며 고를 수 있는 공격 수를 센다. 빨강 = 고를 공격이 없는 곳.")))
			.OnClicked_Lambda([M]() { M->BuildGapMap(); return FReply::Handled(); })
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.Text(Str(TEXT("지우기")))
			.OnClicked_Lambda([M]() { M->ClearGapMap(); return FReply::Handled(); })
		]
	];
	Section->AddSlot()
	.AutoHeight()
	[
		SNew(STextBlock)
		.AutoWrapText(true)
		.Text_Lambda([M]() { return FText::FromString(M->HasGapMap() ? M->GetGapSummary() : FString(TEXT("공백 지도 없음"))); })
	];

	// 선택 조건 편집.
	Section->AddSlot()
	.AutoHeight()
	.Padding(0.f, 8.f, 0.f, 2.f)
	[
		SNew(STextBlock)
		.AutoWrapText(true)
		.Text_Lambda([M]()
		{
			const FknightAbilityAnimInfo* const Ability = M->GetAbility();
			return FText::FromString(FString::Printf(TEXT("선택 조건 편집: %s"), Ability ? *Ability->Name : TEXT("-")));
		})
	];
	Section->AddSlot()
	.AutoHeight()
	[
		SelectionDetails.ToSharedRef()
	];
	Section->AddSlot()
	.AutoHeight()
	.Padding(0.f, 2.f)
	[
		SNew(SButton)
		.Text(Str(TEXT("어빌리티 저장")))
		.ToolTipText(Str(TEXT("고친 선택 조건을 어빌리티 Blueprint 에셋에 저장한다.")))
		.IsEnabled_Lambda([M]()
		{
			const FknightAbilityAnimInfo* const Ability = M->GetAbility();
			return Ability && Ability->AbilityCDO && Ability->AbilityCDO->GetOutermost()->IsDirty();
		})
		.OnClicked_Lambda([M]()
		{
			if (const FknightAbilityAnimInfo* const Ability = M->GetAbility(); Ability && Ability->AbilityCDO)
			{
				UEditorLoadingAndSavingUtils::SavePackages({ Ability->AbilityCDO->GetOutermost() }, true);
			}
			return FReply::Handled();
		})
	];
	return Section;
}

void SknightAttackPreview::RebuildSelectionRows()
{
	if (!SelectionRowsBox.IsValid())
	{
		return;
	}
	SelectionRowsBox->ClearChildren();
	FknightAttackPreviewModel* const M = Model.Get();

	// 행 구성(어떤 공격들이 있는지)은 몬스터가 바뀔 때만 바뀌고, 각 행의 내용은 매 프레임 모델에서 읽는다.
	for (const FknightSelectionRow& Row : Model->GetSelectionRows())
	{
		const int32 AbilityIndex = Row.AbilityIndex;
		auto FindRow = [M, AbilityIndex]() { return M->GetSelectionRows().FindByPredicate([AbilityIndex](const FknightSelectionRow& Candidate) { return Candidate.AbilityIndex == AbilityIndex; }); };
		SelectionRowsBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 1.f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
			.OnClicked_Lambda([this, AbilityIndex]()
			{
				Model->SelectAbility(AbilityIndex);
				OnAbilityChanged();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text_Lambda([M, AbilityIndex, FindRow]()
				{
					const FknightSelectionRow* const Current = FindRow();
					const FString Name = M->GetAbilities().IsValidIndex(AbilityIndex) ? M->GetAbilities()[AbilityIndex].Name : FString();
					if (!Current)
					{
						return FText::FromString(Name);
					}
					const FString Marker = M->GetAbilityIndex() == AbilityIndex ? TEXT("> ") : TEXT("   ");
					return FText::FromString(Current->Result.bAvailable
						? FString::Printf(TEXT("%s%s  %.0f%% (가중치 %.1f)"), *Marker, *Name, Current->Probability * 100.f, Current->Result.Weight)
						: FString::Printf(TEXT("%s%s  불가: %s"), *Marker, *Name, *Current->Result.Reason));
				})
				.ColorAndOpacity_Lambda([FindRow]()
				{
					const FknightSelectionRow* const Current = FindRow();
					if (!Current)
					{
						return FSlateColor::UseForeground();
					}
					if (Current->Result.bUnknownCondition)
					{
						return FSlateColor(FLinearColor(1.f, 0.4f, 1.f));
					}
					return FSlateColor(Current->Result.bAvailable ? FLinearColor(0.35f, 0.9f, 0.45f) : FLinearColor(0.6f, 0.6f, 0.6f));
				})
			]
		];
	}
	if (Model->GetSelectionRows().IsEmpty())
	{
		SelectionRowsBox->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("이 몬스터에는 선택 조건을 가진 공격이 없다.")))
		];
	}
}

void SknightAttackPreview::RebuildMonsterOptions()
{
	MonsterOptions.Reset();
	for (int32 Index = 0; Index < Model->GetNumMonsters(); ++Index)
	{
		MonsterOptions.Add(MakeShared<int32>(Index));
	}
	if (MonsterCombo.IsValid())
	{
		MonsterCombo->RefreshOptions();
	}
	RebuildAbilityOptions();
}

void SknightAttackPreview::RebuildAbilityOptions()
{
	AbilityOptions.Reset();
	for (int32 Index = 0; Index < Model->GetAbilities().Num(); ++Index)
	{
		AbilityOptions.Add(MakeShared<int32>(Index));
	}
	if (AbilityCombo.IsValid())
	{
		AbilityCombo->RefreshOptions();
	}
	RebuildSelectionRows();
	OnAbilityChanged();
}

void SknightAttackPreview::OnAbilityChanged()
{
	if (!SelectionDetails.IsValid())
	{
		return;
	}
	const TSharedPtr<IknightAttackSelectionSimulator> Simulator = knightAnimDebugEditor::GetAttackSelectionSimulator();
	const FknightAbilityAnimInfo* const Ability = Model->GetAbility();
	UObject* const Target = Simulator.IsValid() && Ability && Ability->AbilityCDO && Simulator->Supports(Ability->AbilityClass)
		? const_cast<UGameplayAbility*>(Ability->AbilityCDO) : nullptr;
	SelectionDetails->SetObject(Target, true);
}

void SknightAttackPreview::OnSelectionEdited(const FPropertyChangedEvent& Event)
{
	Model->RecomputeSelection();
	if (Model->HasGapMap())
	{
		Model->BuildGapMap();
	}
}

void SknightAttackPreview::SelectAbility(const UClass* MonsterClass, const UClass* AbilityClass)
{
	if (!Model->Select(MonsterClass, AbilityClass))
	{
		// 새로 만든 몬스터일 수 있으니 목록을 다시 읽고 한 번 더 시도한다.
		Model->RefreshMonsters();
		RebuildMonsterOptions();
		Model->Select(MonsterClass, AbilityClass);
	}
	RebuildAbilityOptions();
}

FText SknightAttackPreview::GetInfoText() const
{
	FString Text;
	if (const FknightAbilityAnimInfo* const Ability = Model->GetAbility())
	{
		Text += FString::Printf(TEXT("종류: %s\n"), *Ability->Kind);
		if (!Ability->Phases.IsEmpty())
		{
			Text += FString::Printf(TEXT("타이밍: %s (판정 %s)\n"), *Ability->GetTimingText(),
				Ability->HitTime >= 0.f ? *FString::Printf(TEXT("%.2fs"), Ability->HitTime) : TEXT("-"));
		}
		for (const FknightHitWindow& Window : Model->GetHitWindows())
		{
			Text += FString::Printf(TEXT("%s: 몽타주 %.2f-%.2fs, %s→%s, 반지름 %.0f\n"), *Window.Name, Window.Start, Window.End,
				*Window.StartSocket.ToString(), *Window.EndSocket.ToString(), Window.Radius);
		}
		Text += TEXT("\n") + Model->GetHitSummary() + TEXT("\n");
	}
	if (!Model->GetNotice().IsEmpty())
	{
		Text += TEXT("\n") + Model->GetNotice() + TEXT("\n");
	}
	Text += TEXT("\n근접 궤적은 1/60초 간격 포즈로 다시 만든 근사라 게임의 실제 판정과 조금 다를 수 있다.");
	Text += TEXT("\n공격 선택의 분홍 줄은 시뮬레이터가 흉내 내지 못한 조건(게임 판정 함수만 아는 조건)이 있다는 뜻이다.");
	return FText::FromString(Text);
}
