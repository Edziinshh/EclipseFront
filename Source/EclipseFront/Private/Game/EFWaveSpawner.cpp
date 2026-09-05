#include "Game/EFWaveSpawner.h"

#include "Components/CapsuleComponent.h"
#include "Debug/EFLogCategories.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Units/EFCreepCharacter.h"
#include "Game/EFGameState.h"

AEFWaveSpawner::AEFWaveSpawner()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    CreepClass = AEFCreepCharacter::StaticClass();
}

void AEFWaveSpawner::StartSpawning()
{
    if (!HasAuthority() || bStarted || !CreepClass)
    {
        return;
    }

    bStarted = true;
    GetWorldTimerManager().SetTimer(
        WaveTimerHandle,
        this,
        &AEFWaveSpawner::SpawnWave,
        WaveInterval,
        true,
        InitialWaveDelay);
    UE_LOG(LogEFAI, Display, TEXT("[%s] WaveSpawner started interval=%.1f initialDelay=%.1f"),
        EFLog::GetNetContext(this), WaveInterval, InitialWaveDelay);
}

void AEFWaveSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(WaveTimerHandle);
    Super::EndPlay(EndPlayReason);
}

void AEFWaveSpawner::StopSpawning()
{
    GetWorldTimerManager().ClearTimer(WaveTimerHandle);
    bStarted = false;
}

void AEFWaveSpawner::SpawnWave()
{
    const AEFGameState* Match = GetWorld()->GetGameState<AEFGameState>();
    if (!HasAuthority() || !bStarted || (Match && Match->GetMatchPhase() == EEFMatchPhase::PostGame))
    {
        return;
    }

    SpawnSideWave(EEFMatchSide::Dawn);
    SpawnSideWave(EEFMatchSide::Dusk);
    UE_LOG(LogEFAI, Display, TEXT("[%s] Wave spawned sides=Dawn,Dusk"), EFLog::GetNetContext(this));
}

void AEFWaveSpawner::SpawnSideWave(EEFMatchSide Side)
{
    const float CenteredIndexOffset = (CreepsPerSide - 1) * 0.5f;
    for (int32 CreepIndex = 0; CreepIndex < CreepsPerSide; ++CreepIndex)
    {
        const float LaneOffset = (CreepIndex - CenteredIndexOffset) * CreepSpacing;
        TArray<FVector> Route = BuildRoute(Side, LaneOffset);
        if (Route.Num() < 2)
        {
            continue;
        }

        const AEFCreepCharacter* CreepDefaults = CreepClass->GetDefaultObject<AEFCreepCharacter>();
        const float CapsuleHalfHeight = CreepDefaults
            ? CreepDefaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
            : 58.0f;
        const FVector SpawnLocation = Route[0] + FVector(0.0f, 0.0f, CapsuleHalfHeight);
        const FRotator SpawnRotation(0.0f, Side == EEFMatchSide::Dawn ? 0.0f : 180.0f, 0.0f);

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = this;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        AEFCreepCharacter* Creep = GetWorld()->SpawnActor<AEFCreepCharacter>(
            CreepClass, SpawnLocation, SpawnRotation, SpawnParameters);
        if (Creep)
        {
            Creep->InitializeLaneCreep(Side, Route);
            const bool bRanged = CreepIndex == CreepsPerSide - 1;
            const AEFGameState* State = GetWorld()->GetGameState<AEFGameState>();
            Creep->ConfigureWaveUnit(bRanged, State && State->HasCreepUpgrade(Side, bRanged));
        }
    }
}

TArray<FVector> AEFWaveSpawner::BuildRoute(EEFMatchSide Side, float LaneOffset) const
{
    const float Direction = Side == EEFMatchSide::Dawn ? 1.0f : -1.0f;
    const FVector Center = GetActorLocation();
    // Do not place a waypoint at the lane center: the M0 test wall occupies it,
    // and projection can select an isolated NavMesh polygon on top of the wall.
    // The path between the two inner points naturally routes around the wall.
    const float RouteSamples[] = {-1.0f, -0.5f, 0.5f, 1.0f};
    TArray<FVector> Route;
    Route.Reserve(UE_ARRAY_COUNT(RouteSamples));

    for (const float RouteSample : RouteSamples)
    {
        const FVector DesiredLocation = Center + FVector(
            Direction * RouteSample * RouteHalfLength,
            LaneOffset,
            0.0f);
        FVector ProjectedLocation;
        if (!ProjectToLane(DesiredLocation, ProjectedLocation))
        {
            UE_LOG(LogEFMovement, Warning,
                TEXT("[%s] LaneRoute rejected reason=NavProjectionFailed point=%s"),
                EFLog::GetNetContext(this), *DesiredLocation.ToCompactString());
            Route.Reset();
            return Route;
        }
        Route.Add(ProjectedLocation);
    }

    return Route;
}

bool AEFWaveSpawner::ProjectToLane(const FVector& DesiredLocation, FVector& OutLocation) const
{
    const UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavigationSystem)
    {
        return false;
    }

    FNavLocation NavLocation;
    if (!NavigationSystem->ProjectPointToNavigation(
        DesiredLocation,
        NavLocation,
        FVector(350.0f, 350.0f, 1000.0f)))
    {
        return false;
    }

    OutLocation = NavLocation.Location;
    return true;
}
