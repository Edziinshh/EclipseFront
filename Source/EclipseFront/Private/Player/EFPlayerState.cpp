#include "Player/EFPlayerState.h"

#include "AbilitySystem/EFAbilitySystemComponent.h"
#include "AbilitySystem/EFHeroAttributeSet.h"
#include "Debug/EFLogCategories.h"
#include "Game/EFGameplayTags.h"
#include "Net/UnrealNetwork.h"

AEFPlayerState::AEFPlayerState()
{
    SetNetUpdateFrequency(100.0f);

    AbilitySystemComponent = CreateDefaultSubobject<UEFAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    HeroAttributeSet = CreateDefaultSubobject<UEFHeroAttributeSet>(TEXT("HeroAttributeSet"));

    // Temporary M3 siege-test tuning: player heroes can clear each objective in one hit.
    // Creeps and towers own separate attribute sets and keep their normal damage values.
    HeroAttributeSet->InitAttackDamage(5000.0f);
}

UAbilitySystemComponent* AEFPlayerState::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AEFPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AEFPlayerState, HeroLevel);
    DOREPLIFETIME(AEFPlayerState, Experience);
    DOREPLIFETIME(AEFPlayerState, Gold);
    DOREPLIFETIME(AEFPlayerState, LastHits);
    DOREPLIFETIME(AEFPlayerState, Kills);
    DOREPLIFETIME(AEFPlayerState, Deaths);
    DOREPLIFETIME(AEFPlayerState, Assists);
    DOREPLIFETIME(AEFPlayerState, MatchSide);
}

void AEFPlayerState::SetMatchSide(EEFMatchSide NewSide)
{
    if (HasAuthority())
    {
        MatchSide = NewSide;
        OnRep_MatchSide();
    }
}

void AEFPlayerState::AddExperience(int32 Amount)
{
    if (!HasAuthority() || Amount <= 0)
    {
        return;
    }

    Experience += Amount;
    while (HeroLevel < 30 && Experience >= GetExperienceForNextLevel())
    {
        Experience -= GetExperienceForNextLevel();
        ++HeroLevel;
        UE_LOG(LogEFCore, Display, TEXT("[%s] LevelUp player=%s level=%d"),
            EFLog::GetNetContext(this), *GetPlayerName(), HeroLevel);
    }
    if (HeroLevel >= 30)
    {
        Experience = 0;
    }
    OnRep_Progression();
}

int32 AEFPlayerState::GetExperienceForNextLevel() const
{
    return HeroLevel < 30 ? HeroLevel * 100 : 0;
}

bool AEFPlayerState::SpendGold(int32 Amount)
{
    if (!HasAuthority() || Amount < 0 || Gold < Amount)
    {
        return false;
    }

    Gold -= Amount;
    OnRep_Progression();
    return true;
}

void AEFPlayerState::AddGold(int32 Amount)
{
    if (HasAuthority() && Amount > 0)
    {
        Gold += Amount;
        OnRep_Progression();
    }
}

void AEFPlayerState::AwardCreepLastHit(int32 GoldAmount)
{
    if (!HasAuthority() || GoldAmount <= 0)
    {
        return;
    }

    Gold += GoldAmount;
    ++LastHits;
    OnRep_Progression();
    UE_LOG(LogEFCore, Display, TEXT("[%s] LastHit player=%s reward=%d gold=%d lastHits=%d"),
        EFLog::GetNetContext(this), *GetPlayerName(), GoldAmount, Gold, LastHits);
}

void AEFPlayerState::ResetCombatAttributesForRespawn()
{
    if (!HasAuthority() || !AbilitySystemComponent || !HeroAttributeSet)
    {
        return;
    }

    AbilitySystemComponent->SetNumericAttributeBase(
        UEFHeroAttributeSet::GetHealthAttribute(), HeroAttributeSet->GetMaxHealth());
    AbilitySystemComponent->SetNumericAttributeBase(
        UEFHeroAttributeSet::GetManaAttribute(), HeroAttributeSet->GetMaxMana());
    AbilitySystemComponent->RemoveLooseGameplayTag(EFGameplayTags::State_Dead);
}

void AEFPlayerState::RecordKill()
{
    if (HasAuthority())
    {
        ++Kills;
    }
}

void AEFPlayerState::RecordDeath()
{
    if (HasAuthority())
    {
        ++Deaths;
    }
}

void AEFPlayerState::OnRep_Progression()
{
}

void AEFPlayerState::OnRep_MatchSide()
{
}
