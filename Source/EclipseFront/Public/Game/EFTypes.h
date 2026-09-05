#pragma once

#include "CoreMinimal.h"
#include "EFTypes.generated.h"

UENUM(BlueprintType)
enum class EEFLoreFaction : uint8
{
    Unknown,
    Aurelion,
    Veyra,
    Outcast,
    Ancient
};

UENUM(BlueprintType)
enum class EEFMatchSide : uint8
{
    Unassigned,
    Dawn,
    Dusk
};

UENUM(BlueprintType)
enum class EEFMatchPhase : uint8
{
    WaitingForPlayers,
    HeroSelection,
    PreGame,
    InProgress,
    PostGame
};

UENUM(BlueprintType)
enum class EEFStructureKind : uint8
{
    T1, T2, T3, MeleeBarracks, RangedBarracks, T4, Nexus
};
