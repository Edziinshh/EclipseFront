#pragma once

#include "GameplayEffect.h"
#include "EFBasicAttackDamageEffect.generated.h"

/** Instant GAS effect used by the server-authoritative basic attack contact. */
UCLASS()
class ECLIPSEFRONT_API UEFBasicAttackDamageEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UEFBasicAttackDamageEffect();
};
