// WBP_KnightButton 자동화 테스트
//
// 실행 방법: 콘솔에서 Automation RunTests Knight.UI.Button
//
// 레이아웃 위젯이 C++ 부모(UknightButtonBase)에 이름으로 연결되는지와 Locked 처리를 확인한다.
// WBP_KnightButton의 위젯 이름(Txt_Label, Bracket_L, Bracket_R)을 바꾸면 이 테스트가 먼저 깨진다.

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Editor.h"
#include "Widgets/knightButtonBase.h"
#include "Menu/UI/KnightMaintenancePresentation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FknightButtonWidgetTest, "Knight.UI.Button",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FknightButtonWidgetTest::RunTest(const FString& Parameters)
{
	UClass* const ButtonClass = LoadClass<UknightButtonBase>(nullptr, TEXT("/Game/Knight/UI/Common/WBP_KnightButton.WBP_KnightButton_C"));
	if (!TestNotNull(TEXT("WBP_KnightButton 클래스 로드"), ButtonClass))
	{
		return false;
	}
	UWorld* const World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("에디터 월드"), World))
	{
		return false;
	}

	UknightButtonBase* const Button = CreateWidget<UknightButtonBase>(World, ButtonClass);
	if (!TestNotNull(TEXT("버튼 생성"), Button))
	{
		return false;
	}

	UCommonTextBlock* const Label = Cast<UCommonTextBlock>(Button->WidgetTree->FindWidget(TEXT("Txt_Label")));
	TestNotNull(TEXT("Txt_Label 존재"), Label);
	TestNotNull(TEXT("Bracket_L 존재"), Button->WidgetTree->FindWidget(TEXT("Bracket_L")));
	TestNotNull(TEXT("Bracket_R 존재"), Button->WidgetTree->FindWidget(TEXT("Bracket_R")));

	const FText Text = INVTEXT("Raid Start");
	Button->SetButtonText(Text);
	if (Label)
	{
		TestTrue(TEXT("SetButtonText가 Txt_Label에 반영 (BindWidget 연결)"), Label->GetText().EqualTo(Text));
	}

	Button->SetLockedWithReason(true, INVTEXT("Clear stage 1 first"));
	TestTrue(TEXT("Locked 상태"), Button->GetLocked());
	TestTrue(TEXT("Locked면 반투명"), Button->GetRenderOpacity() < 1.f);
	TestTrue(TEXT("Locked면 사유가 툴팁"), Button->GetToolTipText().ToString() == TEXT("Clear stage 1 first"));

	Button->SetLockedWithReason(false, FText::GetEmpty());
	TestFalse(TEXT("Unlock"), Button->GetLocked());
	TestEqual(TEXT("Unlock하면 불투명"), Button->GetRenderOpacity(), 1.f);

	// Exercise CommonUI's actual click handler (including its automatic selection), not OnClicked.Broadcast alone.
	Button->TakeWidget();
	Button->ConfigureMaintenanceButton(false);
	if (UUserWidget* FocusBracket = Cast<UUserWidget>(Button->WidgetTree->FindWidget(TEXT("Bracket_L"))))
	{
		UImage* Bar = FocusBracket->WidgetTree ? Cast<UImage>(FocusBracket->WidgetTree->FindWidget(TEXT("Img_Bar"))) : nullptr;
		TestNotNull(TEXT("Nested focus bracket image exists"), Bar);
		if (Bar) TestTrue(TEXT("Maintenance focus uses the translucent orange outline palette"),
			Bar->GetColorAndOpacity().Equals(KnightMaintenancePresentation::Focus)
			&& Bar->GetColorAndOpacity().A < 1.f);
	}
	int32 ActionCalls = 0;
	const FDelegateHandle ActionHandle = Button->OnClicked().AddLambda([&ActionCalls]() { ++ActionCalls; });
	auto Click = [](UCommonButtonBase* Target)
	{
		if (UFunction* Function = Target->FindFunction(TEXT("HandleButtonClicked"))) Target->ProcessEvent(Function, nullptr);
	};
	Click(Button); Click(Button);
	TestEqual(TEXT("Action receives both CommonUI clicks"), ActionCalls, 2);
	TestFalse(TEXT("Navigation/action clicks never latch selection"), Button->GetSelected());
	Button->SetIsEnabled(false);
	Click(Button);
	TestEqual(TEXT("Disabled action does not execute"), ActionCalls, 2);
	Button->SetIsEnabled(true);
	Button->OnClicked().Remove(ActionHandle);

	UknightButtonBase* Other = CreateWidget<UknightButtonBase>(World, ButtonClass);
	Other->TakeWidget();
	Button->ConfigureMaintenanceButton(true);
	Other->ConfigureMaintenanceButton(true);
	UCommonButtonBase* Selected = nullptr;
	auto Select = [&Selected, Button, Other](UCommonButtonBase* Candidate)
	{
		Selected = Selected == Candidate ? nullptr : Candidate;
		for (UCommonButtonBase* Entry : { Button, Other })
		{
			if (Entry == Selected) Entry->SetIsSelected(true, false);
			else Entry->ClearSelection();
		}
	};
	const FDelegateHandle FirstHandle = Button->OnClicked().AddLambda([&Select, Button]() { Select(Button); });
	const FDelegateHandle SecondHandle = Other->OnClicked().AddLambda([&Select, Other]() { Select(Other); });
	Click(Button);
	TestTrue(TEXT("Owner selects first row"), Button->GetSelected());
	Click(Other);
	TestTrue(TEXT("Changing rows clears previous selection"), !Button->GetSelected() && Other->GetSelected());
	Click(Other);
	TestTrue(TEXT("Owner deselection is not undone after click"), !Button->GetSelected() && !Other->GetSelected());
	Button->SetLockedWithReason(true, INVTEXT("Locked fixture"));
	Click(Button);
	TestFalse(TEXT("Locked clicks cannot latch selection"), Button->GetSelected());
	Button->SetLockedWithReason(false, FText::GetEmpty());
	if (UFunction* Focus = Button->FindFunction(TEXT("HandleFocusReceived"))) Button->ProcessEvent(Focus, nullptr);
	TestFalse(TEXT("Keyboard focus does not select a row"), Button->GetSelected());
	if (UFunction* Blur = Button->FindFunction(TEXT("HandleFocusLost"))) Button->ProcessEvent(Blur, nullptr);
	Button->OnClicked().Remove(FirstHandle);
	Other->OnClicked().Remove(SecondHandle);
	Other->RemoveFromParent();

	Button->RemoveFromParent();
	return true;
}

#endif
