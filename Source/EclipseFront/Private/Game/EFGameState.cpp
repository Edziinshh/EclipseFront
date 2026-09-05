#include "Game/EFGameState.h"

#include "Net/UnrealNetwork.h"

AEFGameState::AEFGameState()
{
    SetReplicates(true);
}

void AEFGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AEFGameState, MatchPhase);
    DOREPLIFETIME(AEFGameState, Winner);
    DOREPLIFETIME(AEFGameState, DawnUpgrades);
    DOREPLIFETIME(AEFGameState, DuskUpgrades);
}

void AEFGameState::SetMatchPhase(EEFMatchPhase NewPhase)
{
    if (HasAuthority())
    {
        MatchPhase = NewPhase;
        OnRep_MatchPhase();
    }
}

void AEFGameState::OnRep_MatchPhase()
{
    // Blueprint/UI hooks will be added when M0 gains its diagnostic HUD.
}

void AEFGameState::SetWinner(EEFMatchSide Side)
{
    if (HasAuthority() && Winner == EEFMatchSide::Unassigned)
    {
        Winner = Side;
        ForceNetUpdate();
    }
}

void AEFGameState::UnlockCreepUpgrade(EEFMatchSide Side, bool bRanged)
{
    if (!HasAuthority() || Side == EEFMatchSide::Unassigned) { return; }
    uint8& Upgrades = Side == EEFMatchSide::Dawn ? DawnUpgrades : DuskUpgrades;
    Upgrades |= bRanged ? 2 : 1;
    ForceNetUpdate();
}

bool AEFGameState::HasCreepUpgrade(EEFMatchSide Side, bool bRanged) const
{
    if (Side == EEFMatchSide::Unassigned) { return false; }
    return ((Side == EEFMatchSide::Dawn ? DawnUpgrades : DuskUpgrades) & (bRanged ? 2 : 1)) != 0;
}
