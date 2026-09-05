#include "Units/EFTowerProjectile.h"

#include "Debug/EFLogCategories.h"

#include "Combat/EFCombatComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Units/EFCreepCharacter.h"
#include "Units/EFHeroCharacter.h"
#include "Units/EFLaneTower.h"
#include "UObject/ConstructorHelpers.h"

AEFTowerProjectile::AEFTowerProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
    InitialLifeSpan = 4.0f;

    CollisionRoot = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionRoot"));
    SetRootComponent(CollisionRoot);
    CollisionRoot->InitSphereRadius(18.0f);
    CollisionRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(CollisionRoot);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ProjectileMesh->SetRelativeScale3D(FVector(0.24f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        ProjectileMesh->SetStaticMesh(SphereMesh.Object);
    }
}

void AEFTowerProjectile::BeginPlay()
{
    Super::BeginPlay();
    ProjectileMaterial = ProjectileMesh->CreateAndSetMaterialInstanceDynamic(0);
    UpdatePresentation();
}

void AEFTowerProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority())
    {
        return;
    }

    if (!IsValid(SourceTower) || !IsValid(TargetActor) || SourceTower->IsDead()
        || (Cast<AEFHeroCharacter>(TargetActor) && Cast<AEFHeroCharacter>(TargetActor)->IsDead())
        || (Cast<AEFCreepCharacter>(TargetActor) && Cast<AEFCreepCharacter>(TargetActor)->IsDead()))
    {
        UE_LOG(LogEFCombat, Verbose, TEXT("[%s] TowerProjectile cancelled projectile=%s reason=SourceOrTargetUnavailable"),
            EFLog::GetNetContext(this), *GetName());
        Destroy();
        return;
    }

    // A newly spawned actor can tick in its launch frame. World DeltaSeconds
    // includes time before its birth; only simulate time elapsed since launch.
    const double Now = GetWorld()->GetTimeSeconds();
    const float FlightStep = static_cast<float>(FMath::Max(0.0, Now - LastSimulationTime));
    if (LastSimulationTime < 0.0 || FlightStep <= 0.0f)
    {
        return;
    }
    LastSimulationTime = Now;
    const FVector TargetPoint = GetTargetPoint();
    const FVector ToTarget = TargetPoint - GetActorLocation();
    const float TravelDistance = TravelSpeed * FlightStep;
    if (ToTarget.SizeSquared() <= FMath::Square(ImpactRadius + TravelDistance))
    {
        if (UEFCombatComponent* SourceCombat = SourceTower->GetCombatComponent())
        {
            if (SourceCombat->ResolveBasicAttackImpact(TargetActor))
            {
                SourceTower->NotifyBasicAttackContact();
                UE_LOG(LogEFCombat, Verbose, TEXT("[%s] TowerProjectile impact target=%s projectile=%s flight=%.3fs"),
                    EFLog::GetNetContext(this), *TargetActor->GetName(), *GetName(), Now - LaunchTime);
            }
        }
        Destroy();
        return;
    }

    SetActorLocation(GetActorLocation() + ToTarget.GetSafeNormal() * TravelDistance, false);
}

void AEFTowerProjectile::InitializeProjectile(
    AEFLaneTower* NewSourceTower, AActor* NewTargetActor, EEFMatchSide NewMatchSide)
{
    if (!HasAuthority() || !IsValid(NewSourceTower) || !IsValid(NewTargetActor))
    {
        return;
    }

    SourceTower = NewSourceTower;
    TargetActor = NewTargetActor;
    MatchSide = NewMatchSide;
    LaunchTime = GetWorld()->GetTimeSeconds();
    LastSimulationTime = LaunchTime;
    OnRep_MatchSide();
    ForceNetUpdate();
}

FVector AEFTowerProjectile::GetTargetPoint() const
{
    if (Cast<AEFLaneTower>(TargetActor))
    {
        return TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 220.0f);
    }
    if (Cast<AEFHeroCharacter>(TargetActor))
    {
        return TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
    }
    if (Cast<AEFCreepCharacter>(TargetActor))
    {
        return TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
    }
    return IsValid(TargetActor) ? TargetActor->GetActorLocation() : GetActorLocation();
}

void AEFTowerProjectile::UpdatePresentation()
{
    if (!ProjectileMaterial)
    {
        return;
    }
    const FLinearColor Color = MatchSide == EEFMatchSide::Dawn
        ? FLinearColor(0.05f, 0.35f, 2.5f)
        : (MatchSide == EEFMatchSide::Dusk
            ? FLinearColor(2.5f, 0.08f, 0.03f)
            : FLinearColor::White);
    ProjectileMaterial->SetVectorParameterValue(TEXT("Color"), Color);
}

void AEFTowerProjectile::OnRep_MatchSide()
{
    UpdatePresentation();
}

void AEFTowerProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AEFTowerProjectile, SourceTower);
    DOREPLIFETIME(AEFTowerProjectile, TargetActor);
    DOREPLIFETIME(AEFTowerProjectile, MatchSide);
}
