#pragma once
#include "Game/EFTypes.h"

namespace EFSiegeRules
{
inline bool IsVulnerable(EEFStructureKind Kind, bool bT1Alive, bool bT2Alive, bool bT3Alive, bool bAnyT4Alive)
{
    switch (Kind)
    {
    case EEFStructureKind::T1: return true;
    case EEFStructureKind::T2: return !bT1Alive;
    case EEFStructureKind::T3: return !bT2Alive;
    case EEFStructureKind::MeleeBarracks:
    case EEFStructureKind::RangedBarracks:
    case EEFStructureKind::T4: return !bT3Alive;
    case EEFStructureKind::Nexus: return !bAnyT4Alive;
    default: return false;
    }
}
}
