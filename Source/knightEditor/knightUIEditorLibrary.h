#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightUIEditorLibrary.generated.h"

class UCommonButtonStyle;
class UCommonTextStyle;
class UTexture2D;
class UUserWidget;
class UWidgetBlueprint;

/**
 * UI 위젯 블루프린트의 디자이너 트리를 코드로 만드는 에디터 도구. Python(unreal.knightUIEditorLibrary)에서 부른다.
 *
 * 만든 트리는 디자이너에서 직접 만든 것과 같아서, 이후에는 Hierarchy 패널에서 자유롭게 고칠 수 있다.
 * 디자이너 작업을 덮어쓰지 않도록 트리가 이미 있는 블루프린트는 건드리지 않는다. 컴파일과 저장은 호출한 쪽이 한다.
 * 위젯 이름은 각 C++ 부모의 BindWidgetOptional 이름과 같아야 한다.
 */
UCLASS()
class UknightUIEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** WBP_KnightButton: Overlay 아래 Txt_Label(가운데)과 Bracket_L/Bracket_R(양 끝). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildKnightButtonLayout(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> BracketClass, TSubclassOf<UCommonTextStyle> LabelStyle);

	/** WBP_KnightPrimaryLayout: 화면을 채우는 Layer_Game, Layer_GameMenu, Layer_Menu, Layer_Modal 스택 (아래에서 위 순서). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildPrimaryLayout(UWidgetBlueprint* WidgetBlueprint);

	/** WBP_HUDPlayerStatus: 화면 전체 캔버스. 왼쪽 아래 체력(HP 캡션, 큰 현재값, /최대값, 고스트 바), 아래 가운데 부스트(라벨/과열/수치, 3등분 바, 낮을 때 빨간 테두리). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDPlayerStatus(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> WarningStyle,
		TSubclassOf<UCommonTextStyle> NumberStyle = nullptr);

	/** WBP_HUDRaidTimer: OBJECTIVE 캡션, 목표(Txt_Objective)와 남은 시간(Txt_Time) 한 줄, 밑줄. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDRaidTimer(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> TimeStyle, TSubclassOf<UCommonTextStyle> CaptionStyle,
		TSubclassOf<UCommonTextStyle> ObjectiveStyle = nullptr);

	/**
	 * WBP_HUDLockOnMarker: 캔버스 직속의 Box(주황 귀퉁이), PartFrame/PartFlash(부위 틀, PartFrameTexture가 있을 때), Line_Diagonal, Line_Shelf,
	 * Plate(TARGET LOCK + 이름 + 부위 이름), Stagger(경직 게이지), Txt_Distance. 위치는 C++가 매 프레임 정한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDLockOnMarker(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> NameStyle,
		UTexture2D* PartFrameTexture = nullptr, TSubclassOf<UCommonTextStyle> StaggerCaptionStyle = nullptr);

	/** WBP_HUDLayout: 화면 전체의 락온 마커(맨 아래)와 플레이어 패널(체력 왼쪽 아래, 부스트 아래 가운데), 세이프존 안쪽 왼쪽 위에 목표/타이머. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDLayout(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> PlayerStatusClass, TSubclassOf<UUserWidget> RaidTimerClass, TSubclassOf<UUserWidget> LockOnMarkerClass);

	/** 이름으로 위젯 하나를 자식째 지운다. 이미 만든 블루프린트에서 요소를 빼낼 때 쓴다. 없으면 false. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool RemoveWidget(UWidgetBlueprint* WidgetBlueprint, FName WidgetName);

	/** WBP_KnightConfirmModal: 화면을 덮는 어두운 막(클릭 차단), 가운데 패널(제목, 본문, 오른쪽 아래 [취소] [확인]). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildConfirmModal(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> ButtonClass, TSubclassOf<UCommonButtonStyle> CancelButtonStyle,
		TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> BodyStyle);

	/** WBP_KnightPauseMenu: 어두운 막 + 왼쪽 세로 패널(SYSTEM PAUSE, 제목, 계속하기 / 설정 / 레이드 포기, Esc 안내). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildPauseMenu(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> ButtonClass, TSubclassOf<UCommonButtonStyle> SecondaryButtonStyle,
		TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> CaptionStyle);

	/** WBP_KnightRaidResult: 어두운 막 + 가로 배너(성공/실패) + 가운데 보고 패널(시간, 최고 기록, 부위 파괴, 보상, 귀환 카운트다운, 지금 귀환). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildRaidResult(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> ButtonClass, TSubclassOf<UCommonTextStyle> DisplayStyle,
		TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> SubheadStyle, TSubclassOf<UCommonTextStyle> BodyStyle,
		TSubclassOf<UCommonTextStyle> BodyDimStyle, TSubclassOf<UCommonTextStyle> NumberStyle, TSubclassOf<UCommonTextStyle> CaptionStyle);

	/** WBP_KnightSettingRow_*: 왼쪽 항목 이름 + 미적용 점 + 오른쪽 조작부. ControlType 0 = 회전식(‹ 값 ›), 1 = 슬라이더, 2 = 키. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildSettingRow(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> BracketClass, int32 ControlType,
		TSubclassOf<UCommonTextStyle> LabelStyle, TSubclassOf<UCommonTextStyle> ValueStyle);

	/** WBP_KnightSettingsTab: 선택 표시(주황 세로선 + 옅은 바탕) + 영문 캡션 + 이름. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildSettingsTab(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> LabelStyle);

	/** WBP_KnightSettingsSection: 섹션 띠 ("VOLUME"). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildSettingsSectionHeader(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle);

	/** WBP_KnightSettings: 머리(제목, 미적용 개수) / 왼쪽 분류 / 가운데 항목 / 오른쪽 설명 / 아래 키 안내, 상태, 버튼. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildSettingsScreen(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> ButtonClass, TSubclassOf<UUserWidget> TabClass,
		TSubclassOf<UCommonButtonStyle> SecondaryButtonStyle, TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> SubheadStyle,
		TSubclassOf<UCommonTextStyle> BodyDimStyle, TSubclassOf<UCommonTextStyle> CaptionStyle);

	/** WBP_KnightMenuItem: 주황 세로 막대(Bracket_L) + 글자(Txt_Label). 배경 없는 글자 메뉴 항목. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildKnightMenuItem(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> LabelStyle);

	/** WBP_KnightSaveSlotCard: 세로 카드(이름, 최근 플레이 표시, 기록 네 줄 / 빈 슬롯 안내) + 포커스 주황 틀. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildSaveSlotCard(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> TitleStyle,
		TSubclassOf<UCommonTextStyle> SubheadStyle, TSubclassOf<UCommonTextStyle> BodyDimStyle, TSubclassOf<UCommonTextStyle> NumberStyle);

	/** WBP_KnightTitleMenu: 왼쪽 로고 + 글자 메뉴(오른쪽은 3D 화면), 저장 파일 선택(카드 3장), 설정 자리. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildTitleMenu(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> MenuItemClass, TSubclassOf<UUserWidget> ButtonClass,
		TSubclassOf<UCommonButtonStyle> SecondaryButtonStyle, TSubclassOf<UUserWidget> SaveCardClass, UTexture2D* Logo, UTexture2D* Gradient,
		TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> BodyDimStyle);

	/** WBP_KnightMainMenu: 로비 글자 메뉴, 지구본 스테이지 선택(가운데 지구본 + 옆 정보 패널), 정비/설정/도감 자리. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildMainMenu(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> MenuItemClass, TSubclassOf<UUserWidget> ButtonClass,
		TSubclassOf<UCommonButtonStyle> SecondaryButtonStyle, TSubclassOf<UUserWidget> GlobeClass, UTexture2D* Gradient,
		TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> BodyStyle, TSubclassOf<UCommonTextStyle> BodyDimStyle,
		TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> WarningStyle, TSubclassOf<UCommonTextStyle> NumberStyle);

	/** WBP_KnightLoadingScreen: 검은 배경, 가운데 마크와 팁 한 줄, 오른쪽 아래 점 세 개. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildLoadingScreen(UWidgetBlueprint* WidgetBlueprint, UTexture2D* Mark, TSubclassOf<UCommonTextStyle> TipStyle);

	/**
	 * 기존 WBP_KnightTitleMenu에 시작 로고 화면(Panel_Intro: 검은 막 + 팀 로고 + 엔진 로고, 맨 위)과 메뉴 자리의 Txt_PressKey를 더한다.
	 * 나머지 트리는 건드리지 않는다. 다시 부르면 두 위젯을 지우고 새로 만든다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool AddTitleIntro(UWidgetBlueprint* WidgetBlueprint, UTexture2D* StudioLogo, UTexture2D* EngineLogo, TSubclassOf<UCommonTextStyle> PressKeyStyle);
};
