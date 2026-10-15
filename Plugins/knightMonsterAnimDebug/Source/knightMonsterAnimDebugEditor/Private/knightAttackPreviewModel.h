#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "UObject/GCObject.h"
#include "knightAnimAbilityInfo.h"
#include "knightAttackSelectionSimulator.h"

class FPreviewScene;
class FScopedTransaction;
struct FAnimNotifyEvent;
class FPrimitiveDrawInterface;
class UAnimMontage;
class USkeletalMeshComponent;

/** 근접 판정 궤적의 한 표본: 어빌리티 시간 Time에 두 소켓의 월드 위치. */
/** 공격 선택 표의 한 행. */
struct FknightSelectionRow
{
	int32 AbilityIndex = INDEX_NONE;
	FknightSelectionResult Result;
	/** 사용 가능한 공격들 사이에서 이 공격이 뽑힐 확률(0~1). */
	float Probability = 0.f;
};

/** 공백 지도의 한 칸: 이 위치에 더미가 있을 때 고를 수 있는 공격 수. */
struct FknightGapCell
{
	FVector Location = FVector::ZeroVector;
	int32 AvailableCount = 0;
};

struct FknightTrailSample
{
	float Time = 0.f;
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	float Radius = 0.f;
	int32 Window = 0;
};

/**
 * 공격 미리보기의 상태와 계산. 뷰포트, 타임라인, 컨트롤 패널이 이 객체 하나를 같이 쓴다.
 *
 * 시간은 어빌리티 시간(실제 초)이다. 몽타주를 재생하지 않고 매 프레임 위치만 지정하므로 노티파이가 실행되지 않는다
 * (미리보기 월드에서 게임 코드가 돌지 않는다). 그래서 구간 속도 노티파이는 직접 읽어 TimeMap으로 실제 초와 몽타주 시간을 바꾼다.
 *
 * 근접 궤적은 판정 노티파이 구간을 1/60초 간격으로 포즈를 잡아 두 소켓 위치를 기록해 만든다. 게임의 MeleeTrace가
 * 프레임마다 쓸고 지나가는 범위를 다시 만든 근사이므로, 실제 게임의 프레임 간격과 판정 세부 방식에 따라 약간 다를 수 있다.
 *
 * 미리보기 장면은 모델이 공유 소유한다. 모델이 장면의 컴포넌트를 쥐고 있으므로, 위젯이 어떤 순서로 파괴되든
 * 모델보다 장면이 먼저 사라지면 안 된다(탭을 닫을 때 사라진 장면에서 컴포넌트를 빼다 크래시가 났었다).
 */
class FknightAttackPreviewModel : public FGCObject, public FSelfRegisteringEditorUndoClient
{
public:
	explicit FknightAttackPreviewModel(const TSharedRef<FPreviewScene>& InScene);
	virtual ~FknightAttackPreviewModel() override;

	//~ FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FknightAttackPreviewModel"); }

	//~ FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

	FPreviewScene& GetScene() const { return *Scene; }

	// 선택
	void RefreshMonsters();
	int32 GetNumMonsters() const { return MonsterClasses.Num(); }
	FString GetMonsterName(int32 Index) const;
	const TArray<FknightAbilityAnimInfo>& GetAbilities() const { return Abilities; }
	int32 GetMonsterIndex() const { return MonsterIndex; }
	int32 GetAbilityIndex() const { return AbilityIndex; }
	void SelectMonster(int32 Index);
	void SelectAbility(int32 Index);
	/** 커버리지 보드에서 열 때. 찾지 못하면 false. */
	bool Select(const UClass* MonsterClass, const UClass* AbilityClass);
	const FknightAbilityAnimInfo* GetAbility() const { return Abilities.IsValidIndex(AbilityIndex) ? &Abilities[AbilityIndex] : nullptr; }
	const FString& GetNotice() const { return Notice; }

	// 시간
	void Tick(float DeltaSeconds);
	float GetTime() const { return Time; }
	float GetDuration() const;
	void SetTime(float NewTime);
	void StepFrame(int32 Direction);
	bool bPlaying = false;
	bool bLoop = true;
	float Speed = 1.f;

	// 타임라인용
	UAnimMontage* GetMontage() const { return Montage; }
	const FknightMontageTimeMap& GetTimeMap() const { return TimeMap; }
	const TArray<FknightHitWindow>& GetHitWindows() const { return Windows; }

	// 구간 속도 편집. 시간은 몽타주 시간(초)이고, 몽타주 에셋의 노티파이를 바로 고친다(되돌리기 가능, 저장은 따로).
	const TArray<FknightRateWindow>& GetRateWindows() const { return RateWindows; }
	/** 타임라인과 옆 패널이 같이 보는 선택 구간. */
	FGuid SelectedRateWindow;
	const FknightRateWindow* GetSelectedRateWindow() const { return RateWindows.FindByPredicate([this](const FknightRateWindow& Window) { return Window.Guid == SelectedRateWindow; }); }
	bool CanEditRate() const;
	/** 새 구간을 만들고 그 Guid를 돌려준다. 만들지 못하면 빈 Guid. */
	FGuid AddRateWindow(float Start, float End, float Rate);
	void SetRateWindowRange(const FGuid& Guid, float Start, float End);
	void SetRateWindowRate(const FGuid& Guid, float Rate);
	void RemoveRateWindow(const FGuid& Guid);
	/** 끌기 동안의 수정을 되돌리기 한 번으로 묶는다. 끌기 중에는 궤적을 다시 만들지 않는다. */
	void BeginRateDrag(const FText& Description);
	void EndRateDrag();
	bool IsRateDragging() const { return bRateDragging; }
	bool IsMontageDirty() const;
	void SaveMontage();

	/** 몽타주 첫 판정 노티파이의 시작(실제 초). 없으면 첫 착지 같은 판정 표시 노티파이(설정), 그것도 없으면 -1. */
	float GetMontageHitTime() const;
	/** 어빌리티 판정 시각(HitTime)을 몽타주 첫 판정에 맞출 수 있으면(고칠 프로퍼티가 있고 어긋나 있으면) true. */
	bool CanAlignHitPhase() const;
	/** HitTime이 몽타주 첫 판정과 같아지도록 판정 직전 구간(보통 WindupTime)을 고친다. */
	void AlignHitPhase();

	// 카메라용
	/** 몬스터 메시의 경계. 메시가 없으면 사람 크기 정도의 기본값. */
	FBoxSphereBounds GetFocusBounds() const;
	/** 메시를 새로 만들 때마다 오른다. 뷰포트가 카메라를 다시 맞출 때 쓴다. */
	int32 GetMeshRevision() const { return MeshRevision; }
	/** 선택 범위·진입 거리·범위 공격 중 가장 먼 거리(cm). 위에서 보는 카메라가 이만큼 담는다. */
	float GetMaxReach() const;

	// 더미와 명중
	float DummyDistance = 200.f;
	float DummyAngle = 0.f;
	float DummyRadius = 42.f;
	float DummyHalfHeight = 96.f;
	/** 더미 값이 바뀌면 부른다. */
	void RecomputeHits();
	/** 근접 궤적이 더미에 처음 닿는 어빌리티 시간, 안 닿으면 -1. */
	float GetMeleeHitTime() const { return MeleeHitTime; }
	bool IsAreaHit() const { return bAreaHit; }
	FString GetHitSummary() const;

	bool bShowTrail = true;
	bool bShowArea = true;
	bool bShowDummy = true;
	bool bShowSelectionShape = true;

	// 공격 선택 시뮬레이션(knightEditor가 시뮬레이터를 등록했을 때만 동작한다)
	bool HasSelectionSimulator() const { return knightAnimDebugEditor::GetAttackSelectionSimulator().IsValid(); }
	bool bTargetAirborne = false;
	FGameplayTagContainer SimulatedTags;
	/**
	 * StateTree가 Chase에서 Attack으로 넘어가는 거리(cm). StateTree는 연동하지 않으므로 직접 입력한다. 0이면 쓰지 않는다.
	 * 이 거리 안쪽의 공백이 게임에서 "공격 상태인데 쓸 공격이 없어 멈추는" 위치다.
	 */
	float EngageDistance = 0.f;
	/** 더미 위치·태그가 바뀌면 부른다(RecomputeHits가 함께 부른다). */
	void RecomputeSelection();
	const TArray<FknightSelectionRow>& GetSelectionRows() const { return SelectionRows; }

	/** 더미를 몬스터 주변 거리 × 각도 격자로 옮겨 가며 고를 수 있는 공격 수를 센다. */
	void BuildGapMap();
	void ClearGapMap();
	bool HasGapMap() const { return !GapCells.IsEmpty(); }
	const FString& GetGapSummary() const { return GapSummary; }

	void Draw(FPrimitiveDrawInterface* PDI) const;

	/** 커맨드렛 점검용 한 줄 요약: 궤적 표본 수, 첫·끝 표본의 소켓 위치, 명중 결과. */
	FString DescribeForLog() const;

private:
	void Rebuild();
	/** 몽타주 노티파이나 어빌리티 값이 바뀌었을 때: 시간 변환·판정 구간·궤적·명중을 다시 계산한다. 현재 시간은 유지한다. */
	void RebuildTiming(bool bWithTrail = true);
	void RefreshAbilityInfo();
	void OnRateEdited();
	FAnimNotifyEvent* FindRateNotify(const FGuid& Guid) const;
	void OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event);
	void ApplyPose(float AbilityTime);
	void BuildTrail();
	FTransform GetActorTransform() const;
	FVector GetDummyCenter() const;
	void GetDummySegment(FVector& OutA, FVector& OutB) const;

	TSharedRef<FPreviewScene> Scene;
	TObjectPtr<USkeletalMeshComponent> MeshComponent;
	TArray<TObjectPtr<UClass>> MonsterClasses;
	TArray<FknightAbilityAnimInfo> Abilities;
	/** Abilities의 클래스들을 GC에서 지킨다. */
	TArray<TObjectPtr<UClass>> AbilityClassRefs;
	int32 MonsterIndex = INDEX_NONE;
	int32 AbilityIndex = INDEX_NONE;

	FknightMonsterMeshInfo MeshInfo;
	TObjectPtr<UAnimMontage> Montage;
	FknightMontageTimeMap TimeMap;
	TArray<FknightRateWindow> RateWindows;
	TArray<FknightHitWindow> Windows;
	int32 MeshRevision = 0;
	bool bRateDragging = false;
	TUniquePtr<FScopedTransaction> DragTransaction;
	/** 다른 창(몽타주 에디터 등)에서 몽타주나 어빌리티가 바뀌면 다음 Tick에 다시 계산한다. */
	bool bTimingDirty = false;
	FDelegateHandle PropertyChangedHandle;
	TArray<FknightTrailSample> Trail;

	FknightSelectionQuery MakeQuery(float Distance, float Angle) const;

	TArray<FknightSelectionRow> SelectionRows;
	TArray<FknightGapCell> GapCells;
	FString GapSummary;

	float Time = 0.f;
	float MeleeHitTime = -1.f;
	bool bAreaHit = false;
	FString Notice;
};
