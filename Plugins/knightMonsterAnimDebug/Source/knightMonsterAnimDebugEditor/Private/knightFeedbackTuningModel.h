#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class APawn;
class APlayerController;
class UWorld;

/** 값을 어디서 찾았는지. 몬스터 묶음만 기본으로 저장할 수 있다. */
enum class EknightFeedbackGroup : uint8
{
	Monster,
	Player,
	/** 카메라 매니저, 플레이어 카메라 컴포넌트, 카메라 모드·모디파이어 클래스 기본값. */
	Camera
};

/** 값을 가진 원본 객체 하나(어빌리티 CDO, 컴포넌트 템플릿, 데이터 에셋, 노티파이, 셰이크 패턴). */
struct FknightFeedbackSource
{
	/** 저장되는 쪽. PIE의 살아 있는 복사본이 아니라 그 원본이다. */
	TWeakObjectPtr<UObject> Object;
	FString Label;
	/** 몬스터가 아니면 설정이 허락하지 않는 한 저장하지 않는다. */
	EknightFeedbackGroup Group = EknightFeedbackGroup::Monster;
	/** /Script/ 패키지(네이티브 기본값)라 저장할 수 없다. 바꿔 보기만 된다. */
	bool bNative = false;
};

/** 숫자 프로퍼티 하나. Chain은 원본 객체에서 값까지의 프로퍼티 경로(구조체를 거치면 여러 개). */
struct FknightFeedbackEntry
{
	int32 SourceIndex = INDEX_NONE;
	FString Path;
	TArray<const FProperty*> Chain;
	double Original = 0.0;
	/** 마지막으로 저장한 값. 처음엔 Original과 같다. */
	double Saved = 0.0;
	bool bInteger = false;
};

/**
 * 전투 피드백 튜닝 탭의 데이터. 리플렉션만 쓰므로 게임 모듈에 의존하지 않는다.
 *
 * 수집: PIE의 몬스터와 플레이어가 가진 어빌리티, 컴포넌트, 데이터 에셋을 훑고, 거기서 참조하는 몽타주의 노티파이와
 *       카메라 셰이크까지 내려간다. 살아 있는 객체에서 찾았어도 값은 그 원본(아키타입)에 연결한다.
 * 적용: 원본과, PIE에서 그 원본으로 만들어진 살아 있는 복사본 모두에 쓴다. 어빌리티는 액터마다 인스턴스를 미리 만들어 두므로
 *       원본만 바꾸면 다음 PIE까지 반영되지 않는다.
 * 저장: 원본 패키지를 저장한다. 저장하지 않은 값은 PIE가 끝날 때 원래대로 돌린다(에디터의 에셋이 몰래 바뀐 채로 남지 않게).
 */
class FknightFeedbackTuningModel
{
public:
	/** 튜닝 탭과 콘솔 명령(knight.FeedbackTuning.*)이 같이 쓰는 모델. PIE가 끝나면 저장하지 않은 값을 돌리고 비운다. */
	static FknightFeedbackTuningModel& Get();
	/** 모듈을 내릴 때 PIE 종료 구독을 푼다. */
	static void Shutdown();

	/** PIE가 끝나 비운 뒤 부른다. 인자는 되돌린 값 개수. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnReset, int32);
	FOnReset OnReset;

	/** 값을 새로 모은 뒤 부른다(탭의 다시 읽기, 콘솔 명령 어느 쪽이든). */
	FSimpleMulticastDelegate OnCollected;

	static const TCHAR* GroupLabel(EknightFeedbackGroup Group);

	/**
	 * 디버거가 고른 몬스터(없으면 가장 가까운 몬스터), 카메라, 첫 플레이어 폰 순서로 값을 모은다. 먼저 저장하지 않은 값을 돌린다.
	 * 같은 원본은 먼저 찾은 묶음에 넣는다(몬스터 공격이 쓰는 셰이크는 몬스터 쪽).
	 */
	void Collect(UWorld* PlayWorld);
	void Clear();
	bool IsCollected(const UWorld* PlayWorld) const { return PlayWorld && World.Get() == PlayWorld; }
	APawn* GetMonster() const { return Monster.Get(); }
	APawn* GetPlayer() const { return Player.Get(); }

	/** "[묶음] 출처 · 경로"에 Filter의 낱말(공백으로 나눔)이 모두 들어 있는지. 대소문자는 가리지 않는다. */
	bool MatchesFilter(int32 EntryIndex, const FString& Filter) const;
	void FindEntries(const FString& Filter, TArray<int32>& OutIndices) const;
	FString DescribeEntry(int32 EntryIndex) const;

	/** 플레이어 어빌리티 중 히트스톱 값이 있는 것(공격)의 이름. */
	void GetPlayerAttackNames(TArray<FString>& OutNames) const;
	/** 플레이어를 몬스터 정면 Distance에 세우고 몬스터를 보게 한다. */
	void PlacePlayer(float Distance) const;
	/** 플레이어 공격이 닿는 거리: 몬스터 몸통 반지름 + 설정의 간격. */
	float GetPlayerAttackDistance() const;
	/** 카메라 반응 표의 순간 이름들과 그 표의 출처 이름(찾기 칸에 넣을 값). 표를 못 찾으면 비어 있다. */
	void GetImpactMoments(TArray<FString>& OutMoments, FString& OutSourceLabel) const;
	/** 카메라 매니저의 미리보기 함수로 순간 하나를 최대 세기로 재생한다. */
	bool PreviewImpactMoment(const FString& Moment) const;

	/** bPlace면 공격 거리에 세우고 착지를 기다렸다가(최대 2초) 플레이어 어빌리티를 강제로 실행한다. 이때 true는 예약했다는 뜻. */
	bool PlayPlayerAttack(const FString& Name, bool bPlace) const;

	const TArray<FknightFeedbackSource>& GetSources() const { return Sources; }
	const TArray<FknightFeedbackEntry>& GetEntries() const { return Entries; }

	double GetValue(int32 EntryIndex) const;
	void SetValue(int32 EntryIndex, double Value);
	bool IsChanged(int32 EntryIndex) const;
	bool IsSaveLocked(int32 SourceIndex) const;

	/** 저장할 수 있는 바뀐 값의 패키지를 저장하고 저장한 값 개수를 돌려준다. */
	int32 SaveChanged(FString& OutMessage);
	/** 저장하지 않은 값을 마지막 저장값으로 돌린다. */
	int32 RevertUnsaved();
	/** 바뀐 값을 "출처 · 경로: 전 → 후" 줄로. 플레이어 쪽은 인계용으로 표시한다. */
	FString DescribeChanges() const;
	int32 CountChanged() const;

private:
	void AddObject(UObject* Live, const FString& Label, EknightFeedbackGroup Group, int32 Depth);
	void AddCamera(APlayerController* PC);
	void AddProperties(int32 SourceIndex, const UStruct* Struct, const void* Container, TArray<const FProperty*>& Chain,
		const FString& Prefix, bool bIncludeAll, bool bSkipEngineProperties, int32 Depth);
	bool MatchesKeyword(const FString& Name) const;
	void ApplyToLiveCopies(const FknightFeedbackEntry& Entry, double Value) const;

	static UObject* FindPersistentArchetype(UObject* Live);
	static void* ResolveValue(UObject* Object, const TArray<const FProperty*>& Chain);
	static double ReadNumber(const FProperty* Leaf, const void* Ptr);
	static void WriteNumber(const FProperty* Leaf, void* Ptr, double Value);

	void HandleEndPIE(bool bIsSimulating);

	FDelegateHandle EndPIEHandle;

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<APawn> Monster;
	TWeakObjectPtr<APawn> Player;
	TArray<FknightFeedbackSource> Sources;
	TArray<FknightFeedbackEntry> Entries;
	TSet<const UObject*> Visited;
	TArray<FString> Keywords;
	TArray<const UClass*> AllPropertyClasses;
};
