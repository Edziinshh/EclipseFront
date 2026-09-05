#pragma once

#include "GameFramework/PlayerController.h"
#include "EFPlayerController.generated.h"

class AEFHeroCharacter;

UCLASS()
class ECLIPSEFRONT_API AEFPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AEFPlayerController();

    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual void SetupInputComponent() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category="Eclipse Front|Orders")
    AEFHeroCharacter* GetControlledHero() const { return ControlledHero; }

    void SetControlledHero(AEFHeroCharacter* NewHero);

protected:
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ControlledHero, Category="Eclipse Front|Orders")
    TObjectPtr<AEFHeroCharacter> ControlledHero;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|Orders", meta=(ClampMin="1000.0"))
    float MaximumOrderDistance = 50000.0f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M0")
    bool bAllowDirectMoveFallback = false;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|Orders", meta=(ClampMin="0.0"))
    float ContextTargetAssistRadius = 250.0f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|Orders", meta=(ClampMin="0.03", ClampMax="0.5"))
    float HeldOrderUpdateInterval = 0.10f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|Orders", meta=(ClampMin="1.0"))
    float HeldOrderMinCursorDistance = 75.0f;

    UFUNCTION(Server, Reliable)
    void ServerRequestMove(FVector_NetQuantize Destination);

    UFUNCTION(Server, Unreliable)
    void ServerUpdateHeldMove(FVector_NetQuantize Destination);

    UFUNCTION(Server, Reliable)
    void ServerRequestAttack(AActor* TargetActor);

    UFUNCTION()
    void OnRep_ControlledHero();

private:
    void HandleContextOrderPressed();
    void HandleContextOrderReleased();
    void HandleSelectionPressed();
    void DrawAttackRanges(float Lifetime) const;
    void IssueContextOrder(bool bContinuousUpdate);
    void ApplyMoveOrder(const FVector& Destination);
    bool ResolveMoveDestination(const FVector& Destination, FVector& OutResolvedDestination, bool& bOutUseNavigation) const;
    AActor* FindAssistedAttackTarget(const FVector& CursorWorldLocation) const;

    UPROPERTY(Transient)
    TObjectPtr<AEFHeroCharacter> LocallySelectedUnit;

    TWeakObjectPtr<AActor> LastHeldAttackTarget;
    FVector LastHeldMoveDestination = FVector::ZeroVector;
    float HeldOrderTimeUntilUpdate = 0.0f;
    double LastServerHeldMoveTime = -1.0;
    bool bContextOrderHeld = false;
    bool bHasLastHeldMoveDestination = false;
    bool bAttackRangesHeld = false;
};
