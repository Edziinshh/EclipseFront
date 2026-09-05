#include "Units/EFHeroCharacter.h"

#include "AIController.h"
#include "AbilitySystem/EFAbilitySystemComponent.h"
#include "AbilitySystem/EFHeroAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Combat/EFCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Game/EFGameMode.h"
#include "Game/EFGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Player/EFPlayerState.h"
#include "UObject/ConstructorHelpers.h"

AEFHeroCharacter::AEFHeroCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();

    CombatComponent = CreateDefaultSubobject<UEFCombatComponent>(TEXT("CombatComponent"));

    // Pawn collision presets may ignore visibility traces. Explicitly make heroes
    // clickable for MOBA selection/context orders.
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    PrototypeBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeBody"));
    PrototypeBody->SetupAttachment(GetCapsuleComponent());
    PrototypeBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrototypeBody->SetRelativeScale3D(FVector(0.65f, 0.65f, 1.8f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PrototypeMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (PrototypeMesh.Succeeded())
    {
        PrototypeBody->SetStaticMesh(PrototypeMesh.Object);
    }

    OverheadPresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("OverheadPresentationRoot"));
    OverheadPresentationRoot->SetupAttachment(GetCapsuleComponent());
    OverheadPresentationRoot->SetUsingAbsoluteRotation(true);

    HealthBarBackground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HealthBarBackground"));
    HealthBarBackground->SetupAttachment(OverheadPresentationRoot);
    HealthBarBackground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HealthBarBackground->SetRelativeLocation(FVector(0.0f, 0.0f, 135.0f));
    HealthBarBackground->SetRelativeScale3D(FVector(0.75f, 0.09f, 0.035f));

    HealthBarFill = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HealthBarFill"));
    HealthBarFill->SetupAttachment(OverheadPresentationRoot);
    HealthBarFill->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HealthBarFill->SetRelativeLocation(FVector(0.0f, 0.0f, 140.0f));
    HealthBarFill->SetRelativeScale3D(FVector(0.72f, 0.06f, 0.025f));

    SelectionRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionRing"));
    SelectionRing->SetupAttachment(GetCapsuleComponent());
    SelectionRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SelectionRing->SetRelativeLocation(FVector(0.0f, 0.0f, -86.0f));
    SelectionRing->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.025f));
    SelectionRing->SetVisibility(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        HealthBarBackground->SetStaticMesh(CubeMesh.Object);
        HealthBarFill->SetStaticMesh(CubeMesh.Object);
    }
    if (PrototypeMesh.Succeeded())
    {
        SelectionRing->SetStaticMesh(PrototypeMesh.Object);
    }

    TeamLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TeamLabel"));
    TeamLabel->SetupAttachment(OverheadPresentationRoot);
    TeamLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TeamLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 165.0f));
    TeamLabel->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
    TeamLabel->SetHorizontalAlignment(EHTA_Center);
    TeamLabel->SetVerticalAlignment(EVRTA_TextCenter);
    TeamLabel->SetWorldSize(42.0f);
    TeamLabel->SetCastShadow(false);

    DamageText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DamageText"));
    DamageText->SetupAttachment(OverheadPresentationRoot);
    DamageText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DamageText->SetRelativeLocation(FVector(0.0f, 0.0f, 215.0f));
    DamageText->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
    DamageText->SetHorizontalAlignment(EHTA_Center);
    DamageText->SetVerticalAlignment(EVRTA_TextCenter);
    DamageText->SetWorldSize(58.0f);
    DamageText->SetTextRenderColor(FColor(255, 210, 20));
    DamageText->SetCastShadow(false);
    DamageText->SetVisibility(false);

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
    GetCharacterMovement()->MaxWalkSpeed = 600.0f;
    bUseControllerRotationYaw = false;
}

void AEFHeroCharacter::BeginPlay()
{
    Super::BeginPlay();

    BodyMaterial = PrototypeBody->CreateAndSetMaterialInstanceDynamic(0);
    HealthFillMaterial = HealthBarFill->CreateAndSetMaterialInstanceDynamic(0);
    HealthBackgroundMaterial = HealthBarBackground->CreateAndSetMaterialInstanceDynamic(0);
    SelectionMaterial = SelectionRing->CreateAndSetMaterialInstanceDynamic(0);
    if (HealthFillMaterial)
    {
        HealthFillMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.8f, 0.1f));
    }
    if (HealthBackgroundMaterial)
    {
        HealthBackgroundMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.02f, 0.02f));
    }
    if (SelectionMaterial)
    {
        SelectionMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.8f, 0.05f));
    }

    UpdateTeamPresentation();
    UpdateHealthBar();
}

void AEFHeroCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    FaceOverheadPresentationToLocalCamera();

    if (AttackPulseTimeRemaining > 0.0f)
    {
        AttackPulseTimeRemaining = FMath::Max(0.0f, AttackPulseTimeRemaining - DeltaSeconds);
        const float Progress = 1.0f - AttackPulseTimeRemaining / AttackPulseDuration;
        const float Pulse = FMath::Sin(Progress * UE_PI) * 0.18f;
        PrototypeBody->SetRelativeScale3D(FVector(0.65f + Pulse, 0.65f + Pulse, 1.8f - Pulse));
        PrototypeBody->SetRelativeLocation(FVector(FMath::Sin(Progress * UE_PI) * 35.0f, 0.0f, 0.0f));
        if (AttackPulseTimeRemaining <= 0.0f)
        {
            PrototypeBody->SetRelativeScale3D(FVector(0.65f, 0.65f, 1.8f));
            PrototypeBody->SetRelativeLocation(FVector::ZeroVector);
        }
    }

    if (DamageTextTimeRemaining > 0.0f)
    {
        DamageTextTimeRemaining = FMath::Max(0.0f, DamageTextTimeRemaining - DeltaSeconds);
        const float Progress = 1.0f - DamageTextTimeRemaining / DamageTextDuration;
        DamageText->SetRelativeLocation(FVector(0.0f, 0.0f, 215.0f + Progress * 55.0f));
        if (DamageTextTimeRemaining <= 0.0f)
        {
            DamageText->SetVisibility(false);
        }
    }

    if (OwningPlayerState && PresentedMatchSide != OwningPlayerState->GetMatchSide())
    {
        UpdateTeamPresentation();
    }

    if (!HasAuthority() || !bHasDirectMoveDestination)
    {
        return;
    }

    const FVector ToDestination = DirectMoveDestination - GetActorLocation();
    const FVector PlanarOffset(ToDestination.X, ToDestination.Y, 0.0f);
    if (PlanarOffset.SizeSquared() <= FMath::Square(DirectMoveAcceptanceRadius))
    {
        CancelDirectMove();
        return;
    }

    AddMovementInput(PlanarOffset.GetSafeNormal());
}

void AEFHeroCharacter::SetDirectMoveDestination(const FVector& NewDestination)
{
    if (HasAuthority() && !NewDestination.ContainsNaN())
    {
        DirectMoveDestination = NewDestination;
        bHasDirectMoveDestination = true;
    }
}

void AEFHeroCharacter::CancelDirectMove()
{
    bHasDirectMoveDestination = false;
}

void AEFHeroCharacter::StopMovementForAttack()
{
    CancelDirectMove();
    if (AAIController* AIController = Cast<AAIController>(GetController()))
    {
        AIController->StopMovement();
    }
    GetCharacterMovement()->StopMovementImmediately();
}

void AEFHeroCharacter::FaceAttackTarget(const AActor* TargetActor)
{
    if (!HasAuthority() || !IsValid(TargetActor))
    {
        return;
    }

    const FVector ToTarget = TargetActor->GetActorLocation() - GetActorLocation();
    if (!ToTarget.IsNearlyZero())
    {
        SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
    }
}

void AEFHeroCharacter::NotifyBasicAttackContact()
{
    if (!HasAuthority())
    {
        return;
    }

    ++AttackContactCounter;
    OnRep_AttackContactCounter();
}

void AEFHeroCharacter::NotifyDamageReceived(float DamageAmount)
{
    if (!HasAuthority() || DamageAmount <= 0.0f)
    {
        return;
    }

    LastDamageAmount = DamageAmount;
    ++DamageEventCounter;
    OnRep_DamageEventCounter();
}

void AEFHeroCharacter::HandleHealthDepleted(AEFPlayerState* KillerPlayerState)
{
    if (!HasAuthority() || bIsDead)
    {
        return;
    }

    bIsDead = true;
    OnRep_IsDead();
    if (CachedAbilitySystemComponent)
    {
        CachedAbilitySystemComponent->AddLooseGameplayTag(EFGameplayTags::State_Dead);
    }
    if (CombatComponent)
    {
        CombatComponent->CancelBasicAttack();
    }
    StopMovementForAttack();

    if (AEFGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AEFGameMode>() : nullptr)
    {
        GameMode->HandleHeroDeath(this, OwningPlayerState, KillerPlayerState);
    }
}

void AEFHeroCharacter::SetLocallySelected(bool bSelected)
{
    if (SelectionRing)
    {
        SelectionRing->SetVisibility(bSelected);
    }
}

float AEFHeroCharacter::GetBasicAttackRange() const
{
    const UEFHeroAttributeSet* Attributes = OwningPlayerState ? OwningPlayerState->GetHeroAttributeSet() : nullptr;
    return Attributes ? FMath::Max(0.0f, Attributes->GetAttackRange()) : 150.0f;
}

UAbilitySystemComponent* AEFHeroCharacter::GetAbilitySystemComponent() const
{
    return CachedAbilitySystemComponent;
}

void AEFHeroCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitializeAbilityActorInfo();
}

void AEFHeroCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AEFHeroCharacter, OwningPlayerState);
    DOREPLIFETIME(AEFHeroCharacter, bIsDead);
    DOREPLIFETIME(AEFHeroCharacter, AttackContactCounter);
    DOREPLIFETIME(AEFHeroCharacter, LastDamageAmount);
    DOREPLIFETIME(AEFHeroCharacter, DamageEventCounter);
}

void AEFHeroCharacter::SetOwningPlayerState(AEFPlayerState* NewPlayerState)
{
    if (!HasAuthority())
    {
        return;
    }

    OwningPlayerState = NewPlayerState;
    OnRep_OwningPlayerState();
}

void AEFHeroCharacter::OnRep_OwningPlayerState()
{
    InitializeAbilityActorInfo();
    UpdateTeamPresentation();
}

void AEFHeroCharacter::OnRep_IsDead()
{
    GetCapsuleComponent()->SetCollisionEnabled(bIsDead ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    GetCharacterMovement()->SetMovementMode(bIsDead ? MOVE_None : MOVE_Walking);
    PrototypeBody->SetRelativeRotation(bIsDead ? FRotator(0.0f, 90.0f, 0.0f) : FRotator::ZeroRotator);
    HealthBarBackground->SetVisibility(!bIsDead);
    HealthBarFill->SetVisibility(!bIsDead);
    TeamLabel->SetVisibility(!bIsDead);
    DamageText->SetVisibility(false);
    SelectionRing->SetVisibility(false);
}

void AEFHeroCharacter::OnRep_AttackContactCounter()
{
    AttackPulseTimeRemaining = AttackPulseDuration;
}

void AEFHeroCharacter::OnRep_DamageEventCounter()
{
    DamageTextTimeRemaining = DamageTextDuration;
    DamageText->SetRelativeLocation(FVector(0.0f, 0.0f, 215.0f));
    DamageText->SetText(FText::FromString(FString::Printf(TEXT("-%.0f"), LastDamageAmount)));
    DamageText->SetVisibility(true);
}

void AEFHeroCharacter::InitializeAbilityActorInfo()
{
    AEFPlayerState* EFPlayerState = OwningPlayerState;
    if (!EFPlayerState)
    {
        return;
    }

    CachedAbilitySystemComponent = EFPlayerState->GetEFAbilitySystemComponent();
    if (CachedAbilitySystemComponent)
    {
        CachedAbilitySystemComponent->RefreshAbilityActorInfo(EFPlayerState, this);
        BindAttributePresentation();
        UpdateHealthBar();
    }
}

void AEFHeroCharacter::BindAttributePresentation()
{
    if (!CachedAbilitySystemComponent || bAttributePresentationBound)
    {
        return;
    }

    CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UEFHeroAttributeSet::GetHealthAttribute())
        .AddUObject(this, &AEFHeroCharacter::HandleHealthChanged);
    CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UEFHeroAttributeSet::GetMaxHealthAttribute())
        .AddUObject(this, &AEFHeroCharacter::HandleMaxHealthChanged);
    bAttributePresentationBound = true;
}

void AEFHeroCharacter::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
    UpdateHealthBar();
}

void AEFHeroCharacter::HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
    UpdateHealthBar();
}

void AEFHeroCharacter::UpdateHealthBar()
{
    const UEFHeroAttributeSet* Attributes = OwningPlayerState ? OwningPlayerState->GetHeroAttributeSet() : nullptr;
    const float Percent = Attributes ? FMath::Clamp(Attributes->GetHealth() / FMath::Max(1.0f, Attributes->GetMaxHealth()), 0.0f, 1.0f) : 1.0f;
    const float FullScaleX = 0.72f;
    HealthBarFill->SetRelativeScale3D(FVector(FullScaleX * Percent, 0.06f, 0.025f));
    HealthBarFill->SetRelativeLocation(FVector(-50.0f * FullScaleX * (1.0f - Percent), 0.0f, 140.0f));

    if (Attributes && OwningPlayerState && TeamLabel)
    {
        const TCHAR* SideName = OwningPlayerState->GetMatchSide() == EEFMatchSide::Dawn ? TEXT("DAWN") : TEXT("DUSK");
        TeamLabel->SetText(FText::FromString(FString::Printf(
            TEXT("%s  %.0f / %.0f HP"), SideName, Attributes->GetHealth(), Attributes->GetMaxHealth())));
    }
}

void AEFHeroCharacter::UpdateTeamPresentation()
{
    if (!BodyMaterial || !OwningPlayerState)
    {
        return;
    }

    const FLinearColor TeamColor = OwningPlayerState->GetMatchSide() == EEFMatchSide::Dawn
        ? FLinearColor(0.05f, 0.25f, 1.0f)
        : FLinearColor(1.0f, 0.08f, 0.04f);
    BodyMaterial->SetVectorParameterValue(TEXT("Color"), TeamColor);
    const bool bIsDawn = OwningPlayerState->GetMatchSide() == EEFMatchSide::Dawn;
    TeamLabel->SetTextRenderColor(bIsDawn ? FColor(30, 100, 255) : FColor(255, 30, 20));
    PresentedMatchSide = OwningPlayerState->GetMatchSide();
    UpdateHealthBar();
}

void AEFHeroCharacter::FaceOverheadPresentationToLocalCamera()
{
    if (!OverheadPresentationRoot || !GetWorld())
    {
        return;
    }

    const APlayerController* LocalController = GetWorld()->GetFirstPlayerController();
    const APlayerCameraManager* CameraManager = LocalController ? LocalController->PlayerCameraManager : nullptr;
    if (!CameraManager)
    {
        return;
    }

    // The indicators lie in the world XY plane. TextRender's in-plane up axis is
    // opposite to the camera's screen-up direction, so face the root away from the
    // camera yaw to keep labels readable instead of displaying them upside down.
    OverheadPresentationRoot->SetWorldRotation(
        FRotator(0.0f, CameraManager->GetCameraRotation().Yaw + 180.0f, 0.0f));
}
