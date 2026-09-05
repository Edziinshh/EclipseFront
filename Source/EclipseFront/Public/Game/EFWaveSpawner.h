#pragma once

#include "GameFramework/Actor.h"
#include "Game/EFTypes.h"
#include "EFWaveSpawner.generated.h"

class AEFCreepCharacter;

UCLASS()
class ECLIPSEFRONT_API AEFWaveSpawner : public AActor
{
    GENERATED_BODY()

public:
    AEFWaveSpawner();

    void StartSpawning();
    void StopSpawning();

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M2")
    TSubclassOf<AEFCreepCharacter> CreepClass;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M2", meta=(ClampMin="1"))
    int32 CreepsPerSide = 3;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M2", meta=(ClampMin="5.0"))
    float WaveInterval = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M2", meta=(ClampMin="0.0"))
    float InitialWaveDelay = 2.0f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M2", meta=(ClampMin="500.0"))
    // Keep wave spawns on the baked navigation surface, between T4 and Nexus.
    float RouteHalfLength = 5900.0f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M2", meta=(ClampMin="50.0"))
    float CreepSpacing = 140.0f;

private:
    void SpawnWave();
    void SpawnSideWave(EEFMatchSide Side);
    TArray<FVector> BuildRoute(EEFMatchSide Side, float LaneOffset) const;
    bool ProjectToLane(const FVector& DesiredLocation, FVector& OutLocation) const;

    FTimerHandle WaveTimerHandle;
    bool bStarted = false;
};
