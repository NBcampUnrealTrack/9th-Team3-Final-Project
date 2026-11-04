#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "Engine/DataAsset.h"
#include "Character/knightPartHealthComponent.h"
#include "Monster/knightMonsterReadout.h"
#include "knightHUDMessageLog.generated.h"

class UCommonTextBlock;
class UCommonTextStyle;
class USoundBase;
class UVerticalBox;
class UknightVM_PlayerStatus;
class UknightMonsterDefinition;

USTRUCT(BlueprintType)
struct KNIGHTUI_API FknightVoiceSubtitle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knight|HUD")
	TSoftObjectPtr<USoundBase> Voice;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knight|HUD", meta=(MultiLine=true))
	FText Line;
};

/** [AI] 대사 하나와, 같은 대사를 다시 하기까지 기다리는 시간. */
USTRUCT(BlueprintType)
struct KNIGHTUI_API FknightMonsterReadoutLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knight|HUD", meta=(MultiLine=true))
	FText Line;

	/** 패링 경직처럼 자주 오는 판독은 길게 둬 같은 말을 되풀이하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knight|HUD", meta=(ClampMin=0, Units="s"))
	float Cooldown = 0.f;
};

/** What the HUD message log says: the subtitle of each pilot voice line and the notices for the Knight's parts. */
UCLASS(BlueprintType)
class KNIGHTUI_API UknightHUDMessageData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knight|HUD")
	TArray<FknightVoiceSubtitle> VoiceSubtitles;

	/** Notice when a part breaks, in EknightBodyPart order (head, torso, arms, legs). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knight|HUD", EditFixedSize)
	TArray<FText> PartBrokenLines = { FText(), FText(), FText(), FText() };

	/**
	 * 몬스터 상태 판독(UknightHUDMonsterReadout)이 뜰 때 [AI]가 하는 말. 몬스터 정의의 ReadoutTexts에 AssistantLine이 있으면
	 * 그 문장을 대신 쓴다(쿨타임은 여기 값).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knight|HUD", meta=(ForceInlineRow))
	TMap<EknightMonsterReadout, FknightMonsterReadoutLine> MonsterReadoutLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knight|HUD")
	FText VoiceSpeaker = INVTEXT("[AI]");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knight|HUD")
	FText SystemSpeaker = INVTEXT("[SYSTEM]");
};

/**
 * Message log at the bottom centre of the HUD, like the comms feed of a mech cockpit: a new line comes in at the bottom and
 * pushes the older ones up; each line fades after LineTime and at most MaxLines are shown.
 *
 * Shows the subtitle of every pilot voice line when it starts playing (only while subtitles are on in the settings), and
 * a system notice when one of the Knight's parts breaks. Other code can add lines with AddLine.
 * Lines go into Box_Lines (a vertical box aligned to the bottom).
 */
UCLASS(Abstract, Blueprintable, meta=(DisableNativeTick))
class KNIGHTUI_API UknightHUDMessageLog : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Adds a line. bSystem picks the system speaker and color instead of the pilot's. */
	UFUNCTION(BlueprintCallable, Category="Knight|HUD")
	void AddLine(const FText& Text, bool bSystem);

	/** 몬스터 상태 판독에 맞는 [AI] 대사를 한 줄 넣는다. 그 판독의 쿨타임 안이면 넣지 않는다. */
	void AddMonsterReadoutLine(EknightMonsterReadout Type, const UknightMonsterDefinition* Definition);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category="Knight|HUD", meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> Box_Lines;

	/** Hard reference so the lines are packaged with the HUD. */
	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	TObjectPtr<UknightHUDMessageData> Messages;

	UPROPERTY(EditAnywhere, Category="Knight|HUD")
	TSubclassOf<UCommonTextStyle> LineStyle;

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Colors")
	FLinearColor VoiceColor = FLinearColor(FColor(0xBF, 0xE6, 0xFF));

	UPROPERTY(EditAnywhere, Category="Knight|HUD|Colors")
	FLinearColor SystemColor = FLinearColor(FColor(0xFF, 0xB0, 0x8A));

	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=1))
	int32 MaxLines = 2;

	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0.5, Units="s"))
	float LineTime = 4.f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0.01, Units="s"))
	float SlideTime = 0.25f;

	UPROPERTY(EditAnywhere, Category="Knight|HUD", meta=(ClampMin=0.01, Units="s"))
	float FadeTime = 0.35f;

private:
	struct FLine
	{
		TWeakObjectPtr<UCommonTextBlock> Text;
		float Age = 0.f;
		/** Seconds left of the fade once the line is pushed out or expires; below 0 while it stays. */
		float FadeLeft = -1.f;
	};

	void HandleVoiceStarted(USoundBase* Voice);
	void HandlePartBroken(EknightBodyPart Part);
	bool Tick(float DeltaTime);
	void StartFade(FLine& Line);

	TArray<FLine> Lines;
	/** 판독마다 마지막으로 대사를 한 시각(실제 시간). */
	TMap<EknightMonsterReadout, double> ReadoutLineTimes;
	/** Pixels the lines still sit below their layout slot after a new line pushed them up. */
	float SlideOffset = 0.f;
	float SlideSpeed = 0.f;
	FTSTicker::FDelegateHandle Ticker;
	FDelegateHandle VoiceHandle;
	FDelegateHandle PartHandle;
	TWeakObjectPtr<UknightVM_PlayerStatus> ViewModel;
};
