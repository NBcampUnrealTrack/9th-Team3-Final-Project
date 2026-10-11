#include "SMechPartsWorkbench.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "ContentBrowserModule.h"
#include "FileHelpers.h"
#include "IContentBrowserSingleton.h"
#include "IDetailsView.h"
#include "MechPartReflection.h"
#include "MechPartsWorkbenchSettings.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SMechPartsWorkbenchViewport.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MechPartsWorkbench"

namespace
{
	/** 분류 버튼 글자. 태그는 마지막 부분만(Equipment.Slot.Head → Head), 빈 값은 '없음'. */
	FText MakeFilterValueLabel(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return LOCTEXT("NoValue", "없음");
		}
		int32 DotIndex = INDEX_NONE;
		return FText::FromString(Value.FindLastChar(TEXT('.'), DotIndex) ? Value.RightChop(DotIndex + 1) : Value);
	}
}

void SMechPartsWorkbench::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs PartArgs;
	PartArgs.bUpdatesFromSelection = false;
	PartArgs.bLockable = false;
	PartArgs.bHideSelectionTip = true;
	PartArgs.NameAreaSettings = FDetailsViewArgs::ObjectsUseNameArea;
	PartDetails = PropertyEditor.CreateDetailView(PartArgs);

	FDetailsViewArgs SettingsArgs = PartArgs;
	SettingsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	SettingsArgs.bAllowSearch = false;
	SettingsDetails = PropertyEditor.CreateDetailView(SettingsArgs);
	SettingsDetails->SetObject(GetMutableDefault<UMechPartsWorkbenchSettings>());

	PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SMechPartsWorkbench::HandleObjectPropertyChanged);
	SettingsChangedHandle = GetMutableDefault<UMechPartsWorkbenchSettings>()->OnSettingChanged().AddSP(this, &SMechPartsWorkbench::HandleSettingsChanged);

	PartClassPaths = MechPartReflection::FindPartClassPaths();
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	AssetRegistry.OnAssetAdded().AddSP(this, &SMechPartsWorkbench::MarkFiltersDirty);
	AssetRegistry.OnAssetRemoved().AddSP(this, &SMechPartsWorkbench::MarkFiltersDirty);
	AssetRegistry.OnAssetUpdated().AddSP(this, &SMechPartsWorkbench::MarkFiltersDirty);
	AssetRegistry.OnAssetRenamed().AddSPLambda(this, [this](const FAssetData& AssetData, const FString&) { MarkFiltersDirty(AssetData); });

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Horizontal)

		// 왼쪽: 분류 버튼 + 파츠 목록
		+ SSplitter::Slot()
		.Value(0.22f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(FilterBox, SVerticalBox)
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				MakePartPicker()
			]
		]

		// 가운데: 3D 미리보기
		+ SSplitter::Slot()
		.Value(0.53f)
		[
			SAssignNew(Viewport, SMechPartsWorkbenchViewport)
		]

		// 오른쪽: 저장 버튼 + 파츠 속성 + 미리보기 설정
		+ SSplitter::Slot()
		.Value(0.25f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("Save", "저장"))
				.ToolTipText(LOCTEXT("SaveTooltip", "선택한 파츠 에셋 파일을 저장합니다. 바뀐 내용이 있을 때만 누를 수 있습니다."))
				.IsEnabled(this, &SMechPartsWorkbench::CanSave)
				.OnClicked(this, &SMechPartsWorkbench::HandleSaveClicked)
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)

				+ SSplitter::Slot()
				.Value(0.7f)
				[
					PartDetails.ToSharedRef()
				]

				+ SSplitter::Slot()
				.Value(0.3f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PreviewSettings", "미리보기 설정 (개인 설정)"))
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					[
						SettingsDetails.ToSharedRef()
					]
				]
			]
		]
	];

	RebuildFilters();
}

SMechPartsWorkbench::~SMechPartsWorkbench()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
	if (UObjectInitialized())
	{
		GetMutableDefault<UMechPartsWorkbenchSettings>()->OnSettingChanged().Remove(SettingsChangedHandle);
	}
	if (IAssetRegistry* const AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->OnAssetAdded().RemoveAll(this);
		AssetRegistry->OnAssetRemoved().RemoveAll(this);
		AssetRegistry->OnAssetUpdated().RemoveAll(this);
		AssetRegistry->OnAssetRenamed().RemoveAll(this);
	}
}

void SMechPartsWorkbench::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (bFiltersDirty)
	{
		RebuildFilters();
	}
}

TSharedRef<SWidget> SMechPartsWorkbench::MakePartPicker()
{
	FAssetPickerConfig Config;
	Config.Filter.ClassPaths = PartClassPaths;
	Config.Filter.bRecursiveClasses = true;
	Config.SelectionMode = ESelectionMode::Single;
	Config.InitialAssetViewType = EAssetViewType::List;
	Config.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SMechPartsWorkbench::HandlePartSelected);
	Config.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SMechPartsWorkbench::ShouldHidePart);
	Config.RefreshAssetViewDelegates.Add(&RefreshPartList);
	Config.bAllowNullSelection = true;
	Config.bCanShowClasses = false;
	Config.bAllowDragging = false;
	Config.SaveSettingsName = TEXT("MechPartsWorkbenchPartPicker");

	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	return ContentBrowser.CreateAssetPicker(Config);
}

void SMechPartsWorkbench::RebuildFilters()
{
	bFiltersDirty = false;

	// 분류 줄: AssetRegistrySearchable 속성마다 한 줄(지금은 Slot Tag) + 폴더(세트) 한 줄.
	TArray<FFilterRow> OldRows = MoveTemp(FilterRows);
	FilterRows.Reset();
	for (const FProperty* const Property : MechPartReflection::FindGroupingProperties(PartClassPaths))
	{
		FFilterRow& Row = FilterRows.AddDefaulted_GetRef();
		Row.Label = Property->GetDisplayNameText();
		Row.Property = Property;
	}
	FFilterRow& FolderRow = FilterRows.AddDefaulted_GetRef();
	FolderRow.Label = LOCTEXT("FolderRow", "세트(폴더)");

	// 파츠마다 값을 읽어 둔다. 세트 = 에셋이 바로 들어 있는 폴더 이름 (Parts/Janyeol/DA_... → Janyeol).
	FARFilter Filter;
	Filter.ClassPaths = PartClassPaths;
	Filter.bRecursiveClasses = true;
	TArray<FAssetData> Assets;
	IAssetRegistry::GetChecked().GetAssets(Filter, Assets);

	PartGroupValues.Reset();
	for (const FAssetData& AssetData : Assets)
	{
		TArray<FString>& Values = PartGroupValues.Add(AssetData.GetSoftObjectPath());
		for (FFilterRow& Row : FilterRows)
		{
			const FString Value = Row.Property
				? MechPartReflection::GetGroupingValue(AssetData, Row.Property)
				: FPackageName::GetShortName(AssetData.PackagePath);
			Values.Add(Value);
			Row.Values.AddUnique(Value);
		}
	}

	for (FFilterRow& Row : FilterRows)
	{
		// 이름순, 빈 값('없음')은 맨 뒤.
		Row.Values.Sort([](const FString& A, const FString& B)
		{
			return A.IsEmpty() != B.IsEmpty() ? B.IsEmpty() : A < B;
		});
		// 다시 만들기 전에 고른 값이 아직 있으면 유지한다.
		for (const FFilterRow& OldRow : OldRows)
		{
			if (OldRow.Label.EqualTo(Row.Label) && OldRow.Selected.IsSet() && Row.Values.Contains(OldRow.Selected.GetValue()))
			{
				Row.Selected = OldRow.Selected;
			}
		}
	}

	FilterBox->ClearChildren();
	for (int32 RowIndex = 0; RowIndex < FilterRows.Num(); ++RowIndex)
	{
		const TSharedRef<SWrapBox> Buttons = SNew(SWrapBox).UseAllottedSize(true);
		Buttons->AddSlot()
		.Padding(2.f)
		[
			MakeFilterButton(RowIndex, TOptional<FString>(), LOCTEXT("AllValues", "전체"))
		];
		for (const FString& Value : FilterRows[RowIndex].Values)
		{
			Buttons->AddSlot()
			.Padding(2.f)
			[
				MakeFilterButton(RowIndex, Value, MakeFilterValueLabel(Value))
			];
		}

		FilterBox->AddSlot()
		.AutoHeight()
		.Padding(4.f, 2.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FilterRows[RowIndex].Label)
				.MinDesiredWidth(64.f)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				Buttons
			]
		];
	}

	RefreshPartList.ExecuteIfBound(true);
}

TSharedRef<SWidget> SMechPartsWorkbench::MakeFilterButton(int32 RowIndex, const TOptional<FString>& Value, const FText& Label)
{
	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
		.IsChecked_Lambda([this, RowIndex, Value]()
		{
			return FilterRows.IsValidIndex(RowIndex) && FilterRows[RowIndex].Selected == Value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([this, RowIndex, Value](ECheckBoxState)
		{
			if (FilterRows.IsValidIndex(RowIndex))
			{
				FilterRows[RowIndex].Selected = Value;
				RefreshPartList.ExecuteIfBound(true);
			}
		})
		[
			SNew(STextBlock)
			.Text(Label)
			.Margin(FMargin(6.f, 1.f))
		];
}

bool SMechPartsWorkbench::ShouldHidePart(const FAssetData& AssetData) const
{
	const TArray<FString>* const Values = PartGroupValues.Find(AssetData.GetSoftObjectPath());
	if (!Values)
	{
		// 분류 전인 새 에셋은 보이게 둔다 (다음 프레임에 분류된다).
		return false;
	}
	for (int32 RowIndex = 0; RowIndex < FilterRows.Num() && RowIndex < Values->Num(); ++RowIndex)
	{
		const TOptional<FString>& Selected = FilterRows[RowIndex].Selected;
		if (Selected.IsSet() && (*Values)[RowIndex] != Selected.GetValue())
		{
			return true;
		}
	}
	return false;
}

void SMechPartsWorkbench::MarkFiltersDirty(const FAssetData& AssetData)
{
	bFiltersDirty = true;
}

void SMechPartsWorkbench::HandlePartSelected(const FAssetData& AssetData)
{
	UObject* const Asset = AssetData.IsValid() ? AssetData.GetAsset() : nullptr;
	PartAsset = Asset;
	PartDetails->SetObject(Asset);
	Viewport->SetPartAsset(Asset);
}

void SMechPartsWorkbench::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	if (Object && Object == PartAsset.Get())
	{
		Viewport->RefreshPart();
		// 부위(Slot Tag)를 바꿨을 수 있으니 분류도 다시 읽는다.
		bFiltersDirty = true;
	}
}

void SMechPartsWorkbench::HandleSettingsChanged(UObject* Settings, FPropertyChangedEvent& Event)
{
	// 숫자를 끄는 중(Interactive)에는 파일에 쓰지 않고, 놓았을 때 한 번 저장한다.
	if (Event.ChangeType != EPropertyChangeType::Interactive)
	{
		Settings->SaveConfig();
	}
	Viewport->RefreshMech();
}

FReply SMechPartsWorkbench::HandleSaveClicked()
{
	if (const UObject* const Asset = PartAsset.Get())
	{
		FEditorFileUtils::PromptForCheckoutAndSave({ Asset->GetPackage() }, /*bCheckDirty=*/true, /*bPromptToSave=*/false);
	}
	return FReply::Handled();
}

bool SMechPartsWorkbench::CanSave() const
{
	const UObject* const Asset = PartAsset.Get();
	return Asset && Asset->GetPackage()->IsDirty();
}

#undef LOCTEXT_NAMESPACE
