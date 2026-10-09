// 몬스터 부위 상태(남은 내구 비율, 빈사, 파괴) 자동화 테스트
//
// 실행 방법: 에디터 출력 로그의 명령 입력칸(Cmd)에
//   Automation RunTests Knight.Combat.MonsterPartCondition
//
// 통과한 항목은 로그에 남지 않고, 틀린 항목만 이름과 기대값이 Error로 찍힌다.

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Monster/knightMonsterPartComponent.h"
#include "Monster/knightMonsterPartData.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FknightMonsterPartConditionTest, "Knight.Combat.MonsterPartCondition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FknightMonsterPartConditionTest::RunTest(const FString& Parameters)
{
	constexpr float Tolerance = 0.001f;
	const FGameplayTag Head = FGameplayTag::RequestGameplayTag(TEXT("Monster.Part.Janyeol.Head"));
	const FGameplayTag Body = FGameplayTag::RequestGameplayTag(TEXT("Monster.Part.Janyeol.Body"));
	const FGameplayTag Tail = FGameplayTag::RequestGameplayTag(TEXT("Monster.Part.Janyeol.Tail"));

	// 머리: 내구 1000, 빈사 0.3 / 몸통: 파괴 불가
	UknightMonsterPartData* const Data = NewObject<UknightMonsterPartData>();
	FknightMonsterPartDefinition& HeadDef = Data->Parts.AddDefaulted_GetRef();
	HeadDef.PartTag = Head;
	HeadDef.bBreakable = true;
	HeadDef.BreakDamage = 1000.f;
	HeadDef.CriticalRatio = 0.3f;
	FknightMonsterPartDefinition& BodyDef = Data->Parts.AddDefaulted_GetRef();
	BodyDef.PartTag = Body;

	UWorld* const World = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* const Owner = World->SpawnActor<AActor>();
	UknightMonsterPartComponent* const Parts = NewObject<UknightMonsterPartComponent>(Owner);
	// ASC 없이 부위 상태만 본다(파괴 효과·태그·큐는 건너뜀).
	AddExpectedMessage(TEXT("needs an ASC"), EAutomationExpectedMessageFlags::Contains, 1);
	Parts->InitializeParts(Data, nullptr);

	TestEqual(TEXT("처음 머리 비율 1"), Parts->GetPartHealthRatio(Head), 1.f, Tolerance);
	TestTrue(TEXT("처음 머리 Normal"), Parts->GetPartCondition(Head) == EknightPartCondition::Normal);
	TestTrue(TEXT("몸통 Unbreakable"), Parts->GetPartCondition(Body) == EknightPartCondition::Unbreakable);
	TestTrue(TEXT("데이터에 없는 부위 Unbreakable"), Parts->GetPartCondition(Tail) == EknightPartCondition::Unbreakable);
	TestEqual(TEXT("데이터에 없는 부위 비율 1"), Parts->GetPartHealthRatio(Tail), 1.f, Tolerance);

	Parts->ApplyPartDamage(Body, 5000.f, nullptr);
	TestEqual(TEXT("몸통은 피해를 받아도 비율 1"), Parts->GetPartHealthRatio(Body), 1.f, Tolerance);

	Parts->ApplyPartDamage(Head, 600.f, nullptr);
	TestEqual(TEXT("600 피해 후 비율 0.4"), Parts->GetPartHealthRatio(Head), 0.4f, Tolerance);
	TestTrue(TEXT("비율 0.4는 Normal"), Parts->GetPartCondition(Head) == EknightPartCondition::Normal);

	Parts->ApplyPartDamage(Head, 100.f, nullptr);
	TestEqual(TEXT("700 피해 후 비율 0.3"), Parts->GetPartHealthRatio(Head), 0.3f, Tolerance);
	TestTrue(TEXT("비율 0.3(빈사 기준과 같음)은 Critical"), Parts->GetPartCondition(Head) == EknightPartCondition::Critical);

	Parts->ApplyPartDamage(Head, 500.f, nullptr);
	TestEqual(TEXT("파괴 후 비율 0"), Parts->GetPartHealthRatio(Head), 0.f, Tolerance);
	TestTrue(TEXT("파괴 후 Broken"), Parts->GetPartCondition(Head) == EknightPartCondition::Broken);
	TestTrue(TEXT("IsPartBroken도 true"), Parts->IsPartBroken(Head));

	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
