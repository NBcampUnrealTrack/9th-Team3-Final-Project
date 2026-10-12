// 피해 공식(knightDamageMath) 자동화 테스트
//
// 실행 방법: 에디터 상단 메뉴 Tools > Session Frontend > Automation 탭에서
//   "Knight.Combat.DamageMath"를 검색해 체크하고 Start Tests.
// 또는 콘솔에서: Automation RunTests Knight.Combat.DamageMath
//
// GAS 객체 없이 숫자만 넣고 결과를 확인한다. 공식을 바꾸면 이 테스트로 기존 동작이 깨지지 않았는지 먼저 본다.

#include "Misc/AutomationTest.h"
#include "AbilitySystem/Executions/knightDamageExecution.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FknightDamageMathTest, "Knight.Combat.DamageMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FknightDamageMathTest::RunTest(const FString& Parameters)
{
	using namespace knightDamageMath;
	constexpr float Tolerance = 0.001f;

	// ---- 가드 효율 배율: 남는 비율 = 1 - (1 - 기준) x 효율 ----
	TestEqual(TEXT("가드 효율 1.0이면 피해 기준값(0.2) 그대로"), GetGuardedMultiplier(0.2f, 1.f), 0.2f, Tolerance);
	TestEqual(TEXT("가드 효율 0.5면 피해 0.6"), GetGuardedMultiplier(0.2f, 0.5f), 0.6f, Tolerance);
	TestEqual(TEXT("가드 효율 1.25면 피해 0 (완전 차단)"), GetGuardedMultiplier(0.2f, 1.25f), 0.f, Tolerance);
	TestEqual(TEXT("가드 효율 2.0이어도 0 아래로 내려가지 않음"), GetGuardedMultiplier(0.2f, 2.f), 0.f, Tolerance);
	TestEqual(TEXT("가드 효율 0이면 가드해도 전부 받음"), GetGuardedMultiplier(0.2f, 0.f), 1.f, Tolerance);
	TestEqual(TEXT("가드 효율이 음수여도 0으로 취급"), GetGuardedMultiplier(0.2f, -1.f), 1.f, Tolerance);
	TestEqual(TEXT("가드 효율 1.0이면 그로기 기준값(0.5) 그대로"), GetGuardedMultiplier(0.5f, 1.f), 0.5f, Tolerance);
	TestEqual(TEXT("가드 효율 1.25면 그로기 0.375"), GetGuardedMultiplier(0.5f, 1.25f), 0.375f, Tolerance);

	// ---- 방어력 배율: 100 / (100 + 방어력) ----
	TestEqual(TEXT("방어력 0이면 1.0"), GetDefenseMultiplier(0.f), 1.f, Tolerance);
	TestEqual(TEXT("방어력 100이면 0.5"), GetDefenseMultiplier(100.f), 0.5f, Tolerance);
	TestEqual(TEXT("방어력이 음수면 0으로 취급"), GetDefenseMultiplier(-50.f), 1.f, Tolerance);

	// ---- 기본 피격: 기존 공식과 같은지 ----
	FInputs Base;
	Base.BaseDamage = 10.f;
	Base.AttackPower = 100.f;
	Base.MotionValue = 50.f;      // 공격력의 50%
	Base.BasePoiseDamage = 20.f;
	{
		const FResult R = Compute(Base);
		TestEqual(TEXT("기본 피해 = 10 + 100 x 50 / 100 = 60"), R.Damage, 60.f, Tolerance);
		TestEqual(TEXT("기본 그로기 = 20 (경직치 1.0)"), R.PoiseDamage, 20.f, Tolerance);
	}

	// ---- 경직치는 그로기에만 곱해지고 피해에는 영향 없음 ----
	{
		FInputs In = Base;
		In.StaggerPower = 2.f;
		const FResult R = Compute(In);
		TestEqual(TEXT("경직치 2.0이면 그로기 2배(40)"), R.PoiseDamage, 40.f, Tolerance);
		TestEqual(TEXT("경직치는 피해에 영향 없음(60)"), R.Damage, 60.f, Tolerance);
	}

	// ---- 가드: 기본 효율이면 기존 고정값(피해 x0.2, 그로기 x0.5)과 같음 ----
	{
		FInputs In = Base;
		In.bGuarded = true;
		const FResult R = Compute(In);
		TestEqual(TEXT("가드(효율 1.0) 피해 = 60 x 0.2 = 12"), R.Damage, 12.f, Tolerance);
		TestEqual(TEXT("가드(효율 1.0) 그로기 = 20 x 0.5 = 10"), R.PoiseDamage, 10.f, Tolerance);
	}
	{
		FInputs In = Base;
		In.bGuarded = true;
		In.GuardEfficiency = 1.25f;
		const FResult R = Compute(In);
		TestEqual(TEXT("가드(효율 1.25) 피해 0"), R.Damage, 0.f, Tolerance);
		TestEqual(TEXT("가드(효율 1.25) 그로기 = 20 x 0.375 = 7.5"), R.PoiseDamage, 7.5f, Tolerance);
	}
	{
		FInputs In = Base;
		In.GuardEfficiency = 0.f;
		const FResult R = Compute(In);
		TestEqual(TEXT("가드하지 않았으면 가드 효율은 무시"), R.Damage, 60.f, Tolerance);
	}

	// ---- 경직치와 가드 효율이 함께 적용 ----
	{
		FInputs In = Base;
		In.bGuarded = true;
		In.StaggerPower = 2.f;
		In.GuardEfficiency = 0.5f;
		const FResult R = Compute(In);
		TestEqual(TEXT("그로기 = 20 x 2 x 0.75 = 30"), R.PoiseDamage, 30.f, Tolerance);
		TestEqual(TEXT("피해 = 60 x 0.6 = 36"), R.Damage, 36.f, Tolerance);
	}

	// ---- 나머지 배율 ----
	{
		FInputs In = Base;
		In.bCritical = true;
		In.CriticalDamageMultiplier = 1.5f;
		In.PartMultiplier = 2.f;
		In.Defense = 100.f;
		const FResult R = Compute(In);
		TestEqual(TEXT("피해 = 60 x 부위 2 x 치명타 1.5 x 방어 0.5 = 90"), R.Damage, 90.f, Tolerance);
	}
	{
		FInputs In = Base;
		In.bCritical = true;
		In.CriticalDamageMultiplier = 0.5f;
		TestEqual(TEXT("치명타 배율이 1 미만이면 1로 취급"), Compute(In).Damage, 60.f, Tolerance);
	}
	{
		FInputs In = Base;
		In.PartMultiplier = -1.f;
		In.StaggerPower = -1.f;
		const FResult R = Compute(In);
		TestEqual(TEXT("부위 배율이 음수면 피해 0"), R.Damage, 0.f, Tolerance);
		TestEqual(TEXT("경직치가 음수면 그로기 0"), R.PoiseDamage, 0.f, Tolerance);
	}

	// ---- 받는 피해·그로기 배율 (2026-10-10, 파츠 효과용): 마지막에 곱해서 정확한 비율로 줄인다 ----
	{
		FInputs In = Base;
		In.DamageTakenMultiplier = 0.85f;
		In.PoiseDamageTakenMultiplier = 0.75f;
		const FResult R = Compute(In);
		TestEqual(TEXT("받는 피해 배율 0.85: 60 x 0.85 = 51 (-15%)"), R.Damage, 51.f, Tolerance);
		TestEqual(TEXT("받는 그로기 배율 0.75: 20 x 0.75 = 15 (-25%)"), R.PoiseDamage, 15.f, Tolerance);
	}
	{
		FInputs In = Base;
		In.Defense = 100.f;
		In.bGuarded = true;
		In.DamageTakenMultiplier = 0.85f;
		In.PoiseDamageTakenMultiplier = 0.75f;
		const FResult R = Compute(In);
		TestEqual(TEXT("방어 0.5·가드 0.2와 겹쳐도 비율 그대로: 60 x 0.5 x 0.2 x 0.85 = 5.1"), R.Damage, 5.1f, Tolerance);
		TestEqual(TEXT("가드 0.5와 겹쳐도 비율 그대로: 20 x 0.5 x 0.75 = 7.5"), R.PoiseDamage, 7.5f, Tolerance);
	}
	{
		FInputs In = Base;
		In.DamageTakenMultiplier = -1.f;
		In.PoiseDamageTakenMultiplier = -1.f;
		const FResult R = Compute(In);
		TestEqual(TEXT("받는 피해 배율이 음수면 피해 0 (회복되지 않음)"), R.Damage, 0.f, Tolerance);
		TestEqual(TEXT("받는 그로기 배율이 음수면 그로기 0"), R.PoiseDamage, 0.f, Tolerance);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
