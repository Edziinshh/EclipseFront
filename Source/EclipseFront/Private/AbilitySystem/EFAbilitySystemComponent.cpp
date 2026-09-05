#include "AbilitySystem/EFAbilitySystemComponent.h"

UEFAbilitySystemComponent::UEFAbilitySystemComponent()
{
    SetIsReplicatedByDefault(true);
    SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

void UEFAbilitySystemComponent::RefreshAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
    if (!IsValid(InOwnerActor) || !IsValid(InAvatarActor))
    {
        return;
    }

    InitAbilityActorInfo(InOwnerActor, InAvatarActor);
}
