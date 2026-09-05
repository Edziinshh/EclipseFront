#include "Game/EFGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Debug/EFLogCategories.h"
#include "Game/EFGameState.h"
#include "Game/EFSiegeRules.h"
#include "Game/EFWaveSpawner.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationSystem.h"
#include "Player/EFMOBACameraPawn.h"
#include "Player/EFPlayerController.h"
#include "Player/EFPlayerState.h"
#include "TimerManager.h"
#include "UI/EFHUD.h"
#include "Units/EFHeroCharacter.h"
#include "Units/EFLaneTower.h"
#include "Units/EFCreepCharacter.h"
#include "Units/EFTowerProjectile.h"
#include "Combat/EFCombatComponent.h"

AEFGameMode::AEFGameMode()
{
    GameStateClass = AEFGameState::StaticClass();
    PlayerStateClass = AEFPlayerState::StaticClass();
    PlayerControllerClass = AEFPlayerController::StaticClass();
    DefaultPawnClass = AEFMOBACameraPawn::StaticClass();
    HeroClass = AEFHeroCharacter::StaticClass();
    WaveSpawnerClass = AEFWaveSpawner::StaticClass();
    TowerClass = AEFLaneTower::StaticClass();
    HUDClass = AEFHUD::StaticClass();

    bDelayedStart = true;
    MinRespawnDelay = 5.0f;
}

void AEFGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    AEFPlayerController* EFController = Cast<AEFPlayerController>(NewPlayer);
    if (!EFController)
    {
        return;
    }

    if (AEFPlayerState* EFPlayerState = EFController->GetPlayerState<AEFPlayerState>())
    {
        int32 DawnPlayers = 0;
        int32 DuskPlayers = 0;
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            const AEFPlayerController* ExistingController = Cast<AEFPlayerController>(It->Get());
            const AEFPlayerState* ExistingState = ExistingController
                ? ExistingController->GetPlayerState<AEFPlayerState>()
                : nullptr;
            if (!ExistingState || ExistingState == EFPlayerState)
            {
                continue;
            }

            DawnPlayers += ExistingState->GetMatchSide() == EEFMatchSide::Dawn ? 1 : 0;
            DuskPlayers += ExistingState->GetMatchSide() == EEFMatchSide::Dusk ? 1 : 0;
        }

        const EEFMatchSide AssignedSide = DawnPlayers <= DuskPlayers ? EEFMatchSide::Dawn : EEFMatchSide::Dusk;
        EFPlayerState->SetMatchSide(AssignedSide);
        UE_LOG(LogEFNetwork, Display, TEXT("[%s] MatchSide assigned player=%s side=%s"),
            EFLog::GetNetContext(this), *EFPlayerState->GetPlayerName(),
            AssignedSide == EEFMatchSide::Dawn ? TEXT("Dawn") : TEXT("Dusk"));
    }

    SpawnHeroFor(*EFController);

    if (GetNumPlayers() >= PlayersRequiredToStart && !HasMatchStarted())
    {
        StartMatch();
    }
    else
    {
        RefreshWaitingPhase();
    }
}

void AEFGameMode::Logout(AController* Exiting)
{
    if (const AEFPlayerController* EFController = Cast<AEFPlayerController>(Exiting))
    {
        if (AEFHeroCharacter* Hero = EFController->GetControlledHero())
        {
            Hero->Destroy();
        }
    }

    Super::Logout(Exiting);
    RefreshWaitingPhase();
}

void AEFGameMode::RestartPlayer(AController* NewPlayer)
{
    if (!NewPlayer)
    {
        return;
    }

    int32 PlayerStartCount = 0;
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        ++PlayerStartCount;
    }

    if (PlayerStartCount >= PlayersRequiredToStart)
    {
        Super::RestartPlayer(NewPlayer);
        return;
    }

    if (bUseFallbackSpawnsWhenNoPlayerStarts)
    {
        RestartPlayerAtTransform(NewPlayer, GetFallbackSpawnTransform(*NewPlayer));
        return;
    }

    Super::RestartPlayer(NewPlayer);
}

void AEFGameMode::HandleMatchHasStarted()
{
    Super::HandleMatchHasStarted();
    if (AEFGameState* EFGameState = GetGameState<AEFGameState>())
    {
        EFGameState->SetMatchPhase(EEFMatchPhase::InProgress);
    }

    if (HasAuthority() && WaveSpawnerClass && !IsValid(ActiveWaveSpawner))
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = this;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ActiveWaveSpawner = GetWorld()->SpawnActor<AEFWaveSpawner>(
            WaveSpawnerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
        if (ActiveWaveSpawner)
        {
            ActiveWaveSpawner->StartSpawning();
        }
    }

    SpawnPrototypeTowers();
}

void AEFGameMode::SpawnPrototypeTowers()
{
    if (!HasAuthority() || !TowerClass || ActiveLaneTowers.Num() > 0)
    {
        return;
    }

    const UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
    struct FTowerSpawnDefinition
    {
        EEFStructureKind Kind;
        float X;
        float Y;
    };
    const FTowerSpawnDefinition TowerSpawns[] = {
        {EEFStructureKind::T1, 1800.0f, 0.0f},
        {EEFStructureKind::T2, 3100.0f, 0.0f},
        {EEFStructureKind::T3, 4400.0f, 0.0f},
        {EEFStructureKind::MeleeBarracks, 4800.0f, -240.0f},
        {EEFStructureKind::RangedBarracks, 4800.0f, 240.0f},
        {EEFStructureKind::T4, 5500.0f, -260.0f},
        {EEFStructureKind::T4, 5500.0f, 260.0f},
        {EEFStructureKind::Nexus, 6200.0f, 0.0f}
    };

    for (EEFMatchSide Side : {EEFMatchSide::Dawn, EEFMatchSide::Dusk})
    {
    for (const FTowerSpawnDefinition& Definition : TowerSpawns)
    {
        const float Sign = Side == EEFMatchSide::Dawn ? -1.0f : 1.0f;
        FVector SpawnLocation(Sign * Definition.X, Definition.Y, 0.0f);
        FNavLocation NavLocation;
        if (NavigationSystem && NavigationSystem->ProjectPointToNavigation(
            SpawnLocation, NavLocation, FVector(300.0f, 300.0f, 1000.0f)))
        {
            SpawnLocation = NavLocation.Location;
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = this;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AEFLaneTower* Tower = GetWorld()->SpawnActor<AEFLaneTower>(
            TowerClass, SpawnLocation, FRotator::ZeroRotator, SpawnParameters);
        if (Tower)
        {
            Tower->InitializeStructure(Side, Definition.Kind);
            ActiveLaneTowers.Add(Tower);
            UE_LOG(LogEFSiege, Verbose, TEXT("[%s] Structure spawned side=%s kind=%d location=%s"),
                EFLog::GetNetContext(this), Side == EEFMatchSide::Dawn ? TEXT("Dawn") : TEXT("Dusk"),
                static_cast<int32>(Definition.Kind),
                *SpawnLocation.ToCompactString());
        }
    }
    }
    RefreshStructureProtection();
}

void AEFGameMode::RefreshStructureProtection()
{
    for (AEFLaneTower* Building : ActiveLaneTowers)
    {
        if (!IsValid(Building) || Building->IsDead()) { continue; }
        const auto HasLiving = [this, Building](EEFStructureKind Kind)
        {
            for (const AEFLaneTower* Other : ActiveLaneTowers)
            {
                if (IsValid(Other) && !Other->IsDead() && Other->GetMatchSide() == Building->GetMatchSide()
                    && Other->GetStructureKind() == Kind) { return true; }
            }
            return false;
        };
        const bool bOpen = EFSiegeRules::IsVulnerable(Building->GetStructureKind(),
            HasLiving(EEFStructureKind::T1), HasLiving(EEFStructureKind::T2),
            HasLiving(EEFStructureKind::T3), HasLiving(EEFStructureKind::T4));
        Building->SetVulnerable(bOpen);
    }
}

void AEFGameMode::HandleStructureDestroyed(AEFLaneTower* Structure)
{
    if (!HasAuthority() || HasMatchEnded() || !IsValid(Structure) || !Structure->IsDead()) { return; }
    AEFGameState* State = GetGameState<AEFGameState>();
    const EEFMatchSide Opponent = Structure->GetMatchSide() == EEFMatchSide::Dawn ? EEFMatchSide::Dusk : EEFMatchSide::Dawn;
    const EEFStructureKind Kind = Structure->GetStructureKind();
    if (State && (Kind == EEFStructureKind::MeleeBarracks || Kind == EEFStructureKind::RangedBarracks))
    {
        State->UnlockCreepUpgrade(Opponent, Kind == EEFStructureKind::RangedBarracks);
        UE_LOG(LogEFSiege, Display, TEXT("[%s] Barracks destroyed upgradedSide=%d ranged=%d"),
            EFLog::GetNetContext(this), static_cast<int32>(Opponent), Kind == EEFStructureKind::RangedBarracks);
    }
    if (State && Kind == EEFStructureKind::Nexus)
    {
        State->SetWinner(Opponent);
        EndMatch();
        UE_LOG(LogEFSiege, Display, TEXT("[%s] Nexus destroyed winner=%d"),
            EFLog::GetNetContext(this), static_cast<int32>(Opponent));
    }
    else { RefreshStructureProtection(); }
}

void AEFGameMode::HandleMatchHasEnded()
{
    Super::HandleMatchHasEnded();
    if (AEFGameState* EFGameState = GetGameState<AEFGameState>())
    {
        EFGameState->SetMatchPhase(EEFMatchPhase::PostGame);
    }
    if (ActiveWaveSpawner) { ActiveWaveSpawner->StopSpawning(); }
    for (TActorIterator<AEFTowerProjectile> It(GetWorld()); It; ++It) { It->Destroy(); }
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (UEFCombatComponent* Combat = It->FindComponentByClass<UEFCombatComponent>()) { Combat->CancelBasicAttack(); }
        if (AEFHeroCharacter* Hero = Cast<AEFHeroCharacter>(*It)) { Hero->CancelDirectMove(); Hero->StopMovementForAttack(); }
        if (AEFCreepCharacter* Creep = Cast<AEFCreepCharacter>(*It)) { Creep->StopMovementForAttack(); }
    }
}

void AEFGameMode::HandleHeroDeath(AEFHeroCharacter* DeadHero, AEFPlayerState* VictimPlayerState, AEFPlayerState* KillerPlayerState)
{
    if (!HasAuthority() || !IsValid(DeadHero) || !VictimPlayerState)
    {
        return;
    }

    VictimPlayerState->RecordDeath();
    if (KillerPlayerState && KillerPlayerState != VictimPlayerState)
    {
        KillerPlayerState->RecordKill();
    }

    AEFPlayerController* VictimController = Cast<AEFPlayerController>(DeadHero->GetOwner());
    if (!VictimController)
    {
        return;
    }

    VictimController->SetControlledHero(nullptr);
    DeadHero->SetLifeSpan(FMath::Min(1.5f, MinRespawnDelay));

    FTimerDelegate RespawnDelegate = FTimerDelegate::CreateUObject(
        this, &AEFGameMode::RespawnHero, TWeakObjectPtr<AEFPlayerController>(VictimController));
    FTimerHandle RespawnTimer;
    GetWorldTimerManager().SetTimer(RespawnTimer, RespawnDelegate, MinRespawnDelay, false);
}

void AEFGameMode::SpawnHeroFor(AEFPlayerController& PlayerController)
{
    if (!HeroClass || PlayerController.GetControlledHero())
    {
        return;
    }

    int32 PlayerStartCount = 0;
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        ++PlayerStartCount;
    }

    AActor* StartSpot = PlayerStartCount >= PlayersRequiredToStart ? FindPlayerStart(&PlayerController) : nullptr;
    const FTransform SpawnTransform = StartSpot
        ? StartSpot->GetActorTransform()
        : GetFallbackSpawnTransform(PlayerController);

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = &PlayerController;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AEFHeroCharacter* Hero = GetWorld()->SpawnActor<AEFHeroCharacter>(HeroClass, SpawnTransform, SpawnParameters);
    if (!Hero)
    {
        return;
    }

    Hero->SpawnDefaultController();
    PlayerController.SetControlledHero(Hero);
    UE_LOG(LogEFNetwork, Display, TEXT("[%s] Hero spawned side=%s hero=%s location=%s"),
        EFLog::GetNetContext(this),
        PlayerController.GetPlayerState<AEFPlayerState>() && PlayerController.GetPlayerState<AEFPlayerState>()->GetMatchSide() == EEFMatchSide::Dusk
            ? TEXT("Dusk")
            : TEXT("Dawn"),
        *Hero->GetName(),
        *Hero->GetActorLocation().ToCompactString());

    if (AEFPlayerState* EFPlayerState = PlayerController.GetPlayerState<AEFPlayerState>())
    {
        EFPlayerState->ResetCombatAttributesForRespawn();
    }

    if (APawn* CameraPawn = PlayerController.GetPawn())
    {
        CameraPawn->SetActorLocation(Hero->GetActorLocation());
    }
}

void AEFGameMode::RespawnHero(TWeakObjectPtr<AEFPlayerController> PlayerController)
{
    if (!HasMatchEnded() && PlayerController.IsValid() && !PlayerController->GetControlledHero())
    {
        SpawnHeroFor(*PlayerController.Get());
    }
}

FTransform AEFGameMode::GetFallbackSpawnTransform(const AController& Controller) const
{
    int32 PlayerIndex = 0;
    const APlayerState* ControllerPlayerState = Controller.GetPlayerState<APlayerState>();
    if (GameState && ControllerPlayerState)
    {
        PlayerIndex = FMath::Max(0, GameState->PlayerArray.IndexOfByKey(ControllerPlayerState));
    }

    const AEFPlayerState* EFPlayerState = Controller.GetPlayerState<AEFPlayerState>();
    const float Side = EFPlayerState && EFPlayerState->GetMatchSide() == EEFMatchSide::Dusk
        ? 1.0f
        : (EFPlayerState && EFPlayerState->GetMatchSide() == EEFMatchSide::Dawn
            ? -1.0f
            : ((PlayerIndex % 2) == 0 ? -1.0f : 1.0f));
    // Spawn inside the baked lane navigation and offset from the Nexus/T4 chain.
    FVector SpawnLocation(Side * FallbackSpawnSeparation * 0.5f, 650.0f, 500.0f);

    const UNavigationSystemV1* NavigationSystem = GetWorld()
        ? UNavigationSystemV1::GetCurrent(GetWorld())
        : nullptr;
    FNavLocation ProjectedSpawn;
    if (NavigationSystem && NavigationSystem->ProjectPointToNavigation(
        SpawnLocation, ProjectedSpawn, FVector(500.0f, 500.0f, 1000.0f)))
    {
        SpawnLocation = ProjectedSpawn.Location + FVector(0.0f, 0.0f, 120.0f);
    }
    else
    {
        UE_LOG(LogEFMovement, Error,
            TEXT("[%s] HeroSpawn invalid reason=OutsideNavMesh location=%s movementAvailable=0"),
            EFLog::GetNetContext(this), *SpawnLocation.ToCompactString());
    }

    FHitResult GroundHit;
    const FVector TraceStart(SpawnLocation.X, SpawnLocation.Y, 100000.0f);
    const FVector TraceEnd(SpawnLocation.X, SpawnLocation.Y, -100000.0f);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EFFallbackSpawn), false);
    if (GetWorld() && GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
    {
        SpawnLocation.Z = GroundHit.ImpactPoint.Z + 120.0f;
    }

    const FRotator SpawnRotation(0.0f, Side < 0.0f ? 0.0f : 180.0f, 0.0f);
    return FTransform(SpawnRotation, SpawnLocation);
}

void AEFGameMode::RefreshWaitingPhase()
{
    if (!HasMatchStarted())
    {
        if (AEFGameState* EFGameState = GetGameState<AEFGameState>())
        {
            EFGameState->SetMatchPhase(EEFMatchPhase::WaitingForPlayers);
        }
    }
}
