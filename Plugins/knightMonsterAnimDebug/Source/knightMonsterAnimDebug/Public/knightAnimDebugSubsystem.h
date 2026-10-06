#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "knightAnimDebugSubsystem.generated.h"

class APawn;
class UAbilitySystemComponent;
class UAnimInstance;
class UAnimMontage;
class UGameplayAbility;
struct FAbilityEndedData;
struct FGameplayTag;

enum class EknightAnimDebugEventType : uint8
{
	State,
	AbilityStart,
	AbilityEnd,
	MontageStart,
	MontageEnd,
	NotifyBegin,
	NotifyEnd,
	TagAdded,
	TagRemoved,
	Control,
};

struct FknightAnimDebugEvent
{
	/** 월드 시간(초). 슬로 모션과 일시정지가 반영된다. */
	double WorldTime = 0.0;
	/** 녹화 시작부터의 실제 시간(초). 녹화 중이 아니면 0이다. */
	double RealTime = 0.0;
	FString Actor;
	EknightAnimDebugEventType Type = EknightAnimDebugEventType::State;
	FString Detail;
	/** 이벤트가 난 프레임의 실제 프레임 시간(ms). */
	float FrameMs = 0.f;
};

struct FknightAnimDebugPhase
{
	FString Label;
	float Duration = 0.f;
	FColor Color = FColor::White;
	bool bHitAtEnd = false;
};

/** 실행 중인 어빌리티 한 번. 타임라인 구간은 활성화 순간 어빌리티 인스턴스에서 읽는다. */
struct FknightAnimDebugAbilityRun
{
	TWeakObjectPtr<UGameplayAbility> Ability;
	FString Name;
	double StartWorldTime = 0.0;
	TArray<FknightAnimDebugPhase> Phases;
	bool bHasArea = false;
	FVector AreaOffset = FVector::ZeroVector;
	float AreaRadius = 0.f;

	float GetTotalPhaseTime() const;
};

/** 추적 중인 몬스터 하나의 현재 상태와 최근 기록. */
struct FknightAnimDebugTrack
{
	TWeakObjectPtr<APawn> Pawn;
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	FString DisplayName;

	FDelegateHandle AbilityActivatedHandle;
	FDelegateHandle AbilityEndedHandle;
	FDelegateHandle TagHandle;

	/** StateTree의 활성 상태 경로("Root/Combat/Attack"). */
	FString StatePath;
	double StateEnterWorldTime = 0.0;
	/** 직전 상태 경로들, 최근 것이 앞. */
	TArray<FString> PreviousStates;

	TWeakObjectPtr<UAnimMontage> Montage;
	TArray<FString> ActiveNotifies;

	bool bHasActiveRun = false;
	FknightAnimDebugAbilityRun ActiveRun;
	FString LastRunName;
	double LastRunEndWorldTime = -1.0;

	/** 화면 패널용 최근 이벤트, 최근 것이 뒤. */
	TArray<FknightAnimDebugEvent> RecentEvents;
};

struct FknightAnimDebugPerfValue
{
	FName Name;
	double Value = 0.0;
	bool bIsTimeMs = false;
	/** 프레임마다 지워지지 않고 마지막 값을 유지하는 카운터(살아 있는 몬스터 수 등). */
	bool bIsGauge = false;
};

/**
 * 몬스터 애니메이션 디버거의 데이터와 조작을 맡는다. 화면 표시는 Gameplay Debugger 카테고리
 * (FGameplayDebuggerCategory_knightMonsterAnim)가, 콘솔 명령은 knightAnimDebugCommands.cpp가 이 서브시스템을 쓴다.
 *
 * 추적: 설정의 TrackedPawnClass 폰을 찾아 어빌리티 시작·끝과 태그 변화는 델리게이트로, StateTree 상태와 몽타주,
 * 노티파이 스테이트는 매 프레임 폴링으로 기록한다. 폴링은 디버거가 보이거나 녹화·반복 실행·프레임 진행 중일 때만 한다.
 *
 * 녹화: 이벤트와 프레임 비용을 모았다가 멈추면 Saved/Profiling/knightAnimDebug/에 Markdown과 CSV로 저장한다.
 * 상태 전환과 어빌리티 시작·끝, 판정 구간 시작에는 Unreal Insights 북마크도 찍는다.
 *
 * 화면 표시가 로컬 월드를 직접 읽으므로 Standalone, PIE, 리슨 서버 호스트에서만 동작한다.
 */
UCLASS()
class KNIGHTMONSTERANIMDEBUG_API UknightAnimDebugSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UknightAnimDebugSubsystem* Get(const UObject* WorldContextObject);

	//~ USubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

	//~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual TStatId GetStatId() const override;

	/** 디버거가 화면에 그릴 때마다 부른다. 잠시 동안 폴링을 켜 둔다. */
	void MarkViewed();

	const TArray<FknightAnimDebugTrack>& GetTracks() const { return Tracks; }
	const FknightAnimDebugTrack* FindTrack(const APawn* Pawn) const;

	/** Gameplay Debugger가 고른 액터. 추적 대상이 아니면 무시한다. */
	void SetFocusedPawn(APawn* Pawn);

	/** 조작 대상: 디버거가 고른 몬스터, 없으면 플레이어에게 가장 가까운 몬스터. */
	APawn* ResolveTarget();

	/** 강제 실행할 수 있는 어빌리티 이름(클래스 이름에서 _C를 뺀 것). */
	void GetPlayableAbilityNames(const APawn* Pawn, TArray<FString>& OutNames) const;

	/** 이름이 정확히 같은 어빌리티, 없으면 이름에 Name이 들어간 첫 어빌리티를 실행한다. */
	bool PlayAbility(APawn* Pawn, const FString& Name);

	/** 어빌리티가 끝나고 Delay초 뒤에 다시 실행하기를 반복한다. */
	void StartRepeat(APawn* Pawn, const FString& Name, float Delay);
	void StopRepeat();
	bool IsRepeating() const { return RepeatPawn.IsValid() && !RepeatAbilityName.IsEmpty(); }
	const FString& GetRepeatAbilityName() const { return RepeatAbilityName; }

	/** 추적 중인 모든 몬스터의 AI(Brain)를 멈추거나 재개한다. 새로 찾은 몬스터에도 적용한다. */
	void SetAIFrozen(bool bFrozen);
	bool IsAIFrozen() const { return bAIFrozen; }

	/** 일시정지 중이면 재개할 때의 배속으로 저장한다. */
	void SetTimeScale(float Scale);
	/** 일시정지 중에도 재개했을 때의 배속을 돌려준다. */
	float GetTimeScale() const;

	/**
	 * 월드 배속을 엔진 최저값(MinGlobalTimeDilation, 기본 0.0001)으로 내려 멈춘다. SetGamePaused로 진짜 일시정지를 하면
	 * Gameplay Debugger의 키 바인딩이 모두 막혀 다시 풀 수 없으므로 이렇게 한다. 입력과 카메라 회전은 계속 동작한다.
	 */
	void SetPaused(bool bPause);
	bool IsPaused() const { return bTimePaused; }
	/** 일시정지 상태에서 Frames 프레임만 원래 배속으로 진행하고 다시 멈춘다. */
	void StepFrames(int32 Frames);

	void StartRecording(const FString& Label);
	/** 저장한 Markdown 경로를 돌려준다. 녹화 중이 아니었으면 빈 문자열. */
	FString StopRecording();
	bool IsRecording() const { return bRecording; }
	double GetRecordingSeconds() const;

	/** 마지막 프레임의 실제 프레임 시간(ms)과 설정 접두사에 맞는 성능 카운터. */
	float GetFrameMs() const { return SmoothedFrameMs; }
	void GetPerfValues(TArray<FknightAnimDebugPerfValue>& OutValues, double& OutAliveCount) const;

	/** 속성 세트에서 이름으로 속성 현재 값을 찾는다. */
	static bool FindAttributeValue(const UAbilitySystemComponent* ASC, FName AttributeName, float& OutValue);

	/** 노티파이 스테이트 이름이 설정의 판정 구간 키워드를 포함하는지. */
	static bool IsHitWindowNotify(const FString& NotifyName);

	static UAnimInstance* GetAnimInstance(const APawn* Pawn);
	static FString GetNotifyStateName(const struct FAnimNotifyEvent& Event);

private:
	void RefreshTracks();
	void BeginTrack(APawn* Pawn);
	void EndTrack(FknightAnimDebugTrack& Track);
	FknightAnimDebugTrack* FindTrackMutable(const APawn* Pawn);
	void PollTrack(FknightAnimDebugTrack& Track);

	bool IsPlayableAbility(const UGameplayAbility* Ability) const;
	void HandleAbilityActivated(UGameplayAbility* Ability);
	void HandleAbilityEnded(const FAbilityEndedData& Data);
	void HandleTagChanged(const FGameplayTag Tag, int32 NewCount, TWeakObjectPtr<APawn> WeakPawn);

	void AddEvent(FknightAnimDebugTrack* Track, EknightAnimDebugEventType Type, const FString& Detail, bool bBookmark);
	void SampleRecordingPerf();
	FString WriteRecording();

	double GetWorldTime() const;

	UPROPERTY(Transient)
	TArray<TSubclassOf<UGameplayAbility>> PlayableClasses;

	UPROPERTY(Transient)
	TSubclassOf<APawn> TrackedClass;

	bool bSettingsResolved = false;
	void ResolveSettings();

	TArray<FknightAnimDebugTrack> Tracks;
	TWeakObjectPtr<APawn> FocusedPawn;

	double LastViewedRealTime = -1.0;
	double LastRefreshRealTime = -1.0;
	double LastTickRealTime = 0.0;
	float SmoothedFrameMs = 0.f;
	float LastFrameMs = 0.f;

	TWeakObjectPtr<APawn> RepeatPawn;
	FString RepeatAbilityName;
	float RepeatDelay = 1.f;

	bool bAIFrozen = false;
	bool bTimePaused = false;
	/** 일시정지 중일 때 재개하면 돌아갈 배속. */
	float PausedTimeScale = 1.f;
	uint64 StepUntilFrame = 0;

	void ApplyTimeDilation(float Dilation);

	bool bRecording = false;
	FString RecordingLabel;
	FDateTime RecordingStartDate;
	double RecordingStartRealTime = 0.0;
	double RecordingStartWorldTime = 0.0;
	TArray<FknightAnimDebugEvent> RecordedEvents;

	struct FPerfAccumulator
	{
		double Sum = 0.0;
		double Max = 0.0;
		int32 Count = 0;
		bool bIsTimeMs = false;
		bool bIsGauge = false;

		void Add(double Value)
		{
			Sum += Value;
			Max = FMath::Max(Max, Value);
			++Count;
		}
	};
	FPerfAccumulator RecordedFrameMs;
	TMap<FName, FPerfAccumulator> RecordedCounters;

	struct FAbilityStat
	{
		FString Actor;
		FString Ability;
		int32 Count = 0;
		int32 Cancelled = 0;
		double TotalSeconds = 0.0;
		double MaxSeconds = 0.0;
	};
	/** "몬스터|어빌리티"별 실행 통계. */
	TMap<FString, FAbilityStat> RecordedAbilities;
};
