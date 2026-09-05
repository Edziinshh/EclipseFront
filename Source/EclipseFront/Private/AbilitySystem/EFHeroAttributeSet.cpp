#include "AbilitySystem/EFHeroAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Player/EFPlayerState.h"
#include "Units/EFCreepCharacter.h"
#include "Units/EFHeroCharacter.h"
#include "Units/EFLaneTower.h"
#include "Game/EFGameState.h"

UEFHeroAttributeSet::UEFHeroAttributeSet()
{
    InitMaxHealth(1000.0f);
    InitHealth(1000.0f);
    InitMaxMana(500.0f);
    InitMana(500.0f);
    InitAttackDamage(60.0f);
    InitAttackSpeed(1.0f);
    InitAttackRange(150.0f);
    InitArmor(0.0f);
    InitMagicResistance(0.25f);
    InitMoveSpeed(600.0f);
    InitDamage(0.0f);
}

void UEFHeroAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(UEFHeroAttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UEFHeroAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UEFHeroAttributeSet, Mana, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UEFHeroAttributeSet, MaxMana, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UEFHeroAttributeSet, AttackDamage, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UEFHeroAttributeSet, AttackSpeed, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UEFHeroAttributeSet, AttackRange, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UEFHeroAttributeSet, Armor, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UEFHeroAttributeSet, MagicResistance, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UEFHeroAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
}

void UEFHeroAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxManaAttribute())
    {
        NewValue = FMath::Max(NewValue, 1.0f);
    }
    else if (Attribute == GetMoveSpeedAttribute() || Attribute == GetAttackSpeedAttribute() || Attribute == GetAttackRangeAttribute())
    {
        NewValue = FMath::Max(NewValue, 0.0f);
    }
    else if (Attribute == GetMagicResistanceAttribute())
    {
        NewValue = FMath::Clamp(NewValue, -1.0f, 0.95f);
    }
}

void UEFHeroAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetDamageAttribute())
    {
        const float RawDamage = FMath::Max(0.0f, GetDamage());
        SetDamage(0.0f);

        const AEFGameState* Match = GetWorld()->GetGameState<AEFGameState>();
        const AEFLaneTower* Structure = Cast<AEFLaneTower>(Data.Target.GetAvatarActor());
        if ((Match && Match->GetMatchPhase() == EEFMatchPhase::PostGame)
            || (Structure && !Structure->IsVulnerable())) { return; }

        const float ArmorValue = GetArmor();
        const float ArmorMultiplier = ArmorValue >= 0.0f
            ? 100.0f / (100.0f + ArmorValue)
            : 2.0f - (100.0f / (100.0f - ArmorValue));
        const float HealthBeforeDamage = GetHealth();
        const float AppliedDamage = FMath::Min(HealthBeforeDamage, RawDamage * ArmorMultiplier);
        SetHealth(FMath::Clamp(HealthBeforeDamage - AppliedDamage, 0.0f, GetMaxHealth()));

        AActor* TargetActor = Data.Target.AbilityActorInfo.IsValid()
            ? Data.Target.AbilityActorInfo->AvatarActor.Get()
            : nullptr;
        AEFHeroCharacter* TargetHero = Cast<AEFHeroCharacter>(TargetActor);
        AEFCreepCharacter* TargetCreep = Cast<AEFCreepCharacter>(TargetActor);
        AEFLaneTower* TargetTower = Cast<AEFLaneTower>(TargetActor);
        if (TargetHero)
        {
            TargetHero->NotifyDamageReceived(AppliedDamage);
        }
        else if (TargetCreep)
        {
            TargetCreep->NotifyDamageReceived(AppliedDamage);
        }
        else if (TargetTower)
        {
            TargetTower->NotifyDamageReceived(AppliedDamage);
        }

        if (GetHealth() <= 0.0f)
        {
            AEFPlayerState* SourcePlayerState = Cast<AEFPlayerState>(Data.EffectSpec.GetEffectContext().GetOriginalInstigator());
            if (TargetHero)
            {
                TargetHero->HandleHealthDepleted(SourcePlayerState);
            }
            else if (TargetCreep)
            {
                TargetCreep->HandleHealthDepleted(
                    Data.EffectSpec.GetEffectContext().GetOriginalInstigator());
            }
            else if (TargetTower)
            {
                TargetTower->HandleHealthDepleted(
                    Data.EffectSpec.GetEffectContext().GetOriginalInstigator());
            }
        }
    }
    else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
    else if (Data.EvaluatedData.Attribute == GetManaAttribute())
    {
        SetMana(FMath::Clamp(GetMana(), 0.0f, GetMaxMana()));
    }
}

#define EF_REP_NOTIFY(PropertyName) \
    void UEFHeroAttributeSet::OnRep_##PropertyName(const FGameplayAttributeData& OldValue) const \
    { \
        GAMEPLAYATTRIBUTE_REPNOTIFY(UEFHeroAttributeSet, PropertyName, OldValue); \
    }

EF_REP_NOTIFY(Health)
EF_REP_NOTIFY(MaxHealth)
EF_REP_NOTIFY(Mana)
EF_REP_NOTIFY(MaxMana)
EF_REP_NOTIFY(AttackDamage)
EF_REP_NOTIFY(AttackSpeed)
EF_REP_NOTIFY(AttackRange)
EF_REP_NOTIFY(Armor)
EF_REP_NOTIFY(MagicResistance)
EF_REP_NOTIFY(MoveSpeed)

#undef EF_REP_NOTIFY
