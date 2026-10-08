#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightUIEditorLibrary.generated.h"

class UCommonButtonStyle;
class UCommonTextStyle;
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

	/** WBP_HUDPlayerStatus: 이름/체력 수치, 체력 바(고스트 바 겹침), 부스트 라벨/과열/수치, 부스트 바. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDPlayerStatus(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> WarningStyle);

	/** WBP_HUDRaidTimer: 오른쪽 정렬된 Txt_Time과 Txt_Caption. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDRaidTimer(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> TimeStyle, TSubclassOf<UCommonTextStyle> CaptionStyle);

	/** WBP_HUDLockOnMarker: 캔버스 직속의 Box(주황 귀퉁이), Line_Diagonal, Line_Shelf, Plate(TARGET LOCK + 이름). 위치는 C++가 매 프레임 정한다. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDLockOnMarker(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UCommonTextStyle> CaptionStyle, TSubclassOf<UCommonTextStyle> NameStyle);

	/** WBP_HUDLayout: 화면 전체의 락온 마커(맨 아래), 1080p 세이프존(4.5%) 안쪽에 플레이어 상태(좌상), 타이머(우상), 화면 중앙 조준점. */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildHUDLayout(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> PlayerStatusClass, TSubclassOf<UUserWidget> RaidTimerClass, TSubclassOf<UUserWidget> LockOnMarkerClass);

	/** WBP_KnightConfirmModal: 화면을 덮는 어두운 막(클릭 차단), 가운데 패널(제목, 본문, 오른쪽 아래 [취소] [확인]). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildConfirmModal(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> ButtonClass, TSubclassOf<UCommonButtonStyle> CancelButtonStyle,
		TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> BodyStyle);

	/** WBP_KnightPauseMenu: 어두운 막 + 왼쪽 세로 패널(SYSTEM PAUSE, 제목, 계속하기 / 설정 / 레이드 포기, Esc 안내). */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildPauseMenu(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> ButtonClass, TSubclassOf<UCommonButtonStyle> SecondaryButtonStyle,
		TSubclassOf<UCommonTextStyle> TitleStyle, TSubclassOf<UCommonTextStyle> CaptionStyle);
};
