#include "knightInventoryTypes.h"

#include "knightInventoryLibrary.h"
#include "knightItemDefinition.h"
#include "knightInventoryModule.h"

int32 FknightInventoryList::AddItem(const UknightItemDefinition* Item, int32 Count)
{
	if (!Item || Count <= 0)
	{
		return 0;
	}
	FknightItemStack* Stack = Stacks.FindByPredicate([Item](const FknightItemStack& Entry) { return Entry.Item == Item; });
	if (!Stack)
	{
		Stack = &Stacks.Emplace_GetRef(Item, 0);
	}
	const int32 Added = FMath::Min(Count, Item->MaxStack - Stack->Count);
	Stack->Count += FMath::Max(0, Added);
	if (Stack->Count <= 0)
	{
		Stacks.RemoveAll([Item](const FknightItemStack& Entry) { return Entry.Item == Item; });
	}
	return FMath::Max(0, Added);
}

bool FknightInventoryList::RemoveItem(const UknightItemDefinition* Item, int32 Count)
{
	const int32 Index = Stacks.IndexOfByPredicate([Item](const FknightItemStack& Entry) { return Entry.Item == Item; });
	if (!Item || Count <= 0 || Index == INDEX_NONE || Stacks[Index].Count < Count)
	{
		return false;
	}
	Stacks[Index].Count -= Count;
	if (Stacks[Index].Count == 0)
	{
		Stacks.RemoveAt(Index);
	}
	return true;
}

int32 FknightInventoryList::GetCount(const UknightItemDefinition* Item) const
{
	const FknightItemStack* const Stack = Stacks.FindByPredicate([Item](const FknightItemStack& Entry) { return Entry.Item == Item; });
	return Stack ? Stack->Count : 0;
}

void FknightInventoryList::ToSaved(TArray<FknightSavedItemStack>& OutSaved) const
{
	OutSaved.Reset(Stacks.Num());
	for (const FknightItemStack& Stack : Stacks)
	{
		if (Stack.Item && Stack.Count > 0)
		{
			FknightSavedItemStack& Saved = OutSaved.AddDefaulted_GetRef();
			Saved.ItemId = Stack.Item->GetPrimaryAssetId();
			Saved.Count = Stack.Count;
		}
	}
}

void FknightInventoryList::FromSaved(const TArray<FknightSavedItemStack>& Saved)
{
	Stacks.Reset(Saved.Num());
	for (const FknightSavedItemStack& Entry : Saved)
	{
		if (const UknightItemDefinition* const Item = UknightInventoryLibrary::LoadItemDefinition(Entry.ItemId))
		{
			AddItem(Item, Entry.Count);
		}
		else
		{
			UE_LOG(LogknightInventory, Warning, TEXT("Saved item %s no longer exists; %d dropped."), *Entry.ItemId.ToString(), Entry.Count);
		}
	}
}
