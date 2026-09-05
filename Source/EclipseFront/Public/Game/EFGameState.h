#pragma once

#include "GameFramework/GameState.h"
#include "Game/EFTypes.h"
#include "EFGameState.generated.h"

UCLASS()
class ECLIPSEFRONT_API AEFGameState : public AGameState
{
    GENERATED_BODY()

public:
    AEFGameState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Match")
    EEFMatchPhase GetMatchPhase() const { return MatchPhase; }

    void SetMatchPhase(EEFMatchPhase NewPhase);
    void SetWinner(EEFMatchSide Side);
    EEFMatchSide GetWinner() const { return Winner; }
    void UnlockCreepUpgrade(EEFMatchSide Side, bool bRanged);
    bool HasCreepUpgrade(EEFMatchSide Side, bool bRanged) const;

protected:
    UPROPERTY(Replicated)
    EEFMatchSide Winner = EEFMatchSide::Unassigned;
    UPROPERTY(Replicated)
    uint8 DawnUpgrades = 0;
    UPROPERTY(Replicated)
    uint8 DuskUpgrades = 0;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MatchPhase, Category="Eclipse Front|Match")
    EEFMatchPhase MatchPhase = EEFMatchPhase::WaitingForPlayers;

    UFUNCTION()
    void OnRep_MatchPhase();
};
