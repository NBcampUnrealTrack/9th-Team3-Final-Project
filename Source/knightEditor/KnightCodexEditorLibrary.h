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
	/** Inspect the saved Designer tree from editor scripting tools. */
	UFUNCTION(BlueprintPure, Category="Knight|Codex Editor") static FString DescribeTree(UWidgetBlueprint* Blueprint);
	/** Restyle the existing editable Designer hierarchy without replacing its bound widgets. */
	UFUNCTION(BlueprintCallable, Category="Knight|Codex Editor") static bool StyleCodex(UWidgetBlueprint* Blueprint);
	UFUNCTION(BlueprintCallable, Category="Knight|Codex Editor") static bool StyleBossEntry(UWidgetBlueprint* Blueprint);
	UFUNCTION(BlueprintCallable, Category="Knight|Codex Editor") static bool StyleDropEntry(UWidgetBlueprint* Blueprint);
	/** Rearrange the existing bound controls into a three-column records view. Idempotent. */
	UFUNCTION(BlueprintCallable, Category="Knight|Codex Editor") static bool RedesignCodex(UWidgetBlueprint* Blueprint);
	UFUNCTION(BlueprintCallable, Category="Knight|Codex Editor") static bool RedesignBossEntry(UWidgetBlueprint* Blueprint);
	UFUNCTION(BlueprintCallable, Category="Knight|Codex Editor") static bool RedesignDropEntry(UWidgetBlueprint* Blueprint);
	/** Rebuild only the codex Designer presentation; class defaults and source data are retained. */
	UFUNCTION(BlueprintCallable, Category="Knight|Codex Editor") static bool BuildArchivePresentation(UWidgetBlueprint* Blueprint, int32 Kind);
	/** PIE inspection of a named scroll box in the compiled Designer tree. */
	UFUNCTION(BlueprintPure, Category="Knight|Codex Editor") static UScrollBox* FindScrollBox(UUserWidget* Widget, FName WidgetName);
};
