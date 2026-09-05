#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"

DECLARE_LOG_CATEGORY_EXTERN(LogEFCore, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEFNetwork, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEFMovement, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEFCombat, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEFAI, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEFSiege, Log, All);

namespace EFLog
{
inline const TCHAR* GetNetContext(const UObject* ContextObject)
{
    const UWorld* World = ContextObject ? ContextObject->GetWorld() : nullptr;
    if (!World)
    {
        return TEXT("NoWorld");
    }

    switch (World->GetNetMode())
    {
    case NM_Standalone:
        return TEXT("Standalone");
    case NM_DedicatedServer:
        return TEXT("DedicatedServer");
    case NM_ListenServer:
        return TEXT("ListenServer");
    case NM_Client:
        return TEXT("Client");
    default:
        return TEXT("Unknown");
    }
}
}
