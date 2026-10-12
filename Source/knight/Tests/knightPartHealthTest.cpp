// 부위 체력 피해 분배(knightPartHealth::DistributeDamage)·수리 분배(DistributeRepair) 자동화 테스트
//
// 실행 방법: 에디터 출력 로그의 명령 입력칸(Cmd)에
//   Automation RunTests Knight.Combat.PartHealth
// 통과한 항목은 로그에 남지 않고, 틀린 항목만 이름과 기대값이 Error로 찍힌다.
//
// 부위 순서: 0 머리, 1 몸통, 2 팔, 3 다리

#include "Misc/AutomationTest.h"
#include "Character/knightPartHealthComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FknightPartHealthTest, "Knight.Combat.PartHealth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FknightPartHealthTest::RunTest(const FString& Parameters)
{
	using knightPartHealth::DistributeDamage;
	constexpr float Tolerance = 0.001f;

	// ---- 부위를 아는 피해: 그 부위에서만 뺀다 ----
	{
		float H[4] = { 25.f, 25.f, 25.f, 25.f };
		DistributeDamage(H, 10.f, 2);
		TestEqual(TEXT("팔 피격 10: 팔 25 -> 15"), H[2], 15.f, Tolerance);
		TestEqual(TEXT("팔 피격 10: 머리는 그대로"), H[0], 25.f, Tolerance);
		TestEqual(TEXT("팔 피격 10: 다리는 그대로"), H[3], 25.f, Tolerance);
	}
	{
		float H[4] = { 25.f, 25.f, 5.f, 25.f };
		DistributeDamage(H, 10.f, 2);
		TestEqual(TEXT("팔 체력 5에 피격 10: 0 아래로 내려가지 않음"), H[2], 0.f, Tolerance);
	}
	{
		float H[4] = { 25.f, 25.f, 0.f, 25.f };
		DistributeDamage(H, 10.f, 2);
		TestEqual(TEXT("이미 파괴된 팔 재피격: 팔은 0 그대로"), H[2], 0.f, Tolerance);
		TestEqual(TEXT("이미 파괴된 팔 재피격: 다른 부위로 넘기지 않음"), H[1], 25.f, Tolerance);
	}

	// ---- 부위를 모르는 피해(-1): 파괴되지 않은 부위에 나눈다 ----
	{
		float H[4] = { 25.f, 25.f, 25.f, 25.f };
		DistributeDamage(H, 40.f, -1);
		TestEqual(TEXT("범위 피해 40: 네 부위 각각 -10 (머리)"), H[0], 15.f, Tolerance);
		TestEqual(TEXT("범위 피해 40: 네 부위 각각 -10 (다리)"), H[3], 15.f, Tolerance);
	}
	{
		float H[4] = { 0.f, 25.f, 25.f, 0.f };
		DistributeDamage(H, 20.f, -1);
		TestEqual(TEXT("머리·다리 파괴 상태 범위 피해 20: 몸통 -10"), H[1], 15.f, Tolerance);
		TestEqual(TEXT("머리·다리 파괴 상태 범위 피해 20: 팔 -10"), H[2], 15.f, Tolerance);
		TestEqual(TEXT("머리·다리 파괴 상태 범위 피해 20: 머리는 0 그대로"), H[0], 0.f, Tolerance);
	}
	{
		float H[4] = { 0.f, 0.f, 0.f, 0.f };
		DistributeDamage(H, 20.f, -1);
		TestEqual(TEXT("모든 부위 파괴 상태 범위 피해: 변화 없음"), H[1], 0.f, Tolerance);
	}

	// ---- 그 밖의 경우 ----
	{
		float H[4] = { 25.f, 25.f, 25.f, 25.f };
		DistributeDamage(H, 0.f, 1);
		TestEqual(TEXT("피해 0이면 변화 없음"), H[1], 25.f, Tolerance);
		DistributeDamage(H, -5.f, 1);
		TestEqual(TEXT("피해 음수면 변화 없음"), H[1], 25.f, Tolerance);
		DistributeDamage(H, 10.f, 7);
		TestEqual(TEXT("잘못된 부위 번호(7)는 부위를 모르는 피해로 처리: 몸통 -2.5"), H[1], 22.5f, Tolerance);
	}

	// ---- 수리(DistributeRepair): 체력 비율이 낮은 부위부터 최대치까지, 남은 양은 다음 부위로 (2026-10-09) ----
	using knightPartHealth::DistributeRepair;
	{
		const float Max[4] = { 25.f, 25.f, 25.f, 25.f };
		float H[4] = { 10.f, 25.f, 0.f, 20.f };
		DistributeRepair(H, Max, 30.f);
		TestEqual(TEXT("수리 30: 가장 낮은 팔(0%) 0 -> 25"), H[2], 25.f, Tolerance);
		TestEqual(TEXT("수리 30: 남은 5는 다음으로 낮은 머리(40%) 10 -> 15"), H[0], 15.f, Tolerance);
		TestEqual(TEXT("수리 30: 그다음 다리(80%)는 그대로"), H[3], 20.f, Tolerance);
		TestEqual(TEXT("수리 30: 가득 찬 몸통은 그대로"), H[1], 25.f, Tolerance);
	}
	{
		// 최대치가 부위마다 달라도 '비율'로 순서를 정한다. 비율이 같으면 머리 → 몸통 → 팔 순서.
		const float Max[4] = { 20.f, 60.f, 40.f, 20.f };
		float H[4] = { 10.f, 30.f, 20.f, 20.f };
		DistributeRepair(H, Max, 15.f);
		TestEqual(TEXT("비율 동률(50%): 머리 먼저 10 -> 20"), H[0], 20.f, Tolerance);
		TestEqual(TEXT("비율 동률(50%): 남은 5는 몸통 30 -> 35"), H[1], 35.f, Tolerance);
		TestEqual(TEXT("비율 동률(50%): 팔은 차례가 오기 전에 수리량 소진"), H[2], 20.f, Tolerance);
	}
	{
		const float Max[4] = { 25.f, 25.f, 25.f, 25.f };
		float H[4] = { 25.f, 25.f, 5.f, 25.f };
		DistributeRepair(H, Max, 30.f);
		TestEqual(TEXT("수리량이 남아도 최대치를 넘지 않음 (팔 5 -> 25)"), H[2], 25.f, Tolerance);
		TestEqual(TEXT("모든 부위가 차면 남은 양은 버림 (머리 25 그대로)"), H[0], 25.f, Tolerance);
	}
	{
		const float Max[4] = { 25.f, 25.f, 25.f, 25.f };
		float H[4] = { 10.f, 10.f, 10.f, 10.f };
		DistributeRepair(H, Max, 0.f);
		TestEqual(TEXT("수리량 0이면 변화 없음"), H[0], 10.f, Tolerance);
		DistributeRepair(H, Max, -5.f);
		TestEqual(TEXT("수리량 음수면 변화 없음"), H[0], 10.f, Tolerance);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
