#include "Combat/EFCombatComponent.h"

#include "AIController.h"
#include "Debug/EFLogCategories.h"
#include "AbilitySystem/EFBasicAttackDamageEffect.h"
#include "AbilitySystem/EFHeroAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "Game/EFGameplayTags.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/EFPlayerState.h"
#include "Units/EFCreepCharacter.h"
#include "Units/EFHeroCharacter.h"
#include "Units/EFLaneTower.h"
#include "VisualLogger/VisualLogger.h"
#include "EngineUtils.h"
#include "Game/EFGameState.h"

namespace
{
const UEFHeroAttributeSet* GetCombatAttributes(const AActor* Unit)
{
    if (const AEFHeroCharacter* Hero = Cast<AEFHeroCharacter>(Unit))
    {
        const AEFPlayerState* PlayerState = Hero->GetOwningPlayerState();
        return PlayerState ? PlayerState->GetHeroAttributeSet() : nullptr;
    }
    if (const AEFCreepCharacter* Creep = Cast<AEFCreepCharacter>(Unit))
    {
        return Creep->GetCreepAttributeSet();
    }
    if (const AEFLaneTower* Tower = Cast<AEFLaneTower>(Unit))
    {
        return Tower->GetTowerAttributeSet();
    }
    return nullptr;
}

EEFMatchSide GetCombatSide(const AActor* Unit)
{
    if (const AEFHeroCharacter* Hero = Cast<AEFHeroCharacter>(Unit))
    {
        const AEFPlayerState* PlayerState = Hero->GetOwningPlayerState();
        return PlayerState ? PlayerState->GetMatchSide() : EEFMatchSide::Unassigned;
    }
    if (const AEFCreepCharacter* Creep = Cast<AEFCreepCharacter>(Unit))
    {
        return Creep->GetMatchSide();
    }
    if (const AEFLaneTower* Tower = Cast<AEFLaneTower>(Unit))
    {
        return Tower->GetMatchSide();
    }
    return EEFMatchSide::Unassigned;
}

bool IsCombatUnitDead(const AActor* Unit)
{
    if (const AEFHeroCharacter* Hero = Cast<AEFHeroCharacter>(Unit))
    {
        return Hero->IsDead();
    }
    if (const AEFCreepCharacter* Creep = Cast<AEFCreepCharacter>(Unit))
    {
        return Creep->IsDead();
    }
    if (const AEFLaneTower* Tower = Cast<AEFLaneTower>(Unit))
    {
        return Tower->IsDead();
    }
    return true;
}

float GetCombatRange(const AActor* Unit)
{
    if (const AEFHeroCharacter* Hero = Cast<AEFHeroCharacter>(Unit))
    {
        return Hero->GetBasicAttackRange();
    }
    if (const AEFCreepCharacter* Creep = Cast<AEFCreepCharacter>(Unit))
    {
        return Creep->GetBasicAttackRange();
    }
    if (const AEFLaneTower* Tower = Cast<AEFLaneTower>(Unit))
    {
        return Tower->GetBasicAttackRange();
    }
    return 0.0f;
}

const TCHAR* GetAttackStateLabel(EEFAttackState State)
{
    switch (State)
    {
    case EEFAttackState::Idle:
        return TEXT("Idle");
    case EEFAttackState::MovingIntoRange:
        return TEXT("MovingIntoRange");
    case EEFAttackState::WindUp:
        return TEXT("WindUp");
    case EEFAttackState::Backswing:
        return TEXT("Backswing");
    default:
        return TEXT("Unknown");
    }
}
}

UEFCombatComponent::UEFCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    SetIsReplicatedByDefault(true);
}

void UEFCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    SetComponentTickEnabled(GetOwner() && GetOwner()->HasAuthority());
}

void UEFCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!GetOwner() || !GetOwner()->HasAuthority() || AttackState == EEFAttackState::Idle)
    {
        return;
    }

    if (!IsTargetValidAndHostile())
    {
        CancelBasicAttack();
        return;
    }

    if (AttackState == EEFAttackState::MovingIntoRange)
    {
        UpdateMovingIntoRange();
        return;
    }

    if (!IsTargetInRange())
    {
        EnterAttackState(EEFAttackState::MovingIntoRange);
        UpdateMovingIntoRange();
        return;
    }

    StateElapsedSeconds += DeltaTime;
    const float SafeAttackSpeed = FMath::Max(0.1f, GetAttackSpeed());
    if (AttackState == EEFAttackState::WindUp && StateElapsedSeconds >= BaseAttackPoint / SafeAttackSpeed)
    {
        EmitBasicAttackContact();
    }
    else if (AttackState == EEFAttackState::Backswing)
    {
        const float BackswingDuration = FMath::Max(0.05f, (1.0f - BaseAttackPoint) / SafeAttackSpeed);
        if (StateElapsedSeconds >= BackswingDuration)
        {
            EnterAttackState(IsTargetInRange() ? EEFAttackState::WindUp : EEFAttackState::MovingIntoRange);
        }
    }
}

void UEFCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UEFCombatComponent, AttackTarget);
    DOREPLIFETIME(UEFCombatComponent, AttackState);
}

bool UEFCombatComponent::BeginBasicAttack(AActor* TargetActor)
{
    const AEFGameState* Match = GetWorld()->GetGameState<AEFGameState>();
    if (Match && Match->GetMatchPhase() == EEFMatchPhase::PostGame) { return false; }
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(TargetActor) || TargetActor == GetOwner())
    {
        return false;
    }

    // Repeating the same order must not restart wind-up or navigation.
    if (AttackTarget == TargetActor && AttackState != EEFAttackState::Idle && IsTargetValidAndHostile())
    {
        return true;
    }

    AttackTarget = TargetActor;
    if (!IsTargetValidAndHostile())
    {
        CancelBasicAttack();
        return false;
    }

    EnterAttackState(IsTargetInRange() ? EEFAttackState::WindUp : EEFAttackState::MovingIntoRange);
    return true;
}

void UEFCombatComponent::CancelBasicAttack()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    AttackTarget = nullptr;
    EnterAttackState(EEFAttackState::Idle);
}

void UEFCombatComponent::EmitBasicAttackContact()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(AttackTarget))
    {
        CancelBasicAttack();
        return;
    }

    bool bContactStarted = false;
    if (AEFLaneTower* OwnerTower = Cast<AEFLaneTower>(GetOwner()))
    {
        bContactStarted = OwnerTower->LaunchProjectile(AttackTarget);
    }
    else
    {
        bContactStarted = ResolveBasicAttackImpact(AttackTarget);
        if (bContactStarted)
        {
            if (AEFHeroCharacter* OwnerHero = Cast<AEFHeroCharacter>(GetOwner()))
            {
                OwnerHero->NotifyBasicAttackContact();
            }
            else if (AEFCreepCharacter* OwnerCreep = Cast<AEFCreepCharacter>(GetOwner()))
            {
                OwnerCreep->NotifyBasicAttackContact();
            }
        }
    }

    if (!bContactStarted)
    {
        CancelBasicAttack();
        return;
    }

    if (IsTargetValidAndHostile())
    {
        EnterAttackState(EEFAttackState::Backswing);
    }
    else
    {
        CancelBasicAttack();
    }
}

bool UEFCombatComponent::ResolveBasicAttackImpact(AActor* ImpactTarget)
{
    const AEFGameState* Match = GetWorld()->GetGameState<AEFGameState>();
    const AEFLaneTower* Structure = Cast<AEFLaneTower>(ImpactTarget);
    if ((Match && Match->GetMatchPhase() == EEFMatchPhase::PostGame)
        || (Structure && !Structure->IsVulnerable())) { return false; }
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(ImpactTarget)
        || !GetCombatAttributes(GetOwner()) || !GetCombatAttributes(ImpactTarget)
        || IsCombatUnitDead(GetOwner()) || IsCombatUnitDead(ImpactTarget))
    {
        return false;
    }

    const EEFMatchSide SourceSide = GetCombatSide(GetOwner());
    const EEFMatchSide ImpactSide = GetCombatSide(ImpactTarget);
    if (SourceSide == EEFMatchSide::Unassigned || ImpactSide == EEFMatchSide::Unassigned || SourceSide == ImpactSide)
    {
        return false;
    }

    FGameplayEventData EventData;
    EventData.EventTag = EFGameplayTags::Event_Combat_BasicAttack_Contact;
    EventData.Instigator = GetOwner();
    EventData.Target = ImpactTarget;
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), EventData.EventTag, EventData);

    UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ImpactTarget);
    const AEFPlayerState* SourcePlayerState = Cast<AEFHeroCharacter>(GetOwner())
        ? Cast<AEFHeroCharacter>(GetOwner())->GetOwningPlayerState()
        : nullptr;
    const UEFHeroAttributeSet* SourceAttributes = GetCombatAttributes(GetOwner());
    if (!SourceASC || !TargetASC || !SourceAttributes)
    {
        return false;
    }

    const float AttackDamage = FMath::Max(0.0f, SourceAttributes->GetAttackDamage());
    FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
    EffectContext.AddInstigator(SourcePlayerState
        ? static_cast<AActor*>(const_cast<AEFPlayerState*>(SourcePlayerState))
        : GetOwner(), GetOwner());
    EffectContext.AddSourceObject(GetOwner());
    FGameplayEffectSpecHandle DamageSpec = SourceASC->MakeOutgoingSpec(
        UEFBasicAttackDamageEffect::StaticClass(), 1.0f, EffectContext);
    if (!DamageSpec.IsValid())
    {
        return false;
    }

    const UEFHeroAttributeSet* TargetAttributes = GetCombatAttributes(ImpactTarget);
    const float HealthBefore = TargetAttributes ? TargetAttributes->GetHealth() : -1.0f;
    DamageSpec.Data->SetSetByCallerMagnitude(EFGameplayTags::Data_Damage_BasicAttack, AttackDamage);
    TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpec.Data.Get());
    UE_LOG(LogEFCombat, Verbose, TEXT("[%s] BasicAttack source=%s target=%s damage=%.1f health=%.1f->%.1f"),
        EFLog::GetNetContext(this), *GetOwner()->GetName(), *ImpactTarget->GetName(), AttackDamage, HealthBefore,
        TargetAttributes ? TargetAttributes->GetHealth() : -1.0f);

    AEFHeroCharacter* SourceHero = Cast<AEFHeroCharacter>(GetOwner());
    AEFHeroCharacter* TargetHero = Cast<AEFHeroCharacter>(ImpactTarget);
    if (SourceHero && TargetHero)
    {
        for (TActorIterator<AEFLaneTower> It(GetWorld()); It; ++It)
        {
            It->NotifyAlliedHeroAttacked(SourceHero, TargetHero);
        }
    }
    return true;
}

void UEFCombatComponent::EnterAttackState(EEFAttackState NewState)
{
    const EEFAttackState PreviousState = AttackState;
    AttackState = NewState;
    StateElapsedSeconds = 0.0f;

    if (PreviousState != NewState && GetOwner())
    {
        UE_VLOG(GetOwner(), LogEFCombat, Display, TEXT("AttackState %s -> %s target=%s"),
            GetAttackStateLabel(PreviousState), GetAttackStateLabel(NewState), *GetNameSafe(AttackTarget));
        UE_VLOG_WIRECIRCLE(GetOwner(), LogEFCombat, Verbose, GetOwner()->GetActorLocation(), FVector::UpVector,
            GetCombatRange(GetOwner()) + RangeLeeway, FColor::Orange, TEXT("Attack range"));
        if (AttackTarget)
        {
            UE_VLOG_ARROW(GetOwner(), LogEFCombat, Display, GetOwner()->GetActorLocation(),
                AttackTarget->GetActorLocation(), FColor::Orange, TEXT("Target %s"), *AttackTarget->GetName());
        }
    }

    if (NewState != EEFAttackState::MovingIntoRange || PreviousState != EEFAttackState::MovingIntoRange)
    {
        bApproachOrderIssued = false;
    }

    if (NewState == EEFAttackState::WindUp)
    {
        if (AEFHeroCharacter* OwnerHero = Cast<AEFHeroCharacter>(GetOwner()))
        {
            OwnerHero->StopMovementForAttack();
            OwnerHero->FaceAttackTarget(AttackTarget);
        }
        else if (AEFCreepCharacter* OwnerCreep = Cast<AEFCreepCharacter>(GetOwner()))
        {
            OwnerCreep->StopMovementForAttack();
            OwnerCreep->FaceAttackTarget(AttackTarget);
        }
        else if (AEFLaneTower* OwnerTower = Cast<AEFLaneTower>(GetOwner()))
        {
            OwnerTower->StopMovementForAttack();
            OwnerTower->FaceAttackTarget(AttackTarget);
        }
    }
}

void UEFCombatComponent::UpdateMovingIntoRange()
{
    if (Cast<AEFLaneTower>(GetOwner()))
    {
        if (IsTargetInRange())
        {
            EnterAttackState(EEFAttackState::WindUp);
        }
        else
        {
            CancelBasicAttack();
        }
        return;
    }

    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter || !AttackTarget)
    {
        CancelBasicAttack();
        return;
    }

    if (IsTargetInRange())
    {
        EnterAttackState(EEFAttackState::WindUp);
        return;
    }

    const FVector CurrentTargetLocation = AttackTarget->GetActorLocation();
    if (bApproachOrderIssued
        && FVector::DistSquared2D(CurrentTargetLocation, LastIssuedApproachGoal) < FMath::Square(ApproachRetargetDistance))
    {
        return;
    }

    const float AcceptanceRadius = FMath::Max(10.0f, GetCombatRange(GetOwner()) - RangeLeeway);
    if (AAIController* AIController = Cast<AAIController>(OwnerCharacter->GetController()))
    {
        const EPathFollowingRequestResult::Type Result = AIController->MoveToActor(
            AttackTarget, AcceptanceRadius, false, true, true, nullptr, true);
        if (Result != EPathFollowingRequestResult::Failed)
        {
            if (AEFHeroCharacter* OwnerHero = Cast<AEFHeroCharacter>(GetOwner()))
            {
                OwnerHero->CancelDirectMove();
            }
            LastIssuedApproachGoal = CurrentTargetLocation;
            bApproachOrderIssued = true;
            UE_VLOG_ARROW(GetOwner(), LogEFMovement, Display, GetOwner()->GetActorLocation(),
                CurrentTargetLocation, FColor::Green, TEXT("Combat approach target=%s acceptance=%.1f"),
                *AttackTarget->GetName(), AcceptanceRadius);
            return;
        }
    }

    if (AEFHeroCharacter* OwnerHero = Cast<AEFHeroCharacter>(GetOwner()))
    {
        OwnerHero->SetDirectMoveDestination(CurrentTargetLocation);
    }
    LastIssuedApproachGoal = CurrentTargetLocation;
    bApproachOrderIssued = true;
    UE_VLOG_ARROW(GetOwner(), LogEFMovement, Warning, GetOwner()->GetActorLocation(),
        CurrentTargetLocation, FColor::Yellow, TEXT("Direct combat approach fallback target=%s"),
        *AttackTarget->GetName());
}

bool UEFCombatComponent::IsTargetValidAndHostile() const
{
    const AEFLaneTower* Structure = Cast<AEFLaneTower>(AttackTarget);
    if (Structure && !Structure->IsVulnerable()) { return false; }
    if (!GetCombatAttributes(GetOwner()) || !GetCombatAttributes(AttackTarget)
        || IsCombatUnitDead(GetOwner()) || IsCombatUnitDead(AttackTarget))
    {
        return false;
    }

    const EEFMatchSide OwnerSide = GetCombatSide(GetOwner());
    const EEFMatchSide TargetSide = GetCombatSide(AttackTarget);
    return OwnerSide != EEFMatchSide::Unassigned
        && TargetSide != EEFMatchSide::Unassigned
        && OwnerSide != TargetSide;
}

bool UEFCombatComponent::IsTargetInRange() const
{
    return GetOwner() && AttackTarget
        && FVector::DistSquared2D(GetOwner()->GetActorLocation(), AttackTarget->GetActorLocation())
            <= FMath::Square(GetCombatRange(GetOwner()) + RangeLeeway);
}

float UEFCombatComponent::GetAttackSpeed() const
{
    const UEFHeroAttributeSet* Attributes = GetCombatAttributes(GetOwner());
    return Attributes ? Attributes->GetAttackSpeed() : 1.0f;
}
