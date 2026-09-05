#pragma once

#include "AbilitySystemComponent.h"
#include "EFAbilitySystemComponent.generated.h"

UCLASS(ClassGroup=(EclipseFront), meta=(BlueprintSpawnableComponent))
class ECLIPSEFRONT_API UEFAbilitySystemComponent : public UAbilitySystemComponent
{
    GENERATED_BODY()

public:
    UEFAbilitySystemComponent();

    UFUNCTION(BlueprintCallable, Category="Eclipse Front|Abilities")
    void RefreshAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor);
};
