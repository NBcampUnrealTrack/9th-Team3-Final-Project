#include "SknightAnimCoverageBoard.h"

#include "Editor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "knightMonsterAnimDebugEditorModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"

namespace knightAnimCoverageBoard
{
	static const FName ColumnSeverity(TEXT("Severity"));
	static const FName ColumnMonster(TEXT("Monster"));
	static const FName ColumnAbility(TEXT("Ability"));
	static const FName ColumnKind(TEXT("Kind"));
	static const FName ColumnSlot(TEXT("Slot"));
	static const FName ColumnMontage(TEXT("Montage"));
	static const FName ColumnLength(TEXT("Length"));
	static const FName ColumnTiming(TEXT("Timing"));
	static const FName ColumnHitNotify(TEXT("HitNotify"));
	static const FName ColumnIssues(TEXT("Issues"));

	static FSlateColor SeverityColor(EknightAnimCoverageSeverity Severity)
	{
		switch (Severity)
		{
		case EknightAnimCoverageSeverity::Ok:      return FSlateColor(FLinearColor(0.3f, 0.85f, 0.4f));
		case EknightAnimCoverageSeverity::Warning: return FSlateColor(FLinearColor(1.f, 0.75f, 0.2f));
		case EknightAnimCoverageSeverity::Error:   return FSlateColor(FLinearColor(1.f, 0.3f, 0.3f));
		}
		return FSlateColor::UseForeground();
	}

	static void Notify(const FString& Message, bool bSuccess)
	{
		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = 5.f;
		Info.bUseLargeFont = false;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	/** 표의 한 행. 칸마다 텍스트 하나이고, 상태 칸과 내용 칸은 심각도 색을 쓴다. */
	class SRow : public SMultiColumnTableRow<TSharedPtr<FknightAnimCoverageRow>>
	{
	public:
		SLATE_BEGIN_ARGS(SRow) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedPtr<FknightAnimCoverageRow> InRow)
		{
			Row = InRow;
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments().Padding(FMargin(2.f, 1.f)), OwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			FString Text;
			FSlateColor Color = FSlateColor::UseForeground();
			if (ColumnName == ColumnSeverity)
			{
				Text = knightAnimCoverage::SeverityLabel(Row->Severity);
				Color = SeverityColor(Row->Severity);
			}
			else if (ColumnName == ColumnMonster)   { Text = Row->Monster; }
			else if (ColumnName == ColumnAbility)   { Text = Row->Ability; }
			else if (ColumnName == ColumnKind)      { Text = Row->Kind; }
			else if (ColumnName == ColumnSlot)      { Text = Row->Slot; }
			else if (ColumnName == ColumnMontage)   { Text = Row->MontageName; }
			else if (ColumnName == ColumnLength)    { Text = Row->MontageLength > 0.f ? FString::Printf(TEXT("%.2f"), Row->MontageLength) : FString(); }
			else if (ColumnName == ColumnTiming)    { Text = Row->Timing; }
			else if (ColumnName == ColumnHitNotify) { Text = Row->HitNotify; }
			else if (ColumnName == ColumnIssues)
			{
				Text = Row->GetIssueText();
				Color = SeverityColor(Row->Severity);
			}

			return SNew(STextBlock)
				.Text(FText::FromString(Text))
				.ToolTipText(FText::FromString(Text))
				.ColorAndOpacity(Color);
		}

	private:
		TSharedPtr<FknightAnimCoverageRow> Row;
	};
}

void SknightAnimCoverageBoard::Construct(const FArguments& InArgs)
{
	using namespace knightAnimCoverageBoard;

	auto Column = [](FName Id, const TCHAR* Label, float Width)
	{
		return SHeaderRow::Column(Id).DefaultLabel(FText::FromString(Label)).FillWidth(Width);
	};

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("새로고침")))
				.ToolTipText(FText::FromString(TEXT("몬스터 Blueprint와 어빌리티를 다시 읽어 검사한다.")))
				.OnClicked(this, &SknightAnimCoverageBoard::OnRefreshClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Markdown 저장")))
				.ToolTipText(FText::FromString(TEXT("Saved/Profiling/knightAnimDebug/AnimCoverage_<시각>.md로 저장한다.")))
				.OnClicked(this, &SknightAnimCoverageBoard::OnSaveClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("클립보드에 복사")))
				.ToolTipText(FText::FromString(TEXT("같은 Markdown을 클립보드에 복사한다. 이슈나 문서에 바로 붙여 넣는다.")))
				.OnClicked(this, &SknightAnimCoverageBoard::OnCopyClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bProblemsOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					bProblemsOnly = State == ECheckBoxState::Checked;
					ApplyFilter();
				})
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("주의·문제만 보기")))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(this, &SknightAnimCoverageBoard::GetSummaryText)
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SBorder)
			.Padding(2.f)
			[
				SAssignNew(ListView, SListView<TSharedPtr<FknightAnimCoverageRow>>)
				.ListItemsSource(&VisibleRows)
				.OnGenerateRow(this, &SknightAnimCoverageBoard::GenerateRow)
				.OnMouseButtonDoubleClick(this, &SknightAnimCoverageBoard::OnRowDoubleClicked)
				.OnContextMenuOpening(this, &SknightAnimCoverageBoard::OnContextMenuOpening)
				.SelectionMode(ESelectionMode::Single)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ Column(ColumnSeverity, TEXT("상태"), 0.5f)
					+ Column(ColumnMonster, TEXT("몬스터"), 1.4f)
					+ Column(ColumnAbility, TEXT("어빌리티"), 1.4f)
					+ Column(ColumnKind, TEXT("종류"), 0.9f)
					+ Column(ColumnSlot, TEXT("슬롯"), 0.9f)
					+ Column(ColumnMontage, TEXT("몽타주"), 1.4f)
					+ Column(ColumnLength, TEXT("길이(s)"), 0.5f)
					+ Column(ColumnTiming, TEXT("타이밍(s)"), 1.1f)
					+ Column(ColumnHitNotify, TEXT("판정 노티파이"), 1.1f)
					+ Column(ColumnIssues, TEXT("내용"), 2.5f)
				)
			]
		]
	];

	Refresh();
}

void SknightAnimCoverageBoard::Refresh()
{
	Result = knightAnimCoverage::Gather();
	ApplyFilter();
}

void SknightAnimCoverageBoard::ApplyFilter()
{
	VisibleRows.Reset();
	for (const TSharedPtr<FknightAnimCoverageRow>& Row : Result.Rows)
	{
		if (!bProblemsOnly || Row->Severity != EknightAnimCoverageSeverity::Ok)
		{
			VisibleRows.Add(Row);
		}
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SknightAnimCoverageBoard::GenerateRow(TSharedPtr<FknightAnimCoverageRow> Row, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(knightAnimCoverageBoard::SRow, OwnerTable, Row);
}

void SknightAnimCoverageBoard::OnRowDoubleClicked(TSharedPtr<FknightAnimCoverageRow> Row)
{
	UObject* const Target = Row.IsValid() ? Row->OpenTarget.Get() : nullptr;
	if (Target && GEditor)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Target);
	}
}

TSharedPtr<SWidget> SknightAnimCoverageBoard::OnContextMenuOpening()
{
	const TArray<TSharedPtr<FknightAnimCoverageRow>> Selected = ListView->GetSelectedItems();
	const TSharedPtr<FknightAnimCoverageRow> Row = Selected.IsEmpty() ? nullptr : Selected[0];
	if (!Row.IsValid())
	{
		return nullptr;
	}

	FMenuBuilder Menu(true, nullptr);
	Menu.AddMenuEntry(FText::FromString(TEXT("공격 미리보기에서 열기")),
		FText::FromString(TEXT("게임을 실행하지 않고 이 어빌리티의 판정 궤적과 범위를 본다.")),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([Row]() { knightAnimDebugEditor::OpenAttackPreview(Row->MonsterClass.Get(), Row->AbilityClass.Get()); }),
			FCanExecuteAction::CreateLambda([Row]() { return Row->MonsterClass.IsValid(); })));
	Menu.AddMenuEntry(FText::FromString(TEXT("에셋 열기")),
		FText::FromString(TEXT("몽타주, 없으면 어빌리티나 몬스터 Blueprint를 연다.")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SknightAnimCoverageBoard::OnRowDoubleClicked, Row)));
	return Menu.MakeWidget();
}

FReply SknightAnimCoverageBoard::OnRefreshClicked()
{
	Refresh();
	return FReply::Handled();
}

FReply SknightAnimCoverageBoard::OnSaveClicked()
{
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProfilingDir() / TEXT("knightAnimDebug")
		/ FString::Printf(TEXT("AnimCoverage_%s.md"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))));
	const bool bSaved = FFileHelper::SaveStringToFile(knightAnimCoverage::ToMarkdown(Result), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	knightAnimCoverageBoard::Notify(bSaved ? TEXT("저장했다: ") + Path : TEXT("저장하지 못했다: ") + Path, bSaved);
	return FReply::Handled();
}

FReply SknightAnimCoverageBoard::OnCopyClicked()
{
	FPlatformApplicationMisc::ClipboardCopy(*knightAnimCoverage::ToMarkdown(Result));
	knightAnimCoverageBoard::Notify(TEXT("커버리지 Markdown을 클립보드에 복사했다."), true);
	return FReply::Handled();
}

FText SknightAnimCoverageBoard::GetSummaryText() const
{
	FString Text = FString::Printf(TEXT("몬스터 %d종 · 항목 %d개 · 정상 %d · 주의 %d · 문제 %d"), Result.MonsterCount, Result.Rows.Num(),
		Result.CountSeverity(EknightAnimCoverageSeverity::Ok), Result.CountSeverity(EknightAnimCoverageSeverity::Warning),
		Result.CountSeverity(EknightAnimCoverageSeverity::Error));
	if (!Result.Notice.IsEmpty())
	{
		Text += TEXT("   ") + Result.Notice;
	}
	return FText::FromString(Text);
}
