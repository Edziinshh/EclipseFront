#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "Game/EFTypes.h"
#include "EFLaneTower.generated.h"

class UEFAbilitySystemComponent;
class UEFCombatComponent;
class UEFHeroAttributeSet;
class AEFTowerProjectile;
class AEFHeroCharacter;
class UMaterialInstanceDynamic;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class ECLIPSEFRONT_API AEFLaneTower : public AActor, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AEFLaneTower();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void InitializeTower(EEFMatchSide NewMatchSide);
    void InitializeStructure(EEFMatchSide Side, EEFStructureKind Kind);
    void SetVulnerable(bool bValue);
    bool IsVulnerable() const { return bVulnerable && !bIsDead; }
    bool CanFire() const { return StructureKind <= EEFStructureKind::T3 || StructureKind == EEFStructureKind::T4; }
    EEFStructureKind GetStructureKind() const { return StructureKind; }

    EEFMatchSide GetMatchSide() const { return MatchSide; }
    bool IsDead() const { return bIsDead; }
    float GetBasicAttackRange() const;
    const UEFHeroAttributeSet* GetTowerAttributeSet() const { return TowerAttributeSet; }
    UEFCombatComponent* GetCombatComponent() const { return CombatComponent; }

    void StopMovementForAttack();
    void FaceAttackTarget(const AActor* TargetActor);
    void NotifyBasicAttackContact();
    void NotifyDamageReceived(float DamageAmount);
    void HandleHealthDepleted(AActor* DamageInstigator);
    bool LaunchProjectile(AActor* TargetActor);
    void NotifyAlliedHeroAttacked(AEFHeroCharacter* AttackingHero, AEFHeroCharacter* AlliedVictim);

protected:
    UPROPERTY(ReplicatedUsing=OnRep_MatchSide)
    EEFStructureKind StructureKind = EEFStructureKind::T1;

    UPROPERTY(ReplicatedUsing=OnRep_MatchSide)
    bool bVulnerable = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M3")
    TObjectPtr<USphereComponent> TargetCollision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M3")
    TObjectPtr<UStaticMeshComponent> TowerBase;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M3")
    TObjectPtr<UStaticMeshComponent> TowerHead;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M3")
    TObjectPtr<USceneComponent> OverheadPresentationRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M3")
    TObjectPtr<UStaticMeshComponent> HealthBarBackground;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M3")
    TObjectPtr<UStaticMeshComponent> HealthBarFill;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M3")
    TObjectPtr<UEFAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY()
    TObjectPtr<UEFHeroAttributeSet> TowerAttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M3")
    TObjectPtr<UEFCombatComponent> CombatComponent;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M3")
    TSubclassOf<AEFTowerProjectile> ProjectileClass;

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
    AActor* FindPriorityTarget() const;
    void UpdatePresentation();
    void FaceOverheadPresentationToLocalCamera();

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M3", meta=(ClampMin="100.0"))
    float TargetAcquisitionRadius = 800.0f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M3", meta=(ClampMin="0.05"))
    float TargetScanInterval = 0.2f;

    float TargetScanTimeRemaining = 0.0f;
    float ForcedHeroAggroTimeRemaining = 0.0f;
    float AttackPulseTimeRemaining = 0.0f;
    TObjectPtr<UMaterialInstanceDynamic> BaseMaterial;
    TObjectPtr<UMaterialInstanceDynamic> HeadMaterial;
    TObjectPtr<UMaterialInstanceDynamic> HealthFillMaterial;
    TObjectPtr<UMaterialInstanceDynamic> HealthBackgroundMaterial;
    TWeakObjectPtr<AEFHeroCharacter> ForcedHeroTarget;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M3", meta=(ClampMin="0.1"))
    float ForcedHeroAggroDuration = 2.5f;

    static constexpr float AttackPulseDuration = 0.18f;
};
