#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "CoreMinimal.h"
#include "GameplayDebuggerCategory.h"

/**
 * Project gameplay debugger categories (open with the apostrophe key, toggle categories on the numpad).
 * Each shows the selected debug actor, or the local player's pawn when nothing is selected.
 *   Knight Abilities: owned State/Window tags, active abilities with activation group, core attributes
 *   Knight Combat:    input buffer, lock-on target, hit stop, weapon trace
 *   Knight Monster:   current target, rage/stamina, part HP and breaks
 */
class FknightGameplayDebuggerCategoryBase : public FGameplayDebuggerCategory
{
public:
	FknightGameplayDebuggerCategoryBase();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

protected:
	/** Fills Lines for Actor. Actor is never null. */
	virtual void CollectLines(const AActor& Actor, TArray<FString>& OutLines) const = 0;

	struct FRepData
	{
		TArray<FString> Lines;
		void Serialize(FArchive& Ar) { Ar << Lines; }
	};
	FRepData DataPack;
};

class FknightGameplayDebuggerCategory_Abilities : public FknightGameplayDebuggerCategoryBase
{
public:
	static TSharedRef<FGameplayDebuggerCategory> MakeInstance() { return MakeShared<FknightGameplayDebuggerCategory_Abilities>(); }

protected:
	virtual void CollectLines(const AActor& Actor, TArray<FString>& OutLines) const override;
};

class FknightGameplayDebuggerCategory_Combat : public FknightGameplayDebuggerCategoryBase
{
public:
	static TSharedRef<FGameplayDebuggerCategory> MakeInstance() { return MakeShared<FknightGameplayDebuggerCategory_Combat>(); }

protected:
	virtual void CollectLines(const AActor& Actor, TArray<FString>& OutLines) const override;
};

class FknightGameplayDebuggerCategory_Monster : public FknightGameplayDebuggerCategoryBase
{
public:
	static TSharedRef<FGameplayDebuggerCategory> MakeInstance() { return MakeShared<FknightGameplayDebuggerCategory_Monster>(); }

protected:
	virtual void CollectLines(const AActor& Actor, TArray<FString>& OutLines) const override;
};

namespace knightGameplayDebugger
{
	void RegisterCategories();
	void UnregisterCategories();
}

#endif // WITH_GAMEPLAY_DEBUGGER
