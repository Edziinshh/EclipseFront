#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Game/EFTypes.h"
#include "EFCreepCharacter.generated.h"

class UMaterialInstanceDynamic;
class UEFAbilitySystemComponent;
class UEFCombatComponent;
class UEFHeroAttributeSet;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class ECLIPSEFRONT_API AEFCreepCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AEFCreepCharacter();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    void InitializeLaneCreep(EEFMatchSide NewMatchSide, const TArray<FVector>& NewRoute);
    void ConfigureWaveUnit(bool bRanged, bool bUpgraded);

    UFUNCTION(BlueprintPure, Category="Eclipse Front|M2")
    EEFMatchSide GetMatchSide() const { return MatchSide; }

    UFUNCTION(BlueprintPure, Category="Eclipse Front|M2")
    bool IsDead() const { return bIsDead; }

    UFUNCTION(BlueprintPure, Category="Eclipse Front|M2")
    UEFCombatComponent* GetCombatComponent() const { return CombatComponent; }

    const UEFHeroAttributeSet* GetCreepAttributeSet() const { return CreepAttributeSet; }
    float GetBasicAttackRange() const;
    void StopMovementForAttack();
    void FaceAttackTarget(const AActor* TargetActor);
    void NotifyBasicAttackContact();
    void NotifyDamageReceived(float DamageAmount);
    void HandleHealthDepleted(AActor* DamageInstigator);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M2")
    TObjectPtr<UStaticMeshComponent> PrototypeBody;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M2")
    TObjectPtr<UStaticMeshComponent> SideMarker;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M2")
    TObjectPtr<UEFAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY()
    TObjectPtr<UEFHeroAttributeSet> CreepAttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M2")
    TObjectPtr<UEFCombatComponent> CombatComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M2")
    TObjectPtr<USceneComponent> OverheadPresentationRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M2")
    TObjectPtr<UStaticMeshComponent> HealthBarBackground;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M2")
    TObjectPtr<UStaticMeshComponent> HealthBarFill;

    UPROPERTY(ReplicatedUsing=OnRep_MatchSide)
    EEFMatchSide MatchSide = EEFMatchSide::Unassigned;

    UFUNCTION()
    void OnRep_MatchSide();

    UPROPERTY(ReplicatedUsing=OnRep_IsDead)
    bool bIsDead = false;

    UFUNCTION()
    void OnRep_IsDead();

    UPROPERTY(ReplicatedUsing=OnRep_AttackContactCounter)
    uint8 AttackContactCounter = 0;

    UFUNCTION()
    void OnRep_AttackContactCounter();

private:
    void IssueMoveToCurrentWaypoint();
    void UpdateTeamPresentation();
    void UpdateHealthBar();
    void FaceOverheadPresentationToLocalCamera();
    AActor* FindNearestHostileTarget() const;

    TArray<FVector> LaneRoute;
    int32 CurrentWaypointIndex = INDEX_NONE;
    int32 LastFailedWaypointLogIndex = INDEX_NONE;
    float MoveRetryTimeRemaining = 0.0f;
    float AggroScanTimeRemaining = 0.0f;
    float AttackPulseTimeRemaining = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M2", meta=(ClampMin="10.0"))
    float WaypointAcceptanceRadius = 90.0f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M2", meta=(ClampMin="0.1"))
    float MoveRetryInterval = 0.75f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M2", meta=(ClampMin="50.0"))
    float AggroRadius = 500.0f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M2", meta=(ClampMin="0.05"))
    float AggroScanInterval = 0.2f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M2", meta=(ClampMin="0"))
    int32 GoldBounty = 45;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M2", meta=(ClampMin="0"))
    int32 ExperienceBounty = 60;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M2", meta=(ClampMin="0.0"))
    float ExperienceRadius = 1200.0f;

    TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;
    TObjectPtr<UMaterialInstanceDynamic> MarkerMaterial;
    TObjectPtr<UMaterialInstanceDynamic> HealthFillMaterial;
    TObjectPtr<UMaterialInstanceDynamic> HealthBackgroundMaterial;

    static constexpr float AttackPulseDuration = 0.28f;
    static constexpr float AttackLungeDistance = 42.0f;
};
