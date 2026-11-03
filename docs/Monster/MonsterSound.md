# 몬스터 사운드 교체 가이드 (잔열 · 잔향)

잔열·잔향 소리를 새 음원으로 바꿀 때 보는 문서. 에셋 이름과 연결은 그대로 두고 **음원 파일만 바꿔서 다시 가져오기(Reimport)** 하는 것이 기본이다.

- 에셋: `/Game/Knight/Creatures/Janyeol/Sounds`, `/Game/Knight/Creatures/Janhyang/Sounds`
- 원본 wav: `Tools/Sound/Janyeol/CRT_Janyeol_*.wav`, `Tools/Sound/Janhyang/CRT_Janhyang_*.wav` (git에 올리지 않음)
- 이름 규칙: `S_<몬스터>_<이름>` ← `CRT_<몬스터>_<이름>.wav`, 여러 개 중 무작위면 `_01`, `_02` … + 큐 `SC_<몬스터>_<이름>`

---

## 1. 교체 순서

1. 새 wav를 원본 경로에 **같은 파일 이름으로** 덮어쓴다.
2. 콘텐츠 브라우저에서 해당 `S_` 에셋(폴더째 여러 개 선택해도 됨) 우클릭 → **Reimport**.
3. PIE에서 해당 동작을 확인한다(`knight.Monster.Attack <이름>`으로 원하는 공격을 바로 쓸 수 있다).

모든 `S_` 에셋의 원본 경로는 프로젝트 기준 상대 경로(`../../../../../Tools/Sound/...`)로 저장돼 있어서, 저장소 위치가 달라도 Tools 폴더에 파일만 있으면 Reimport가 된다.

예외로 원본 파일 이름이 다른 것:

| 에셋 | 원본 파일 |
|---|---|
| S_Janyeol_Slam_01 / _02 | `Tools/Sound/TestSound_Janyeol_Slam_1.wav` / `_2.wav` (예전 임시 소리. 팀 공용 타격음 SC_Janyeol_Slam에서 씀) |
| S_Janhyang_Sonic_Tinnitus | `CRT_Janhyang_Sonic_Tinnitus_3s.wav` |
| S_Janhyang_WingFlap_Hover | `CRT_Janhyang_WingFlap_Hover_Loop.wav` |

## 2. Reimport해도 유지되는 것 / 다시 봐야 하는 것

유지됨 (에셋에 저장된 값):
- 볼륨·반복(Looping)·사운드 클래스(SC_Knight_SFX)·감쇠(ATT_Monster_Voice)
- 노티파이 위치와 노티파이 볼륨, 큐 구성, 코드 슬롯 연결

새 음원이 아래에 해당하면 손봐야 한다:

| 바뀐 점 | 할 일 |
|---|---|
| 앞쪽 무음 길이가 다름 | 타격음 노티파이는 "발이 땅에 닿는 프레임 − wav 앞 무음"으로 맞춰 둠. 소리 시작이 늦어지면 노티파이를 그만큼 당긴다 |
| 변형 개수가 늘거나 줄어듦 | `S_..._03`을 더 가져오고 해당 `SC_` 큐의 Random 노드에 연결(빼면 반대로) |
| 단일 소리에 변형이 생김 | 큐 `SC_`를 새로 만들어 기존 `S_` 대신 연결(노티파이·데이터 에셋·GA 슬롯) |
| 반복용 소리 | 처음과 끝이 이어지게(seamless) 만든다. `_Loop` 에셋은 Looping이 켜진 채로 둔다 |
| 음량 차이 | 원본을 비슷한 크기로 맞춰 온다. 개별 조정값은 아래 표의 "현재 볼륨" |

## 3. 반복 소리와 현재 볼륨

| 에셋 | 쓰는 곳 | 비고 |
|---|---|---|
| S_Janyeol_Body_Heat_Loop | DA_Janyeol LoopSounds | 살아 있는 동안 계속. 볼륨 0.45 |
| S_Janyeol_Idle_Growl_Loop | DA_Janyeol LoopSounds | 교전 중 3초 들리고 6~10초 쉼(BurstTime/RestTime). 볼륨 0.5 |
| S_Janyeol_Breath_Fire_Loop | AM_Janyeol_Breath (Knight Loop Sound 구간) | 브레스 분출 동안 |
| S_Janyeol_Charge_Run_Loop | GA_Janyeol_Charge (RushEffects.LoopSound) | 돌진 동안 |
| S_Janhyang_Tornado_Chase_Loop | GA_Janhyang_Tornado (LoopSound) | 토네이도가 쫓는 동안 |
| S_Janhyang_WingFlap_Hover | BP_Janhyang (AirTurn HoverSound) | 반복 아님. 1.5초마다 다시 틂 |

그 밖의 볼륨: 큐 `SC_` 전부 0.75, 잔열 발소리 노티파이 0.6, Burn_Sizzle 0.6, **포효 노티파이 1.5**(잔열 Roar_Engage·Roar_Enrage, 잔향 Roar_Encounter·Roar_GlideAlert·Roar_Resonance).

그로기(다운) 울부짖음: 다운 몽타주 시작(0초)에 크게 겹쳐 낸다. 그로기를 만든 손맛을 주는 소리라 다른 소리보다 크다.
- 잔열 `AM_Janyeol_Down`: Roar_Enrage(볼륨 1.5, 피치 1.15) + 기존 Hit_Moan. 이어서 Down_Collapse 1.65초, GroggyBreath 2.3초
- 잔향 `AM_Janhyang_Down`: Hit_Screech_01(볼륨 1.8, 피치 0.75) + Hit_Screech_02(0.15초, 볼륨 1.2, 피치 0.9). GroggyBreath는 울부짖음과 겹치지 않게 0.1 → 1.8초로 옮김
- 전용 음원이 생기면 이 노티파이들을 그 소리 하나로 바꾼다.

잔향 근접 공격 소리 (기존 음원 재사용, 전용 음원이 생기면 교체):
- `AM_Janhyang_Bite`: 잔열 SC_Janyeol_Bite_Snap(1.02초, 피치 1.2) — 머리가 멈추는 순간
- `AM_Janhyang_ShoulderSlam`: SC_Janhyang_WingFlap(0.05초) + Exhaust_Landing_Thud(0.70초, 볼륨 1.3) — 몸이 내려꽂히는 순간
- `AM_Janhyang_WingStrike`: 날갯짓(0.35초)에 SC_Janhyang_TailStab(0.74초) 추가 — 꼬리가 앞으로 찌르는 순간

타격음 시간을 정할 때는 Play Rate 구간을 풀어 실제 시간으로 바꾼 뒤, wav 앞 무음을 빼서 소리가 커지는 순간을 접지·타격 프레임에 맞춘다.

## 4. 노티파이가 아닌 곳에 연결된 소리

노티파이는 몽타주를 열면 보이지만, 아래는 데이터 에셋·GA·BP의 슬롯에 들어 있다.

| 에셋 | 슬롯 |
|---|---|
| S_Janyeol_GroundFire_Beat / Eruption / Rumble | GA_Janyeol_GroundFire |
| SC_Janyeol_BackSpike_Fall / Impact | GA_Janyeol_BackSpike (가시 비 Fall/ImpactSound) |
| SC_Janyeol_Part_Break, S_Janyeol_Tail_Drop | DA_Janyeol_Parts (BreakSound, BreakPieceLandSound). Part_Break는 DA_Recipe_Janyeol에도 있음 |
| SC_Janyeol_Burn_Sizzle | GCN_Burn_Loop (플레이어 화상) |
| S_Janhyang_Sonic_Shot | GA_Janhyang_Sonic, GA_Janhyang_SonicShock (FireSound) |
| S_Janhyang_Sonic_Breath | GA_Janhyang_SonicBreath |
| S_Janhyang_Tornado_Spawn | GA_Janhyang_Tornado (SpawnSound) |
| S_Janhyang_Chest_Core_Break | DA_Janhyang_Parts, DA_Recipe_Janhyang (BreakSound) |
| SC_Janhyang_DarkEcho | GA_Janhyang_DarkAmbush, GA_Janhyang_Glide (DarkEchoSound) |
| S_Janhyang_Sonic_Tinnitus | 코드에서 경로로 불러옴(청각 마비). 이름을 바꾸면 `knightCameraModifier_Deafen.cpp`도 바꿔야 함 |

## 5. 잔향 어둠 초음파 (SC_Janhyang_DarkEcho)

어둠이 덮인 동안 플레이어 둘레 아무 방향(800~2000cm)에서 0.4~0.9초마다 초음파를 하나씩 튼다. 소리 길이(4초)보다 간격이 짧아 여러 개가 겹쳐 사방에서 들린다. 위치 단서는 날갯짓(CircleFlapSound)이 따로 맡는다.

- 지금은 초음파 브레스 소리(S_Janhyang_Sonic_Breath, 박쥐 울음)를 피치 0.9~1.3배, 볼륨 0.4로 쓴다. 전용 음원이 생기면 큐의 Random 노드만 바꾸면 된다(여러 개 넣으면 무작위로 섞임).
- 간격·거리·피치·볼륨은 GA의 Attack|Darkness 항목 DarkEcho* 값으로 조절한다.
