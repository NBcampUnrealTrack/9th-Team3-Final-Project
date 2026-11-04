// 행동 에너지(스테미너) 자연 회복(knightBoostRecovery::Step) 자동화 테스트
//
// 실행 방법: 에디터 출력 로그의 명령 입력칸(Cmd)에
//   Automation RunTests Knight.Combat.BoostRecovery
// 또는 Tools > Session Frontend > Automation 탭에서 검색해 체크하고 Start Tests.
//
// 통과한 항목은 로그에 남지 않고, 틀린 항목만 이름과 기대값이 Error로 찍힌다.

#include "Misc/AutomationTest.h"
#include "AbilitySystem/knightAbilitySystemComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FknightBoostRecoveryTest, "Knight.Combat.BoostRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FknightBoostRecoveryTest::RunTest(const FString& Parameters)
{
	using knightBoostRecovery::Step;
	constexpr float Tolerance = 0.001f;

	// 공통 조건: 최대 100, 초당 25 회복(기본값), 0.1초 간격, 사용 후 대기 1.5초
	// (기본값은 1.2초지만, 계산 규칙 검사라 대기 시간은 그대로 1.5초로 둔다)
	constexpr float Max = 100.f;
	constexpr float Rate = 25.f;
	constexpr float Dt = 0.1f;
	constexpr float Delay = 1.5f;

	// ---- 회복하는 경우 ----
	TestEqual(TEXT("대기 시간이 지나면 초당 25 x 0.1초 = +2.5"), Step(50.f, Max, Rate, Dt, 2.f, Delay), 52.5f, Tolerance);
	TestEqual(TEXT("대기 시간과 경과 시간이 같으면 회복 시작"), Step(50.f, Max, Rate, Dt, 1.5f, Delay), 52.5f, Tolerance);
	TestEqual(TEXT("0에서도 회복(그로기와 다름)"), Step(0.f, Max, Rate, Dt, 2.f, Delay), 2.5f, Tolerance);
	TestEqual(TEXT("최대치를 넘지 않음"), Step(99.f, Max, Rate, Dt, 2.f, Delay), 100.f, Tolerance);
	TestEqual(TEXT("가불로 음수여도 같은 속도로 회복"), Step(-20.f, Max, Rate, Dt, 2.f, Delay), -17.5f, Tolerance);
	TestEqual(TEXT("음수에서 0을 지나 그대로 참"), Step(-1.f, Max, Rate, Dt, 2.f, Delay), 1.5f, Tolerance);
	TestEqual(TEXT("가불 중에도 쓴 직후(대기 중)면 그대로"), Step(-20.f, Max, Rate, Dt, 1.f, Delay), -20.f, Tolerance);
	TestEqual(TEXT("대기 시간 0이면 바로 회복"), Step(50.f, Max, Rate, Dt, 0.f, 0.f), 52.5f, Tolerance);
	TestEqual(TEXT("아직 쓴 적 없음(아주 먼 과거)이면 회복"), Step(50.f, Max, Rate, Dt, UE_BIG_NUMBER, Delay), 52.5f, Tolerance);

	// ---- 회복하지 않는 경우 ----
	TestEqual(TEXT("쓴 뒤 1초(대기 중)면 그대로"), Step(50.f, Max, Rate, Dt, 1.f, Delay), 50.f, Tolerance);
	TestEqual(TEXT("회복 속도 0이면 그대로"), Step(50.f, Max, 0.f, Dt, 2.f, Delay), 50.f, Tolerance);
	TestEqual(TEXT("이미 최대치면 그대로"), Step(100.f, Max, Rate, Dt, 2.f, Delay), 100.f, Tolerance);

	// ---- 달리는 중 멈춤 ----
	TestEqual(TEXT("달리는 중이면 대기 시간이 지났어도 그대로"), Step(50.f, Max, Rate, Dt, 2.f, Delay, true), 50.f, Tolerance);
	TestEqual(TEXT("멈췄다 풀리면 대기 시간을 다시 세지 않고 바로 회복"), Step(50.f, Max, Rate, Dt, 2.f, Delay, false), 52.5f, Tolerance);

	// ---- 연속 회복: 대기가 끝난 뒤 0.1초 간격으로 1초(10회) 지나면 +25 ----
	{
		float Gauge = 30.f;
		for (int32 i = 0; i < 10; ++i)
		{
			Gauge = Step(Gauge, Max, Rate, Dt, Delay + i * Dt, Delay);
		}
		TestEqual(TEXT("대기 후 1초 동안 10회 회복하면 30 -> 55"), Gauge, 55.f, Tolerance);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
