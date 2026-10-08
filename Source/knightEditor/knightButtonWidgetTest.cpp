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
#include "Editor.h"
#include "Widgets/knightButtonBase.h"

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

	Button->RemoveFromParent();
	return true;
}

#endif
