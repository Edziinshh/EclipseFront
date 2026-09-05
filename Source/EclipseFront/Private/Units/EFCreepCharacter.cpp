#include "Units/EFCreepCharacter.h"

#include "AIController.h"
#include "AbilitySystem/EFAbilitySystemComponent.h"
#include "AbilitySystem/EFHeroAttributeSet.h"
#include "Combat/EFCombatComponent.h"
#include "Debug/EFLogCategories.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/EFPlayerState.h"
#include "UObject/ConstructorHelpers.h"
#include "Units/EFHeroCharacter.h"
#include "Units/EFLaneTower.h"
#include "Game/EFGameState.h"
#include "VisualLogger/VisualLogger.h"

AEFCreepCharacter::AEFCreepCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;
    bReplicates = true;
    SetReplicateMovement(true);
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();

    AbilitySystemComponent = CreateDefaultSubobject<UEFAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
    CreepAttributeSet = CreateDefaultSubobject<UEFHeroAttributeSet>(TEXT("CreepAttributeSet"));
    CombatComponent = CreateDefaultSubobject<UEFCombatComponent>(TEXT("CombatComponent"));

    GetCapsuleComponent()->InitCapsuleSize(34.0f, 58.0f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    // Combat state controls stopping and attack range. Ignoring Pawn collision
    // prevents physical deadlocks while preserving world/obstacle collision.
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

    PrototypeBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeBody"));
    PrototypeBody->SetupAttachment(GetCapsuleComponent());
    PrototypeBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrototypeBody->SetRelativeScale3D(FVector(0.38f, 0.38f, 0.9f));

    SideMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SideMarker"));
    SideMarker->SetupAttachment(GetCapsuleComponent());
    SideMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SideMarker->SetRelativeLocation(FVector(0.0f, 0.0f, 67.0f));
    SideMarker->SetRelativeScale3D(FVector(0.2f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (CylinderMesh.Succeeded())
    {
        PrototypeBody->SetStaticMesh(CylinderMesh.Object);
    }
    if (SphereMesh.Succeeded())
    {
        SideMarker->SetStaticMesh(SphereMesh.Object);
    }

    OverheadPresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("OverheadPresentationRoot"));
    OverheadPresentationRoot->SetupAttachment(GetCapsuleComponent());
    OverheadPresentationRoot->SetUsingAbsoluteRotation(true);

    HealthBarBackground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HealthBarBackground"));
    HealthBarBackground->SetupAttachment(OverheadPresentationRoot);
    HealthBarBackground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HealthBarBackground->SetRelativeLocation(FVector(0.0f, 0.0f, 104.0f));
    HealthBarBackground->SetRelativeScale3D(FVector(0.42f, 0.07f, 0.025f));

    HealthBarFill = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HealthBarFill"));
    HealthBarFill->SetupAttachment(OverheadPresentationRoot);
    HealthBarFill->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HealthBarFill->SetRelativeLocation(FVector(0.0f, 0.0f, 108.0f));
    HealthBarFill->SetRelativeScale3D(FVector(0.4f, 0.045f, 0.018f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        HealthBarBackground->SetStaticMesh(CubeMesh.Object);
        HealthBarFill->SetStaticMesh(CubeMesh.Object);
    }

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);
    GetCharacterMovement()->MaxWalkSpeed = 360.0f;
    bUseControllerRotationYaw = false;

    SetNetCullDistanceSquared(FMath::Square(15000.0f));
    InitialLifeSpan = 120.0f;
}

void AEFCreepCharacter::BeginPlay()
{
    Super::BeginPlay();

    BodyMaterial = PrototypeBody->CreateAndSetMaterialInstanceDynamic(0);
    MarkerMaterial = SideMarker->CreateAndSetMaterialInstanceDynamic(0);
    HealthFillMaterial = HealthBarFill->CreateAndSetMaterialInstanceDynamic(0);
    HealthBackgroundMaterial = HealthBarBackground->CreateAndSetMaterialInstanceDynamic(0);
    if (HealthFillMaterial)
    {
        HealthFillMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.8f, 0.1f));
    }
    if (HealthBackgroundMaterial)
    {
        HealthBackgroundMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.02f, 0.02f));
    }

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->RefreshAbilityActorInfo(this, this);
        if (HasAuthority())
        {
            AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetMaxHealthAttribute(), 300.0f);
            AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetHealthAttribute(), 300.0f);
            AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetAttackDamageAttribute(), 35.0f);
            AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetAttackSpeedAttribute(), 1.0f);
            AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetAttackRangeAttribute(), 125.0f);
            AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetArmorAttribute(), 0.0f);
            AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetMoveSpeedAttribute(), 360.0f);
        }
    }
    UpdateTeamPresentation();
    UpdateHealthBar();
}

void AEFCreepCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    FaceOverheadPresentationToLocalCamera();
    UpdateHealthBar();

    if (AttackPulseTimeRemaining > 0.0f)
    {
        AttackPulseTimeRemaining = FMath::Max(0.0f, AttackPulseTimeRemaining - DeltaSeconds);
        const float Progress = 1.0f - AttackPulseTimeRemaining / AttackPulseDuration;
        const float Pulse = FMath::Sin(Progress * UE_PI);
        PrototypeBody->SetRelativeLocation(FVector(Pulse * AttackLungeDistance, 0.0f, 0.0f));
        PrototypeBody->SetRelativeScale3D(FVector(0.38f + Pulse * 0.14f, 0.38f + Pulse * 0.14f, 0.9f - Pulse * 0.12f));
        if (BodyMaterial)
        {
            const FLinearColor SideColor = MatchSide == EEFMatchSide::Dawn
                ? FLinearColor(0.05f, 0.35f, 1.0f)
                : FLinearColor(1.0f, 0.05f, 0.03f);
            BodyMaterial->SetVectorParameterValue(
                TEXT("Color"), FMath::Lerp(SideColor, FLinearColor(1.0f, 0.8f, 0.15f), Pulse * 0.8f));
        }
        if (AttackPulseTimeRemaining <= 0.0f)
        {
            PrototypeBody->SetRelativeLocation(FVector::ZeroVector);
            PrototypeBody->SetRelativeScale3D(FVector(0.38f, 0.38f, 0.9f));
            UpdateTeamPresentation();
        }
    }

    const AEFGameState* Match = GetWorld()->GetGameState<AEFGameState>();
    if (!HasAuthority() || bIsDead || !LaneRoute.IsValidIndex(CurrentWaypointIndex)
        || (Match && Match->GetMatchPhase() == EEFMatchPhase::PostGame))
    {
        return;
    }

    AggroScanTimeRemaining -= DeltaSeconds;
    if (AggroScanTimeRemaining <= 0.0f)
    {
        AggroScanTimeRemaining = AggroScanInterval;
        if (AActor* HostileTarget = FindNearestHostileTarget())
        {
            AActor* CurrentTarget = CombatComponent ? CombatComponent->GetAttackTarget() : nullptr;
            const bool bShouldAcquire = !IsValid(CurrentTarget);
            const bool bShouldRetargetToCreep = IsValid(CurrentTarget)
                && !Cast<AEFCreepCharacter>(CurrentTarget)
                && Cast<AEFCreepCharacter>(HostileTarget);
            if ((bShouldAcquire || bShouldRetargetToCreep)
                && CombatComponent && CombatComponent->BeginBasicAttack(HostileTarget))
            {
                UE_LOG(LogEFAI, Verbose, TEXT("[%s] Creep target side=%s action=%s target=%s"),
                    EFLog::GetNetContext(this), MatchSide == EEFMatchSide::Dawn ? TEXT("Dawn") : TEXT("Dusk"),
                    bShouldRetargetToCreep ? TEXT("re-aggroed to") : TEXT("engaged"),
                    *HostileTarget->GetName());
                UE_VLOG_ARROW(this, LogEFAI, Display, GetActorLocation(), HostileTarget->GetActorLocation(),
                    bShouldRetargetToCreep ? FColor::Magenta : FColor::Orange,
                    TEXT("%s %s"), bShouldRetargetToCreep ? TEXT("Re-aggro") : TEXT("Engage"),
                    *HostileTarget->GetName());
                return;
            }
        }
    }

    if (CombatComponent && CombatComponent->GetAttackTarget())
    {
        return;
    }

    const FVector PlanarOffset = FVector::VectorPlaneProject(
        LaneRoute[CurrentWaypointIndex] - GetActorLocation(), FVector::UpVector);
    if (PlanarOffset.SizeSquared() <= FMath::Square(WaypointAcceptanceRadius))
    {
        UE_LOG(LogEFAI, Verbose, TEXT("[%s] Creep waypoint side=%s index=%d/%d"),
            EFLog::GetNetContext(this), MatchSide == EEFMatchSide::Dawn ? TEXT("Dawn") : TEXT("Dusk"),
            CurrentWaypointIndex,
            LaneRoute.Num() - 1);
        ++CurrentWaypointIndex;
        LastFailedWaypointLogIndex = INDEX_NONE;
        if (!LaneRoute.IsValidIndex(CurrentWaypointIndex))
        {
            UE_LOG(LogEFAI, Verbose, TEXT("[%s] Creep route completed side=%s creep=%s"),
                EFLog::GetNetContext(this), MatchSide == EEFMatchSide::Dawn ? TEXT("Dawn") : TEXT("Dusk"), *GetName());
            Destroy();
            return;
        }

        IssueMoveToCurrentWaypoint();
        return;
    }

    MoveRetryTimeRemaining -= DeltaSeconds;
    if (MoveRetryTimeRemaining <= 0.0f)
    {
        IssueMoveToCurrentWaypoint();
    }
}

void AEFCreepCharacter::InitializeLaneCreep(EEFMatchSide NewMatchSide, const TArray<FVector>& NewRoute)
{
    if (!HasAuthority() || NewMatchSide == EEFMatchSide::Unassigned || NewRoute.Num() < 2)
    {
        return;
    }

    MatchSide = NewMatchSide;
    LaneRoute = NewRoute;
    CurrentWaypointIndex = 1;
    UpdateTeamPresentation();
    ForceNetUpdate();

    if (!GetController())
    {
        SpawnDefaultController();
    }
    IssueMoveToCurrentWaypoint();
}

void AEFCreepCharacter::ConfigureWaveUnit(bool bRanged, bool bUpgraded)
{
    if (!HasAuthority()) { return; }
    const float Health = (bRanged ? 220.0f : 300.0f) * (bUpgraded ? 1.5f : 1.0f);
    AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetMaxHealthAttribute(), Health);
    AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetHealthAttribute(), Health);
    AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetAttackDamageAttribute(),
        (bRanged ? 28.0f : 35.0f) * (bUpgraded ? 1.5f : 1.0f));
    AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetAttackRangeAttribute(), bRanged ? 450.0f : 125.0f);
    UE_LOG(LogEFAI, Verbose, TEXT("[%s] WaveUnit configured creep=%s ranged=%d upgraded=%d"),
        EFLog::GetNetContext(this), *GetName(), bRanged, bUpgraded);
}

void AEFCreepCharacter::IssueMoveToCurrentWaypoint()
{
    MoveRetryTimeRemaining = MoveRetryInterval;
    AAIController* CreepController = Cast<AAIController>(GetController());
    if (!CreepController || !LaneRoute.IsValidIndex(CurrentWaypointIndex))
    {
        return;
    }

    const EPathFollowingRequestResult::Type MoveResult = CreepController->MoveToLocation(
        LaneRoute[CurrentWaypointIndex],
        WaypointAcceptanceRadius,
        false,
        true,
        true,
        false,
        nullptr,
        false);
    if (MoveResult == EPathFollowingRequestResult::Failed)
    {
        UE_VLOG_ARROW(this, LogEFMovement, Warning, GetActorLocation(), LaneRoute[CurrentWaypointIndex],
            FColor::Red, TEXT("Lane waypoint %d/%d failed"), CurrentWaypointIndex, LaneRoute.Num() - 1);
    }
    else
    {
        UE_VLOG_ARROW(this, LogEFMovement, Verbose, GetActorLocation(), LaneRoute[CurrentWaypointIndex],
            FColor::Green, TEXT("Lane waypoint %d/%d accepted result=%d"), CurrentWaypointIndex,
            LaneRoute.Num() - 1, static_cast<int32>(MoveResult));
    }
    if (MoveResult == EPathFollowingRequestResult::Failed && LastFailedWaypointLogIndex != CurrentWaypointIndex)
    {
        LastFailedWaypointLogIndex = CurrentWaypointIndex;
        UE_LOG(LogEFMovement, Warning, TEXT("[%s] CreepMove rejected side=%s reason=PathRequestFailed waypoint=%d/%d location=%s"),
            EFLog::GetNetContext(this), MatchSide == EEFMatchSide::Dawn ? TEXT("Dawn") : TEXT("Dusk"),
            CurrentWaypointIndex,
            LaneRoute.Num() - 1,
            *LaneRoute[CurrentWaypointIndex].ToCompactString());
    }
}

void AEFCreepCharacter::OnRep_MatchSide()
{
    UpdateTeamPresentation();
}

void AEFCreepCharacter::OnRep_IsDead()
{
    GetCapsuleComponent()->SetCollisionEnabled(bIsDead ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    GetCharacterMovement()->SetMovementMode(bIsDead ? MOVE_None : MOVE_Walking);
    HealthBarBackground->SetVisibility(!bIsDead);
    HealthBarFill->SetVisibility(!bIsDead);
    SideMarker->SetVisibility(!bIsDead);
    PrototypeBody->SetRelativeRotation(bIsDead ? FRotator(0.0f, 90.0f, 0.0f) : FRotator::ZeroRotator);
}

void AEFCreepCharacter::OnRep_AttackContactCounter()
{
    AttackPulseTimeRemaining = AttackPulseDuration;
}

void AEFCreepCharacter::UpdateTeamPresentation()
{
    const FLinearColor TeamColor = MatchSide == EEFMatchSide::Dawn
        ? FLinearColor(0.05f, 0.35f, 1.0f)
        : (MatchSide == EEFMatchSide::Dusk
            ? FLinearColor(1.0f, 0.05f, 0.03f)
            : FLinearColor(0.35f, 0.35f, 0.35f));

    if (BodyMaterial)
    {
        BodyMaterial->SetVectorParameterValue(TEXT("Color"), TeamColor);
    }
    if (MarkerMaterial)
    {
        MarkerMaterial->SetVectorParameterValue(TEXT("Color"), TeamColor * 2.0f);
    }
}

void AEFCreepCharacter::UpdateHealthBar()
{
    if (!HealthBarFill || !CreepAttributeSet)
    {
        return;
    }

    const float Percent = FMath::Clamp(
        CreepAttributeSet->GetHealth() / FMath::Max(1.0f, CreepAttributeSet->GetMaxHealth()), 0.0f, 1.0f);
    constexpr float FullScaleX = 0.4f;
    HealthBarFill->SetRelativeScale3D(FVector(FullScaleX * Percent, 0.045f, 0.018f));
    HealthBarFill->SetRelativeLocation(FVector(-50.0f * FullScaleX * (1.0f - Percent), 0.0f, 108.0f));
}

void AEFCreepCharacter::FaceOverheadPresentationToLocalCamera()
{
    const APlayerController* LocalController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    const APlayerCameraManager* CameraManager = LocalController ? LocalController->PlayerCameraManager : nullptr;
    if (OverheadPresentationRoot && CameraManager)
    {
        OverheadPresentationRoot->SetWorldRotation(
            FRotator(0.0f, CameraManager->GetCameraRotation().Yaw + 180.0f, 0.0f));
    }
}

AActor* AEFCreepCharacter::FindNearestHostileTarget() const
{
    AActor* BestTarget = nullptr;
    float BestDistanceSquared = FMath::Square(AggroRadius);

    for (TActorIterator<AEFCreepCharacter> It(GetWorld()); It; ++It)
    {
        AEFCreepCharacter* Candidate = *It;
        if (!IsValid(Candidate) || Candidate == this || Candidate->IsDead()
            || Candidate->GetMatchSide() == MatchSide || Candidate->GetMatchSide() == EEFMatchSide::Unassigned)
        {
            continue;
        }
        const float DistanceSquared = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = Candidate;
        }
    }

    // Lane units are the primary aggro target. Once one is nearby, do not keep
    // chasing a hero who previously pulled the wave.
    if (BestTarget)
    {
        return BestTarget;
    }

    for (TActorIterator<AEFHeroCharacter> It(GetWorld()); It; ++It)
    {
        AEFHeroCharacter* Candidate = *It;
        const AEFPlayerState* CandidateState = Candidate ? Candidate->GetOwningPlayerState() : nullptr;
        if (!IsValid(Candidate) || Candidate->IsDead() || !CandidateState
            || CandidateState->GetMatchSide() == MatchSide
            || CandidateState->GetMatchSide() == EEFMatchSide::Unassigned)
        {
            continue;
        }
        const float DistanceSquared = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = Candidate;
        }
    }

    if (BestTarget)
    {
        return BestTarget;
    }

    for (TActorIterator<AEFLaneTower> It(GetWorld()); It; ++It)
    {
        AEFLaneTower* Candidate = *It;
        if (!IsValid(Candidate) || !Candidate->IsVulnerable()
            || Candidate->GetMatchSide() == MatchSide || Candidate->GetMatchSide() == EEFMatchSide::Unassigned)
        {
            continue;
        }
        const float DistanceSquared = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = Candidate;
        }
    }

    return BestTarget;
}

UAbilitySystemComponent* AEFCreepCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

float AEFCreepCharacter::GetBasicAttackRange() const
{
    return CreepAttributeSet ? FMath::Max(0.0f, CreepAttributeSet->GetAttackRange()) : 125.0f;
}

void AEFCreepCharacter::StopMovementForAttack()
{
    if (AAIController* CreepController = Cast<AAIController>(GetController()))
    {
        CreepController->StopMovement();
    }
    GetCharacterMovement()->StopMovementImmediately();
}

void AEFCreepCharacter::FaceAttackTarget(const AActor* TargetActor)
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

void AEFCreepCharacter::NotifyBasicAttackContact()
{
    if (HasAuthority())
    {
        ++AttackContactCounter;
        OnRep_AttackContactCounter();
    }
}

void AEFCreepCharacter::NotifyDamageReceived(float DamageAmount)
{
    if (HasAuthority() && DamageAmount > 0.0f)
    {
        UpdateHealthBar();
    }
}

void AEFCreepCharacter::HandleHealthDepleted(AActor* DamageInstigator)
{
    if (!HasAuthority() || bIsDead)
    {
        return;
    }

    bIsDead = true;

    AEFPlayerState* LastHitPlayerState = Cast<AEFPlayerState>(DamageInstigator);
    if (!LastHitPlayerState)
    {
        const AEFHeroCharacter* InstigatorHero = Cast<AEFHeroCharacter>(DamageInstigator);
        LastHitPlayerState = InstigatorHero ? InstigatorHero->GetOwningPlayerState() : nullptr;
    }
    if (LastHitPlayerState && LastHitPlayerState->GetMatchSide() != MatchSide)
    {
        LastHitPlayerState->AwardCreepLastHit(GoldBounty);
    }

    TSet<AEFPlayerState*> RewardedPlayerStates;
    for (TActorIterator<AEFHeroCharacter> It(GetWorld()); It; ++It)
    {
        AEFHeroCharacter* NearbyHero = *It;
        AEFPlayerState* NearbyPlayerState = NearbyHero ? NearbyHero->GetOwningPlayerState() : nullptr;
        if (!IsValid(NearbyHero) || NearbyHero->IsDead() || !NearbyPlayerState
            || NearbyPlayerState->GetMatchSide() == MatchSide
            || NearbyPlayerState->GetMatchSide() == EEFMatchSide::Unassigned
            || RewardedPlayerStates.Contains(NearbyPlayerState)
            || FVector::DistSquared2D(GetActorLocation(), NearbyHero->GetActorLocation()) > FMath::Square(ExperienceRadius))
        {
            continue;
        }

        RewardedPlayerStates.Add(NearbyPlayerState);
        NearbyPlayerState->AddExperience(ExperienceBounty);
        UE_LOG(LogEFCore, Verbose, TEXT("[%s] CreepXP player=%s reward=%d"),
            EFLog::GetNetContext(this), *NearbyPlayerState->GetPlayerName(), ExperienceBounty);
    }

    if (CombatComponent)
    {
        CombatComponent->CancelBasicAttack();
    }
    StopMovementForAttack();
    OnRep_IsDead();
    UE_LOG(LogEFCombat, Display, TEXT("[%s] CreepDeath side=%s creep=%s instigator=%s"),
        EFLog::GetNetContext(this), MatchSide == EEFMatchSide::Dawn ? TEXT("Dawn") : TEXT("Dusk"), *GetName(),
        IsValid(DamageInstigator) ? *DamageInstigator->GetName() : TEXT("unknown"));
    SetLifeSpan(0.5f);
}

void AEFCreepCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AEFCreepCharacter, MatchSide);
    DOREPLIFETIME(AEFCreepCharacter, bIsDead);
    DOREPLIFETIME(AEFCreepCharacter, AttackContactCounter);
}
