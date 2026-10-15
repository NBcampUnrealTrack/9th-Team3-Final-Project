#pragma once

#include "CoreMinimal.h"
#include "knightAnimDebugSubsystem.h"

class UAnimMontage;
class UGameplayAbility;
class USkeletalMesh;

/** 몬스터 Blueprint 기본 객체에서 읽은 메시 정보. 미리보기는 이 값으로 몬스터를 세운다. */
struct FknightMonsterMeshInfo
{
	USkeletalMesh* Mesh = nullptr;
	UClass* AnimClass = nullptr;
	/** 액터 기준 메시 컴포넌트 변환(캐릭터 메시의 -90 높이·-90 회전 같은 값). */
	FTransform MeshRelativeTransform = FTransform::Identity;
	float CapsuleHalfHeight = 0.f;
};

/** 몽타주의 판정 노티파이 구간 하나. 시간은 몽타주 시간(초)이다. */
struct FknightHitWindow
{
	FString Name;
	float Start = 0.f;
	float End = 0.f;
	/** 근접 궤적용. 소켓을 못 찾았으면 비어 있다. */
	FName StartSocket;
	FName EndSocket;
	float Radius = 0.f;

	bool HasTraceShape() const { return !StartSocket.IsNone() && !EndSocket.IsNone(); }
};

/** 몽타주의 구간 속도 노티파이 하나. 시간은 몽타주 시간(초)이다. */
struct FknightRateWindow
{
	/** 편집할 때 노티파이를 다시 찾는 열쇠. 몽타주는 노티파이를 시간순으로 다시 정렬하므로 순서로는 찾을 수 없다. */
	FGuid Guid;
	float Start = 0.f;
	float End = 0.f;
	float Rate = 1.f;
};

/**
 * 몽타주 시간과 실제 시간(어빌리티 시간, 초) 사이의 변환. 몽타주의 RateScale과 구간 속도 노티파이를 함께 반영하고,
 * 겹친 구간은 게임처럼 배율을 곱한다. 게임은 노티파이를 프레임 단위로 켜고 끄므로 한 프레임 정도 차이가 날 수 있다.
 */
struct FknightMontageTimeMap
{
	void Build(const UAnimMontage* Montage, const TArray<FknightRateWindow>& RateWindows);
	float ToReal(float MontageTime) const;
	float ToMontage(float RealTime) const;
	float GetRealLength() const { return RealLength; }

private:
	/** 배율이 일정한 한 구간. 마지막 구간의 배율은 몽타주 끝 너머로도 이어진다. */
	struct FSegment
	{
		float MontageStart = 0.f;
		float RealStart = 0.f;
		float Rate = 1.f;
	};
	TArray<FSegment> Segments;
	float RealLength = 0.f;
};

/** 어빌리티 기본 객체에서 설정의 프로퍼티 이름으로 읽은 애니메이션 관련 정보. */
struct FknightAbilityAnimInfo
{
	UClass* AbilityClass = nullptr;
	const UGameplayAbility* AbilityCDO = nullptr;
	FString Name;
	/** 맞는 PlayableAbilityClasses 기반 클래스 이름(MonsterAttack, MonsterReaction). */
	FString Kind;
	bool bIsAttack = false;

	TArray<FknightAnimDebugPhase> Phases;
	float PhaseTotal = 0.f;
	/** 판정 표시 구간이 끝나는 시각, 없으면 -1. */
	float HitTime = -1.f;
	/** 지속 판정이 끝나는 시각. 지속 구간이 없으면 HitTime과 같다. */
	float HitEndTime = -1.f;
	/** 끝이 HitTime인 구간의 프로퍼티(보통 WindupTime). 판정 시각을 몽타주에 맞출 때 이 값을 고친다. */
	const FFloatProperty* HitPhaseProperty = nullptr;

	bool bHasArea = false;
	FVector AreaOffset = FVector::ZeroVector;
	float AreaRadius = 0.f;

	/** 편집 가능한 몽타주 프로퍼티(슬롯). 첫 번째 필수 슬롯이 주 몽타주다. */
	TArray<const FObjectProperty*> MontageProperties;

	UAnimMontage* GetMontage(const FObjectProperty* Property) const;
	bool IsOptionalSlot(const FObjectProperty* Property) const;
	/** 첫 번째 필수 슬롯의 몽타주(없으면 nullptr). */
	UAnimMontage* GetPrimaryMontage() const;
	const FObjectProperty* GetPrimarySlot() const;
	FString GetTimingText() const;
};

namespace knightAnimAbilityInfo
{
	FString CleanName(const UObject* Object);

	/** 더블클릭으로 열 에셋: Blueprint 클래스면 Blueprint, 아니면 클래스 자체. */
	UObject* GetBlueprintOrClass(UClass* Class);

	/** 설정의 TrackedPawnClass를 상속한 몬스터 Blueprint 클래스(추상 제외, 이름순). 문제가 있으면 OutNotice에 안내를 쓴다. */
	TArray<UClass*> FindMonsterClasses(FString& OutNotice);

	/** 몬스터 기본 객체에서 데이터 에셋을 따라가 찾은, 설정의 PlayableAbilityClasses에 맞는 어빌리티들. */
	void FindPlayableAbilities(UClass* MonsterClass, TArray<FknightAbilityAnimInfo>& OutAbilities);

	FknightMonsterMeshInfo GetMonsterMeshInfo(UClass* MonsterClass);

	/** 판정 키워드에 맞는 노티파이 스테이트 구간과, 노티파이 객체에서 읽은 소켓·반지름. */
	void GetHitWindows(const UAnimMontage* Montage, TArray<FknightHitWindow>& OutWindows);

	/** 몽타주 전체에 걸린 재생 배율(RateScale). 구간 속도까지 반영한 변환은 FknightMontageTimeMap을 쓴다. */
	float GetMontageRate(const UAnimMontage* Montage);

	/** 설정의 PlayRateNotifyClass 구간들(시작 시간순). 클래스나 배율 프로퍼티를 찾지 못하면 비어 있다. */
	void GetRateWindows(const UAnimMontage* Montage, TArray<FknightRateWindow>& OutWindows);

	/** 구간 속도 노티파이 클래스와 그 배율 프로퍼티. 둘 중 하나라도 없으면 false. */
	bool GetPlayRateNotify(UClass*& OutClass, const FFloatProperty*& OutRateProperty);

	/** 판정 노티파이가 없을 때 판정 시점으로 볼 첫 노티파이(설정의 HitMarkerNotifyKeywords)의 몽타주 시간, 없으면 -1. */
	float GetFirstHitMarkerTime(const UAnimMontage* Montage);

	/** RateScale과 구간 속도를 반영한 변환표. */
	FknightMontageTimeMap MakeTimeMap(const UAnimMontage* Montage);

	/** AnimBP의 Slot 노드 이름들(리플렉션으로 AnimNode_Slot 구조체의 SlotName을 읽는다). */
	void GetAnimBlueprintSlots(const UClass* AnimClass, TSet<FName>& OutSlots);
}
