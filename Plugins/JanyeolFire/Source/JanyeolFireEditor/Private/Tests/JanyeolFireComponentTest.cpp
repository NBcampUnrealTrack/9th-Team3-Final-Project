#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "JanyeolFireComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJanyeolFireIntegrationTest,
    "Janyeol.Fire.ExistingMonsterIntegration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJanyeolFireIntegrationTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<AActor>(nullptr, TEXT("/Game/Knight/Creatures/Janyeol/BP_Janyeol.BP_Janyeol_C"));
    if (!TestNotNull(TEXT("Integrated Janyeol Blueprint"), Class)) return false;
    const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ACharacter* Actor = World->SpawnActor<ACharacter>(Class, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!TestNotNull(TEXT("Original Character parent preserved"), Actor))
    {
        World->DestroyWorld(false); return false;
    }
    USkeletalMeshComponent* Mesh = Actor->GetMesh();
    UJanyeolFireComponent* Fire = Actor->FindComponentByClass<UJanyeolFireComponent>();
    if (!TestNotNull(TEXT("Fire component on existing BP"), Fire))
    {
        World->DestroyWorld(false); return false;
    }
    UClass* AnimClass = Mesh->GetAnimClass();
    const EAnimationMode::Type Mode = Mesh->GetAnimationMode();
    const ECollisionEnabled::Type Collision = Mesh->GetCollisionEnabled();
    const FTransform MeshTransform = Mesh->GetRelativeTransform();
    Fire->RefreshFire();
    TestTrue(TEXT("Effect initialized"), Fire->IsFireReady());
    TestEqual(TEXT("Uses original Character mesh"), Fire->GetTargetMesh(), Mesh);
    TestEqual(TEXT("Animation BP preserved"), Mesh->GetAnimClass(), AnimClass);
    TestEqual(TEXT("Animation mode preserved"), Mesh->GetAnimationMode(), Mode);
    TestEqual(TEXT("Collision preserved"), Mesh->GetCollisionEnabled(), Collision);
    TestTrue(TEXT("Mesh transform preserved"), Mesh->GetRelativeTransform().Equals(MeshTransform));
    TInlineComponentArray<USkeletalMeshComponent*> Meshes(Actor);
    TestEqual(TEXT("No duplicate creature mesh"), Meshes.Num(), 1);
    Fire->MouthSystem = nullptr;
    for (int32 I = 0; I < 3; ++I) Fire->RefreshFire();
    TInlineComponentArray<UNiagaraComponent*> Flames(Actor);
    TInlineComponentArray<UProceduralMeshComponent*> Mantles(Actor);
    TestEqual(TEXT("None skips both mouth emitters, even after repeated refresh"), Flames.Num(), 14);
    TestEqual(TEXT("One continuous back mantle"), Mantles.Num(), 1);
    int32 Mouths = 0;
    for (UNiagaraComponent* Flame : Flames)
    {
        TestEqual(TEXT("Each effect attached to existing mesh"), Flame->GetAttachParent(), static_cast<USceneComponent*>(Mesh));
        if (Flame->GetAttachSocketName() == FName(TEXT("head"))) ++Mouths;
    }
    TestEqual(TEXT("No head-attached smoke components when Mouth System is None"), Mouths, 0);
    TestTrue(TEXT("Back fire stays ready without mouth smoke"), Fire->IsFireReady());
    TestEqual(TEXT("Manual heat defaults to calm"), Fire->Heat, 0.f);
    Fire->SetFireHeat(1.f);
    for (int32 I = 0; I < 60; ++I) Fire->TickComponent(1.f / 30.f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Manual heat smoothly approaches requested value"), Fire->GetCurrentHeat() > .98f && Fire->GetCurrentHeat() <= 1.f);
    Fire->SetFireHeat(0.f);
    for (int32 I = 0; I < 60; ++I) Fire->TickComponent(1.f / 30.f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Manual heat returns to calm"), Fire->GetCurrentHeat() < .02f);
    TestEqual(TEXT("Manual strength defaults to full"), Fire->Strength, 1.f);
    Fire->SetFireStrength(.3f);
    for (int32 I = 0; I < 60; ++I) Fire->TickComponent(1.f / 30.f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Manual strength smoothly dims the fire"), FMath::IsNearlyEqual(Fire->GetCurrentStrength(), .3f, .02f));
    Fire->SetFireStrength(1.f);
    for (int32 I = 0; I < 60; ++I) Fire->TickComponent(1.f / 30.f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Manual strength returns to full"), Fire->GetCurrentStrength() > .98f);

    if (Mantles.Num() == 1)
    {
        for (const TCHAR* Name : {TEXT("AN_Janyeol_17_Attack_BackSpikes_Tackle_L"), TEXT("AN_Janyeol_18_Attack_Turn_Swipe")})
        {
            UAnimSequence* Animation = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Knight/Creatures/Janyeol/Animations/Attacks/%s.%s"), Name, Name));
            if (!TestNotNull(TEXT("Attack animation available"), Animation)) continue;
            Mesh->PlayAnimation(Animation, false);
            Mesh->SetPosition(.1f, false); Mesh->TickAnimation(0.f, false); Mesh->RefreshBoneTransforms();
            Fire->TickComponent(0.f, LEVELTICK_All, nullptr);
            FProcMeshSection* Section = Mantles[0]->GetProcMeshSection(0);
            if (!TestNotNull(TEXT("Mantle section created"), Section)) continue;
            const FVector Before = Section->ProcVertexBuffer[12].Position;
            Mesh->SetPosition(1.5f, false); Mesh->TickAnimation(0.f, false); Mesh->RefreshBoneTransforms();
            Fire->TickComponent(0.f, LEVELTICK_All, nullptr);
            Section = Mantles[0]->GetProcMeshSection(0);
            TestTrue(FString::Printf(TEXT("Mantle follows the skeletal pose in %s"), Name), FVector::Dist(Before, Section->ProcVertexBuffer[12].Position) > 1.f);
            TestEqual(TEXT("825 continuous mantle vertices"), Section->ProcVertexBuffer.Num(), 825);
            for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
                if (Vertex.Position.ContainsNaN()) { AddError(TEXT("Non-finite mantle vertex")); break; }
        }
    }
    Fire->SetFireEnabled(false);
    for (UNiagaraComponent* Flame : Flames) TestFalse(TEXT("Disable stops emission"), Flame->IsActive());
    if (Mantles.Num() == 1) TestFalse(TEXT("Disable hides mantle"), Mantles[0]->IsVisible());
    Fire->SetFireEnabled(true);
    TestTrue(TEXT("Enable keeps a valid effect"), Fire->IsFireReady());
    Fire->MouthSystem = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Knight/Creatures/Janyeol/VFX/Systems/NS_Janyeol_Mouth_Smoke.NS_Janyeol_Mouth_Smoke"));
    Fire->RefreshFire();
    TInlineComponentArray<UNiagaraComponent*> WithMouth(Actor);
    TestEqual(TEXT("Restoring mouth system creates exactly two extra emitters"), WithMouth.Num(), 16);
    Fire->MouthSystem = nullptr;
    Fire->TickComponent(1.f / 30.f, LEVELTICK_All, nullptr);
    TInlineComponentArray<UNiagaraComponent*> WithoutMouth(Actor);
    TestEqual(TEXT("Clearing system at runtime removes mouth emitters"), WithoutMouth.Num(), 14);
    TestTrue(TEXT("Back fire remains ready after removing mouth again"), Fire->IsFireReady());
    UMaterialInterface* ExpectedMaterial = CastChecked<ACharacter>(Class->GetDefaultObject())->GetMesh()->GetMaterial(0);
    Fire->DestroyComponent();
    TestEqual(TEXT("Removal restores original mesh material"), Mesh->GetMaterial(0), ExpectedMaterial);
    TInlineComponentArray<UNiagaraComponent*> RemainingFlames(Actor);
    TInlineComponentArray<UProceduralMeshComponent*> RemainingMantles(Actor);
    TestEqual(TEXT("No leaked Niagara components"), RemainingFlames.Num(), 0);
    TestEqual(TEXT("No leaked mantle component"), RemainingMantles.Num(), 0);
    World->DestroyWorld(false);
    return !HasAnyErrors();
}
#endif
