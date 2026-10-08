# UI 입력 요청 사항 (입력 담당 → UI)

UI 화면(확인창, 일시정지, 설정, 메뉴)을 키보드+마우스로 안정적으로 조작하기 위한 입력 설정 요청입니다.
버튼 선택(화살표)은 엔진 기본 동작입니다. 누르기(Enter / Space)는 UI 쪽에서
`DefaultGame.ini`의 `[/Script/CommonUI.CommonUISettings] CommonButtonAcceptKeyHandling=TriggerClick`으로 켜 두었습니다
(엔진 기본값 Ignore는 Enter를 입력 액션에 맡기는데, 이 프로젝트에는 그 액션이 없습니다). 아래는 그 외 부분입니다.

---

## 1. CommonUI 뷰포트 클라이언트 지정 (P0)

**문제**
메뉴가 열려 있어도 메뉴가 처리하지 않은 키는 게임 뷰포트로 넘어갑니다 (UMG 위젯이 게임 뷰포트 안에 있는 엔진 구조 때문).
예: 게임을 멈추지 않는 확인창이 떠 있을 때 WASD로 캐릭터가 움직일 수 있습니다. (실제 재현은 아직 확인 전)

**요청**
`Config/DefaultEngine.ini`

```ini
[/Script/Engine.Engine]
GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient
```

**효과**
UI가 "메뉴 입력" 상태를 요청하면 게임 입력이 자동으로 막히고, 닫으면 다시 열립니다.

**확인할 것**
- 전투 입력(공격, 가드, 부스트, 락온)이 평소처럼 동작하는지
- Esc 설정창, F6 복귀창이 그대로 동작하는지

---

## 2. UI 공용 입력 액션 2개 + CommonUI 입력 데이터 (P1)

**문제**
지금 확인창의 Esc(취소)는 위젯마다 직접 키를 검사합니다. 화면이 늘어나면 같은 코드가 반복되고, 나중에 키를 바꾸거나 패드를 붙이기 어렵습니다.

**요청**
- Enhanced Input 액션 2개: `IA_UI_Confirm`(Enter, Space), `IA_UI_Back`(Esc)
- CommonUI 입력 데이터 에셋 1개(`UCommonUIInputData` 상속 BP)에 위 두 액션을 기본 클릭 / 뒤로가기로 지정
- Project Settings > Game > Common Input Settings > Input Data 에 그 에셋 지정
- Common Input Settings의 Enhanced Input 지원 켜기

**효과**
모든 화면이 같은 "뒤로가기"를 쓰고, UI 쪽은 위젯별 Esc 검사 코드를 지울 수 있습니다.

---

## 3. Esc 처리 정리 (P1, 일시정지 화면 작업과 함께)

**현재**
`AknightPlayerController::SetupInputComponent`가 Esc 키를 직접 묶어(`BindKey(EKeys::Escape)`) 설정창을 엽니다.

**요청**
일시정지 화면(#45)이 들어오면 Esc는 "일시정지 열기"가 됩니다. 아래 중 편한 방식으로 바꿔 주세요.
- 전투 IMC에 `IA_Pause`(Esc)를 추가하고, PC는 그 액션으로 UI의 일시정지 열기를 호출
- 또는 UI가 PC 코드를 직접 수정 (UI C++ 담당이 UI 쪽이므로 협의만 되면 UI에서 처리 가능)

메뉴가 열려 있는 동안의 Esc는 2번의 `IA_UI_Back`이 처리합니다.

---

## 우선순위 요약

| 항목 | 우선순위 | 이유 |
|---|---|---|
| 1. 뷰포트 클라이언트 | P0 | 메뉴 뒤로 게임 입력이 새는 문제 |
| 2. UI 입력 액션 + 입력 데이터 | P1 | 화면이 늘어나기 전에 공용화 |
| 3. Esc 정리 | P1 | 일시정지 화면(#45)과 같이 |
