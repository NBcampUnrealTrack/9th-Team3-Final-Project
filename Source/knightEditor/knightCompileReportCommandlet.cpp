#include "knightCompileReportCommandlet.h"

#include "Engine/Blueprint.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"

DEFINE_LOG_CATEGORY_STATIC(LogknightCompileReport, Display, All);

int32 UknightCompileReportCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> SwitchParams;
	ParseCommandLine(*Params, Tokens, Switches, SwitchParams);

	// -Asset=는 여러 번 줄 수 있으므로 SwitchParams(마지막 값만 남음) 대신 직접 훑는다.
	TArray<FString> Assets;
	FString Remaining = Params;
	FString Value;
	while (FParse::Value(*Remaining, TEXT("-Asset="), Value))
	{
		Assets.Add(Value);
		const int32 At = Remaining.Find(TEXT("-Asset="));
		Remaining = Remaining.Mid(At + 7);
	}
	if (Assets.IsEmpty())
	{
		UE_LOG(LogknightCompileReport, Error, TEXT("COMPILE| usage: -run=knightCompileReport -Asset=/Game/Path/BP_Name"));
		return 1;
	}

	int32 TotalErrors = 0;
	for (const FString& Path : Assets)
	{
		UBlueprint* const Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
		if (!Blueprint)
		{
			UE_LOG(LogknightCompileReport, Error, TEXT("COMPILE| %s: not a Blueprint"), *Path);
			++TotalErrors;
			continue;
		}
		FCompilerResultsLog Results;
		Results.SetSourcePath(Blueprint->GetPathName());
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);

		UE_LOG(LogknightCompileReport, Display, TEXT("COMPILE| %s: %d error(s), %d warning(s), %d message(s)"), *Path, Results.NumErrors, Results.NumWarnings,
			Results.Messages.Num());
		for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages)
		{
			const TCHAR* const Severity = Message->GetSeverity() == EMessageSeverity::Error ? TEXT("ERROR")
				: Message->GetSeverity() == EMessageSeverity::Warning || Message->GetSeverity() == EMessageSeverity::PerformanceWarning ? TEXT("WARNING")
				: TEXT("NOTE");
			UE_LOG(LogknightCompileReport, Display, TEXT("COMPILE|   %s %s"), Severity, *Message->ToText().ToString());
		}
		TotalErrors += Results.NumErrors;
	}
	return TotalErrors > 0 ? 1 : 0;
}
