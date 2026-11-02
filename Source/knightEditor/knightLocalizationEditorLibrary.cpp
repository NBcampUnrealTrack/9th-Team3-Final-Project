#include "knightLocalizationEditorLibrary.h"

#include "Internationalization/TextNamespaceUtil.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

namespace
{
	/** 한글 음절·자모가 하나라도 있으면 true. 한글 없는 텍스트(영어 장식 문구·숫자)는 일부러 번역에서 뺀 것으로 본다. */
	bool HasHangul(const FString& String)
	{
		for (const TCHAR Char : String)
		{
			if ((Char >= 0xAC00 && Char <= 0xD7A3) || (Char >= 0x1100 && Char <= 0x11FF) || (Char >= 0x3130 && Char <= 0x318F))
			{
				return true;
			}
		}
		return false;
	}

	/** 번역 수집에서 빠지는 텍스트인지. 빈 텍스트와 String Table 텍스트는 제외. */
	bool NeedsKey(const FText& Text)
	{
		return !Text.IsEmpty() && !Text.IsFromStringTable() && !Text.ShouldGatherForLocalization() && HasHangul(Text.ToString());
	}

	struct FTextFixer
	{
		FString PackageNamespace;
		bool bApply = false;
		TArray<FString> Lines;

		void VisitValue(const FProperty* Property, void* Value, const FString& Path)
		{
			if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
			{
				FText& Text = *static_cast<FText*>(Value);
				if (!NeedsKey(Text))
				{
					return;
				}
				Lines.Add(FString::Printf(TEXT("%s: %s"), *Path, *Text.ToString().Replace(TEXT("\n"), TEXT("\\n")).Left(80)));
				if (bApply)
				{
					// 에디터에서 새로 입력한 텍스트와 같은 형태: 패키지 네임스페이스 + 무작위 키
					Text = FText::AsLocalizable_Advanced(FTextKey(TextNamespaceUtil::BuildFullNamespace(FString(), PackageNamespace, true)),
						FTextKey(FGuid::NewGuid().ToString()), Text.ToString());
				}
			}
			else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				VisitStruct(StructProperty->Struct, Value, Path);
			}
			else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				FScriptArrayHelper Array(ArrayProperty, Value);
				for (int32 Index = 0; Index < Array.Num(); ++Index)
				{
					VisitValue(ArrayProperty->Inner, Array.GetRawPtr(Index), FString::Printf(TEXT("%s(%d)"), *Path, Index));
				}
			}
			else if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
			{
				FScriptMapHelper Map(MapProperty, Value);
				for (FScriptMapHelper::FIterator It(Map); It; ++It)
				{
					VisitValue(MapProperty->ValueProp, Map.GetValuePtr(It), FString::Printf(TEXT("%s[%d]"), *Path, It.GetLogicalIndex()));
				}
			}
			// 객체 참조는 따라가지 않는다. 같은 패키지의 하위 객체는 FindUnlocalizableTexts가 따로 훑는다.
		}

		void VisitStruct(const UStruct* Struct, void* Container, const FString& Path)
		{
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				for (int32 Index = 0; Index < It->ArrayDim; ++Index)
				{
					const FString Name = It->ArrayDim > 1 ? FString::Printf(TEXT("%s[%d]"), *It->GetName(), Index) : It->GetName();
					VisitValue(*It, It->ContainerPtrToValuePtr<void>(Container, Index), Path.IsEmpty() ? Name : Path + TEXT(".") + Name);
				}
			}
		}
	};
}

TArray<FString> UknightLocalizationEditorLibrary::FindUnlocalizableTexts(UObject* Asset, bool bApply)
{
	if (!Asset)
	{
		return {};
	}
	UPackage* const Package = Asset->GetPackage();
	FTextFixer Fixer;
	Fixer.bApply = bApply;
	Fixer.PackageNamespace = TextNamespaceUtil::EnsurePackageNamespace(Package);

	TArray<UObject*> Objects;
	GetObjectsWithPackage(Package, Objects, EGetObjectsFlags::IncludeNestedObjects);
	for (UObject* Object : Objects)
	{
		if (!Object || Object->HasAnyFlags(RF_Transient) || Object->IsA<UClass>())
		{
			continue;
		}
		FString Path = Object->GetPathName(Package);
		Fixer.VisitStruct(Object->GetClass(), Object, Path);
	}
	if (bApply && Fixer.Lines.Num() > 0)
	{
		Package->MarkPackageDirty();
	}
	return Fixer.Lines;
}
