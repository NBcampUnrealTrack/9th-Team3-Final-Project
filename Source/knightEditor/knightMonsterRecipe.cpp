#include "knightMonsterRecipe.h"

#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "knightMonsterRecipeLibrary.h"

#define LOCTEXT_NAMESPACE "knightMonsterRecipe"

void UknightMonsterRecipe::Build()
{
	UknightMonsterRecipeLibrary::BuildMonsterRecipe(this);
}

void UknightMonsterRecipe::Validate()
{
	const TArray<FString> Problems = UknightMonsterRecipeLibrary::ValidateMonsterRecipe(this);
	const FText Message = Problems.IsEmpty() ? LOCTEXT("ValidateOk", "문제 없음")
		: FText::FromString(FString::Join(Problems, TEXT("\n")));
	FMessageDialog::Open(EAppMsgType::Ok, Message, FText::FromName(MonsterName));
}

void UknightMonsterRecipe::Resize()
{
	const float Factor = ResizeFactor;
	if (FMath::IsNearlyEqual(Factor, 1.f))
	{
		return;
	}
	Modify();
	MeshScale *= Factor;
	HoverDrop *= Factor;
	HoverFootHeight *= Factor;
	CapsuleRadius *= Factor;
	CapsuleHalfHeight *= Factor;
	MaxWalkSpeed *= Factor;
	BodyBlockerExtent *= Factor;
	BodyBlockerOffset *= Factor;
	PushOutBodyAxisHalfLength *= Factor;
	TargetRadius *= Factor;
	ChaseStopDistance *= Factor;
	RangedMinDistance *= Factor;
	RangedMaxDistance *= Factor;
	for (FknightRecipeAttack& Attack : Attacks)
	{
		Attack.Selection.MinRange *= Factor;
		Attack.Selection.MaxRange *= Factor;
		Attack.AreaOffset *= Factor;
		Attack.AreaRadius *= Factor;
		Attack.LungeMaxDistance *= Factor;
		for (FknightRecipeHitWindow& Hit : Attack.HitWindows)
		{
			Hit.Trace.Radius *= Factor;
		}
	}
	ResizeFactor = 1.f;
	MarkPackageDirty();
}

FString UknightMonsterRecipe::GetOutputPath() const
{
	FString Path = OutputFolder.Path;
	Path.RemoveFromEnd(TEXT("/"));
	return Path.IsEmpty() ? FPackageName::GetLongPackagePath(GetPackage()->GetName()) : Path;
}

#undef LOCTEXT_NAMESPACE
