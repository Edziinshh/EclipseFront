#include "AbilitySystem/EFBasicAttackDamageEffect.h"

#include "AbilitySystem/EFHeroAttributeSet.h"
#include "Game/EFGameplayTags.h"

UEFBasicAttackDamageEffect::UEFBasicAttackDamageEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    FGameplayModifierInfo& DamageModifier = Modifiers.AddDefaulted_GetRef();
    DamageModifier.Attribute = UEFHeroAttributeSet::GetDamageAttribute();
    DamageModifier.ModifierOp = EGameplayModOp::Additive;

    FSetByCallerFloat SetByCallerDamage;
    SetByCallerDamage.DataTag = EFGameplayTags::Data_Damage_BasicAttack;
    DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDamage);
}
