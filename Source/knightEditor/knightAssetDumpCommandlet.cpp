#include "knightAssetDumpCommandlet.h"

#include "Animation/AnimBlueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UObjectHash.h"

DEFINE_LOG_CATEGORY_STATIC(LogknightAssetDump, Display, All);

namespace knightAssetDump
{
	/** 값 비교에 쓸모가 없는 에셋(텍스처, 메시 등)은 이름만 적는다. */
	bool IsNameOnly(const FTopLevelAssetPath& ClassPath)
	{
		static const TCHAR* const Names[] = {
			TEXT("Texture2D"), TEXT("SkeletalMesh"), TEXT("StaticMesh"), TEXT("Skeleton"), TEXT("PhysicsAsset"), TEXT("Material"),
			TEXT("MaterialInstanceConstant"), TEXT("NiagaraSystem"), TEXT("AnimSequence"), TEXT("ControlRigBlueprint"), TEXT("SoundWave")
		};
		for (const TCHAR* Name : Names)
		{
			if (ClassPath.GetAssetName() == Name)
			{
				return true;
			}
		}
		return false;
	}

	struct FWriter
	{
		FString Root;
		FString Text;
		static constexpr int32 MaxValueLength = 6000;

		FString Clean(FString Value) const
		{
			Value.ReplaceInline(*Root, TEXT("<Root>"));
			if (Value.Len() > MaxValueLength)
			{
				Value = Value.Left(MaxValueLength) + TEXT("...");
			}
			return Value;
		}

		void Line(int32 Depth, const FString& Value)
		{
			Text += FString::ChrN(Depth * 2, TEXT(' ')) + Clean(Value) + TEXT("\n");
		}

		/** Object의 프로퍼티 중 Base와 다른 것만 적고, Object 안의 하위 객체도 각자의 원형과 비교한다. */
		void DumpObject(const UObject* Object, const UObject* Base, int32 Depth)
		{
			if (Depth > 6)
			{
				return;
			}
			for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
			{
				const FProperty* const Property = *It;
				if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated) || Property->IsA<FMulticastDelegateProperty>())
				{
					continue;
				}
				const void* const Value = Property->ContainerPtrToValuePtr<void>(Object);
				const void* const BaseValue = Base && Base->IsA(Property->GetOwnerClass()) ? Property->ContainerPtrToValuePtr<void>(Base) : nullptr;
				if (const FObjectPropertyBase* const ObjectProperty = CastField<FObjectPropertyBase>(Property))
				{
					// 하위 객체는 아래에서 따로 적는다. 포인터는 객체마다 달라 비교할 의미가 없다.
					const UObject* const Referenced = ObjectProperty->GetObjectPropertyValue(Value);
					if (Referenced && Referenced->IsIn(Object))
					{
						continue;
					}
				}
				if (BaseValue && Property->Identical(Value, BaseValue, PPF_DeepComparison))
				{
					continue;
				}
				FString Exported;
				Property->ExportTextItem_Direct(Exported, Value, nullptr, const_cast<UObject*>(Object), PPF_None);
				Line(Depth, FString::Printf(TEXT("%s = %s"), *Property->GetName(), *Exported));
			}

			TArray<UObject*> Subobjects;
			GetObjectsWithOuter(Object, Subobjects, EGetObjectsFlags::None);
			Subobjects.Sort([](const UObject& A, const UObject& B) { return A.GetName() < B.GetName(); });
			for (const UObject* Subobject : Subobjects)
			{
				if (Subobject->IsA<UField>() || Subobject->HasAnyFlags(RF_Transient))
				{
					continue;
				}
				Line(Depth, FString::Printf(TEXT("+ %s (%s)"), *Subobject->GetName(), *Subobject->GetClass()->GetName()));
				DumpObject(Subobject, Subobject->GetArchetype(), Depth + 1);
			}
		}

		void DumpGraphs(const UBlueprint* Blueprint)
		{
			TArray<UEdGraph*> Graphs;
			Blueprint->GetAllGraphs(Graphs);
			for (const UEdGraph* Graph : Graphs)
			{
				Line(1, FString::Printf(TEXT("graph %s (%d nodes)"), *Graph->GetName(), Graph->Nodes.Num()));
				for (const UEdGraphNode* Node : Graph->Nodes)
				{
					if (Node)
					{
						Line(2, FString::Printf(TEXT("%s | %s"), *Node->GetClass()->GetName(),
							*Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"), TEXT(" / "))));
					}
				}
			}
		}

		void DumpAsset(UObject* Asset)
		{
			if (const UBlueprint* const Blueprint = Cast<UBlueprint>(Asset))
			{
				UClass* const Generated = Blueprint->GeneratedClass;
				Line(1, FString::Printf(TEXT("parent = %s"), *GetPathNameSafe(Blueprint->ParentClass)));
				if (const UAnimBlueprint* const AnimBlueprint = Cast<UAnimBlueprint>(Blueprint))
				{
					Line(1, FString::Printf(TEXT("skeleton = %s template=%d"), *GetPathNameSafe(AnimBlueprint->TargetSkeleton), AnimBlueprint->bIsTemplate));
				}
				if (Generated)
				{
					const UObject* const CDO = Generated->GetDefaultObject();
					const UClass* const Super = Generated->GetSuperClass();
					DumpObject(CDO, Super ? Super->GetDefaultObject() : nullptr, 1);

					// Blueprint에서 추가하거나 덮어쓴 컴포넌트 템플릿은 생성 클래스 안에 있다.
					TArray<UObject*> ClassObjects;
					GetObjectsWithOuter(Generated, ClassObjects, EGetObjectsFlags::None);
					ClassObjects.Sort([](const UObject& A, const UObject& B) { return A.GetName() < B.GetName(); });
					for (const UObject* Object : ClassObjects)
					{
						if (Object == CDO || Object->IsA<UField>() || Object->HasAnyFlags(RF_Transient))
						{
							continue;
						}
						Line(1, FString::Printf(TEXT("+ %s (%s)"), *Object->GetName(), *Object->GetClass()->GetName()));
						DumpObject(Object, Object->GetArchetype(), 2);
					}
				}
				DumpGraphs(Blueprint);
				return;
			}
			DumpObject(Asset, Asset->GetClass()->GetDefaultObject(), 1);
		}
	};
}

int32 UknightAssetDumpCommandlet::Main(const FString& Params)
{
	FString Path;
	FString Out;
	if (!FParse::Value(*Params, TEXT("-Path="), Path))
	{
		UE_LOG(LogknightAssetDump, Error, TEXT("DUMP| usage: -run=knightAssetDump -Path=/Game/Folder [-Out=<file>]"));
		return 1;
	}
	if (!FParse::Value(*Params, TEXT("-Out="), Out))
	{
		Out = FPaths::ProjectSavedDir() / TEXT("AssetDump") / FPaths::GetCleanFilename(Path) + TEXT(".txt");
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.SearchAllAssets(true);
	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByPath(FName(*Path), Assets, true);
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });

	knightAssetDump::FWriter Writer;
	Writer.Root = Path;
	for (const FAssetData& AssetData : Assets)
	{
		Writer.Line(0, FString::Printf(TEXT("== %s (%s)"), *AssetData.PackageName.ToString(), *AssetData.AssetClassPath.GetAssetName().ToString()));
		if (knightAssetDump::IsNameOnly(AssetData.AssetClassPath))
		{
			continue;
		}
		if (UObject* const Asset = AssetData.GetAsset())
		{
			Writer.DumpAsset(Asset);
		}
	}

	if (!FFileHelper::SaveStringToFile(Writer.Text, *Out, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogknightAssetDump, Error, TEXT("DUMP| failed to write %s"), *Out);
		return 1;
	}
	UE_LOG(LogknightAssetDump, Display, TEXT("DUMP| %d assets -> %s"), Assets.Num(), *Out);
	return 0;
}
