#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Game/EFTypes.h"
#include "EFHeroCharacter.generated.h"

class UEFAbilitySystemComponent;
class UEFCombatComponent;
class AEFPlayerState;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
struct FOnAttributeChangeData;

UCLASS()
class ECLIPSEFRONT_API AEFHeroCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AEFHeroCharacter();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void Tick(float DeltaSeconds) override;

    void InitializeAbilityActorInfo();
    void SetOwningPlayerState(AEFPlayerState* NewPlayerState);
    void SetDirectMoveDestination(const FVector& NewDestination);
    void CancelDirectMove();
    void StopMovementForAttack();
    void FaceAttackTarget(const AActor* TargetActor);
    void NotifyBasicAttackContact();
    void NotifyDamageReceived(float DamageAmount);
    void HandleHealthDepleted(AEFPlayerState* KillerPlayerState);
    void SetLocallySelected(bool bSelected);

    float GetBasicAttackRange() const;

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Identity")
    AEFPlayerState* GetOwningPlayerState() const { return OwningPlayerState; }

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Combat")
    bool IsDead() const { return bIsDead; }

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Identity")
    EEFLoreFaction GetLoreFaction() const { return LoreFaction; }

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Combat")
    UEFCombatComponent* GetCombatComponent() const { return CombatComponent; }

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Eclipse Front|Identity")
    EEFLoreFaction LoreFaction = EEFLoreFaction::Unknown;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|Combat")
    TObjectPtr<UEFCombatComponent> CombatComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M0")
    TObjectPtr<UStaticMeshComponent> PrototypeBody;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M1")
    TObjectPtr<USceneComponent> OverheadPresentationRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M1")
    TObjectPtr<UStaticMeshComponent> HealthBarBackground;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M1")
    TObjectPtr<UStaticMeshComponent> HealthBarFill;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M1")
    TObjectPtr<UStaticMeshComponent> SelectionRing;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M1")
    TObjectPtr<UTextRenderComponent> TeamLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|M1")
    TObjectPtr<UTextRenderComponent> DamageText;

    UPROPERTY(Transient)
    TObjectPtr<UEFAbilitySystemComponent> CachedAbilitySystemComponent;

    // Kept separate from APawn::PlayerState because the PlayerController possesses
    // the camera pawn while this AI-controlled hero remains the GAS avatar.
    UPROPERTY(ReplicatedUsing=OnRep_OwningPlayerState)
    TObjectPtr<AEFPlayerState> OwningPlayerState;

    UFUNCTION()
    void OnRep_OwningPlayerState();

    UPROPERTY(ReplicatedUsing=OnRep_IsDead)
    bool bIsDead = false;

    UFUNCTION()
    void OnRep_IsDead();

    UPROPERTY(ReplicatedUsing=OnRep_AttackContactCounter)
    uint8 AttackContactCounter = 0;

    UFUNCTION()
    void OnRep_AttackContactCounter();

    UPROPERTY(Replicated)
    float LastDamageAmount = 0.0f;

    UPROPERTY(ReplicatedUsing=OnRep_DamageEventCounter)
    uint8 DamageEventCounter = 0;

    UFUNCTION()
    void OnRep_DamageEventCounter();

    void BindAttributePresentation();
    void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
    void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);
    void UpdateHealthBar();
    void UpdateTeamPresentation();
    void FaceOverheadPresentationToLocalCamera();

    FVector DirectMoveDestination = FVector::ZeroVector;
    bool bHasDirectMoveDestination = false;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M0", meta=(ClampMin="1.0"))
    float DirectMoveAcceptanceRadius = 80.0f;

    bool bAttributePresentationBound = false;
    EEFMatchSide PresentedMatchSide = EEFMatchSide::Unassigned;
    TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;
    TObjectPtr<UMaterialInstanceDynamic> HealthFillMaterial;
    TObjectPtr<UMaterialInstanceDynamic> HealthBackgroundMaterial;
    TObjectPtr<UMaterialInstanceDynamic> SelectionMaterial;
    float AttackPulseTimeRemaining = 0.0f;
    float DamageTextTimeRemaining = 0.0f;

    static constexpr float AttackPulseDuration = 0.18f;
    static constexpr float DamageTextDuration = 0.75f;
};
