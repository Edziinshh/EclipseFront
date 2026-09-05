#pragma once

#include "Components/ActorComponent.h"
#include "EFCombatComponent.generated.h"

UENUM(BlueprintType)
enum class EEFAttackState : uint8
{
    Idle,
    MovingIntoRange,
    WindUp,
    Backswing
};

UCLASS(ClassGroup=(EclipseFront), meta=(BlueprintSpawnableComponent))
class ECLIPSEFRONT_API UEFCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEFCombatComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Eclipse Front|Combat")
    bool BeginBasicAttack(AActor* TargetActor);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Eclipse Front|Combat")
    void CancelBasicAttack();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Eclipse Front|Combat")
    bool ResolveBasicAttackImpact(AActor* ImpactTarget);

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Combat")
    AActor* GetAttackTarget() const { return AttackTarget; }

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Combat")
    EEFAttackState GetAttackState() const { return AttackState; }

protected:
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Eclipse Front|Combat")
    TObjectPtr<AActor> AttackTarget;

    UPROPERTY(BlueprintReadOnly, Replicated, Category="Eclipse Front|Combat")
    EEFAttackState AttackState = EEFAttackState::Idle;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|Combat", meta=(ClampMin="0.05"))
    float BaseAttackPoint = 0.35f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|Combat", meta=(ClampMin="0.0"))
    float RangeLeeway = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|Combat", meta=(ClampMin="1.0"))
    float ApproachRetargetDistance = 100.0f;

private:
    void EmitBasicAttackContact();
    void EnterAttackState(EEFAttackState NewState);
    void UpdateMovingIntoRange();
    bool IsTargetValidAndHostile() const;
    bool IsTargetInRange() const;
    float GetAttackSpeed() const;

    float StateElapsedSeconds = 0.0f;
    FVector LastIssuedApproachGoal = FVector::ZeroVector;
    bool bApproachOrderIssued = false;
};
