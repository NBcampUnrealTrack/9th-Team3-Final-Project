#include "SMechPartsWorkbench.h"

#include "Animation/AnimationAsset.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ContentBrowserModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "FileHelpers.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IContentBrowserSingleton.h"
#include "IDetailsView.h"
#include "MechBodyRegionSet.h"
#include "MechBodyRegionSplitter.h"
#include "MechPartAttachment.h"
#include "MechPartReflection.h"
#include "MechPartSkinBaker.h"
#include "MechPartsWorkbenchSettings.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"
#include "Widgets/Notifications/SNotificationList.h"
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

		// 왼쪽: 저장 안 된 파츠 알림 + 분류 버튼 + 파츠 목록
		+ SSplitter::Slot()
		.Value(0.22f)
		[
			SNew(SVerticalBox)

			// (10-13) 저장 안 된 파츠가 있을 때만 보인다. 목록의 작은 표시만으로는 놓치기 쉬워서 개수를 크게 보여 준다.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f, 2.f)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([this]() { return GetDirtyPartPackages().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return FText::Format(LOCTEXT("DirtyParts", "저장 안 된 파츠 {0}개"), GetDirtyPartPackages().Num()); })
					.ColorAndOpacity(FLinearColor(1.f, 0.78f, 0.25f))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("SaveAllParts", "모두 저장"))
					.ToolTipText(LOCTEXT("SaveAllPartsTooltip", "저장 안 된 파츠 에셋을 모두 저장합니다."))
					.OnClicked_Lambda([this]() { SaveAllDirtyParts(); return FReply::Handled(); })
				]
			]

			// (10-13) 착용 미리보기 요약. 끼운 파츠가 있거나 원본을 숨겼을 때만 보인다.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f, 2.f)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([this]() { return LoadoutParts.IsEmpty() && !bHideWholeMech ? EVisibility::Collapsed : EVisibility::Visible; })

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text_Lambda([this]() { return GetLoadoutSummary(); })
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("ClearLoadout", "모두 빼기"))
					.ToolTipText(LOCTEXT("ClearLoadoutTooltip", "착용 미리보기에 끼운 파츠를 모두 뺍니다."))
					.Visibility_Lambda([this]() { return LoadoutParts.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
					.OnClicked_Lambda([this]() { ClearLoadout(); return FReply::Handled(); })
				]
			]

			// (10-13) Replace Regions를 미리보기에 보여 줄 수 없을 때(원본 부위 메시가 없음) 안내.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f, 2.f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Visibility_Lambda([this]() { return IsReplaceRegionsUnavailable() ? EVisibility::Visible : EVisibility::Collapsed; })
				.Text(LOCTEXT("NoBodyRegions", "원본 부위 메시가 없어 Replace Regions가 미리보기에 보이지 않습니다. 우클릭 → '원본을 부위별로 나누기'."))
				.ColorAndOpacity(FLinearColor(1.f, 0.78f, 0.25f))
			]

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
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(0.f, 0.f, 2.f, 0.f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("Save", "저장"))
					.ToolTipText(LOCTEXT("SaveTooltip", "선택한 파츠 에셋 파일을 저장합니다. 바뀐 내용이 있을 때만 누를 수 있습니다."))
					.IsEnabled(this, &SMechPartsWorkbench::CanSave)
					.OnClicked(this, &SMechPartsWorkbench::HandleSaveClicked)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(2.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("BakeSkin", "뼈대에 입히기"))
					.ToolTipText(LOCTEXT("BakeSkinTooltip",
						"망토·옷처럼 여러 뼈에 걸친 스태틱 메시 파츠를 메카 뼈대를 따라 휘는 스켈레탈 메시로 바꿉니다.\n"
						"지금 맞춘 위치·회전·크기 그대로 굽고, 원본 옆에 SK_ 메시를 만듭니다(있으면 덮어씀).\n"
						"위치를 다시 맞추려면 Mesh 칸을 비워 스태틱 메시로 돌린 뒤 맞추고 다시 누르세요."))
					.IsEnabled(this, &SMechPartsWorkbench::CanBakeSkin)
					.OnClicked(this, &SMechPartsWorkbench::HandleBakeSkinClicked)
				]
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
	Config.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &SMechPartsWorkbench::MakePartContextMenu);
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
	// 고른 파츠와 같은 부위의 착용 파츠는 빼고 보여 준다.
	PushLoadoutToViewport();
}

void SMechPartsWorkbench::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	if (Object && Object == PartAsset.Get())
	{
		Viewport->RefreshPart();
		// 부위(Slot Tag)를 바꿨을 수 있으니 분류도 다시 읽는다.
		bFiltersDirty = true;
	}
	else if (Object && LoadoutParts.Contains(FSoftObjectPath(Object)))
	{
		// 착용 미리보기 파츠를 되돌리기 등으로 바꿨을 때.
		Viewport->RefreshLoadout();
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

bool SMechPartsWorkbench::CanBakeSkin() const
{
	const UObject* const Asset = PartAsset.Get();
	return Asset
		&& MechPartReflection::FindSkeletalMeshProperty(Asset)
		&& MechPartReflection::LoadPartStaticMesh(Asset)
		&& !GetDefault<UMechPartsWorkbenchSettings>()->PreviewMechMesh.IsNull();
}

FReply SMechPartsWorkbench::HandleBakeSkinClicked()
{
	UObject* const Asset = PartAsset.Get();
	UStaticMesh* const StaticMesh = Asset ? MechPartReflection::LoadPartStaticMesh(Asset) : nullptr;
	FStructProperty* const AttachmentProperty = Asset ? MechPartReflection::FindAttachmentProperty(Asset) : nullptr;
	FSoftObjectProperty* const SkeletalMeshProperty = Asset ? MechPartReflection::FindSkeletalMeshProperty(Asset) : nullptr;
	const UMechPartsWorkbenchSettings* const Settings = GetDefault<UMechPartsWorkbenchSettings>();
	USkeletalMesh* const MechMesh = Settings->PreviewMechMesh.LoadSynchronous();
	if (!StaticMesh || !AttachmentProperty || !SkeletalMeshProperty || !MechMesh)
	{
		return FReply::Handled();
	}

	// 굽기 옵션 (10-12): 따라가지 않을 뼈 = 팔·다리 따라가기를 끈 쪽의 시작 뼈들, 천 시뮬레이션 설정.
	MechPartSkinBaker::FOptions BakeOptions;
	if (!Settings->bSkinFollowArms)
	{
		BakeOptions.ExcludedRootBones.Append(Settings->SkinArmRootBones);
	}
	if (!Settings->bSkinFollowLegs)
	{
		BakeOptions.ExcludedRootBones.Append(Settings->SkinLegRootBones);
	}
	BakeOptions.bMakeCloth = Settings->bMakeCloth;
	BakeOptions.bClothOuterLayerOnly = Settings->bClothOuterLayerOnly;
	// 팔을 따라가지 않는 파츠는 천도 팔과 충돌하지 않게 한다 (팔이 망토 안에 있어서 팔을 들면 천을 끌어올린다, 10-12).
	if (!Settings->bSkinFollowArms)
	{
		BakeOptions.ClothNoCollisionRootBones = Settings->SkinArmRootBones;
	}
	BakeOptions.ClothProxyCellSize = Settings->ClothProxyCellSize;
	BakeOptions.ClothSimVertexCount = Settings->ClothSimVertexCount;
	BakeOptions.ClothFixedTopRatio = Settings->ClothFixedTopRatio;
	BakeOptions.ClothMaxDistance = Settings->ClothMaxDistance;
	BakeOptions.ClothFollowBody = Settings->ClothFollowBody;
	BakeOptions.ClothBendingStiffness = Settings->ClothBendingStiffness;
	BakeOptions.ClothShapeKeepTop = Settings->ClothShapeKeepTop;
	BakeOptions.ClothShapeKeepHem = Settings->ClothShapeKeepHem;
	BakeOptions.ClothGravityScale = Settings->ClothGravityScale;
	FNumberFormattingOptions TwoDigits;
	TwoDigits.SetMaximumFractionalDigits(2);
	const FText ClothText = Settings->bMakeCloth
		? FText::Format(LOCTEXT("BakeSkinClothOn", "켬 (계산용 정점 {0}개, 위 {1}% 고정, 자락 최대 {2}cm, 몸 따라가기 {3}, 접힘 강도 {4}, 형태 유지 {5}~{6}, 중력 {7})"),
			FText::AsNumber(Settings->ClothSimVertexCount), FText::AsNumber(FMath::RoundToInt(Settings->ClothFixedTopRatio * 100.f)),
			FText::AsNumber(FMath::RoundToInt(Settings->ClothMaxDistance)),
			FText::AsNumber(Settings->ClothFollowBody, &TwoDigits), FText::AsNumber(Settings->ClothBendingStiffness, &TwoDigits),
			FText::AsNumber(Settings->ClothShapeKeepTop, &TwoDigits), FText::AsNumber(Settings->ClothShapeKeepHem, &TwoDigits),
			FText::AsNumber(Settings->ClothGravityScale, &TwoDigits))
		: LOCTEXT("BakeSkinClothOff", "끔 (미리보기 설정의 Make Cloth)");
	const FText FollowText = FText::Format(LOCTEXT("BakeSkinFollow", "몸통·머리{0}{1}"),
		Settings->bSkinFollowArms ? LOCTEXT("BakeSkinFollowArms", " + 팔") : FText::GetEmpty(),
		Settings->bSkinFollowLegs ? LOCTEXT("BakeSkinFollowLegs", " + 다리") : FText::GetEmpty());
	// 위치는 기본 자세 기준으로 굽는다. 애니메이션 자세에서 맞춘 값이면 기본 자세에서 다른 곳에 놓인다 (10-12에 겪은 문제).
	const FText PoseWarning = Settings->PreviewAnimation.IsNull() ? FText::GetEmpty() : LOCTEXT("BakeSkinPoseWarning",
		"\n※ 미리보기 애니메이션이 지정돼 있습니다. 위치는 애니메이션이 없는 기본 자세 기준으로 굽습니다.\n"
		"   Preview Animation을 비운 기본 자세에서 위치가 맞는지 먼저 확인하세요.\n");

	const FString TargetPath = MechPartSkinBaker::GetSkinnedMeshPackagePath(StaticMesh);
	const FText Confirm = FText::Format(LOCTEXT("BakeSkinConfirm",
		"'{0}'을(를) {1} 뼈대를 따라 휘는 스켈레탈 메시로 만듭니다.\n\n"
		"- 지금 맞춘 위치·회전·크기 그대로, 기본 자세 기준으로 굽습니다.\n"
		"- 따라갈 부위: {3} (미리보기 설정의 Skin Follow Arms / Legs)\n"
		"- 천 시뮬레이션: {5}\n"
		"- 저장 위치: {2} (이미 있으면 덮어씁니다)\n"
		"- 파츠의 Mesh 칸이 새 메시로, 붙이는 방식이 LeaderPose로 바뀝니다. 원래 스태틱 메시와 위치 값은 남고, Ctrl+Z로 되돌릴 수 있습니다.\n"
		"{4}\n"
		"계속할까요?"),
		FText::FromString(StaticMesh->GetName()), FText::FromString(MechMesh->GetName()), FText::FromString(TargetPath), FollowText, PoseWarning, ClothText);
	if (FMessageDialog::Open(EAppMsgType::OkCancel, Confirm) != EAppReturnType::Ok)
	{
		return FReply::Handled();
	}

	FText Error;
	USkeletalMesh* const SkinnedMesh = MechPartSkinBaker::Bake(
		StaticMesh, *AttachmentProperty->ContainerPtrToValuePtr<FMechPartAttachment>(Asset), MechMesh, BakeOptions, Error);
	if (!SkinnedMesh)
	{
		FMessageDialog::Open(EAppMsgType::Ok, Error);
		return FReply::Handled();
	}

	// 파츠 정의: Mesh 칸 = 새 메시, 붙이는 방식 = LeaderPose. 스태틱 메시·위치 값은 다시 굽기·되돌리기용으로 남긴다.
	{
		const FScopedTransaction Transaction(LOCTEXT("BakeSkinTransaction", "뼈대에 입히기"));
		Asset->Modify();

		SkeletalMeshProperty->SetPropertyValue_InContainer(Asset, FSoftObjectPtr(SkinnedMesh));
		FPropertyChangedEvent MeshChanged(SkeletalMeshProperty);
		Asset->PostEditChangeProperty(MeshChanged);

		AttachmentProperty->ContainerPtrToValuePtr<FMechPartAttachment>(Asset)->AttachMode = EMechPartAttachMode::LeaderPose;
		FPropertyChangedEvent AttachmentChanged(AttachmentProperty);
		Asset->PostEditChangeProperty(AttachmentChanged);
	}
	// 같은 SK_를 다시 구웠으면 메시 칸이 그대로라 미리보기가 예전 천을 쓰므로 새로 붙인다 (10-12).
	Viewport->RecreatePart();

	FNotificationInfo Info(FText::Format(LOCTEXT("BakeSkinDone", "{0}을(를) 만들었습니다. 파츠도 '저장'을 눌러 저장하세요."), FText::FromString(SkinnedMesh->GetName())));
	Info.ExpireDuration = 5.f;
	FSlateNotificationManager::Get().AddNotification(Info);
	return FReply::Handled();
}

TSharedPtr<SWidget> SMechPartsWorkbench::MakePartContextMenu(const TArray<FAssetData>& SelectedAssets)
{
	// 메뉴 기능은 지금 고른 파츠에 동작하므로, 우클릭한 파츠가 다르면 먼저 고른다.
	if (SelectedAssets.Num() == 1 && SelectedAssets[0].GetSoftObjectPath() != FSoftObjectPath(PartAsset.Get()))
	{
		HandlePartSelected(SelectedAssets[0]);
	}

	FMenuBuilder Menu(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	Menu.BeginSection(NAME_None, LOCTEXT("MenuSaveSection", "저장"));
	Menu.AddMenuEntry(LOCTEXT("MenuSave", "저장"),
		LOCTEXT("MenuSaveTooltip", "이 파츠 에셋 파일을 저장합니다. 바뀐 내용이 있을 때만 누를 수 있습니다."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { HandleSaveClicked(); }), FCanExecuteAction::CreateSP(this, &SMechPartsWorkbench::CanSave)));
	const int32 DirtyCount = GetDirtyPartPackages().Num();
	Menu.AddMenuEntry(FText::Format(LOCTEXT("MenuSaveAll", "바뀐 파츠 모두 저장 ({0}개)"), DirtyCount),
		LOCTEXT("MenuSaveAllTooltip", "저장 안 된 파츠 에셋을 모두 저장합니다."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SMechPartsWorkbench::SaveAllDirtyParts), FCanExecuteAction::CreateLambda([DirtyCount]() { return DirtyCount > 0; })));
	Menu.EndSection();

	Menu.BeginSection(NAME_None, LOCTEXT("MenuSkinSection", "뼈대에 입히기"));
	Menu.AddMenuEntry(LOCTEXT("MenuBakeSkin", "뼈대에 입히기"),
		LOCTEXT("MenuBakeSkinTooltip", "오른쪽 '뼈대에 입히기' 버튼과 같습니다. 확인 창에서 설정을 보고 굽습니다."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { HandleBakeSkinClicked(); }), FCanExecuteAction::CreateSP(this, &SMechPartsWorkbench::CanBakeSkin)));
	Menu.AddMenuEntry(LOCTEXT("MenuRevertToStatic", "스태틱 메시로 되돌리기"),
		LOCTEXT("MenuRevertToStaticTooltip",
			"스켈레탈 메시 칸(Mesh)을 비우고 붙이는 방식을 Socket으로 돌립니다. 위치를 다시 맞춘 뒤 다시 구울 때 씁니다.\n"
			"구워 둔 SK_ 메시 파일은 지우지 않습니다. Ctrl+Z로 되돌릴 수 있습니다."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SMechPartsWorkbench::RevertToStaticMesh), FCanExecuteAction::CreateSP(this, &SMechPartsWorkbench::CanRevertToStaticMesh)));
	Menu.EndSection();

	Menu.BeginSection(NAME_None, LOCTEXT("MenuPlacementSection", "배치"));
	Menu.AddMenuEntry(LOCTEXT("MenuCopyPlacement", "배치 복사"),
		LOCTEXT("MenuCopyPlacementTooltip", "이 파츠의 소켓·위치·회전·크기를 복사합니다."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SMechPartsWorkbench::CopyPlacement),
			FCanExecuteAction::CreateLambda([this]() { return PartAsset.IsValid() && MechPartReflection::FindAttachment(PartAsset.Get()); })));
	Menu.AddMenuEntry(CopiedPlacement.IsSet()
			? FText::Format(LOCTEXT("MenuPastePlacementFrom", "배치 붙여넣기 ({0}에서 복사)"), FText::FromString(CopiedPlacementSource))
			: LOCTEXT("MenuPastePlacement", "배치 붙여넣기"),
		LOCTEXT("MenuPastePlacementTooltip", "복사한 소켓·위치·회전·크기를 이 파츠에 넣습니다. 붙이는 방식(Socket/LeaderPose)은 그대로 둡니다. Ctrl+Z로 되돌릴 수 있습니다."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SMechPartsWorkbench::PastePlacement),
			FCanExecuteAction::CreateLambda([this]() { return CopiedPlacement.IsSet() && PartAsset.IsValid() && MechPartReflection::FindAttachmentProperty(PartAsset.Get()); })));
	Menu.EndSection();

	// 착용 미리보기 (10-13)
	const FSoftObjectPath SelectedPath = PartAsset.IsValid() ? FSoftObjectPath(PartAsset.Get()) : FSoftObjectPath();
	const FString FolderKey = SelectedPath.IsValid() ? GetFolderKey(SelectedPath) : FString();
	const int32 LoadoutCount = LoadoutParts.Num();
	Menu.BeginSection(NAME_None, LOCTEXT("MenuLoadoutSection", "착용 미리보기"));
	if (IsInLoadout(SelectedPath))
	{
		Menu.AddMenuEntry(LOCTEXT("MenuLoadoutRemove", "착용 미리보기에서 빼기"),
			LOCTEXT("MenuLoadoutRemoveTooltip", "이 파츠를 착용 미리보기에서 뺍니다."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, SelectedPath]() { RemoveFromLoadout(SelectedPath); })));
	}
	else
	{
		Menu.AddMenuEntry(LOCTEXT("MenuLoadoutAdd", "착용 미리보기에 끼기"),
			LOCTEXT("MenuLoadoutAddTooltip", "이 파츠를 다른 파츠와 함께 끼워 봅니다. 같은 부위(Slot Tag)에 낀 파츠는 이 파츠로 바뀝니다."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, SelectedPath]() { AddToLoadout(SelectedPath); }),
				FCanExecuteAction::CreateLambda([SelectedPath]() { return SelectedPath.IsValid(); })));
	}
	Menu.AddMenuEntry(FText::Format(LOCTEXT("MenuLoadoutFolder", "이 세트({0}) 전부 끼기"), FText::FromString(FolderKey)),
		LOCTEXT("MenuLoadoutFolderTooltip", "같은 폴더의 파츠를 부위마다 하나씩(이름순 첫 번째, A/B가 있으면 A) 끼웁니다."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, FolderKey]() { AddFolderToLoadout(FolderKey); }),
			FCanExecuteAction::CreateLambda([FolderKey]() { return !FolderKey.IsEmpty(); })));
	Menu.AddMenuEntry(FText::Format(LOCTEXT("MenuLoadoutClear", "착용 미리보기 모두 빼기 ({0}개)"), LoadoutCount),
		LOCTEXT("MenuLoadoutClearTooltip", "착용 미리보기에 끼운 파츠를 모두 뺍니다."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SMechPartsWorkbench::ClearLoadout), FCanExecuteAction::CreateLambda([LoadoutCount]() { return LoadoutCount > 0; })));
	Menu.EndSection();

	// 원본 메카 (10-13)
	const UMechPartsWorkbenchSettings* const Settings = GetDefault<UMechPartsWorkbenchSettings>();
	Menu.BeginSection(NAME_None, LOCTEXT("MenuMechSection", "원본 메카"));
	Menu.AddMenuEntry(LOCTEXT("MenuHideWholeMech", "원본 전부 숨기기"),
		LOCTEXT("MenuHideWholeMechTooltip", "원본 메카를 통째로 숨겨 파츠만 봅니다. 파츠만으로 몸이 다 가려지는지 확인할 때 씁니다. 미리보기에서만 적용됩니다."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				bHideWholeMech = !bHideWholeMech;
				Viewport->SetHideWholeMech(bHideWholeMech);
			}),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([this]() { return bHideWholeMech ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	Menu.AddMenuEntry(FText::Format(LOCTEXT("MenuSplitRegions", "원본을 부위별로 나누기 ({0})"), FText::FromString(Settings->PreviewMechMesh.GetAssetName())),
		LOCTEXT("MenuSplitRegionsTooltip",
			"미리보기 메카 메시를 머리·몸통·팔 L/R·다리 L/R 메시로 나눠 원본 옆 BodyRegions 폴더에 저장합니다(다시 누르면 덮어씀).\n"
			"나눠 두면 파츠 Attachment의 Replace Regions에 고른 부위가 그 파츠를 끼는 동안 숨습니다(미리보기와 게임 모두)."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SMechPartsWorkbench::SplitBodyRegions),
			FCanExecuteAction::CreateLambda([Settings]() { return !Settings->PreviewMechMesh.IsNull(); })));
	Menu.EndSection();

	// 미리보기: 애니메이션이 있으면 끄기, 이 창에서 끈 적이 있으면 다시 켜기.
	FText AnimationLabel;
	bool bCanToggleAnimation = true;
	if (!Settings->PreviewAnimation.IsNull())
	{
		AnimationLabel = FText::Format(LOCTEXT("MenuAnimationOff", "미리보기 애니메이션 끄기 (기본 자세로, 지금: {0})"), FText::FromString(Settings->PreviewAnimation.GetAssetName()));
	}
	else if (!ClearedPreviewAnimation.IsNull())
	{
		AnimationLabel = FText::Format(LOCTEXT("MenuAnimationOn", "미리보기 애니메이션 다시 켜기 ({0})"), FText::FromString(ClearedPreviewAnimation.GetAssetName()));
	}
	else
	{
		AnimationLabel = LOCTEXT("MenuAnimationNone", "미리보기 애니메이션 끄기 (지금 없음)");
		bCanToggleAnimation = false;
	}
	Menu.BeginSection(NAME_None, LOCTEXT("MenuPreviewSection", "미리보기"));
	Menu.AddMenuEntry(AnimationLabel,
		LOCTEXT("MenuAnimationTooltip", "미리보기 설정의 Preview Animation을 비워 기본 자세로 봅니다. 위치를 맞추거나 굽기 전에 씁니다(굽기는 기본 자세 기준)."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SMechPartsWorkbench::TogglePreviewAnimation), FCanExecuteAction::CreateLambda([bCanToggleAnimation]() { return bCanToggleAnimation; })));
	Menu.EndSection();

	return Menu.MakeWidget();
}

TArray<UPackage*> SMechPartsWorkbench::GetDirtyPartPackages() const
{
	TArray<UPackage*> Packages;
	for (const TPair<FSoftObjectPath, TArray<FString>>& Entry : PartGroupValues)
	{
		// 불러온 적 없는 에셋은 바뀌었을 수 없으니 찾기만 한다(새로 불러오지 않음).
		if (const UObject* const Asset = Entry.Key.ResolveObject())
		{
			UPackage* const Package = Asset->GetPackage();
			if (Package && Package->IsDirty())
			{
				Packages.AddUnique(Package);
			}
		}
	}
	return Packages;
}

void SMechPartsWorkbench::SaveAllDirtyParts()
{
	const TArray<UPackage*> Packages = GetDirtyPartPackages();
	if (!Packages.IsEmpty())
	{
		FEditorFileUtils::PromptForCheckoutAndSave(Packages, /*bCheckDirty=*/true, /*bPromptToSave=*/false);
	}
}

bool SMechPartsWorkbench::CanRevertToStaticMesh() const
{
	const UObject* const Asset = PartAsset.Get();
	const FSoftObjectProperty* const SkeletalMeshProperty = Asset ? MechPartReflection::FindSkeletalMeshProperty(Asset) : nullptr;
	return SkeletalMeshProperty
		&& !SkeletalMeshProperty->GetPropertyValue_InContainer(Asset).IsNull()
		&& MechPartReflection::LoadPartStaticMesh(Asset);
}

void SMechPartsWorkbench::RevertToStaticMesh()
{
	UObject* const Asset = PartAsset.Get();
	FSoftObjectProperty* const SkeletalMeshProperty = Asset ? MechPartReflection::FindSkeletalMeshProperty(Asset) : nullptr;
	FStructProperty* const AttachmentProperty = Asset ? MechPartReflection::FindAttachmentProperty(Asset) : nullptr;
	if (!CanRevertToStaticMesh() || !AttachmentProperty)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("RevertToStaticTransaction", "스태틱 메시로 되돌리기"));
	Asset->Modify();

	SkeletalMeshProperty->SetPropertyValue_InContainer(Asset, FSoftObjectPtr());
	FPropertyChangedEvent MeshChanged(SkeletalMeshProperty);
	Asset->PostEditChangeProperty(MeshChanged);

	// 스태틱 메시는 소켓 방식으로만 붙는다 (LeaderPose로 두면 게임 쪽 데이터 검증이 경고한다).
	AttachmentProperty->ContainerPtrToValuePtr<FMechPartAttachment>(Asset)->AttachMode = EMechPartAttachMode::Socket;
	FPropertyChangedEvent AttachmentChanged(AttachmentProperty);
	Asset->PostEditChangeProperty(AttachmentChanged);

	// 스켈레탈 → 스태틱으로 컴포넌트 종류가 바뀌므로 미리보기 파츠를 새로 붙인다.
	Viewport->RecreatePart();
	PartDetails->ForceRefresh();
}

void SMechPartsWorkbench::CopyPlacement()
{
	UObject* const Asset = PartAsset.Get();
	if (const FMechPartAttachment* const Attachment = Asset ? MechPartReflection::FindAttachment(Asset) : nullptr)
	{
		CopiedPlacement = *Attachment;
		CopiedPlacementSource = Asset->GetName();
	}
}

void SMechPartsWorkbench::PastePlacement()
{
	UObject* const Asset = PartAsset.Get();
	FStructProperty* const AttachmentProperty = Asset ? MechPartReflection::FindAttachmentProperty(Asset) : nullptr;
	if (!AttachmentProperty || !CopiedPlacement.IsSet())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("PastePlacementTransaction", "배치 붙여넣기"));
	Asset->Modify();
	FMechPartAttachment& Attachment = *AttachmentProperty->ContainerPtrToValuePtr<FMechPartAttachment>(Asset);
	Attachment.AttachSocket = CopiedPlacement->AttachSocket;
	Attachment.RelativeLocation = CopiedPlacement->RelativeLocation;
	Attachment.RelativeRotation = CopiedPlacement->RelativeRotation;
	Attachment.RelativeScale = CopiedPlacement->RelativeScale;
	FPropertyChangedEvent Changed(AttachmentProperty);
	Asset->PostEditChangeProperty(Changed);
	PartDetails->ForceRefresh();
}

void SMechPartsWorkbench::TogglePreviewAnimation()
{
	UMechPartsWorkbenchSettings* const Settings = GetMutableDefault<UMechPartsWorkbenchSettings>();
	if (!Settings->PreviewAnimation.IsNull())
	{
		ClearedPreviewAnimation = Settings->PreviewAnimation;
		Settings->PreviewAnimation.Reset();
	}
	else if (!ClearedPreviewAnimation.IsNull())
	{
		Settings->PreviewAnimation = ClearedPreviewAnimation;
		ClearedPreviewAnimation.Reset();
	}
	else
	{
		return;
	}

	// 설정 창에서 바꾼 것과 같은 알림을 보내 개인 설정 파일 저장 + 메카 다시 그리기(HandleSettingsChanged)가 일어나게 한다.
	FProperty* const Property = FindFProperty<FProperty>(UMechPartsWorkbenchSettings::StaticClass(), GET_MEMBER_NAME_CHECKED(UMechPartsWorkbenchSettings, PreviewAnimation));
	FPropertyChangedEvent Changed(Property, EPropertyChangeType::ValueSet);
	Settings->PostEditChangeProperty(Changed);
	SettingsDetails->ForceRefresh();
}

FString SMechPartsWorkbench::GetSlotKey(const FSoftObjectPath& PartPath) const
{
	// 분류 줄 중 속성 줄(지금은 Slot Tag)의 값. 부위가 비어 있으면 다른 파츠와 겹치지 않게 에셋 경로를 쓴다.
	const TArray<FString>* const Values = PartGroupValues.Find(PartPath);
	const int32 SlotRow = FilterRows.IndexOfByPredicate([](const FFilterRow& Row) { return Row.Property != nullptr; });
	if (Values && Values->IsValidIndex(SlotRow) && !(*Values)[SlotRow].IsEmpty())
	{
		return (*Values)[SlotRow];
	}
	return PartPath.ToString();
}

FString SMechPartsWorkbench::GetFolderKey(const FSoftObjectPath& PartPath) const
{
	// 폴더(세트) 줄은 항상 마지막 분류 줄이다(RebuildFilters).
	const TArray<FString>* const Values = PartGroupValues.Find(PartPath);
	const int32 FolderRow = FilterRows.Num() - 1;
	return Values && Values->IsValidIndex(FolderRow) ? (*Values)[FolderRow] : FString();
}

void SMechPartsWorkbench::AddToLoadout(const FSoftObjectPath& PartPath)
{
	if (!PartPath.IsValid())
	{
		return;
	}
	const FString SlotKey = GetSlotKey(PartPath);
	LoadoutParts.RemoveAll([this, &SlotKey](const FSoftObjectPath& Existing) { return GetSlotKey(Existing) == SlotKey; });
	LoadoutParts.Add(PartPath);
	PushLoadoutToViewport();
}

void SMechPartsWorkbench::RemoveFromLoadout(const FSoftObjectPath& PartPath)
{
	LoadoutParts.Remove(PartPath);
	PushLoadoutToViewport();
}

bool SMechPartsWorkbench::IsInLoadout(const FSoftObjectPath& PartPath) const
{
	return PartPath.IsValid() && LoadoutParts.Contains(PartPath);
}

void SMechPartsWorkbench::AddFolderToLoadout(const FString& FolderKey)
{
	// 부위마다 이름순 첫 번째 파츠(A/B가 있으면 A).
	TMap<FString, FSoftObjectPath> FirstBySlot;
	for (const TPair<FSoftObjectPath, TArray<FString>>& Entry : PartGroupValues)
	{
		if (GetFolderKey(Entry.Key) != FolderKey)
		{
			continue;
		}
		const FString SlotKey = GetSlotKey(Entry.Key);
		const FSoftObjectPath* const First = FirstBySlot.Find(SlotKey);
		if (!First || Entry.Key.GetAssetName() < First->GetAssetName())
		{
			FirstBySlot.Add(SlotKey, Entry.Key);
		}
	}
	for (const TPair<FString, FSoftObjectPath>& Pick : FirstBySlot)
	{
		LoadoutParts.RemoveAll([this, &Pick](const FSoftObjectPath& Existing) { return GetSlotKey(Existing) == Pick.Key; });
		LoadoutParts.Add(Pick.Value);
	}
	PushLoadoutToViewport();
}

void SMechPartsWorkbench::ClearLoadout()
{
	LoadoutParts.Reset();
	PushLoadoutToViewport();
}

void SMechPartsWorkbench::PushLoadoutToViewport()
{
	// 고른 파츠는 따로 보이므로(편집용), 같은 부위의 착용 파츠는 뺀다.
	const FString SelectedSlot = PartAsset.IsValid() ? GetSlotKey(FSoftObjectPath(PartAsset.Get())) : FString();
	TArray<UObject*> Parts;
	for (const FSoftObjectPath& Path : LoadoutParts)
	{
		if (PartAsset.IsValid() && GetSlotKey(Path) == SelectedSlot)
		{
			continue;
		}
		if (UObject* const Asset = Path.TryLoad())
		{
			Parts.Add(Asset);
		}
	}
	Viewport->SetLoadout(Parts);
}

FText SMechPartsWorkbench::GetLoadoutSummary() const
{
	const FString Names = FString::JoinBy(LoadoutParts, TEXT(", "), [](const FSoftObjectPath& Path) { return Path.GetAssetName(); });
	const FText Hidden = bHideWholeMech ? LOCTEXT("LoadoutHiddenMech", "  [원본 전부 숨김]") : FText::GetEmpty();
	return LoadoutParts.IsEmpty()
		? FText::Format(LOCTEXT("LoadoutSummaryEmpty", "착용 미리보기 없음{0}"), Hidden)
		: FText::Format(LOCTEXT("LoadoutSummary", "착용 미리보기 {0}개: {1}{2}"), LoadoutParts.Num(), FText::FromString(Names), Hidden);
}

bool SMechPartsWorkbench::IsReplaceRegionsUnavailable() const
{
	if (!Viewport.IsValid() || Viewport->HasBodyRegions())
	{
		return false;
	}
	auto HasReplaceRegions = [](UObject* Asset)
	{
		const FMechPartAttachment* const Attachment = Asset ? MechPartReflection::FindAttachment(Asset) : nullptr;
		return Attachment && Attachment->ReplaceRegions != 0;
	};
	if (HasReplaceRegions(PartAsset.Get()))
	{
		return true;
	}
	for (const FSoftObjectPath& Path : LoadoutParts)
	{
		if (HasReplaceRegions(Path.ResolveObject()))
		{
			return true;
		}
	}
	return false;
}

void SMechPartsWorkbench::SplitBodyRegions()
{
	const UMechPartsWorkbenchSettings* const Settings = GetDefault<UMechPartsWorkbenchSettings>();
	USkeletalMesh* const MechMesh = Settings->PreviewMechMesh.LoadSynchronous();
	if (!MechMesh)
	{
		return;
	}
	auto JoinBones = [](const TArray<FName>& Bones)
	{
		return FText::FromString(Bones.IsEmpty() ? FString(TEXT("-")) : FString::JoinBy(Bones, TEXT(", "), [](const FName& Bone) { return Bone.ToString(); }));
	};
	const FText Confirm = FText::Format(LOCTEXT("SplitConfirm",
		"'{0}'을(를) 머리·몸통·팔 L/R·다리 L/R 스켈레탈 메시로 나눕니다.\n\n"
		"- 부위 시작 뼈 (미리보기 설정의 Body Regions, 어디에도 없는 뼈는 몸통):\n"
		"   머리 {1} / 왼팔 {2} / 오른팔 {3} / 왼다리 {4} / 오른다리 {5}\n"
		"- 자르기 비율 팔 {7} / 다리 {8}: 시작 뼈 자체에 실린 부분은 이 비율보다 몸 쪽이면 몸통에 남깁니다.\n"
		"- 머리 끌어오기 반지름 {9}cm, 시작 높이 {10}cm: 몸통 중 목 관절보다 위이고 머리 축 가까운 부분은 머리로 옮깁니다.\n"
		"- 저장 위치: {6} (이미 있으면 덮어씁니다). 원본 메시는 바꾸지 않습니다.\n"
		"- 게임에 쓰려면 나이트 파츠 컴포넌트의 Body Regions에 만든 세트(DA_{0}_BodyRegions)를 넣어야 합니다.\n\n"
		"계속할까요?"),
		FText::FromString(MechMesh->GetName()), JoinBones(Settings->RegionHeadRootBones), JoinBones(Settings->RegionArmLeftRootBones),
		JoinBones(Settings->RegionArmRightRootBones), JoinBones(Settings->RegionLegLeftRootBones), JoinBones(Settings->RegionLegRightRootBones),
		FText::FromString(MechBodyRegionSplitter::GetOutputFolder(MechMesh)), FText::AsNumber(Settings->RegionArmSplitRatio),
		FText::AsNumber(Settings->RegionLegSplitRatio), FText::AsNumber(Settings->RegionHeadCaptureRadius), FText::AsNumber(Settings->RegionHeadCaptureOffset));
	if (FMessageDialog::Open(EAppMsgType::OkCancel, Confirm) != EAppReturnType::Ok)
	{
		return;
	}

	MechBodyRegionSplitter::FRootBones Roots;
	Roots.Head = Settings->RegionHeadRootBones;
	Roots.ArmLeft = Settings->RegionArmLeftRootBones;
	Roots.ArmRight = Settings->RegionArmRightRootBones;
	Roots.LegLeft = Settings->RegionLegLeftRootBones;
	Roots.LegRight = Settings->RegionLegRightRootBones;
	Roots.ArmSplitRatio = Settings->RegionArmSplitRatio;
	Roots.LegSplitRatio = Settings->RegionLegSplitRatio;
	Roots.HeadCaptureRadius = Settings->RegionHeadCaptureRadius;
	Roots.HeadCaptureOffset = Settings->RegionHeadCaptureOffset;
	FText Error;
	const UMechBodyRegionSet* const RegionSet = MechBodyRegionSplitter::Split(MechMesh, Roots, Error);
	if (!RegionSet)
	{
		FMessageDialog::Open(EAppMsgType::Ok, Error);
		return;
	}
	Viewport->RefreshBodyRegions();

	FNotificationInfo Info(FText::Format(LOCTEXT("SplitDone", "원본을 부위 메시 {0}개로 나눴습니다: {1}"), RegionSet->Regions.Num(), FText::FromString(RegionSet->GetName())));
	Info.ExpireDuration = 5.f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

#undef LOCTEXT_NAMESPACE
