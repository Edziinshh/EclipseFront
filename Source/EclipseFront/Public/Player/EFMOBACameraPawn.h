#pragma once

#include "GameFramework/Pawn.h"
#include "EFMOBACameraPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class ECLIPSEFRONT_API AEFMOBACameraPawn : public APawn
{
    GENERATED_BODY()

public:
    AEFMOBACameraPawn();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|Camera")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|Camera")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Eclipse Front|Camera")
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Eclipse Front|Camera", meta=(ClampMin="0.0"))
    float PanSpeed = 1800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Eclipse Front|Camera", meta=(ClampMin="0.0"))
    float EdgeScrollThreshold = 16.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Eclipse Front|Camera")
    bool bEnableEdgeScroll = true;

private:
    FVector2D ReadPanInput(const APlayerController& PlayerController) const;
};
