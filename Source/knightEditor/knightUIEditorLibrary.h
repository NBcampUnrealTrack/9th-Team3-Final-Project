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

	/** WBP_HUDPlayerStatus: 화면 전체 캔버스. 왼쪽 아래에 큰 체력 숫자, 체력 바(고스트 바), BOOST 캡션 + 과열, 부스트 바(3등분, 낮을 때 빨간 테두리)를 위에서 아래로 쌓는다. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDPlayerStatus(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> WarningStyle,
		TSubclassOf<UCommonTextStyle> NumberStyle = nullptr);

	/** WBP_HUDRaidTimer: 오른쪽 정렬. OBJECTIVE 캡션, 목표(Txt_Objective)와 남은 시간(Txt_Time) 한 줄, 밑줄. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDRaidTimer(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> TimeStyle, TSubclassOf<UCommonTextStyle> CaptionStyle,
		TSubclassOf<UCommonTextStyle> ObjectiveStyle = nullptr);

	/**
	 * WBP_HUDLockOnMarker: 캔버스 직속의 Box(주황 귀퉁이), PartFrame/PartFlash(부위 틀, PartFrameTexture가 있을 때), Line_Diagonal, Line_Shelf,
	 * Plate(TARGET LOCK + 이름 + 부위 이름), Stagger(경직 게이지), Txt_Distance. 위치는 C++가 매 프레임 정한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDLockOnMarker(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> NameStyle,
		UTexture2D* PartFrameTexture = nullptr);

	/** WBP_HUDFrameStatus: 부위 선화 4장(Img_PartHead/Torso/Arms/Legs)을 겹치고, 그 아래에 실루엣으로 채우는 아래→위 그로기 바(Bar_Groggy). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDFrameStatus(UWidgetBlueprint* WidgetBlueprint, UTexture2D* HeadTexture, UTexture2D* TorsoTexture, UTexture2D* ArmsTexture,
		UTexture2D* LegsTexture, UTexture2D* FillTexture, FVector2D Size);

	/** WBP_HUDRepairKit: 옅은 네모 테두리 안의 스패너(어두운 바탕 위로 아래→위 충전 바, Bar_RepairGauge)와 오른쪽 개수(Txt_RepairKitCount). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDRepairKit(UWidgetBlueprint* WidgetBlueprint, UTexture2D* WrenchTexture, TSubclassOf<UCommonTextStyle> CountStyle);

	/** WBP_HUDMessageLog: 아래 정렬 세로 상자(Box_Lines) 하나. 줄은 C++가 넣는다. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDMessageLog(UWidgetBlueprint* WidgetBlueprint);

	/** WBP_HUDHint: 남색 반투명 상자 안에 키(Txt_HintKey)와 설명(Txt_HintBody). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDHint(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> KeyStyle, TSubclassOf<UCommonTextStyle> BodyStyle);

	/** WBP_HUDGroggyNoise: 화면보다 한 칸 큰 타일 노이즈(Img_NoiseFrame 안의 Img_Noise)와 가로 주사선 6개(Img_Scan0..5). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDGroggyNoise(UWidgetBlueprint* WidgetBlueprint, UTexture2D* NoiseTexture);

	/** WBP_HUDLowHealth: 화면 전체로 늘린 붉은 가장자리(Img_Vignette)와 화면보다 한 칸 큰 타일 주사선(Img_Scanlines). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDLowHealth(UWidgetBlueprint* WidgetBlueprint, UTexture2D* VignetteTexture, UTexture2D* ScanlineTexture);

	/**
	 * WBP_HUDLayout: 화면 전체의 저체력 연출(맨 아래)과 락온 마커, 플레이어 패널, 왼쪽 아래 끝에 메카 설계도(체력 위), 아래 가운데 대화창,
	 * 오른쪽 아래 수리 키트와 그 위 튜토리얼 칸, 오른쪽 위 목표/타이머, 맨 위에 그로기 노이즈.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDLayout(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> PlayerStatusClass, TSubclassOf<UUserWidget> RaidTimerClass,
		TSubclassOf<UUserWidget> LockOnMarkerClass, TSubclassOf<UUserWidget> FrameStatusClass, TSubclassOf<UUserWidget> RepairKitClass,
		TSubclassOf<UUserWidget> MessageLogClass, TSubclassOf<UUserWidget> HintClass, TSubclassOf<UUserWidget> GroggyNoiseClass,
		TSubclassOf<UUserWidget> LowHealthClass);

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

	/**
	 * WBP_KnightMaintenance: 제목 왼쪽 위, 왼쪽 패널(분류 탭, 부위 탭, 목록), 가운데는 메카가 보이게 비우고,
	 * 오른쪽 패널 한 장(선택 장비 상세, Q/E 정보 탭 [스펙|세트|재료], 실행 버튼). 제작 확인 창이 위에 겹친다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildMaintenanceScreen(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> ButtonClass,
		TSubclassOf<UCommonButtonStyle> TabStyle, TSubclassOf<UCommonButtonStyle> CompactTabStyle, TSubclassOf<UCommonButtonStyle> SecondaryButtonStyle,
		TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> SubheadStyle, TSubclassOf<UCommonTextStyle> BodyStyle,
		TSubclassOf<UCommonTextStyle> BodyDimStyle, TSubclassOf<UCommonTextStyle> CaptionStyle);

	/** 정비 목록 한 줄: 아이콘 틀, 이름, 오른쪽 상태 배지(1px 틀). 선택 표시는 버튼 스타일이 맡는다. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildMaintenanceEntry(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> BodyStyle, TSubclassOf<UCommonTextStyle> CaptionStyle);

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
