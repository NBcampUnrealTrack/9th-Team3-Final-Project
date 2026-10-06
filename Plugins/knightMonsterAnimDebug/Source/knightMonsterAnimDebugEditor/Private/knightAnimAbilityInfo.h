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

	/** 몽타주를 재생하는 배율(RateScale). 어빌리티 시간 t는 몽타주 시간 t * 배율이다. */
	float GetMontageRate(const UAnimMontage* Montage);

	/** AnimBP의 Slot 노드 이름들(리플렉션으로 AnimNode_Slot 구조체의 SlotName을 읽는다). */
	void GetAnimBlueprintSlots(const UClass* AnimClass, TSet<FName>& OutSlots);
}
