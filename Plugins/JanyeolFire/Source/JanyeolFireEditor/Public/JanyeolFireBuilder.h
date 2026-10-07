#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "JanyeolFireBuilder.generated.h"
class UNiagaraSystem;
class UMaterialInterface;
UCLASS()
class JANYEOLFIREEDITOR_API UJanyeolFireBuilder : public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Janyeol|Build")
 static UNiagaraSystem* BuildFlameSystem(const FString& PackagePath, UMaterialInterface* Material, int32 Style);
 UFUNCTION(BlueprintCallable,Category="Janyeol|Build")
 static FString InspectFlameSystem(UNiagaraSystem* System);
};
