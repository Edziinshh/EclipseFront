#pragma once

#include "GameFramework/GameMode.h"
#include "EFGameMode.generated.h"

class AEFHeroCharacter;
class AEFLaneTower;
class AEFPlayerController;
class AEFWaveSpawner;
class AEFHUD;

UCLASS()
class ECLIPSEFRONT_API AEFGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    AEFGameMode();

    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void RestartPlayer(AController* NewPlayer) override;

    void HandleHeroDeath(AEFHeroCharacter* DeadHero, class AEFPlayerState* VictimPlayerState, class AEFPlayerState* KillerPlayerState);
    void HandleStructureDestroyed(AEFLaneTower* Structure);

protected:
    virtual void HandleMatchHasStarted() override;
    virtual void HandleMatchHasEnded() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Eclipse Front|M0")
    TSubclassOf<AEFHeroCharacter> HeroClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Eclipse Front|M2")
    TSubclassOf<AEFWaveSpawner> WaveSpawnerClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Eclipse Front|M3")
    TSubclassOf<AEFLaneTower> TowerClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Eclipse Front|M0", meta=(ClampMin="1"))
    int32 PlayersRequiredToStart = 2;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Eclipse Front|M0")
    bool bUseFallbackSpawnsWhenNoPlayerStarts = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Eclipse Front|M0", meta=(ClampMin="100.0"))
    float FallbackSpawnSeparation = 11800.0f;

private:
    void SpawnHeroFor(AEFPlayerController& PlayerController);
    void RespawnHero(TWeakObjectPtr<AEFPlayerController> PlayerController);
    void SpawnPrototypeTowers();
    void RefreshStructureProtection();
    void RefreshWaitingPhase();
    FTransform GetFallbackSpawnTransform(const AController& Controller) const;

    UPROPERTY()
    TObjectPtr<AEFWaveSpawner> ActiveWaveSpawner;

    UPROPERTY()
    TArray<TObjectPtr<AEFLaneTower>> ActiveLaneTowers;
};
