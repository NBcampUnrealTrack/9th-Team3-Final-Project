#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "knightAnimCoverageCommandlet.generated.h"

/**
 * 커버리지 보드와 같은 검사를 명령줄에서 실행하고 Markdown으로 저장한다. CI나 자동화에서 쓴다.
 *
 *   UnrealEditor-Cmd.exe knight.uproject -run=knightAnimCoverage [-Out=<경로>] [-FailOnError]
 *
 * -Out이 없으면 Saved/Profiling/knightAnimDebug/AnimCoverage_<시각>.md에 저장한다.
 * -FailOnError면 '문제' 항목이 하나라도 있을 때 1을 돌려준다.
 */
UCLASS()
class UknightAnimCoverageCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UknightAnimCoverageCommandlet();

	virtual int32 Main(const FString& Params) override;
};
