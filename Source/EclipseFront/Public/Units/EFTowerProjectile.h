#pragma once

#include "GameFramework/Actor.h"
#include "Game/EFTypes.h"
#include "EFTowerProjectile.generated.h"

class AEFLaneTower;
class UMaterialInstanceDynamic;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class ECLIPSEFRONT_API AEFTowerProjectile : public AActor
{
    GENERATED_BODY()

public:
    AEFTowerProjectile();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void InitializeProjectile(AEFLaneTower* NewSourceTower, AActor* NewTargetActor, EEFMatchSide NewMatchSide);

private:
    UFUNCTION()
    void OnRep_MatchSide();

    void UpdatePresentation();
    FVector GetTargetPoint() const;

    UPROPERTY(VisibleAnywhere, Category="Eclipse Front|M3")
    TObjectPtr<USphereComponent> CollisionRoot;

    UPROPERTY(VisibleAnywhere, Category="Eclipse Front|M3")
    TObjectPtr<UStaticMeshComponent> ProjectileMesh;

    UPROPERTY(Replicated)
    TObjectPtr<AEFLaneTower> SourceTower;

    UPROPERTY(Replicated)
    TObjectPtr<AActor> TargetActor;

    UPROPERTY(ReplicatedUsing=OnRep_MatchSide)
    EEFMatchSide MatchSide = EEFMatchSide::Unassigned;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M3", meta=(ClampMin="100.0"))
    float TravelSpeed = 1100.0f;

    UPROPERTY(EditDefaultsOnly, Category="Eclipse Front|M3", meta=(ClampMin="1.0"))
    float ImpactRadius = 35.0f;

    TObjectPtr<UMaterialInstanceDynamic> ProjectileMaterial;
    double LaunchTime = -1.0;
    double LastSimulationTime = -1.0;
};
