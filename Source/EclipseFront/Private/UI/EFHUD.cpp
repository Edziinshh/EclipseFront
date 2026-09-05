#include "UI/EFHUD.h"

#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Player/EFPlayerState.h"
#include "EngineUtils.h"
#include "Game/EFGameState.h"
#include "Units/EFLaneTower.h"

void AEFHUD::DrawHUD()
{
    Super::DrawHUD();

    const APlayerController* PlayerController = GetOwningPlayerController();
    const AEFPlayerState* PlayerState = PlayerController
        ? PlayerController->GetPlayerState<AEFPlayerState>()
        : nullptr;
    if (!PlayerState || !GEngine)
    {
        return;
    }

    const int32 RequiredExperience = PlayerState->GetExperienceForNextLevel();
    const FString ProgressionText = RequiredExperience > 0
        ? FString::Printf(TEXT("LEVEL %d    XP %d / %d    GOLD %d    LH %d"),
            PlayerState->GetHeroLevel(),
            PlayerState->GetExperience(),
            RequiredExperience,
            PlayerState->GetGold(),
            PlayerState->GetLastHits())
        : FString::Printf(TEXT("LEVEL MAX    GOLD %d    LH %d"),
            PlayerState->GetGold(),
            PlayerState->GetLastHits());

    DrawText(
        ProgressionText,
        FLinearColor(1.0f, 0.85f, 0.15f),
        36.0f,
        32.0f,
        GEngine->GetMediumFont(),
        1.25f,
        false);

    const AEFGameState* Match = GetWorld()->GetGameState<AEFGameState>();
    if (Match && Match->GetWinner() != EEFMatchSide::Unassigned)
    {
        DrawText(Match->GetWinner() == PlayerState->GetMatchSide() ? TEXT("VICTORY") : TEXT("DEFEAT"),
            FLinearColor::Yellow, 260, 130, GEngine->GetLargeFont(), 2.0f);
        DrawText(TEXT("Match ended. Stop Play and start again for a new match."),
            FLinearColor::White, 260, 195, GEngine->GetMediumFont());
    }
    for (TActorIterator<AEFLaneTower> It(GetWorld()); It; ++It)
    {
        if (It->IsDead()) { continue; }
        FVector2D Screen;
        if (PlayerController->ProjectWorldLocationToScreen(It->GetActorLocation() + FVector(0, 0, 400), Screen))
        {
            const FString Name = StaticEnum<EEFStructureKind>()->GetNameStringByValue(static_cast<int64>(It->GetStructureKind()));
            DrawText(Name + (It->IsVulnerable() ? TEXT("") : TEXT(" [PROTECTED]")),
                It->IsVulnerable() ? FLinearColor::White : FLinearColor::Gray,
                Screen.X - 55, Screen.Y, GEngine->GetSmallFont());
        }
    }
}
