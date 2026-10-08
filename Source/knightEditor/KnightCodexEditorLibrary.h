#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KnightCodexEditorLibrary.generated.h"

class UWidgetBlueprint;
class UUserWidget;
class UScrollBox;

/** Creates editable Widget Designer trees once; existing layouts are never overwritten. */
UCLASS()
class UKnightCodexEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Knight|Codex Editor") static bool BuildCodex(UWidgetBlueprint* Blueprint);
	UFUNCTION(BlueprintCallable, Category="Knight|Codex Editor") static bool BuildBossEntry(UWidgetBlueprint* Blueprint);
	UFUNCTION(BlueprintCallable, Category="Knight|Codex Editor") static bool BuildDropEntry(UWidgetBlueprint* Blueprint);
	UFUNCTION(BlueprintCallable, Category="Knight|Codex Editor") static bool MoveCodexCloseButton(UWidgetBlueprint* Blueprint);
	/** Read back a Designer hierarchy link after saving the Widget Blueprint. */
	UFUNCTION(BlueprintPure, Category="Knight|Codex Editor") static FName GetWidgetParentName(UWidgetBlueprint* Blueprint, FName WidgetName);
	/** PIE inspection of a named scroll box in the compiled Designer tree. */
	UFUNCTION(BlueprintPure, Category="Knight|Codex Editor") static UScrollBox* FindScrollBox(UUserWidget* Widget, FName WidgetName);
};
