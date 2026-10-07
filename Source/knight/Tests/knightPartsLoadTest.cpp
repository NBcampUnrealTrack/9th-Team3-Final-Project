// 파츠 적재 한도 계산(knightPartsLoad::FitsLoadCapacity) 자동화 테스트
//
// 실행 방법: 에디터 출력 로그의 명령 입력칸(Cmd)에
//   Automation RunTests Knight.Parts.LoadCapacity
// 통과한 항목은 로그에 남지 않고, 틀린 항목만 이름과 기대값이 Error로 찍힌다.
//
// 규칙: 지금 중량 합 - 같은 슬롯의 기존 파츠 중량 + 새 파츠 중량 <= 적재 한도 이면 장착할 수 있다.

#include "Misc/AutomationTest.h"
#include "Item/knightPartsComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FknightPartsLoadTest, "Knight.Parts.LoadCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FknightPartsLoadTest::RunTest(const FString& Parameters)
{
	using knightPartsLoad::FitsLoadCapacity;

	// ---- 빈 슬롯에 장착 ----
	TestTrue(TEXT("중량 0, 한도 100에 30짜리 장착: 가능"), FitsLoadCapacity(0.f, 0.f, 30.f, 100.f));
	TestTrue(TEXT("중량 70, 한도 100에 30짜리 장착: 딱 한도라 가능"), FitsLoadCapacity(70.f, 0.f, 30.f, 100.f));
	TestFalse(TEXT("중량 80, 한도 100에 30짜리 장착: 110이라 불가"), FitsLoadCapacity(80.f, 0.f, 30.f, 100.f));

	// ---- 같은 슬롯 교체: 기존 파츠 중량을 빼고 계산 ----
	TestTrue(TEXT("중량 90(그중 몸통 20), 몸통을 25짜리로 교체: 95라 가능"), FitsLoadCapacity(90.f, 20.f, 25.f, 100.f));
	TestFalse(TEXT("중량 90(그중 몸통 20), 몸통을 40짜리로 교체: 110이라 불가"), FitsLoadCapacity(90.f, 20.f, 40.f, 100.f));
	TestTrue(TEXT("이미 한도를 넘은 상태(120)에서 더 가벼운 파츠로 교체(30 → 10): 100이라 가능"), FitsLoadCapacity(120.f, 30.f, 10.f, 100.f));

	// ---- 그 밖의 경우 ----
	TestTrue(TEXT("중량 0짜리 파츠는 한도 0이어도 가능"), FitsLoadCapacity(0.f, 0.f, 0.f, 0.f));
	TestTrue(TEXT("부동소수 오차: 0.9 + 0.1 = 1.0, 한도 1.0에 가능"), FitsLoadCapacity(0.9f, 0.f, 0.1f, 1.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
