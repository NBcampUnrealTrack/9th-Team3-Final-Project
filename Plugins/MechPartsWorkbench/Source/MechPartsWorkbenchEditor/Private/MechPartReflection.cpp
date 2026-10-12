#include "MechPartReflection.h"

#include "AssetRegistry/AssetData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "MechPartAttachment.h"
#include "UObject/UObjectIterator.h"

namespace MechPartReflection
{
	TArray<FTopLevelAssetPath> FindPartClassPaths()
	{
		TArray<FTopLevelAssetPath> Paths;
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			const UClass* const Class = *ClassIt;
			if (Class->HasAnyClassFlags(CLASS_NewerVersionExists))
			{
				continue;
			}
			for (TFieldIterator<FStructProperty> PropIt(Class, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
			{
				if (PropIt->Struct == FMechPartAttachment::StaticStruct())
				{
					Paths.Add(Class->GetClassPathName());
					break;
				}
			}
		}
		return Paths;
	}

	FStructProperty* FindAttachmentProperty(const UObject* Asset)
	{
		for (TFieldIterator<FStructProperty> PropIt(Asset->GetClass()); PropIt; ++PropIt)
		{
			if (PropIt->Struct == FMechPartAttachment::StaticStruct())
			{
				return *PropIt;
			}
		}
		return nullptr;
	}

	FMechPartAttachment* FindAttachment(UObject* Asset)
	{
		const FStructProperty* const Property = FindAttachmentProperty(Asset);
		return Property ? Property->ContainerPtrToValuePtr<FMechPartAttachment>(Asset) : nullptr;
	}

	FSoftObjectProperty* FindSkeletalMeshProperty(const UObject* Asset)
	{
		for (TFieldIterator<FSoftObjectProperty> PropIt(Asset->GetClass()); PropIt; ++PropIt)
		{
			if (PropIt->PropertyClass && PropIt->PropertyClass->IsChildOf(USkeletalMesh::StaticClass()))
			{
				return *PropIt;
			}
		}
		return nullptr;
	}

	/** MeshClass(또는 하위 클래스)를 가리키는 소프트 참조 중 처음으로 불러와지는 것. */
	static UObject* LoadFirstMesh(const UObject* Asset, const UClass* MeshClass)
	{
		for (TFieldIterator<FSoftObjectProperty> PropIt(Asset->GetClass()); PropIt; ++PropIt)
		{
			if (PropIt->PropertyClass && PropIt->PropertyClass->IsChildOf(MeshClass))
			{
				if (UObject* const Mesh = PropIt->GetPropertyValue_InContainer(Asset).LoadSynchronous())
				{
					return Mesh;
				}
			}
		}
		return nullptr;
	}

	UObject* LoadPartMesh(const UObject* Asset)
	{
		if (UObject* const SkeletalMesh = LoadFirstMesh(Asset, USkeletalMesh::StaticClass()))
		{
			return SkeletalMesh;
		}
		return LoadFirstMesh(Asset, UStaticMesh::StaticClass());
	}

	UStaticMesh* LoadPartStaticMesh(const UObject* Asset)
	{
		return Cast<UStaticMesh>(LoadFirstMesh(Asset, UStaticMesh::StaticClass()));
	}

	TArray<const FProperty*> FindGroupingProperties(const TArray<FTopLevelAssetPath>& PartClassPaths)
	{
		TArray<const FProperty*> Properties;
		for (const FTopLevelAssetPath& ClassPath : PartClassPaths)
		{
			const UClass* const Class = FindObject<UClass>(ClassPath);
			if (!Class)
			{
				continue;
			}
			for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
			{
				const FProperty* const Property = *PropIt;
				if (Property->HasAnyPropertyFlags(CPF_AssetRegistrySearchable)
					&& !Properties.ContainsByPredicate([Property](const FProperty* Existing) { return Existing->GetFName() == Property->GetFName(); }))
				{
					Properties.Add(Property);
				}
			}
		}
		return Properties;
	}

	/** 게임플레이 태그를 글자로 내보내면 (TagName="Equipment.Slot.Head") 모양이라 태그 이름만 꺼낸다. */
	static FString CleanGroupingText(const FString& Text)
	{
		FString TagName = Text;
		if (Text.StartsWith(TEXT("(TagName=\"")) && Text.EndsWith(TEXT("\")")))
		{
			TagName = Text.Mid(10, Text.Len() - 12);
		}
		return TagName == TEXT("None") ? FString() : TagName;
	}

	FString GetGroupingValue(const FAssetData& AssetData, const FProperty* Property)
	{
		FString Text;
		UObject* Asset = AssetData.FastGetAsset(false);
		if (!Asset && !AssetData.GetTagValue(Property->GetFName(), Text))
		{
			Asset = AssetData.GetAsset();
		}
		if (Asset)
		{
			if (!Asset->GetClass()->IsChildOf(Property->GetOwnerClass()))
			{
				return FString();
			}
			Property->ExportTextItem_InContainer(Text, Asset, nullptr, nullptr, PPF_None);
		}
		return CleanGroupingText(Text);
	}
}
