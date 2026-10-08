#include "knightUIEditorLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "WidgetBlueprint.h"

namespace
{
	template<typename WidgetT>
	WidgetT* AddVariableWidget(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<WidgetT> WidgetClass, const FName Name)
	{
		WidgetT* Widget = WidgetBlueprint->WidgetTree->ConstructWidget<WidgetT>(WidgetClass, Name);
		// C++ 부모의 BindWidgetOptional이 이름으로 찾으려면 변수로 표시되어야 한다.
		Widget->bIsVariable = true;
		WidgetBlueprint->OnVariableAdded(Name);
		return Widget;
	}
}

bool UknightUIEditorLibrary::BuildKnightButtonLayout(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> BracketClass, TSubclassOf<UCommonTextStyle> LabelStyle)
{
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree || !BracketClass)
	{
		return false;
	}
	UWidgetTree* const Tree = WidgetBlueprint->WidgetTree;
	if (Tree->RootWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("BuildKnightButtonLayout: %s already has a layout; left unchanged."), *WidgetBlueprint->GetName());
		return false;
	}

	WidgetBlueprint->Modify();
	Tree->Modify();

	UOverlay* const Root = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_Root"));
	Tree->RootWidget = Root;
	// 위젯 블루프린트는 변수가 아닌 위젯도 모두 GUID를 가져야 한다. 빠지면 컴파일러가 ensure를 낸다.
	WidgetBlueprint->OnVariableAdded(Root->GetFName());

	UCommonTextBlock* const Label = AddVariableWidget<UCommonTextBlock>(WidgetBlueprint, UCommonTextBlock::StaticClass(), TEXT("Txt_Label"));
	Label->SetText(NSLOCTEXT("knightUI", "ButtonPlaceholder", "Button"));
	if (LabelStyle)
	{
		Label->SetStyle(LabelStyle);
	}
	UOverlaySlot* const LabelSlot = Root->AddChildToOverlay(Label);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	// 버튼 스타일의 좌우 여백(8)과 합쳐 글자가 테두리에서 30px 떨어진다. 브래킷은 여백 8 안쪽 끝에 붙는다.
	LabelSlot->SetPadding(FMargin(22.f, 0.f));

	for (const TPair<FName, EHorizontalAlignment>& Side : { TPair<FName, EHorizontalAlignment>(TEXT("Bracket_L"), HAlign_Left), TPair<FName, EHorizontalAlignment>(TEXT("Bracket_R"), HAlign_Right) })
	{
		UUserWidget* const Bracket = AddVariableWidget<UUserWidget>(WidgetBlueprint, BracketClass, Side.Key);
		UOverlaySlot* const BracketSlot = Root->AddChildToOverlay(Bracket);
		BracketSlot->SetHorizontalAlignment(Side.Value);
		BracketSlot->SetVerticalAlignment(VAlign_Center);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	return true;
}
