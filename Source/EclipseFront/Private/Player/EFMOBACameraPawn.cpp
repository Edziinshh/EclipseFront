#include "Player/EFMOBACameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Player/EFPlayerController.h"
#include "Units/EFHeroCharacter.h"

AEFMOBACameraPawn::AEFMOBACameraPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(SceneRoot);
    SpringArm->TargetArmLength = 1800.0f;
    SpringArm->SetRelativeRotation(FRotator(-55.0f, 0.0f, 0.0f));
    SpringArm->bDoCollisionTest = false;
    SpringArm->bInheritPitch = false;
    SpringArm->bInheritYaw = false;
    SpringArm->bInheritRoll = false;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;
}

void AEFMOBACameraPawn::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        PlayerController->bShowMouseCursor = true;
        PlayerController->DefaultMouseCursor = EMouseCursor::Default;
    }
}

void AEFMOBACameraPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    AEFPlayerController* PlayerController = Cast<AEFPlayerController>(GetController());
    if (!PlayerController || !PlayerController->IsLocalController())
    {
        return;
    }

    if (PlayerController->IsInputKeyDown(EKeys::SpaceBar))
    {
        if (const AEFHeroCharacter* Hero = PlayerController->GetControlledHero())
        {
            SetActorLocation(Hero->GetActorLocation());
            return;
        }
    }

    const FVector2D PanInput = ReadPanInput(*PlayerController).GetClampedToMaxSize(1.0f);
    const FVector Delta(PanInput.Y, PanInput.X, 0.0f);
    AddActorWorldOffset(Delta * PanSpeed * DeltaSeconds, false);
}

FVector2D AEFMOBACameraPawn::ReadPanInput(const APlayerController& PlayerController) const
{
    FVector2D Input = FVector2D::ZeroVector;
    Input.X += PlayerController.IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f;
    Input.X -= PlayerController.IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f;
    Input.Y += PlayerController.IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f;
    Input.Y -= PlayerController.IsInputKeyDown(EKeys::S) ? 1.0f : 0.0f;

    if (!bEnableEdgeScroll)
    {
        return Input;
    }

    float MouseX = 0.0f;
    float MouseY = 0.0f;
    int32 ViewportX = 0;
    int32 ViewportY = 0;
    PlayerController.GetViewportSize(ViewportX, ViewportY);

    if (ViewportX > 0 && ViewportY > 0 && PlayerController.GetMousePosition(MouseX, MouseY))
    {
        Input.X -= MouseX <= EdgeScrollThreshold ? 1.0f : 0.0f;
        Input.X += MouseX >= ViewportX - EdgeScrollThreshold ? 1.0f : 0.0f;
        Input.Y += MouseY <= EdgeScrollThreshold ? 1.0f : 0.0f;
        Input.Y -= MouseY >= ViewportY - EdgeScrollThreshold ? 1.0f : 0.0f;
    }

    return Input;
}
