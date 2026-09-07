#include "Player/EFPlayerController.h"

#include "AIController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Combat/EFCombatComponent.h"
#include "Debug/EFLogCategories.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "Player/EFPlayerState.h"
#include "Units/EFCreepCharacter.h"
#include "Units/EFHeroCharacter.h"
#include "Units/EFLaneTower.h"
#include "Game/EFGameState.h"
#include "VisualLogger/VisualLogger.h"

namespace
{
EEFMatchSide GetOrderTargetSide(const AActor* Unit)
{
    if (const AEFHeroCharacter* Hero = Cast<AEFHeroCharacter>(Unit))
    {
        const AEFPlayerState* State = Hero->GetOwningPlayerState();
        return State ? State->GetMatchSide() : EEFMatchSide::Unassigned;
    }
    if (const AEFCreepCharacter* Creep = Cast<AEFCreepCharacter>(Unit))
    {
        return Creep->GetMatchSide();
    }
    if (const AEFLaneTower* Tower = Cast<AEFLaneTower>(Unit))
    {
        return Tower->GetMatchSide();
    }
    return EEFMatchSide::Unassigned;
}

bool IsDeadOrderTarget(const AActor* Unit)
{
    if (const AEFHeroCharacter* Hero = Cast<AEFHeroCharacter>(Unit))
    {
        return Hero->IsDead();
    }
    if (const AEFCreepCharacter* Creep = Cast<AEFCreepCharacter>(Unit))
    {
        return Creep->IsDead();
    }
    if (const AEFLaneTower* Tower = Cast<AEFLaneTower>(Unit))
    {
        return Tower->IsDead();
    }
    return true;
}

bool IsCombatOrderTarget(const AActor* Unit)
{
    return Cast<AEFHeroCharacter>(Unit) || Cast<AEFCreepCharacter>(Unit) || Cast<AEFLaneTower>(Unit);
}
}

AEFPlayerController::AEFPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    bShowMouseCursor = true;
    DefaultMouseCursor = EMouseCursor::Default;
}

void AEFPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    if (HasAuthority())
    {
        TickAttackMove(DeltaTime);
    }

    if (!IsLocalController())
    {
        return;
    }

    const bool bAltDown = IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt);
    if (bAttackRangesHeld != bAltDown)
    {
        UE_LOG(LogEFCore, Verbose, TEXT("[%s] AttackRanges visible=%d controller=%s"),
            EFLog::GetNetContext(this), bAltDown, *GetName());
    }
    bAttackRangesHeld = bAltDown;
    if (bAttackRangesHeld)
    {
        DrawAttackRanges(0.0f);
    }

    if (!bContextOrderHeld)
    {
        return;
    }

    HeldOrderTimeUntilUpdate -= DeltaTime;
    if (HeldOrderTimeUntilUpdate <= 0.0f)
    {
        HeldOrderTimeUntilUpdate = HeldOrderUpdateInterval;
        IssueContextOrder(true);
    }
}

void AEFPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalController())
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        SetInputMode(InputMode);
    }
}

void AEFPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AEFPlayerController::HandleContextOrderPressed);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AEFPlayerController::HandleContextOrderReleased);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AEFPlayerController::HandleSelectionPressed);
    InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AEFPlayerController::HandleStopOrder);
    InputComponent->BindKey(EKeys::H, IE_Pressed, this, &AEFPlayerController::HandleHoldPositionOrder);
    InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AEFPlayerController::HandleAttackMovePressed);
}

void AEFPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(AEFPlayerController, ControlledHero, COND_OwnerOnly);
}

void AEFPlayerController::SetControlledHero(AEFHeroCharacter* NewHero)
{
    if (!HasAuthority())
    {
        return;
    }

    ControlledHero = NewHero;
    if (ControlledHero)
    {
        ControlledHero->SetOwner(this);
        ControlledHero->SetOwningPlayerState(GetPlayerState<AEFPlayerState>());
    }
    OnRep_ControlledHero();
}

void AEFPlayerController::HandleContextOrderPressed()
{
    CancelLocalAttackMovePlacement();
    bContextOrderHeld = true;
    HeldOrderTimeUntilUpdate = HeldOrderUpdateInterval;
    bHasLastHeldMoveDestination = false;
    LastHeldAttackTarget.Reset();
    IssueContextOrder(false);
}

void AEFPlayerController::HandleContextOrderReleased()
{
    CancelLocalContinuousOrder();
}

void AEFPlayerController::HandleStopOrder()
{
    CancelLocalContinuousOrder();
    CancelLocalAttackMovePlacement();
    ServerRequestStop(false);
}

void AEFPlayerController::HandleHoldPositionOrder()
{
    CancelLocalContinuousOrder();
    CancelLocalAttackMovePlacement();
    ServerRequestStop(true);
}

void AEFPlayerController::HandleAttackMovePressed()
{
    if (!IsLocalController() || !IsValid(ControlledHero))
    {
        return;
    }

    CancelLocalContinuousOrder();
    bAttackMovePlacementPending = true;
    UE_LOG(LogEFCore, Verbose, TEXT("[%s] AttackMove placement armed controller=%s"),
        EFLog::GetNetContext(this), *GetName());
}

void AEFPlayerController::CancelLocalContinuousOrder()
{
    bContextOrderHeld = false;
    bHasLastHeldMoveDestination = false;
    LastHeldAttackTarget.Reset();
}

void AEFPlayerController::CancelLocalAttackMovePlacement()
{
    bAttackMovePlacementPending = false;
}

void AEFPlayerController::IssueContextOrder(bool bContinuousUpdate)
{
    if (!IsLocalController() || !IsValid(ControlledHero))
    {
        return;
    }

    FHitResult UnitHit;
    if (GetHitResultUnderCursor(ECC_Visibility, true, UnitHit) && UnitHit.bBlockingHit)
    {
        AActor* TargetUnit = UnitHit.GetActor();
        if (IsCombatOrderTarget(TargetUnit) && TargetUnit != ControlledHero && !IsDeadOrderTarget(TargetUnit))
        {
            if (!bContinuousUpdate || LastHeldAttackTarget.Get() != TargetUnit)
            {
                ServerRequestAttack(TargetUnit);
            }
            LastHeldAttackTarget = TargetUnit;
            bHasLastHeldMoveDestination = false;
            return;
        }
    }

    FHitResult GroundHit;
    if (GetHitResultUnderCursor(ECC_Visibility, false, GroundHit) && GroundHit.bBlockingHit)
    {
        if (AActor* AssistedTarget = FindAssistedAttackTarget(GroundHit.ImpactPoint))
        {
            if (!bContinuousUpdate || LastHeldAttackTarget.Get() != AssistedTarget)
            {
                ServerRequestAttack(AssistedTarget);
            }
            LastHeldAttackTarget = AssistedTarget;
            bHasLastHeldMoveDestination = false;
            return;
        }

        LastHeldAttackTarget.Reset();
        const bool bDestinationChanged = !bHasLastHeldMoveDestination
            || FVector::DistSquared2D(LastHeldMoveDestination, GroundHit.ImpactPoint)
                >= FMath::Square(HeldOrderMinCursorDistance);
        if (!bContinuousUpdate)
        {
            ServerRequestMove(GroundHit.ImpactPoint);
        }
        else if (bDestinationChanged)
        {
            ServerUpdateHeldMove(GroundHit.ImpactPoint);
        }

        LastHeldMoveDestination = GroundHit.ImpactPoint;
        bHasLastHeldMoveDestination = true;
    }
}

void AEFPlayerController::HandleSelectionPressed()
{
    if (!IsLocalController())
    {
        return;
    }

    if (bAttackMovePlacementPending)
    {
        bAttackMovePlacementPending = false;
        FHitResult GroundHit;
        if (GetHitResultUnderCursor(ECC_Visibility, false, GroundHit) && GroundHit.bBlockingHit)
        {
            ServerRequestAttackMove(GroundHit.ImpactPoint);
            DrawDebugCircle(GetWorld(), GroundHit.ImpactPoint + FVector(0.0f, 0.0f, 12.0f), 85.0f, 32,
                FColor::Cyan, false, 0.8f, 0, 4.0f, FVector::ForwardVector, FVector::RightVector, false);
        }
        return;
    }

    FHitResult CursorHit;
    AEFHeroCharacter* NewSelection = nullptr;
    if (GetHitResultUnderCursor(ECC_Pawn, true, CursorHit) && CursorHit.bBlockingHit)
    {
        NewSelection = Cast<AEFHeroCharacter>(CursorHit.GetActor());
    }

    if (LocallySelectedUnit)
    {
        LocallySelectedUnit->SetLocallySelected(false);
    }
    LocallySelectedUnit = NewSelection;
    if (LocallySelectedUnit)
    {
        LocallySelectedUnit->SetLocallySelected(true);
    }
}

void AEFPlayerController::DrawAttackRanges(float Lifetime) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const auto DrawUnitRange = [World, Lifetime](const AActor* Unit, float Range, EEFMatchSide Side)
    {
        if (!IsValid(Unit) || Range <= 0.0f || Side == EEFMatchSide::Unassigned)
        {
            return;
        }
        const FColor Color = Side == EEFMatchSide::Dawn ? FColor(30, 140, 255) : FColor(255, 55, 35);
        FVector Center = Unit->GetActorLocation();
        if (const ACharacter* Character = Cast<ACharacter>(Unit))
        {
            Center.Z -= Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        }
        DrawDebugCircle(
            World, Center + FVector(0.0f, 0.0f, 8.0f), Range, 72, Color,
            false, Lifetime, 0, 2.5f, FVector::ForwardVector, FVector::RightVector, false);
    };

    for (TActorIterator<AEFHeroCharacter> It(World); It; ++It)
    {
        const AEFHeroCharacter* Hero = *It;
        if (!Hero->IsDead())
        {
            DrawUnitRange(Hero, Hero->GetBasicAttackRange(), GetOrderTargetSide(Hero));
        }
    }
    for (TActorIterator<AEFCreepCharacter> It(World); It; ++It)
    {
        const AEFCreepCharacter* Creep = *It;
        if (!Creep->IsDead())
        {
            DrawUnitRange(Creep, Creep->GetBasicAttackRange(), Creep->GetMatchSide());
        }
    }
    for (TActorIterator<AEFLaneTower> It(World); It; ++It)
    {
        const AEFLaneTower* Tower = *It;
        if (!Tower->IsDead())
        {
            DrawUnitRange(Tower, Tower->GetBasicAttackRange(), Tower->GetMatchSide());
        }
    }
}

void AEFPlayerController::ServerRequestMove_Implementation(FVector_NetQuantize Destination)
{
    ApplyMoveOrder(Destination);
}

void AEFPlayerController::ServerUpdateHeldMove_Implementation(FVector_NetQuantize Destination)
{
    const UWorld* World = GetWorld();
    const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
    if (LastServerHeldMoveTime >= 0.0 && CurrentTime - LastServerHeldMoveTime < HeldOrderUpdateInterval * 0.5f)
    {
        return;
    }

    LastServerHeldMoveTime = CurrentTime;
    ApplyMoveOrder(Destination);
}

void AEFPlayerController::ApplyMoveOrder(const FVector& Destination)
{
    const AEFGameState* Match = GetWorld()->GetGameState<AEFGameState>();
    if (Match && Match->GetMatchPhase() == EEFMatchPhase::PostGame) { return; }
    FVector ResolvedDestination = FVector::ZeroVector;
    bool bUseNavigation = false;
    if (!ResolveMoveDestination(Destination, ResolvedDestination, bUseNavigation))
    {
        return;
    }

    CancelAttackMoveOrder();
    bHoldPositionOrderActive = false;

    if (UEFCombatComponent* CombatComponent = ControlledHero->GetCombatComponent())
    {
        CombatComponent->CancelBasicAttack();
    }

    IssueResolvedMove(ResolvedDestination, bUseNavigation);
}

void AEFPlayerController::IssueResolvedMove(const FVector& ResolvedDestination, bool bUseNavigation)
{
    if (!IsValid(ControlledHero))
    {
        return;
    }

    if (bUseNavigation)
    {
        if (AAIController* HeroAIController = Cast<AAIController>(ControlledHero->GetController()))
        {
            ControlledHero->CancelDirectMove();
            UAIBlueprintHelperLibrary::SimpleMoveToLocation(HeroAIController, ResolvedDestination);
        }
    }
    else
    {
        ControlledHero->SetDirectMoveDestination(ResolvedDestination);
    }
}

void AEFPlayerController::ServerRequestAttack_Implementation(AActor* TargetActor)
{
    if (!HasAuthority() || !IsValid(ControlledHero) || ControlledHero->GetOwner() != this
        || !IsCombatOrderTarget(TargetActor) || TargetActor == ControlledHero
        || ControlledHero->IsDead() || IsDeadOrderTarget(TargetActor))
    {
        return;
    }

    if (FVector::DistSquared2D(ControlledHero->GetActorLocation(), TargetActor->GetActorLocation())
        > FMath::Square(MaximumOrderDistance))
    {
        return;
    }

    const AEFPlayerState* SourceState = GetPlayerState<AEFPlayerState>();
    const EEFMatchSide TargetSide = GetOrderTargetSide(TargetActor);
    if (!SourceState || SourceState->GetMatchSide() == EEFMatchSide::Unassigned
        || TargetSide == EEFMatchSide::Unassigned
        || SourceState->GetMatchSide() == TargetSide)
    {
        return;
    }

    CancelAttackMoveOrder();
    ControlledHero->CancelDirectMove();
    bHoldPositionOrderActive = false;
    if (UEFCombatComponent* CombatComponent = ControlledHero->GetCombatComponent())
    {
        if (CombatComponent->BeginBasicAttack(TargetActor))
        {
            UE_LOG(LogEFNetwork, Display, TEXT("[%s] AttackOrder accepted controller=%s hero=%s target=%s"),
                EFLog::GetNetContext(this), *GetName(), *ControlledHero->GetName(), *TargetActor->GetName());
        }
    }
}

void AEFPlayerController::ServerRequestStop_Implementation(bool bHoldPosition)
{
    const AEFGameState* Match = GetWorld() ? GetWorld()->GetGameState<AEFGameState>() : nullptr;
    if (!HasAuthority() || !IsValid(ControlledHero) || ControlledHero->GetOwner() != this
        || ControlledHero->IsDead() || (Match && Match->GetMatchPhase() == EEFMatchPhase::PostGame))
    {
        return;
    }

    CancelAttackMoveOrder();
    ControlledHero->CancelDirectMove();
    if (UEFCombatComponent* CombatComponent = ControlledHero->GetCombatComponent())
    {
        CombatComponent->CancelBasicAttack();
    }
    ControlledHero->StopMovementForAttack();
    bHoldPositionOrderActive = bHoldPosition;

    UE_LOG(LogEFNetwork, Display, TEXT("[%s] %s accepted controller=%s hero=%s"),
        EFLog::GetNetContext(this), bHoldPosition ? TEXT("HoldPositionOrder") : TEXT("StopOrder"),
        *GetName(), *ControlledHero->GetName());
}

void AEFPlayerController::ServerRequestAttackMove_Implementation(FVector_NetQuantize Destination)
{
    const AEFGameState* Match = GetWorld() ? GetWorld()->GetGameState<AEFGameState>() : nullptr;
    if (!HasAuthority() || !IsValid(ControlledHero) || ControlledHero->GetOwner() != this
        || ControlledHero->IsDead() || (Match && Match->GetMatchPhase() == EEFMatchPhase::PostGame))
    {
        return;
    }

    FVector ResolvedDestination = FVector::ZeroVector;
    bool bUseNavigation = false;
    if (!ResolveMoveDestination(Destination, ResolvedDestination, bUseNavigation))
    {
        return;
    }

    if (UEFCombatComponent* CombatComponent = ControlledHero->GetCombatComponent())
    {
        CombatComponent->CancelBasicAttack();
    }
    ControlledHero->CancelDirectMove();
    bHoldPositionOrderActive = false;
    bAttackMoveOrderActive = true;
    bAttackMoveUsesNavigation = bUseNavigation;
    AttackMoveDestination = ResolvedDestination;
    AttackMoveTimeUntilScan = 0.0f;
    AttackMoveTarget.Reset();
    bAttackMoveAwaitingTargetResolution = false;
    IssueResolvedMove(AttackMoveDestination, bAttackMoveUsesNavigation);

    UE_LOG(LogEFNetwork, Display, TEXT("[%s] AttackMoveOrder accepted controller=%s hero=%s destination=%s"),
        EFLog::GetNetContext(this), *GetName(), *ControlledHero->GetName(), *AttackMoveDestination.ToCompactString());
}

void AEFPlayerController::TickAttackMove(float DeltaTime)
{
    if (!bAttackMoveOrderActive || !IsValid(ControlledHero) || ControlledHero->IsDead())
    {
        return;
    }

    const AEFGameState* Match = GetWorld() ? GetWorld()->GetGameState<AEFGameState>() : nullptr;
    if (Match && Match->GetMatchPhase() == EEFMatchPhase::PostGame)
    {
        CancelAttackMoveOrder();
        return;
    }

    UEFCombatComponent* CombatComponent = ControlledHero->GetCombatComponent();
    AActor* CurrentTarget = AttackMoveTarget.Get();
    if (CurrentTarget && !IsDeadOrderTarget(CurrentTarget)
        && CombatComponent && CombatComponent->GetAttackTarget() == CurrentTarget
        && CombatComponent->GetAttackState() != EEFAttackState::Idle)
    {
        return;
    }

    if (bAttackMoveAwaitingTargetResolution)
    {
        AttackMoveTarget.Reset();
        bAttackMoveAwaitingTargetResolution = false;
        if (CombatComponent)
        {
            CombatComponent->CancelBasicAttack();
        }
        ResumeAttackMovePath();
    }

    AttackMoveTimeUntilScan -= DeltaTime;
    if (AttackMoveTimeUntilScan <= 0.0f)
    {
        AttackMoveTimeUntilScan = AttackMoveScanInterval;
        if (AActor* NewTarget = FindAttackMoveTarget())
        {
            if (CombatComponent && CombatComponent->BeginBasicAttack(NewTarget))
            {
                AttackMoveTarget = NewTarget;
                bAttackMoveAwaitingTargetResolution = true;
                UE_LOG(LogEFCombat, Verbose, TEXT("[%s] AttackMove acquired hero=%s target=%s"),
                    EFLog::GetNetContext(this), *ControlledHero->GetName(), *NewTarget->GetName());
                return;
            }
        }
    }

    if (FVector::DistSquared2D(ControlledHero->GetActorLocation(), AttackMoveDestination)
        <= FMath::Square(AttackMoveAcceptanceRadius))
    {
        CancelAttackMoveOrder();
    }
}

void AEFPlayerController::CancelAttackMoveOrder()
{
    bAttackMoveOrderActive = false;
    AttackMoveTarget.Reset();
    bAttackMoveAwaitingTargetResolution = false;
    AttackMoveTimeUntilScan = 0.0f;
}

void AEFPlayerController::ResumeAttackMovePath()
{
    if (bAttackMoveOrderActive)
    {
        IssueResolvedMove(AttackMoveDestination, bAttackMoveUsesNavigation);
    }
}

bool AEFPlayerController::ResolveMoveDestination(const FVector& Destination, FVector& OutResolvedDestination, bool& bOutUseNavigation) const
{
    if (!HasAuthority() || !IsValid(ControlledHero) || ControlledHero->GetOwner() != this || Destination.ContainsNaN())
    {
        return false;
    }

    if (!FMath::IsFinite(Destination.X) || !FMath::IsFinite(Destination.Y) || !FMath::IsFinite(Destination.Z))
    {
        return false;
    }

    if (FVector::DistSquared2D(ControlledHero->GetActorLocation(), Destination) > FMath::Square(MaximumOrderDistance))
    {
        return false;
    }

    UWorld* World = GetWorld();
    UNavigationSystemV1* NavigationSystem = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
    FNavLocation ProjectedLocation;
    if (NavigationSystem && NavigationSystem->ProjectPointToNavigation(Destination, ProjectedLocation, FVector(200.0f, 200.0f, 500.0f)))
    {
        OutResolvedDestination = ProjectedLocation.Location;
        bOutUseNavigation = true;
        UE_VLOG_ARROW(ControlledHero, LogEFMovement, Verbose, ControlledHero->GetActorLocation(),
            OutResolvedDestination, FColor::Green, TEXT("Accepted move destination"));
        return true;
    }

    if (!bAllowDirectMoveFallback || !World)
    {
        UE_LOG(LogEFMovement, Warning,
            TEXT("[%s] MoveOrder rejected reason=OutsideNavMesh controller=%s hero=%s destination=%s"),
            EFLog::GetNetContext(this), *GetName(), *GetNameSafe(ControlledHero), *Destination.ToCompactString());
        UE_VLOG_LOCATION(ControlledHero, LogEFMovement, Warning, Destination, 40.0f, FColor::Red,
            TEXT("Rejected: outside NavMesh"));
        UE_VLOG_SEGMENT(ControlledHero, LogEFMovement, Warning, ControlledHero->GetActorLocation(),
            Destination, FColor::Red, TEXT("Rejected move order"));
        return false;
    }

    FHitResult GroundHit;
    const FVector TraceStart(Destination.X, Destination.Y, Destination.Z + 1000.0f);
    const FVector TraceEnd(Destination.X, Destination.Y, Destination.Z - 3000.0f);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EFDirectMoveGround), false, ControlledHero);
    if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
    {
        return false;
    }

    OutResolvedDestination = GroundHit.ImpactPoint;
    bOutUseNavigation = false;
    UE_VLOG_ARROW(ControlledHero, LogEFMovement, Warning, ControlledHero->GetActorLocation(),
        OutResolvedDestination, FColor::Yellow, TEXT("Direct movement fallback"));
    return true;
}

AActor* AEFPlayerController::FindAssistedAttackTarget(const FVector& CursorWorldLocation) const
{
    AActor* BestTarget = nullptr;
    float BestDistanceSquared = FMath::Square(ContextTargetAssistRadius);
    const AEFPlayerState* SourceState = GetPlayerState<AEFPlayerState>();
    const EEFMatchSide SourceSide = SourceState ? SourceState->GetMatchSide() : EEFMatchSide::Unassigned;

    for (TActorIterator<AEFHeroCharacter> It(GetWorld()); It; ++It)
    {
        AEFHeroCharacter* Candidate = *It;
        if (!IsValid(Candidate) || Candidate == ControlledHero || Candidate->IsDead()
            || GetOrderTargetSide(Candidate) == SourceSide)
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared2D(CursorWorldLocation, Candidate->GetActorLocation());
        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = Candidate;
        }
    }

    for (TActorIterator<AEFCreepCharacter> It(GetWorld()); It; ++It)
    {
        AEFCreepCharacter* Candidate = *It;
        if (!IsValid(Candidate) || Candidate->IsDead() || GetOrderTargetSide(Candidate) == SourceSide)
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared2D(CursorWorldLocation, Candidate->GetActorLocation());
        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = Candidate;
        }
    }

    for (TActorIterator<AEFLaneTower> It(GetWorld()); It; ++It)
    {
        AEFLaneTower* Candidate = *It;
        if (!IsValid(Candidate) || Candidate->IsDead() || GetOrderTargetSide(Candidate) == SourceSide)
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared2D(CursorWorldLocation, Candidate->GetActorLocation());
        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = Candidate;
        }
    }
    return BestTarget;
}

AActor* AEFPlayerController::FindAttackMoveTarget() const
{
    if (!IsValid(ControlledHero) || !GetWorld())
    {
        return nullptr;
    }

    const AEFPlayerState* SourceState = GetPlayerState<AEFPlayerState>();
    const EEFMatchSide SourceSide = SourceState ? SourceState->GetMatchSide() : EEFMatchSide::Unassigned;
    if (SourceSide == EEFMatchSide::Unassigned)
    {
        return nullptr;
    }

    AActor* BestTarget = nullptr;
    float BestDistanceSquared = FMath::Square(AttackMoveAcquisitionRadius);
    const auto ConsiderCandidate = [this, SourceSide, &BestTarget, &BestDistanceSquared](AActor* Candidate)
    {
        if (!IsValid(Candidate) || Candidate == ControlledHero || IsDeadOrderTarget(Candidate))
        {
            return;
        }

        const EEFMatchSide CandidateSide = GetOrderTargetSide(Candidate);
        if (CandidateSide == EEFMatchSide::Unassigned || CandidateSide == SourceSide)
        {
            return;
        }

        if (const AEFLaneTower* Tower = Cast<AEFLaneTower>(Candidate); Tower && !Tower->IsVulnerable())
        {
            return;
        }

        const float DistanceSquared = FVector::DistSquared2D(
            ControlledHero->GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = Candidate;
        }
    };

    for (TActorIterator<AEFHeroCharacter> It(GetWorld()); It; ++It)
    {
        ConsiderCandidate(*It);
    }
    for (TActorIterator<AEFCreepCharacter> It(GetWorld()); It; ++It)
    {
        ConsiderCandidate(*It);
    }
    for (TActorIterator<AEFLaneTower> It(GetWorld()); It; ++It)
    {
        ConsiderCandidate(*It);
    }
    return BestTarget;
}

void AEFPlayerController::OnRep_ControlledHero()
{
}
