#include "AI/knightBossAIController.h"

#include "Components/StateTreeAIComponent.h"
#include "Monster/knightMonsterCharacterBase.h"
#include "Monster/knightMonsterTargetingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "System/knightTeams.h"

AknightBossAIController::AknightBossAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	StateTreeAI->SetStartLogicAutomatically(false);
	bStartAILogicOnPossess = false;
	// EQS and perception queries use the controller's location, so keep it on the pawn.
	bAttachToPawn = true;

	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 6000.f;
	SightConfig->LoseSightRadius = 8000.f;
	SightConfig->PeripheralVisionAngleDegrees = 70.f;
	SightConfig->SetMaxAge(10.f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	Perception->ConfigureSense(*SightConfig);

	// Boost thrusters are loud: the Knight is heard well before it is seen.
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 4000.f;
	HearingConfig->SetMaxAge(5.f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = false;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
	Perception->ConfigureSense(*HearingConfig);

	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	DamageConfig->SetMaxAge(10.f);
	Perception->ConfigureSense(*DamageConfig);

	Perception->SetDominantSense(UAISense_Sight::StaticClass());
	SetPerceptionComponent(*Perception);

	SetGenericTeamId(knightTeams::ToTeamId(EknightTeam::Monster));
}

void AknightBossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &ThisClass::HandleTargetPerceptionUpdated);
	Perception->OnTargetPerceptionForgotten.AddUniqueDynamic(this, &ThisClass::HandleTargetPerceptionForgotten);

	StateTreeAI->StartLogic();
}

void AknightBossAIController::OnUnPossess()
{
	StateTreeAI->StopLogic(TEXT("Unpossessed"));
	Perception->OnTargetPerceptionUpdated.RemoveDynamic(this, &ThisClass::HandleTargetPerceptionUpdated);
	Perception->OnTargetPerceptionForgotten.RemoveDynamic(this, &ThisClass::HandleTargetPerceptionForgotten);
	Super::OnUnPossess();
}

void AknightBossAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetPawn());
	UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr;
	if (!Targeting || !Actor)
	{
		return;
	}
	// One sense losing the actor (out of sight) does not mean it is gone while another still senses it.
	const FActorPerceptionInfo* const Info = Perception->GetActorInfo(*Actor);
	Targeting->NotifyPerceived(Actor, Stimulus.WasSuccessfullySensed() || (Info && Info->HasAnyCurrentStimulus()));
}

void AknightBossAIController::HandleTargetPerceptionForgotten(AActor* Actor)
{
	const AknightMonsterCharacterBase* const Monster = Cast<AknightMonsterCharacterBase>(GetPawn());
	if (UknightMonsterTargetingComponent* const Targeting = Monster ? Monster->GetTargetingComponent() : nullptr)
	{
		Targeting->NotifyPerceived(Actor, false);
	}
}
