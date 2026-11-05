#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/knightGameplayAbility.h"
#include "Character/knightPartHealthComponent.h"
#include "knightGameplayAbility_RepairKit.generated.h"

class ACharacter;
class UAnimMontage;
class UAudioComponent;
class UCameraShakeBase;
class UGameplayEffect;
class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class USoundBase;

/** 용접 불꽃이 튀는 자리 하나: 부위 + 본(소켓) 이름. */
USTRUCT(BlueprintType)
struct FknightRepairWeldPoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Repair Kit")
	EknightBodyPart Part = EknightBodyPart::Torso;

	UPROPERTY(EditAnywhere, Category="Repair Kit")
	FName Bone;
};

/**
 * 수리 키트(기획 문서 '부위 체력' 규칙 기준).
 *
 * 사용법: GA_Knight_RepairKit(이 클래스의 BP)를 어빌리티 세트에 InputTag.Item.Use(기본 R 키, IA_UseItem)로 넣는다.
 *
 * 흐름: 누름 → 키트 1개 소모(메카 속성 RepairKitCount, 비용처럼 CheckCost·ApplyCost) → 사용 시간 동안 수리 → 끝나면 회복 → 종료.
 *   사용 시간 = BaseUseTime + 파괴된 부위 수 x TimePerBrokenPart. 시작할 때 정한다.
 *   회복량 = 최대 체력 x HealRatio x 수리 효율(메카 속성 RepairEfficiency).
 *     전체 체력은 회복량만큼(HealEffect), 부위 체력은 같은 양을 체력 비율이 낮은 부위부터(UknightPartHealthComponent::ApplyRepair).
 *
 * 사용 중:
 *   - 걷기만 된다. 달리기 키를 눌러도 걷기 속도(줄지 않음, 무브먼트 SetRunLocked).
 *     공격·가드·퀵스텝은 활성화 그룹(교체 가능 + 캔슬 창 없음) 때문에 나가지 않는다.
 *   - 맞아도 계속된다. State.SuperArmor로 작은 피격 경직이 나오지 않는다.
 *     그로기 붕괴(비틀거림·다운) 반응은 나오지만 수리는 이어서 끝난다. 사망하면 모든 어빌리티가 취소되어 회복하지 않는다.
 * 풀피(전체 체력이 가득 차 있고 깎인 부위도 없음)면 나가지 않는다(키트 낭비 방지).
 * 연출: 수리 중 GameplayCue.Repair.Loop(반복, 끝나면 제거), 회복 순간 GameplayCue.Repair.Finish(한 번).
 *   2026-10-27: GCN_Repair_Loop의 한 번 터지는 이펙트(NS_Hit_Scifi)를 비웠다(시작·끝 두 번 반짝여 끝이 안 보인다는 팀 피드백).
 *     대신 진행도 스캔 링(Scan, 발→머리로 사용 시간 동안 상승)을 코드에서 띄운다.
 *   2026-10-27 추가: 수리 중 부위별 용접(Weld, 깎인 부위 순서대로 불꽃+섬광), 회복 순간 재가동(Reboot, 증기·철컥·음성·흔들림).
 *   둘 다 로컬 연출이다(불꽃·소리는 서버에서도 안전하게 동작, 흔들림·음성은 로컬 플레이어만).
 *   큐 에셋 GCN_Repair_Loop·GCN_Repair_Finish(Content/Knight/Abilities/Cues)는 임시 이펙트(SlashTrail 팩 Scifi). 바꿀 때는 에셋만 바꾸면 된다.
 * 공격 도중에 누르면 캔슬 창이 열릴 때 나간다(입력 버퍼).
 */
UCLASS()
class KNIGHT_API UknightGameplayAbility_RepairKit : public UknightGameplayAbility
{
	GENERATED_BODY()

public:
	UknightGameplayAbility_RepairKit(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 풀피(전체 체력 최대 + 깎인 부위 없음)면 발동하지 않는다. */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** 행동 에너지 비용에 더해 수리 키트가 1개 이상 있는지 본다. 없으면 Ability.ActivateFail.Cost. */
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** 행동 에너지 비용에 더해 수리 키트를 1개 줄인다. CommitAbility에서 불린다. */
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 파괴된 부위가 없을 때의 사용 시간. 기본 1.5초(바뀔 수 있는 값). */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit", meta=(ClampMin=0, Units="s"))
	float BaseUseTime = 1.5f;

	/** 파괴된 부위 하나당 늘어나는 사용 시간. 기본 0.2초 (기획 문서, 바뀔 수 있는 값). 네 부위가 모두 파괴돼 있으면 1.5 + 0.8 = 2.3초. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit", meta=(ClampMin=0, Units="s"))
	float TimePerBrokenPart = 0.2f;

	/** 최대 체력 대비 회복 비율. 기본 0.3 (기획 문서 '최대 체력 30% 회복', 바뀔 수 있는 값). 수리 효율을 곱한다. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit", meta=(ClampMin=0))
	float HealRatio = 0.3f;

	/** 전체 체력 회복 이펙트(Healing += SetByCaller.Healing). 기본은 C++ 회복 이펙트(UknightGameplayEffect_Heal). */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit")
	TSubclassOf<UGameplayEffect> HealEffect;

	/** 선택 사항. 수리 동작 몽타주(걷기와 섞이도록 상체 슬롯 권장). 비어 있으면 동작 없이 사용 시간만 흐른다. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit")
	TObjectPtr<UAnimMontage> RepairMontage;

	// ---- 연출 ⓪ 진행도 스캔 링 (2026-10-27): 발에서 머리까지 사용 시간 동안 올라가는 링. 머리에 닿으면 끝(팀 피드백: 끝나는 시점이 안 보임) ----

	/** 스캔 링. 로컬 공간 반복 이펙트여야 한다(컴포넌트를 올리면 링이 따라감). 기본 NS_Knight_RepairScan(반지름 70). 비어 있으면 생략. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Scan")
	TObjectPtr<UNiagaraSystem> RepairScanEffect;

	/** 링 반지름 = 캡슐 반지름 x 이 값. 이펙트 반지름 70을 기준으로 컴포넌트 XY 크기를 맞춘다. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Scan", meta=(ClampMin=0.1))
	float RepairScanRadiusScale = 1.4f;

	/** 이펙트 에셋의 링 반지름(단위 cm). 에셋을 바꾸면 같이 바꾼다. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Scan", meta=(ClampMin=1))
	float RepairScanEffectRadius = 70.f;

	/** 링이 올라가는 높이 = 캡슐 전체 높이 x 이 값(발바닥 기준). 메시가 캡슐보다 크면 키운다. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Scan", meta=(ClampMin=0.1))
	float RepairScanHeightScale = 1.05f;

	// ---- 연출 ① 부위별 용접 (2026-10-27): 깎인 부위를 체력 비율이 낮은 순서로 돌며 불꽃 + 아크 섬광 ----

	/** 용접 불꽃. 비어 있으면 불꽃 없이 섬광·소리만. 기본 임시값은 NS_Hit_Block(BP에서 지정). */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Weld")
	TObjectPtr<UNiagaraSystem> WeldSparkEffect;

	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Weld", meta=(ClampMin=0))
	float WeldSparkScale = 0.5f;

	/** 용접하는 동안 하나로 이어지는 소리(치지지직). 수리 시작에 틀고 회복 순간 WeldLoopFadeOut초에 걸쳐 끈다.
	 *  반복(Looping) 웨이브면 사용 시간이 길어도 이어진다. 기본 SFX_Knight_RepairWeld_Loop(합성). */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Weld")
	TObjectPtr<USoundBase> WeldLoopSound;

	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Weld", meta=(ClampMin=0))
	float WeldLoopVolume = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Weld", meta=(ClampMin=0, Units="s"))
	float WeldLoopFadeOut = 0.08f;

	/** 선택 사항. 불꽃마다 따로 나는 소리(끊겨 들리므로 보통은 비워 두고 WeldLoopSound를 쓴다). */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Weld")
	TObjectPtr<USoundBase> WeldSound;

	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Weld", meta=(ClampMin=0))
	float WeldSoundVolume = 1.f;

	/** 불꽃 간격(초). 매번 ±WeldIntervalJitter 비율로 흔든다. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Weld", meta=(ClampMin=0.02, Units="s"))
	float WeldInterval = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Weld", meta=(ClampMin=0, ClampMax=0.9))
	float WeldIntervalJitter = 0.5f;

	/** 불꽃 자리. 같은 부위에 여러 개면 매번 무작위로 고른다. 기본값은 생성자(나이트 마네킹식 본). */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Weld")
	TArray<FknightRepairWeldPoint> WeldPoints;

	/** 아크 섬광(점광원) 색. 청백색. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Weld")
	FLinearColor WeldLightColor = FLinearColor(0.55f, 0.75f, 1.f);

	/** 섬광 밝기(칸델라). 매번 50~100% 사이로 깜빡인다. 0이면 섬광 없음. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Weld", meta=(ClampMin=0))
	float WeldLightIntensity = 400.f;

	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Weld", meta=(ClampMin=0))
	float WeldLightRadius = 300.f;

	/** 불꽃 한 번마다 섬광이 켜져 있는 시간. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Weld", meta=(ClampMin=0.01, Units="s"))
	float WeldLightDuration = 0.06f;

	// ---- 연출 ② 재가동 (2026-10-27): 회복 순간 관절 냉각 증기 + 장갑 잠김 철컥 + 파일럿 음성 + 짧은 흔들림 ----

	/** 관절에서 뿜는 냉각 증기. 비어 있으면 생략. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Reboot")
	TObjectPtr<UNiagaraSystem> RebootSteamEffect;

	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Reboot", meta=(ClampMin=0))
	float RebootSteamScale = 1.f;

	/** 증기가 나오는 본. 기본값은 생성자(양 어깨·양 무릎). */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Reboot")
	TArray<FName> RebootSteamBones;

	/** 증기 소리(푸슉). 몸통 위치에서 한 번. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Reboot")
	TObjectPtr<USoundBase> RebootSteamSound;

	/** 장갑 잠김 소리(철컥). 증기 뒤 RebootClankDelay초. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Reboot")
	TObjectPtr<USoundBase> RebootClankSound;

	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Reboot", meta=(ClampMin=0))
	float RebootSoundVolume = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Reboot", meta=(ClampMin=0, Units="s"))
	float RebootClankDelay = 0.12f;

	/** 파일럿 음성(오디오 서브시스템 음성 큐로 재생, 다른 음성과 겹치지 않음). 기본 임시값 VO_Pilot_SystemActivated. */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Reboot")
	TObjectPtr<USoundBase> RebootVoice;

	/** 짧은 카메라 흔들림(로컬 플레이어만). */
	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Reboot")
	TSubclassOf<UCameraShakeBase> RebootCameraShake;

	UPROPERTY(EditDefaultsOnly, Category="Repair Kit|Reboot", meta=(ClampMin=0))
	float RebootCameraShakeScale = 0.4f;

private:
	/** 사용 시간이 끝났을 때: 회복을 적용하고 끝낸다. */
	UFUNCTION()
	void OnRepairFinished();

	/** 수리 시작 때 용접 순서(깎인 부위, 체력 비율 낮은 순)를 정하고 첫 불꽃을 예약한다. */
	void StartWeld(ACharacter* Character, float UseTime);
	void PlayWeldSpark();
	void ScheduleNextWeldSpark();
	void StopWeld();

	/** 스캔 링: 발에서 시작해 UseTime 동안 머리까지 올린다. */
	void StartScan(ACharacter* Character, float UseTime);
	void UpdateScan();
	void StopScan();

	/** 회복 순간 재가동 연출. 어빌리티가 끝난 뒤에도 남아야 하는 철컥 소리는 월드 타이머로 따로 예약한다. */
	void PlayReboot(ACharacter* Character);

	bool bRunLockApplied = false;

	TArray<EknightBodyPart> WeldOrder;
	double WeldStartTime = 0.0;
	float WeldSlotTime = 0.f;
	FTimerHandle WeldSparkTimer;
	FTimerHandle WeldLightTimer;

	double ScanStartTime = 0.0;
	float ScanDuration = 0.f;
	float ScanBottomZ = 0.f;
	float ScanHeight = 0.f;
	FTimerHandle ScanTimer;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ScanComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> WeldLight;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WeldLoopAudio;
};
