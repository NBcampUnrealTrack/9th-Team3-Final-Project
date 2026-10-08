#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "knightUIEditorLibrary.generated.h"

class UCommonTextStyle;
class UUserWidget;
class UWidgetBlueprint;

/**
 * UI 위젯 블루프린트의 디자이너 트리를 코드로 만드는 에디터 도구. Python(unreal.KnightUIEditorLibrary)에서 부른다.
 *
 * 만든 트리는 디자이너에서 직접 만든 것과 같아서, 이후에는 Hierarchy 패널에서 자유롭게 고칠 수 있다.
 * 디자이너 작업을 덮어쓰지 않도록 트리가 이미 있는 블루프린트는 건드리지 않는다.
 */
UCLASS()
class UknightUIEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * WBP_KnightButton 기본 레이아웃: Overlay 아래 Txt_Label(가운데)과 Bracket_L/Bracket_R(양 끝).
	 * 이름은 UknightButtonBase의 BindWidgetOptional 이름과 같아야 한다. 컴파일과 저장은 호출한 쪽이 한다.
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|UI Editor")
	static bool BuildKnightButtonLayout(UWidgetBlueprint* WidgetBlueprint, TSubclassOf<UUserWidget> BracketClass, TSubclassOf<UCommonTextStyle> LabelStyle);
};
