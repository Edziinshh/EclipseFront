#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "Game/EFTypes.h"
#include "EFPlayerState.generated.h"

class UEFAbilitySystemComponent;
class UEFHeroAttributeSet;

UCLASS()
class ECLIPSEFRONT_API AEFPlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AEFPlayerState();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Abilities")
    UEFAbilitySystemComponent* GetEFAbilitySystemComponent() const { return AbilitySystemComponent; }

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Abilities")
    const UEFHeroAttributeSet* GetHeroAttributeSet() const { return HeroAttributeSet; }

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Progression")
    int32 GetHeroLevel() const { return HeroLevel; }

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Progression")
    int32 GetExperience() const { return Experience; }

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Progression")
    int32 GetGold() const { return Gold; }

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Progression")
    int32 GetLastHits() const { return LastHits; }

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Progression")
    int32 GetExperienceForNextLevel() const;

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Match")
    EEFMatchSide GetMatchSide() const { return MatchSide; }

    void SetMatchSide(EEFMatchSide NewSide);
    void AddExperience(int32 Amount);
    bool SpendGold(int32 Amount);
    void AddGold(int32 Amount);
    void AwardCreepLastHit(int32 GoldAmount);
    void ResetCombatAttributesForRespawn();
    void RecordKill();
    void RecordDeath();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|Abilities")
    TObjectPtr<UEFAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY()
    TObjectPtr<UEFHeroAttributeSet> HeroAttributeSet;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Progression, Category="Eclipse Front|Progression")
    int32 HeroLevel = 1;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Progression, Category="Eclipse Front|Progression")
    int32 Experience = 0;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Progression, Category="Eclipse Front|Progression")
    int32 Gold = 600;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Progression, Category="Eclipse Front|Progression")
    int32 LastHits = 0;

    UPROPERTY(BlueprintReadOnly, Replicated, Category="Eclipse Front|Score")
    int32 Kills = 0;

    UPROPERTY(BlueprintReadOnly, Replicated, Category="Eclipse Front|Score")
    int32 Deaths = 0;

    UPROPERTY(BlueprintReadOnly, Replicated, Category="Eclipse Front|Score")
    int32 Assists = 0;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MatchSide, Category="Eclipse Front|Match")
    EEFMatchSide MatchSide = EEFMatchSide::Unassigned;

    UFUNCTION() void OnRep_Progression();
    UFUNCTION() void OnRep_MatchSide();
};
