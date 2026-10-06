#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "CoreMinimal.h"
#include "GameplayDebuggerCategory.h"

class APawn;
class UAnimInstance;
class UknightAnimDebugSubsystem;
struct FknightAnimDebugTrack;

/**
 * Gameplay Debugger의 "MonsterAnim" 카테고리. ' 키로 디버거를 켜고 넘패드 숫자로 카테고리를 켜고 끈다.
 *
 * 화면: 몬스터 머리 위 한 줄 요약, 대상 몬스터의 StateTree 상태·어빌리티·몽타주·노티파이·태그·속성,
 * 어빌리티 구간(예고·판정·후딜)과 몽타주(섹션·노티파이 구간) 타임라인, 최근 이벤트, 성능 줄.
 * 대상은 디버거가 고른 액터이고, 없으면 플레이어에게 가장 가까운 몬스터다.
 *
 * 키 (카테고리가 보일 때만, Alt 조합. 키는 GameplayDebugger 설정에서 바꿀 수 있다):
 *   Alt+[ / Alt+]  실행할 어빌리티 선택        Alt+P  선택한 어빌리티 실행
 *   Alt+R  반복 실행 켜기/끄기                 Alt+F  AI 정지/재개
 *   Alt+T  배속 순환(1 → 0.5 → 0.25 → 0.1)    Alt+G  일시정지/재개
 *   Alt+N  한 프레임 진행                      Alt+C  녹화 시작/저장
 *   Alt+V  머리 위 요약: 전체 몬스터/대상만
 */
class FGameplayDebuggerCategory_knightMonsterAnim : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_knightMonsterAnim();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

private:
	void DrawOverhead(FGameplayDebuggerCanvasContext& CanvasContext, const FknightAnimDebugTrack& Track, bool bIsTarget) const;
	void DrawTargetPanel(FGameplayDebuggerCanvasContext& CanvasContext, UknightAnimDebugSubsystem& Subsystem, APawn& Target) const;
	void DrawTimelines(FGameplayDebuggerCanvasContext& CanvasContext, const FknightAnimDebugTrack& Track, UAnimInstance* AnimInstance) const;
	void DrawPerf(FGameplayDebuggerCanvasContext& CanvasContext, const UknightAnimDebugSubsystem& Subsystem) const;
	void DrawControls(FGameplayDebuggerCanvasContext& CanvasContext, const UknightAnimDebugSubsystem& Subsystem) const;
	void DrawWorldShapes(UWorld* World, const FknightAnimDebugTrack& Track) const;

	UknightAnimDebugSubsystem* GetSubsystem() const;
	FString GetSelectedAbility() const;

	void OnSelectPrevious();
	void OnSelectNext();
	void OnPlay();
	void OnRepeat();
	void OnFreezeAI();
	void OnTimeScale();
	void OnPause();
	void OnStep();
	void OnRecord();
	void OnToggleOverheadAll();

	TWeakObjectPtr<UWorld> CachedWorld;
	TArray<FString> AbilityNames;
	int32 SelectedAbility = 0;
	bool bOverheadForAll = true;
};

#endif // WITH_GAMEPLAY_DEBUGGER
