#include "Units/EFLaneTower.h"

#include "AbilitySystem/EFAbilitySystemComponent.h"
#include "AbilitySystem/EFHeroAttributeSet.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/EFCombatComponent.h"
#include "Debug/EFLogCategories.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Player/EFPlayerState.h"
#include "Units/EFCreepCharacter.h"
#include "Units/EFHeroCharacter.h"
#include "Units/EFTowerProjectile.h"
#include "Game/EFGameMode.h"
#include "Game/EFGameState.h"
#include "UObject/ConstructorHelpers.h"
#include "VisualLogger/VisualLogger.h"

AEFLaneTower::AEFLaneTower()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;
    bReplicates = true;
    SetReplicateMovement(false);

    TargetCollision = CreateDefaultSubobject<USphereComponent>(TEXT("TargetCollision"));
    SetRootComponent(TargetCollision);
    TargetCollision->InitSphereRadius(90.0f);
    TargetCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TargetCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    TargetCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    AbilitySystemComponent = CreateDefaultSubobject<UEFAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
    TowerAttributeSet = CreateDefaultSubobject<UEFHeroAttributeSet>(TEXT("TowerAttributeSet"));
    CombatComponent = CreateDefaultSubobject<UEFCombatComponent>(TEXT("CombatComponent"));
    ProjectileClass = AEFTowerProjectile::StaticClass();

    TowerBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerBase"));
    TowerBase->SetupAttachment(TargetCollision);
    TowerBase->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TowerBase->SetRelativeLocation(FVector(0.0f, 0.0f, 125.0f));
    TowerBase->SetRelativeScale3D(FVector(1.0f, 1.0f, 2.5f));

    TowerHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerHead"));
    TowerHead->SetupAttachment(TargetCollision);
    TowerHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TowerHead->SetRelativeLocation(FVector(0.0f, 0.0f, 275.0f));
    TowerHead->SetRelativeScale3D(FVector(1.25f, 1.25f, 0.45f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CylinderMesh.Succeeded())
    {
        TowerBase->SetStaticMesh(CylinderMesh.Object);
    }
    if (SphereMesh.Succeeded())
    {
        TowerHead->SetStaticMesh(SphereMesh.Object);
    }

    OverheadPresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("OverheadPresentationRoot"));
    OverheadPresentationRoot->SetupAttachment(TargetCollision);
    OverheadPresentationRoot->SetUsingAbsoluteRotation(true);

    HealthBarBackground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HealthBarBackground"));
    HealthBarBackground->SetupAttachment(OverheadPresentationRoot);
    HealthBarBackground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HealthBarBackground->SetRelativeLocation(FVector(0.0f, 0.0f, 350.0f));
    HealthBarBackground->SetRelativeScale3D(FVector(0.9f, 0.09f, 0.035f));

    HealthBarFill = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HealthBarFill"));
    HealthBarFill->SetupAttachment(OverheadPresentationRoot);
    HealthBarFill->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HealthBarFill->SetRelativeLocation(FVector(0.0f, 0.0f, 355.0f));
    HealthBarFill->SetRelativeScale3D(FVector(0.87f, 0.06f, 0.025f));
    if (CubeMesh.Succeeded())
    {
        HealthBarBackground->SetStaticMesh(CubeMesh.Object);
        HealthBarFill->SetStaticMesh(CubeMesh.Object);
    }

    SetNetCullDistanceSquared(FMath::Square(20000.0f));
}

void AEFLaneTower::BeginPlay()
{
    Super::BeginPlay();

    BaseMaterial = TowerBase->CreateAndSetMaterialInstanceDynamic(0);
    HeadMaterial = TowerHead->CreateAndSetMaterialInstanceDynamic(0);
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

    AbilitySystemComponent->RefreshAbilityActorInfo(this, this);
    if (HasAuthority())
    {
        AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetMaxHealthAttribute(), 1800.0f);
        AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetHealthAttribute(), 1800.0f);
        AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetAttackDamageAttribute(), 90.0f);
        AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetAttackSpeedAttribute(), 0.75f);
        AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetAttackRangeAttribute(), 800.0f);
        AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetArmorAttribute(), 10.0f);
    }
    UpdatePresentation();
}

void AEFLaneTower::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FaceOverheadPresentationToLocalCamera();
    UpdatePresentation();

    if (AttackPulseTimeRemaining > 0.0f)
    {
        AttackPulseTimeRemaining = FMath::Max(0.0f, AttackPulseTimeRemaining - DeltaSeconds);
        const float Progress = 1.0f - AttackPulseTimeRemaining / AttackPulseDuration;
        const float Pulse = FMath::Sin(Progress * UE_PI) * 0.18f;
        TowerHead->SetRelativeScale3D(FVector(1.25f + Pulse, 1.25f + Pulse, 0.45f + Pulse));
        if (AttackPulseTimeRemaining <= 0.0f)
        {
            TowerHead->SetRelativeScale3D(FVector(1.25f, 1.25f, 0.45f));
        }
    }

    const AEFGameState* Match = GetWorld()->GetGameState<AEFGameState>();
    if (!HasAuthority() || bIsDead || !CanFire() || (Match && Match->GetMatchPhase() == EEFMatchPhase::PostGame))
    {
        return;
    }

    if (ForcedHeroAggroTimeRemaining > 0.0f)
    {
        ForcedHeroAggroTimeRemaining = FMath::Max(0.0f, ForcedHeroAggroTimeRemaining - DeltaSeconds);
        AEFHeroCharacter* ForcedTarget = ForcedHeroTarget.Get();
        const AEFPlayerState* ForcedState = ForcedTarget ? ForcedTarget->GetOwningPlayerState() : nullptr;
        const bool bForcedTargetValid = IsValid(ForcedTarget) && !ForcedTarget->IsDead() && ForcedState
            && ForcedState->GetMatchSide() != MatchSide
            && FVector::DistSquared2D(GetActorLocation(), ForcedTarget->GetActorLocation())
                <= FMath::Square(TargetAcquisitionRadius);
        if (bForcedTargetValid && ForcedHeroAggroTimeRemaining > 0.0f)
        {
            if (CombatComponent->GetAttackTarget() != ForcedTarget)
            {
                CombatComponent->BeginBasicAttack(ForcedTarget);
            }
            return;
        }
        UE_LOG(LogEFAI, Display, TEXT("[%s] TowerHeroAggro ended tower=%s reason=%s"),
            EFLog::GetNetContext(this), *GetName(),
            bForcedTargetValid ? TEXT("Expired") : TEXT("TargetUnavailableOrOutOfRange"));
        ForcedHeroTarget.Reset();
        ForcedHeroAggroTimeRemaining = 0.0f;
    }

    TargetScanTimeRemaining -= DeltaSeconds;
    if (TargetScanTimeRemaining > 0.0f)
    {
        return;
    }

    TargetScanTimeRemaining = TargetScanInterval;
    if (AActor* Target = FindPriorityTarget())
    {
        AActor* CurrentTarget = CombatComponent->GetAttackTarget();
        const bool bShouldAcquire = !IsValid(CurrentTarget);
        const bool bShouldRetargetToCreep = IsValid(CurrentTarget)
            && !Cast<AEFCreepCharacter>(CurrentTarget)
            && Cast<AEFCreepCharacter>(Target);
        if ((bShouldAcquire || bShouldRetargetToCreep) && CombatComponent->BeginBasicAttack(Target))
        {
            UE_LOG(LogEFAI, Verbose, TEXT("[%s] TowerTarget side=%s action=%s tower=%s target=%s"),
                EFLog::GetNetContext(this), MatchSide == EEFMatchSide::Dawn ? TEXT("Dawn") : TEXT("Dusk"),
                bShouldRetargetToCreep ? TEXT("re-targeted") : TEXT("targeted"),
                *GetName(),
                *Target->GetName());
        }
    }
}

void AEFLaneTower::InitializeTower(EEFMatchSide NewMatchSide)
{
    if (HasAuthority() && NewMatchSide != EEFMatchSide::Unassigned)
    {
        MatchSide = NewMatchSide;
        OnRep_MatchSide();
        ForceNetUpdate();
    }
}

void AEFLaneTower::InitializeStructure(EEFMatchSide Side, EEFStructureKind Kind)
{
    if (!HasAuthority()) { return; }
    StructureKind = Kind;
    InitializeTower(Side);
    const float Health = Kind == EEFStructureKind::Nexus ? 2500.0f : (CanFire() ? 1800.0f : 900.0f);
    AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetMaxHealthAttribute(), Health);
    AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetHealthAttribute(), Health);
    AbilitySystemComponent->SetNumericAttributeBase(UEFHeroAttributeSet::GetAttackRangeAttribute(), CanFire() ? 800.0f : 0.0f);
    SetVulnerable(Kind == EEFStructureKind::T1);
    UpdatePresentation();
    ForceNetUpdate();
}

void AEFLaneTower::SetVulnerable(bool bValue)
{
    if (!HasAuthority()) { return; }
    if (bVulnerable != bValue)
    {
        UE_LOG(LogEFSiege, Display, TEXT("[%s] StructureProtection side=%d kind=%d vulnerable=%d structure=%s"),
            EFLog::GetNetContext(this), static_cast<int32>(MatchSide), static_cast<int32>(StructureKind), bValue, *GetName());
    }
    bVulnerable = bValue;
    UpdatePresentation();
    ForceNetUpdate();
}

AActor* AEFLaneTower::FindPriorityTarget() const
{
    AActor* BestTarget = nullptr;
    float BestDistanceSquared = FMath::Square(TargetAcquisitionRadius);

    for (TActorIterator<AEFCreepCharacter> It(GetWorld()); It; ++It)
    {
        AEFCreepCharacter* Candidate = *It;
        if (!IsValid(Candidate) || Candidate->IsDead() || Candidate->GetMatchSide() == MatchSide)
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

    for (TActorIterator<AEFHeroCharacter> It(GetWorld()); It; ++It)
    {
        AEFHeroCharacter* Candidate = *It;
        const AEFPlayerState* State = Candidate ? Candidate->GetOwningPlayerState() : nullptr;
        if (!IsValid(Candidate) || Candidate->IsDead() || !State || State->GetMatchSide() == MatchSide)
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

UAbilitySystemComponent* AEFLaneTower::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

float AEFLaneTower::GetBasicAttackRange() const
{
    return TowerAttributeSet ? TowerAttributeSet->GetAttackRange() : 800.0f;
}

void AEFLaneTower::StopMovementForAttack()
{
}

void AEFLaneTower::FaceAttackTarget(const AActor* TargetActor)
{
    if (!HasAuthority() || !IsValid(TargetActor))
    {
        return;
    }
    const FVector ToTarget = TargetActor->GetActorLocation() - GetActorLocation();
    if (!ToTarget.IsNearlyZero())
    {
        TowerHead->SetWorldRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
    }
}

void AEFLaneTower::NotifyBasicAttackContact()
{
    if (HasAuthority())
    {
        ++AttackContactCounter;
        OnRep_AttackContactCounter();
    }
}

bool AEFLaneTower::LaunchProjectile(AActor* TargetActor)
{
    if (!HasAuthority() || bIsDead || !CanFire() || !ProjectileClass || !IsValid(TargetActor))
    {
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.Instigator = nullptr;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector SpawnLocation = GetActorLocation() + FVector(0.0f, 0.0f, 275.0f);
    AEFTowerProjectile* Projectile = GetWorld()->SpawnActor<AEFTowerProjectile>(
        ProjectileClass, SpawnLocation, FRotator::ZeroRotator, SpawnParameters);
    if (!Projectile)
    {
        return false;
    }

    Projectile->InitializeProjectile(this, TargetActor, MatchSide);
    UE_LOG(LogEFCombat, Verbose, TEXT("[%s] TowerProjectile launched side=%s tower=%s target=%s"),
        EFLog::GetNetContext(this), MatchSide == EEFMatchSide::Dawn ? TEXT("Dawn") : TEXT("Dusk"),
        *GetName(), *TargetActor->GetName());
    return true;
}

void AEFLaneTower::NotifyAlliedHeroAttacked(AEFHeroCharacter* AttackingHero, AEFHeroCharacter* AlliedVictim)
{
    if (!HasAuthority() || bIsDead || !CanFire() || !IsValid(AttackingHero) || !IsValid(AlliedVictim)
        || AttackingHero->IsDead())
    {
        return;
    }

    const AEFPlayerState* AttackerState = AttackingHero->GetOwningPlayerState();
    const AEFPlayerState* VictimState = AlliedVictim->GetOwningPlayerState();
    if (!AttackerState || !VictimState || VictimState->GetMatchSide() != MatchSide
        || AttackerState->GetMatchSide() == MatchSide
        || FVector::DistSquared2D(GetActorLocation(), AttackingHero->GetActorLocation())
            > FMath::Square(TargetAcquisitionRadius))
    {
        return;
    }

    ForcedHeroTarget = AttackingHero;
    ForcedHeroAggroTimeRemaining = ForcedHeroAggroDuration;
    if (CombatComponent->BeginBasicAttack(AttackingHero))
    {
        UE_LOG(LogEFAI, Display, TEXT("[%s] TowerHeroAggro started side=%s tower=%s attacker=%s victim=%s"),
            EFLog::GetNetContext(this), MatchSide == EEFMatchSide::Dawn ? TEXT("Dawn") : TEXT("Dusk"), *GetName(),
            *AttackingHero->GetName(), *AlliedVictim->GetName());
        UE_VLOG_WIRECIRCLE(this, LogEFAI, Display, GetActorLocation(), FVector::UpVector,
            TargetAcquisitionRadius, FColor::Red, TEXT("Tower acquisition radius"));
        UE_VLOG_ARROW(this, LogEFAI, Display, GetActorLocation(), AttackingHero->GetActorLocation(),
            FColor::Red, TEXT("Forced hero aggro: %s attacked %s"),
            *AttackingHero->GetName(), *AlliedVictim->GetName());
    }
}

void AEFLaneTower::NotifyDamageReceived(float DamageAmount)
{
    if (HasAuthority() && DamageAmount > 0.0f)
    {
        UpdatePresentation();
    }
}

void AEFLaneTower::HandleHealthDepleted(AActor* DamageInstigator)
{
    if (!HasAuthority() || bIsDead)
    {
        return;
    }
    bIsDead = true;
    CombatComponent->CancelBasicAttack();
    OnRep_IsDead();
    UE_LOG(LogEFSiege, Display, TEXT("[%s] StructureDestroyed side=%s kind=%d structure=%s instigator=%s"),
        EFLog::GetNetContext(this), MatchSide == EEFMatchSide::Dawn ? TEXT("Dawn") : TEXT("Dusk"),
        static_cast<int32>(StructureKind), *GetName(),
        IsValid(DamageInstigator) ? *DamageInstigator->GetName() : TEXT("unknown"));
    SetLifeSpan(1.0f);
    if (AEFGameMode* Mode = GetWorld()->GetAuthGameMode<AEFGameMode>()) { Mode->HandleStructureDestroyed(this); }
}

void AEFLaneTower::UpdatePresentation()
{
    const FLinearColor SideColor = MatchSide == EEFMatchSide::Dawn
        ? FLinearColor(0.05f, 0.25f, 1.0f)
        : (MatchSide == EEFMatchSide::Dusk
            ? FLinearColor(1.0f, 0.08f, 0.04f)
            : FLinearColor(0.35f, 0.35f, 0.35f));
    if (BaseMaterial)
    {
        BaseMaterial->SetVectorParameterValue(TEXT("Color"), bVulnerable ? SideColor : SideColor * 0.3f);
    }
    if (HeadMaterial)
    {
        HeadMaterial->SetVectorParameterValue(TEXT("Color"), SideColor * 1.5f);
    }
    if (!CanFire())
    {
        const bool bNexus = StructureKind == EEFStructureKind::Nexus;
        TowerBase->SetRelativeScale3D(bNexus ? FVector(2.0f, 2.0f, 1.0f) : FVector(1.6f, 1.6f, 0.8f));
        TowerBase->SetRelativeLocation(FVector(0, 0, bNexus ? 50.0f : 40.0f));
        TowerHead->SetRelativeLocation(FVector(0, 0, bNexus ? 180.0f : 100.0f));
        TowerHead->SetRelativeScale3D(bNexus ? FVector(1.5f, 1.5f, 2.5f) : FVector(1.1f));
    }

    if (HealthBarFill && TowerAttributeSet)
    {
        const float Percent = FMath::Clamp(
            TowerAttributeSet->GetHealth() / FMath::Max(1.0f, TowerAttributeSet->GetMaxHealth()), 0.0f, 1.0f);
        constexpr float FullScaleX = 0.87f;
        HealthBarFill->SetRelativeScale3D(FVector(FullScaleX * Percent, 0.06f, 0.025f));
        HealthBarFill->SetRelativeLocation(FVector(-50.0f * FullScaleX * (1.0f - Percent), 0.0f, 355.0f));
    }
}

void AEFLaneTower::FaceOverheadPresentationToLocalCamera()
{
    const APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    const APlayerCameraManager* CameraManager = Controller ? Controller->PlayerCameraManager : nullptr;
    if (OverheadPresentationRoot && CameraManager)
    {
        OverheadPresentationRoot->SetWorldRotation(
            FRotator(0.0f, CameraManager->GetCameraRotation().Yaw + 180.0f, 0.0f));
    }
}

void AEFLaneTower::OnRep_MatchSide()
{
    UpdatePresentation();
}

void AEFLaneTower::OnRep_IsDead()
{
    TargetCollision->SetCollisionEnabled(bIsDead ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
    HealthBarBackground->SetVisibility(!bIsDead);
    HealthBarFill->SetVisibility(!bIsDead);
    TowerHead->SetVisibility(!bIsDead);
    TowerBase->SetRelativeRotation(bIsDead ? FRotator(0.0f, 75.0f, 0.0f) : FRotator::ZeroRotator);
}

void AEFLaneTower::OnRep_AttackContactCounter()
{
    AttackPulseTimeRemaining = AttackPulseDuration;
}

void AEFLaneTower::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AEFLaneTower, MatchSide);
    DOREPLIFETIME(AEFLaneTower, StructureKind);
    DOREPLIFETIME(AEFLaneTower, bVulnerable);
    DOREPLIFETIME(AEFLaneTower, bIsDead);
    DOREPLIFETIME(AEFLaneTower, AttackContactCounter);
}
