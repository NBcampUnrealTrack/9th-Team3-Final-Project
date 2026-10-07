// 그로기 자연 회복(knightPoiseRecovery::Step) 자동화 테스트
//
// 실행 방법: 에디터 출력 로그의 명령 입력칸(Cmd)에
//   Automation RunTests Knight.Combat.PoiseRecovery
// 또는 Tools > Session Frontend > Automation 탭에서 검색해 체크하고 Start Tests.
//
// 통과한 항목은 로그에 남지 않고, 틀린 항목만 이름과 기대값이 Error로 찍힌다.

#include "Misc/AutomationTest.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FknightPoiseRecoveryTest, "Knight.Combat.PoiseRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FknightPoiseRecoveryTest::RunTest(const FString& Parameters)
{
	using knightPoiseRecovery::Step;
	constexpr float Tolerance = 0.001f;

	// 공통 조건: 최대 100, 초당 10 회복, 0.1초 간격, 대기 시간 0.25초
	constexpr float Max = 100.f;
	constexpr float Rate = 10.f;
	constexpr float Dt = 0.1f;
	constexpr float Delay = 0.25f;

	// ---- 회복하는 경우 ----
	TestEqual(TEXT("대기 시간이 지나면 초당 10 x 0.1초 = +1"), Step(50.f, Max, Rate, Dt, 1.f, Delay), 51.f, Tolerance);
	TestEqual(TEXT("대기 시간과 경과 시간이 같으면 회복 시작"), Step(50.f, Max, Rate, Dt, 0.25f, Delay), 51.f, Tolerance);
	TestEqual(TEXT("최대치를 넘지 않음"), Step(99.5f, Max, Rate, Dt, 1.f, Delay), 100.f, Tolerance);
	TestEqual(TEXT("대기 시간 0이면 바로 회복"), Step(50.f, Max, Rate, Dt, 0.f, 0.f), 51.f, Tolerance);
	TestEqual(TEXT("아직 맞은 적 없음(아주 먼 과거)이면 회복"), Step(50.f, Max, Rate, Dt, UE_BIG_NUMBER, Delay), 51.f, Tolerance);

	// ---- 회복하지 않는 경우 ----
	TestEqual(TEXT("마지막 피격 후 0.1초(대기 중)면 그대로"), Step(50.f, Max, Rate, Dt, 0.1f, Delay), 50.f, Tolerance);
	TestEqual(TEXT("회복 속도 0(기본값)이면 그대로"), Step(50.f, Max, 0.f, Dt, 1.f, Delay), 50.f, Tolerance);
	TestEqual(TEXT("회복 속도 음수면 그대로"), Step(50.f, Max, -5.f, Dt, 1.f, Delay), 50.f, Tolerance);
	TestEqual(TEXT("Poise 0(경직·다운 중)이면 그대로"), Step(0.f, Max, Rate, Dt, 1.f, Delay), 0.f, Tolerance);
	TestEqual(TEXT("이미 최대치면 그대로"), Step(100.f, Max, Rate, Dt, 1.f, Delay), 100.f, Tolerance);
	TestEqual(TEXT("경과 시간이 음수(같은 프레임)여도 그대로"), Step(50.f, Max, Rate, -Dt, 1.f, Delay), 50.f, Tolerance);

	// ---- 연속 회복: 0.1초 간격으로 1초(10회) 지나면 +10 ----
	{
		float Poise = 30.f;
		for (int32 i = 0; i < 10; ++i)
		{
			Poise = Step(Poise, Max, Rate, Dt, 1.f + i * Dt, Delay);
		}
		TestEqual(TEXT("1초 동안 10회 회복하면 30 -> 40"), Poise, 40.f, Tolerance);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
