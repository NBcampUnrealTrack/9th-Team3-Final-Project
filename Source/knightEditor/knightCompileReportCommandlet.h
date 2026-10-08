#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "knightCompileReportCommandlet.generated.h"

/**
 * Blueprint를 컴파일하고 오류·경고 개수와, 결과 로그에 남은 메시지를 출력한다. 헤드리스 검증용.
 * 주의: 애님 BP 일부 경고는 개수에만 잡히고 문장은 남지 않는다. 그때는 에디터의 컴파일 결과 창에서 확인한다.
 *
 *   UnrealEditor-Cmd.exe knight.uproject -run=knightCompileReport -Asset=/Game/Path/BP_Name [-Asset=...]
 *
 * 출력 줄은 "COMPILE|"로 시작한다. 오류가 하나라도 있으면 1을 돌려준다.
 */
UCLASS()
class UknightCompileReportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
