#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "KnightCodexPresentation.generated.h"

class UTextBlock;

UENUM(BlueprintType)
enum class EKnightCodexSymbol : uint8
{
	World, Mech, Monster, Document, Material, Back, Close, Lock, Frame
};

/** Panel frame only. Legacy symbol enum values remain loadable but do not draw icons. */
UCLASS()
class KNIGHT_API UKnightCodexSymbolWidget : public UWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Codex") EKnightCodexSymbol Symbol = EKnightCodexSymbol::Document;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Codex") FLinearColor Ink = FLinearColor(0.3f, 0.6f, 0.8f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Codex") FVector2D SymbolSize = FVector2D(40.f, 40.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Codex") bool bGrid = false;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	TSharedPtr<SWidget> SymbolSlate;
};

/** Codex-only button chrome: thin accent border and 140 ms state transitions. */
UCLASS()
class KNIGHT_API UKnightCodexArchiveButton : public UButton
{
	GENERATED_BODY()
public:
	UKnightCodexArchiveButton(const FObjectInitializer& ObjectInitializer);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Codex") bool bCategoryTile = false;
	void SetRecordState(bool bSelected, bool bLocked = false);
	bool IsRecordSelected() const { return bRecordSelected; }
	bool IsRecordLocked() const { return bRecordLocked; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	bool bRecordSelected = false;
	bool bRecordLocked = false;
};

/** Local typography and palette. Shared menu styles are not modified. */
namespace KnightCodexPresentation
{
	KNIGHT_API FLinearColor Color(const TCHAR* Hex, float Alpha = 1.f);
	KNIGHT_API void TextStyle(UTextBlock* Text, int32 Size, bool bStrong = false, bool bMuted = false);
}
