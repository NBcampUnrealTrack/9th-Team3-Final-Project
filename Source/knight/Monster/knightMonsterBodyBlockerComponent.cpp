#include "Monster/knightMonsterBodyBlockerComponent.h"

UknightMonsterBodyBlockerComponent::UknightMonsterBodyBlockerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	SetGenerateOverlapEvents(false);
	CanCharacterStepUpOn = ECB_No;
	SetCanEverAffectNavigation(false);
	SetHiddenInGame(true);
	ShapeColor = FColor(255, 140, 0);
	InitBoxExtent(FVector(200.f, 200.f, 100.f));
}
