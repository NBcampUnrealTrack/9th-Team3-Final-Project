# 번역 작업 방법

원문은 한국어(`ko`), 번역 언어는 영어(`en`)·일본어(`ja`)·중국어 간체(`zh-Hans`)·중국어 번체(`zh-Hant`)입니다.
번역은 **`Knight_Localization.xlsx`** 시트 하나에서 관리합니다. 엑셀이나 구글 시트에서 열어도 됩니다.

```
게임 문구(코드 LOCTEXT · 에셋의 Text 칸)
  → loc.bat gather   문구 수집 → Content/Localization/Game/<언어>/Game.po → 시트에 반영
  → 시트에서 번역·검수
  → loc.bat check    검사
  → loc.bat import   시트 → .po → 언리얼 Import → Compile(.locres)
```

엔진 안에 들어 있는 Python을 쓰기 때문에 따로 설치할 것은 없습니다.
엔진을 `C:\Program Files\Epic Games\UE_5.8`가 아닌 곳에 설치했다면 `UE_ROOT` 환경 변수에 엔진 폴더를 넣으세요.

## 명령

| 명령 | 하는 일 | 시간 |
|---|---|---|
| `loc.bat gather` | 게임 문구를 다시 모으고 시트에 반영합니다. 시트의 번역·상태·메모는 남습니다. | 1~2분 |
| `loc.bat export` | 언리얼 수집 없이 `.po` → 시트 반영만 합니다. | 몇 초 |
| `loc.bat check` | 번역을 검사해 콘솔에 출력하고, 시트의 `검사` 칸을 갱신합니다. | 몇 초 |
| `loc.bat import` | 시트 → `.po`로 쓰고 언리얼 Import + Compile까지 합니다. | 1~2분 |

- 시트를 **엑셀에서 닫은 뒤** 실행하세요(열려 있으면 저장하지 못합니다).
- `gather`·`import`는 에디터가 켜져 있어도 됩니다. 결과는 에디터를 다시 열면 반영됩니다.
- 언리얼 로그: `Saved/Logs/Localization.log`

## 시트 구성

### `번역` 탭 — 한 줄 = 문구 하나

| 칸 | 누가 채우나 | 설명 |
|---|---|---|
| ID | 자동 | 언리얼 문구 키(`네임스페이스,키`). **고치지 마세요.** |
| 위치 | 자동 | 코드 파일 줄 또는 에셋 경로 |
| 원문(ko) | 자동 | 게임 원문. 바꾸려면 코드·에셋에서 바꾸고 `gather` |
| 맥락 | 기획·개발 | 번역가에게 줄 설명. 예: "버튼", "보스 이름", "{0} = 부위 이름" |
| 최대 길이 | 기획·UI | 글자 수 상한. 비우면 검사 안 함 |
| `<언어>` 번역 | 번역가 | 번역문 |
| `<언어>` 상태 | 번역가·검수자 | 아래 상태 중 하나(드롭다운) |
| 검수자 · 메모 | 검수자 | 자유롭게 |
| 검사 | 자동 | `check` 결과 |

### 상태

| 상태 | 뜻 |
|---|---|
| 미번역 | 번역 없음 |
| 번역됨 | 번역했고 검수 전 |
| 검수됨 | 다른 사람이 문장을 확인함 |
| 게임 확인됨 | 게임 화면에서 잘림·맥락까지 확인함 |
| 원문 변경됨 | `gather` 때 원문이 바뀐 줄. 예전 번역은 게임에 그대로 들어가니 다시 번역하고 상태를 바꾸세요 |

### `용어집` 탭
게임 용어의 언어별 고정 번역(스탠스, 그로기, 잔열 등). 원문에 용어가 있는데 번역에 고정 번역이 없으면 `check`에서 알려 줍니다.

### `요약` 탭
언어별 상태 개수와 진행률. `export`·`check` 때마다 다시 계산됩니다.

### `보관` 탭
게임에서 사라진 문구 중 번역이 있던 것. 키 이름이 바뀐 경우 여기서 번역을 옮겨 쓰면 됩니다.

## 검사 항목 (`check`)
- 자리표시자 `{0}`, `{Damage}` 가 원문과 다름
- 서식 태그 `<b>…</>` 가 원문과 다름
- 줄바꿈 수가 다름, 앞뒤 공백
- 최대 길이 초과
- 용어집과 다른 번역
- 상태가 `검수됨`·`게임 확인됨`인데 번역이 빔

## 게임에서 확인
- 에디터: 툴바 **Preview Game Language**로 언어를 바꿔 PIE
- 실행 인자: `-culture=en` (또는 `ja`, `zh-Hans`, `zh-Hant`)
- 게임 안 설정창의 **언어** 항목

## 개발자 참고
- 화면에 보이는 문구는 `LOCTEXT` / `NSLOCTEXT` 또는 에셋의 `FText` 칸으로 만들어야 수집됩니다. `FText::FromString`, `INVTEXT`는 수집되지 않습니다.
- **스크립트·MCP·에디터 도구로 에셋에 넣은 텍스트도 빠집니다.** Python에서 문자열을 FText 칸에 넣거나, 위젯 생성 도구가 `INVTEXT`를 쓰면 '번역 안 함'으로 저장됩니다. 에디터에서 직접 입력한 텍스트는 괜찮습니다.
  - 찾기·고치기: 에디터 콘솔 `py`에서 `unreal.knightLocalizationEditorLibrary.find_unlocalizable_texts(에셋, False)`로 목록을 보고, `True`로 고친 뒤 저장(위젯 블루프린트는 컴파일 후 저장)합니다. 한글이 든 텍스트에만 번역 키를 붙이고 글자는 바꾸지 않습니다.
  - 게임에서 한국어 그대로 나오는 곳이 있으면 먼저 이 도구로 그 에셋을 확인하세요.
- 숫자·이름이 들어가는 문장은 이어 붙이지 말고 `FText::Format(LOCTEXT("Key", "{0} 피해"), ...)`를 쓰세요. 언어마다 어순이 다릅니다.
- 같은 네임스페이스·키에 다른 문장을 쓰면 `gather` 때 "Text conflict" 경고가 납니다. 키를 다르게 지어 주세요.
- 수집 범위는 `Config/DefaultEditor.ini`의 `[/Script/Localization.LocalizationSettings]`(에디터 Localization Dashboard)와 `Config/Localization/Game_*.ini`에 있습니다. Dashboard에서 설정을 바꾸면 `Game_*.ini`가 다시 만들어집니다.
  - 코드: `Source/knight`, `Source/knightUI`, `Plugins/knightInventory/Source` (`Tests` 폴더 제외)
  - 에셋: `Content/Knight`, `Content/Input` (`Content/Knight/Dev` 제외)
