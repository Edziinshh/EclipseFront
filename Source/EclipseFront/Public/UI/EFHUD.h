#pragma once

#include "GameFramework/HUD.h"
#include "EFHUD.generated.h"

UCLASS()
class ECLIPSEFRONT_API AEFHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
};
