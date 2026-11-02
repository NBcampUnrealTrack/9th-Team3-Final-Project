# 히트스톱 중 시간축 점검: 플레이어 쪽 인계 (#410)

히트스톱(`UknightHitStopComponent`)은 맞은·때린 액터의 `CustomTimeDilation`만 0.02로 낮춥니다.
그래서 몽타주, 이동, 루트 모션은 멈추지만 **월드 타이머(`SetTimer`)와 `AbilityTask_WaitDelay`, `AbilityTask_Repeat`는 그대로 흐릅니다.**
히트스톱 중에 이런 타이머로 잡은 시간이 흐르면, 몸은 멈춰 있는데 다음 단계가 먼저 시작됩니다.

몬스터 쪽은 공격 시계를 몬스터 시간(`Interval × CustomTimeDilation`)으로 세도록 고쳤습니다(`UknightGameplayAbility_MonsterAttack::GetMonsterTimeStep`).
아래는 플레이어 쪽에서 같은 문제가 있는 곳과, 확인해 보니 문제가 없는 곳입니다.

## 고쳐야 하는 곳

### 1. 내려찍기 후딜이 히트스톱만큼 일찍 끝남
- 위치: `knightGameplayAbility_PlungeAttack.cpp`, 착지 처리에서 `PhaseTimer → FinishRecovery`를 `EndRecoveryTime / Rate` 뒤로 잡는 부분.
- 착지 직후 `OnImpact`가 자기 히트스톱(`ImpactSelfHitStop`, 기본 0.08초)을 걸고, 맞은 대상이 있으면 타격 히트스톱이 이를 연장합니다.
- 히트스톱 동안 몽타주는 멈추지만 `PhaseTimer`는 흐르므로, **후딜이 몽타주 기준으로 히트스톱 길이만큼 일찍 잘립니다.** (`EndRecoveryTime`이 0보다 큰 BP에서만 해당)
- 고치는 방법 예:
  - `HeavyAttack`의 균열 타이밍(`CheckReleaseGroundCrackTiming`)처럼 `Montage_GetPosition`이 `EndRecoveryTime`에 닿을 때 끝내기.
  - 또는 짧은 반복 타이머에서 `DeltaTime × CustomTimeDilation`을 누적해 세기(몬스터 쪽 방식).

### 2. 입력 버퍼가 히트스톱 동안에도 줄어듦
- 위치: `knightInputBufferComponent.cpp`의 `IsExpired()`. `World->GetTimeSeconds() - BufferedTime > BufferDuration`(0.5초).
- 히트스톱 중 누른 입력은 몸이 멈춘 동안에도 유효 시간이 깎입니다. 일반 타격(0.08초)은 체감이 작지만 **패링(0.25초)은 버퍼의 절반을 씁니다.**
- 고치는 방법 예: 버퍼 나이를 소유자 시간으로 세기(틱마다 `DeltaTime × CustomTimeDilation` 누적), 또는 히트스톱이 끝날 때 `BufferedTime`을 멈춰 있던 시간만큼 뒤로 미루기.
- 확인할 것: 히트스톱 중 공격을 눌렀을 때 다음 콤보 창에서 그 입력이 쓰이는지 PIE로 확인.

### 3. 공격 돌진이 히트스톱 중에도 미끄러질 수 있음 (낮음)
- 위치: `UknightCombatLibrary::StartAttackLunge` (`MeleeAttack`, `HeavyAttack`에서 호출). 1/120초 타이머가 월드 시간으로 진행률을 계산하고 `SafeMoveUpdatedComponent`로 직접 옮깁니다.
- 돌진이 끝나기 전에 타격이 들어가면 히트스톱 중에도 캐릭터가 계속 미끄러집니다. 보통 돌진은 판정 전에 끝나서 영향이 작습니다.
- 고치는 방법 예: 진행률(`Alpha`)을 월드 시간 대신 몽타주 위치나 소유자 시간으로 계산.

## 확인해 보니 문제가 없는 곳
| 위치 | 이유 |
|---|---|
| `MeleeAttack` 캔슬·콤보 창 (`MoveCancelTimer`, `TrackWarpTimer`) | 창은 AnimNotifyState 태그(`Window.Cancel`, `Window.Combo`)로 열리고 타이머는 그 태그를 확인만 함 |
| `MeleeAttack` `DrawTimer` | 발도 중(타격 전)에만 돎 |
| `MeleeAttack` `GraceTimer` | 어빌리티가 끝난 뒤에 돎 |
| `HeavyAttack` 균열 타이밍 | `Montage_GetPosition`으로 판단 |
| `FknightMechAttackTiming` | `Montage_GetPosition`으로 단계 판단 |

## 확인하지 못한 것
- 타격 이펙트가 히트스톱 중 같이 느려지는지(액터에 붙여 스폰하는지). `knightGameplayCueNotify_Hit` 쪽 확인 필요.
- 위 1~3을 고친 뒤 PIE에서 콤보 손맛 비교.

## 참고: 몬스터 쪽 수정 결과
잔향 몰아치기(Rush) 3걸음에 매번 0.083초 히트스톱이 걸린 상태에서, 각 걸음이 끝나는 몽타주 위치가 2.235 / 2.035 / 2.652초로
타격 없이 쓸 때(2.233 / 2.033 / 2.650초)와 같습니다.
