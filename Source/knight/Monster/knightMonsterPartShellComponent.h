#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Monster/knightMonsterPartComponent.h"
#include "knightMonsterPartShellComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USkeletalMeshComponent;

/**
 * 부위 와이어프레임 셸. 부위 데이터의 PartShellMesh(몬스터 메시를 단순화해 파괴 가능한 부위만 남긴 메시)를 몬스터 메시에
 * 리더 포즈로 붙여, 몸 윤곽을 따라가는 각진 선으로 부위를 그린다. 선 색이 부위 상태(정상 · 빈사 · 파괴)를 보여 준다.
 *
 * 평소에는 락온한 부위 하나만 보인다. 파괴할 수 없는 부위는 셸이 없다. 스캔이 몬스터에 닿으면 RevealTime 동안
 * 모든 부위 셸이 벽 너머로도 보인다. 첫 조우(처음 플레이어를 타깃으로 잡을 때)에 그 자리에서 스캔을 쏜다.
 *
 * 셸 메시는 knightPartShellBaker.BakePartShell로 굽는다. 머티리얼은 부위 번호(UV1.x)로 부위마다 불투명도와 색을 고른다.
 */
UCLASS(ClassGroup=(Knight), meta=(BlueprintSpawnableComponent))
class KNIGHT_API UknightMonsterPartShellComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UknightMonsterPartShellComponent();

	/** 락온한 부위. 셸이 없는 부위(파괴 불가)나 빈 태그면 락온 셸을 끈다. */
	void SetLockedPart(FGameplayTag PartTag);

	/** 모든 부위 셸을 RevealTime 동안 벽 너머로도 보인다. 스캔 링이 몬스터에 닿을 때 부른다. */
	void PlayScanReveal();

	/** 테스트용: 셸 부위 순서(PartShellParts)의 Index번 부위를 락온한 것처럼 켠다. 범위 밖이면 끈다. */
	void DebugLockShellPart(int32 Index);

	/** 테스트용: 셸 머티리얼 스칼라 값을 바꾼다(Style, BandDensity, LineWidth 등). */
	void DebugSetMaterialScalar(FName Name, float Value);

	/** Origin 위치에서 스캔을 쏜다. 첫 조우에 자동으로 부르며, 테스트용으로 직접 불러도 된다. */
	UFUNCTION(BlueprintCallable, Category="Knight|Monster")
	void StartScanFrom(const AActor* Origin);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 깊이 테스트를 하는 셸 머티리얼(락온). */
	UPROPERTY(EditAnywhere, Category="Knight|PartShell")
	TSoftObjectPtr<UMaterialInterface> ShellMaterial;

	/** 깊이 테스트를 끈 셸 머티리얼(스캔). 파라미터는 ShellMaterial과 같다. */
	UPROPERTY(EditAnywhere, Category="Knight|PartShell")
	TSoftObjectPtr<UMaterialInterface> ShellXRayMaterial;

	/** 스캔 링 포스트 프로세스 머티리얼. 파라미터: ScanOrigin, ScanRadius, ScanAlpha. */
	UPROPERTY(EditAnywhere, Category="Knight|PartShell")
	TSoftObjectPtr<UMaterialInterface> ScanMaterial;

	/** 발광 색(HDR). 빨강이 '위험'으로 읽히게, 멀쩡한 부위는 청록이다. */
	UPROPERTY(EditAnywhere, Category="Knight|PartShell")
	FLinearColor NormalColor = FLinearColor(0.1f, 2.4f, 3.f);

	UPROPERTY(EditAnywhere, Category="Knight|PartShell")
	FLinearColor CriticalColor = FLinearColor(4.f, 0.35f, 0.1f);

	UPROPERTY(EditAnywhere, Category="Knight|PartShell")
	FLinearColor BrokenColor = FLinearColor(0.35f, 0.36f, 0.4f);

	/** 셸을 몸 표면에서 띄우는 거리(월드 cm, 법선 방향). 선이 살에 묻히지 않을 만큼만. */
	UPROPERTY(EditAnywhere, Category="Knight|PartShell", meta=(ClampMin=0, Units="cm"))
	float SurfaceOffset = 6.f;

	/** 선 굵기(화면 픽셀). */
	UPROPERTY(EditAnywhere, Category="Knight|PartShell", meta=(ClampMin=0.1))
	float LineWidth = 1.5f;

	/** 면 채움 불투명도. 0이면 선만. */
	UPROPERTY(EditAnywhere, Category="Knight|PartShell", meta=(ClampMin=0, ClampMax=1))
	float FillOpacity = 0.05f;

	/** 스캔으로 모든 부위를 보여 주는 시간(페이드 아웃 포함). */
	UPROPERTY(EditAnywhere, Category="Knight|PartShell", meta=(ClampMin=0, Units="s"))
	float RevealTime = 3.f;

	UPROPERTY(EditAnywhere, Category="Knight|PartShell", meta=(ClampMin=0.01, Units="s"))
	float FadeInTime = 0.12f;

	UPROPERTY(EditAnywhere, Category="Knight|PartShell", meta=(ClampMin=0.01, Units="s"))
	float FadeOutTime = 0.6f;

	/** 빈사 깜빡임 한 주기. */
	UPROPERTY(EditAnywhere, Category="Knight|PartShell", meta=(ClampMin=0.1, Units="s"))
	float CriticalBlinkPeriod = 0.8f;

	UPROPERTY(EditAnywhere, Category="Knight|PartShell", meta=(ClampMin=0, ClampMax=1))
	float CriticalBlinkMinOpacity = 0.3f;

	/** 첫 조우에 스캔을 쏜다. */
	UPROPERTY(EditAnywhere, Category="Knight|PartShell|Scan")
	bool bScanOnEngage = true;

	/** 스캔 링이 퍼지는 속도. */
	UPROPERTY(EditAnywhere, Category="Knight|PartShell|Scan", meta=(ClampMin=1, Units="cm/s"))
	float ScanSpeed = 9000.f;

	/** 이 반경까지 퍼지면 링이 사라진다(맵 전역). */
	UPROPERTY(EditAnywhere, Category="Knight|PartShell|Scan", meta=(ClampMin=1, Units="cm"))
	float ScanMaxRadius = 60000.f;

private:
	/** 셸 메시의 부위 하나(PartShellParts 순서). */
	struct FShellPart
	{
		FGameplayTag Tag;
		EknightPartCondition Condition = EknightPartCondition::Normal;
		float Opacity = 0.f;
	};

	UFUNCTION()
	void HandleTargetChanged(AActor* OldTarget, AActor* NewTarget);
	UFUNCTION()
	void HandlePartConditionChanged(FGameplayTag PartTag, EknightPartCondition NewCondition);

	/** 셸 컴포넌트를 아직 안 만들었으면 만든다. 셸 메시가 없으면 false. */
	bool EnsureBuilt();
	void SetXRay(bool bInXRay);
	void ApplyColors();
	FLinearColor GetConditionColor(EknightPartCondition Condition) const;
	UMaterialInstanceDynamic* GetActiveMaterial() const;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> ShellMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> XRayMaterial;

	TArray<FShellPart> Parts;
	FGameplayTag LockedPart;
	float RevealRemaining = 0.f;
	float BlinkTime = 0.f;
	bool bBuilt = false;
	bool bXRay = false;
	bool bScanned = false;
};
