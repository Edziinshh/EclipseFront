#pragma once

#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "EFHeroAttributeSet.generated.h"

#define EF_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class ECLIPSEFRONT_API UEFHeroAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UEFHeroAttributeSet();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Vitals")
    FGameplayAttributeData Health;
    EF_ATTRIBUTE_ACCESSORS(UEFHeroAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Vitals")
    FGameplayAttributeData MaxHealth;
    EF_ATTRIBUTE_ACCESSORS(UEFHeroAttributeSet, MaxHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Mana, Category="Vitals")
    FGameplayAttributeData Mana;
    EF_ATTRIBUTE_ACCESSORS(UEFHeroAttributeSet, Mana)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxMana, Category="Vitals")
    FGameplayAttributeData MaxMana;
    EF_ATTRIBUTE_ACCESSORS(UEFHeroAttributeSet, MaxMana)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AttackDamage, Category="Combat")
    FGameplayAttributeData AttackDamage;
    EF_ATTRIBUTE_ACCESSORS(UEFHeroAttributeSet, AttackDamage)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AttackSpeed, Category="Combat")
    FGameplayAttributeData AttackSpeed;
    EF_ATTRIBUTE_ACCESSORS(UEFHeroAttributeSet, AttackSpeed)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AttackRange, Category="Combat")
    FGameplayAttributeData AttackRange;
    EF_ATTRIBUTE_ACCESSORS(UEFHeroAttributeSet, AttackRange)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Armor, Category="Combat")
    FGameplayAttributeData Armor;
    EF_ATTRIBUTE_ACCESSORS(UEFHeroAttributeSet, Armor)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MagicResistance, Category="Combat")
    FGameplayAttributeData MagicResistance;
    EF_ATTRIBUTE_ACCESSORS(UEFHeroAttributeSet, MagicResistance)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MoveSpeed, Category="Movement")
    FGameplayAttributeData MoveSpeed;
    EF_ATTRIBUTE_ACCESSORS(UEFHeroAttributeSet, MoveSpeed)

    // Server-only meta attribute. Effects write raw damage here; the set consumes it into Health.
    UPROPERTY(BlueprintReadOnly, Category="Combat")
    FGameplayAttributeData Damage;
    EF_ATTRIBUTE_ACCESSORS(UEFHeroAttributeSet, Damage)

protected:
    UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_Mana(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_MaxMana(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_AttackDamage(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_AttackSpeed(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_AttackRange(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_Armor(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_MagicResistance(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue) const;
};
