#include "Game/EFSiegeRules.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/EFGameState.h"
#include "Units/EFLaneTower.h"
#include "Combat/EFCombatComponent.h"
#include "AbilitySystem/EFHeroAttributeSet.h"
#include "AbilitySystemComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFSiegeRulesTest, "EclipseFront.Siege.ProtectionSequence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEFSiegeRulesTest::RunTest(const FString& Parameters)
{
    using namespace EFSiegeRules;
    TestTrue(TEXT("T1 starts open"), IsVulnerable(EEFStructureKind::T1, true, true, true, true));
    for (EEFStructureKind Kind : {EEFStructureKind::T2, EEFStructureKind::T3,
        EEFStructureKind::MeleeBarracks, EEFStructureKind::RangedBarracks, EEFStructureKind::T4, EEFStructureKind::Nexus})
    { TestFalse(TEXT("Inner objectives start protected"), IsVulnerable(Kind, true, true, true, true)); }
    TestTrue(TEXT("T1 death opens T2"), IsVulnerable(EEFStructureKind::T2, false, true, true, true));
    TestFalse(TEXT("T1 death cannot skip T2"), IsVulnerable(EEFStructureKind::T3, false, true, true, true));
    TestTrue(TEXT("T2 death opens T3"), IsVulnerable(EEFStructureKind::T3, false, false, true, true));
    for (EEFStructureKind Kind : {EEFStructureKind::MeleeBarracks, EEFStructureKind::RangedBarracks, EEFStructureKind::T4})
    { TestTrue(TEXT("T3 death opens barracks and both T4"), IsVulnerable(Kind, false, false, false, true)); }
    TestFalse(TEXT("One remaining T4 protects Nexus"), IsVulnerable(EEFStructureKind::Nexus, false, false, false, true));
    TestTrue(TEXT("Both T4 dead opens Nexus"), IsVulnerable(EEFStructureKind::Nexus, false, false, false, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFSiegeDamageTest, "EclipseFront.Siege.ServerDamageAndUpgrades",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEFSiegeDamageTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    AEFGameState* State = World->SpawnActor<AEFGameState>();
    World->SetGameState(State);
    AEFLaneTower* Source = World->SpawnActor<AEFLaneTower>();
    AEFLaneTower* Target = World->SpawnActor<AEFLaneTower>();
    Source->GetAbilitySystemComponent()->AddAttributeSetSubobject(
        const_cast<UEFHeroAttributeSet*>(Source->GetTowerAttributeSet()));
    Target->GetAbilitySystemComponent()->AddAttributeSetSubobject(
        const_cast<UEFHeroAttributeSet*>(Target->GetTowerAttributeSet()));
    Source->DispatchBeginPlay();
    Target->DispatchBeginPlay();
    Source->InitializeStructure(EEFMatchSide::Dawn, EEFStructureKind::T1);
    Target->InitializeStructure(EEFMatchSide::Dusk, EEFStructureKind::T2);
    const float HealthBefore = Target->GetTowerAttributeSet()->GetHealth();
    TestFalse(TEXT("Protected objective rejects attack order"), Source->GetCombatComponent()->BeginBasicAttack(Target));
    TestFalse(TEXT("Protected objective rejects impact"), Source->GetCombatComponent()->ResolveBasicAttackImpact(Target));
    TestEqual(TEXT("Protected HP unchanged"), Target->GetTowerAttributeSet()->GetHealth(), HealthBefore);
    Target->SetVulnerable(true);
    TestTrue(TEXT("Open hostile objective accepts GAS impact"), Source->GetCombatComponent()->ResolveBasicAttackImpact(Target));
    TestTrue(TEXT("GAS damage reduces HP"), Target->GetTowerAttributeSet()->GetHealth() < HealthBefore);
    State->UnlockCreepUpgrade(EEFMatchSide::Dawn, false);
    TestTrue(TEXT("Melee upgrade awarded"), State->HasCreepUpgrade(EEFMatchSide::Dawn, false));
    TestFalse(TEXT("Ranged upgrade unaffected"), State->HasCreepUpgrade(EEFMatchSide::Dawn, true));
    TestFalse(TEXT("Other side unaffected"), State->HasCreepUpgrade(EEFMatchSide::Dusk, false));
    State->SetWinner(EEFMatchSide::Dawn);
    State->SetWinner(EEFMatchSide::Dusk);
    TestEqual(TEXT("Winner cannot change"), State->GetWinner(), EEFMatchSide::Dawn);
    State->SetMatchPhase(EEFMatchPhase::PostGame);
    TestFalse(TEXT("Postgame rejects impact"), Source->GetCombatComponent()->ResolveBasicAttackImpact(Target));
    TestFalse(TEXT("Postgame rejects order"), Source->GetCombatComponent()->BeginBasicAttack(Target));
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}
#endif
