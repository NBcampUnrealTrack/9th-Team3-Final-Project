#pragma once

#include "CoreMinimal.h"
#include "UObject/TopLevelAssetPath.h"

class FProperty;
struct FAssetData;
struct FMechPartAttachment;

/**
 * 게임 모듈(knight)을 모르는 워크벤치가 파츠 에셋을 읽는 방법 (2026-10-10, 리플렉션).
 *   - 파츠 클래스 = FMechPartAttachment 속성을 가진 클래스 (지금은 파츠 정의·무기 정의).
 *   - 파츠 메시 = 첫 번째 스켈레탈 메시 소프트 참조, 비어 있으면 첫 번째 스태틱 메시 소프트 참조.
 *     게임의 파츠 컴포넌트(Mesh가 비면 StaticMesh)와 같은 순서다.
 */
namespace MechPartReflection
{
	/** FMechPartAttachment 속성을 직접 선언한 클래스들. 에셋 목록 필터에 쓰고, 하위 클래스는 필터가 함께 보여 준다. */
	TArray<FTopLevelAssetPath> FindPartClassPaths();

	/** 에셋 안의 FMechPartAttachment 값. 없으면 null. */
	FMechPartAttachment* FindAttachment(UObject* Asset);

	/** 파츠 메시(USkeletalMesh 또는 UStaticMesh)를 불러온다. 둘 다 비어 있으면 null. */
	UObject* LoadPartMesh(const UObject* Asset);

	/**
	 * (2026-10-11) 목록 분류에 쓸 속성 = 파츠 클래스들의 AssetRegistrySearchable 속성 (지금은 파츠 정의의 SlotTag).
	 * 이름이 같은 속성은 하나만 넣는다.
	 */
	TArray<const FProperty*> FindGroupingProperties(const TArray<FTopLevelAssetPath>& PartClassPaths);

	/**
	 * 에셋의 분류 값(글자). 불러온 에셋이면 지금 값, 아니면 에셋 목록 정보에 저장된 값을 읽고,
	 * 그것도 없으면(분류 표시를 붙이기 전에 저장된 에셋) 에셋을 불러와 읽는다.
	 * 게임플레이 태그는 태그 이름만 남기고, 비어 있으면 빈 글자.
	 */
	FString GetGroupingValue(const FAssetData& AssetData, const FProperty* Property);
}
