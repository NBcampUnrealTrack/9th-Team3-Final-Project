#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "knightAssetDumpCommandlet.generated.h"

/**
 * 폴더 안 에셋의 값 중 기본값과 다른 것만 텍스트로 저장한다. 몬스터 레시피로 다시 만든 에셋을 원본과 비교할 때 쓴다.
 *
 *   UnrealEditor-Cmd.exe knight.uproject -run=knightAssetDump -Path=/Game/Knight/Creatures/Janyeol -Out=<파일>
 *
 * Blueprint는 생성 클래스의 기본 객체를 부모 클래스 기본 객체와 비교하고, 그 밖의 에셋은 클래스 기본 객체와 비교한다.
 * 컴포넌트, 노티파이 같은 하위 객체는 각자의 원형과 비교해 함께 적는다. 비교하기 쉽게 경로 안의 폴더 이름은
 * -Path 기준 상대 경로로 적는다.
 */
UCLASS()
class UknightAssetDumpCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
