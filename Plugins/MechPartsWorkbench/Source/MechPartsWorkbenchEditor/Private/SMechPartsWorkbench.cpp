#include "SMechPartsWorkbench.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "ContentBrowserModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "FileHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IContentBrowserSingleton.h"
#include "IDetailsView.h"
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

#undef LOCTEXT_NAMESPACE
