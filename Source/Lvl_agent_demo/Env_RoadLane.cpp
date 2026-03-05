// RoadLane.cpp

#include "Env_RoadLane.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Env_TrafficLightTypes.h"
#include "Traffic_AICar.h"
#include "Traffic_AICarManagerComponent.h"

AEnv_RoadLane::AEnv_RoadLane()
{
    PrimaryActorTick.bCanEverTick = false;

    RoadSurface = CreateDefaultSubobject<UBoxComponent>(TEXT("RoadSurface"));
    RootComponent = RoadSurface;

    RoadSurface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    RoadSurface->SetCollisionResponseToAllChannels(ECR_Block);
    RoadSurface->SetGenerateOverlapEvents(false);

    RoadSideLeft = CreateDefaultSubobject<UBoxComponent>(TEXT("RoadSideLeft"));
    RoadSideLeft->SetupAttachment(RoadSurface);
    RoadSideLeft->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    RoadSideLeft->SetCollisionResponseToAllChannels(ECR_Block);
    RoadSideLeft->SetGenerateOverlapEvents(false);

    RoadSideRight = CreateDefaultSubobject<UBoxComponent>(TEXT("RoadSideRight"));
    RoadSideRight->SetupAttachment(RoadSurface);
    RoadSideRight->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    RoadSideRight->SetCollisionResponseToAllChannels(ECR_Block);
    RoadSideRight->SetGenerateOverlapEvents(false);
    
    BrakingArea = CreateDefaultSubobject<UBoxComponent>(TEXT("BrakingArea"));
    BrakingArea->SetupAttachment(RoadSurface);
    BrakingArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BrakingArea->SetCollisionResponseToAllChannels(ECR_Ignore);
    BrakingArea->SetCollisionObjectType(ECC_WorldDynamic);
    BrakingArea->SetCollisionResponseToChannel(ECC_Pawn,    ECR_Overlap);
    BrakingArea->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
    BrakingArea->SetGenerateOverlapEvents(true);
    
    Spawner = CreateDefaultSubobject<UBoxComponent>(TEXT("Spawner"));
    Spawner->SetupAttachment(RootComponent);
    Spawner->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Spawner->SetHiddenInGame(true);

    RoadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoadMesh"));
    RoadMesh->SetupAttachment(RoadSurface);
    RoadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEnv_RoadLane::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateGeometryFromParams();
}

void AEnv_RoadLane::BeginPlay()
{
    Super::BeginPlay();

    if (Intersection)
    {
        TrafficLightManager = Intersection->FindComponentByClass<UEnv_TrafficLightManagerComponent>();
        if (TrafficLightManager)
        {
            BrakingArea->OnComponentBeginOverlap.RemoveDynamic(this, &AEnv_RoadLane::OnBrakeBeginOverlap);
            BrakingArea->OnComponentBeginOverlap.AddDynamic(this, &AEnv_RoadLane::OnBrakeBeginOverlap);

            BrakingArea->OnComponentEndOverlap.RemoveDynamic(this, &AEnv_RoadLane::OnBrakeEndOverlap);
            BrakingArea->OnComponentEndOverlap.AddDynamic(this, &AEnv_RoadLane::OnBrakeEndOverlap);

            TrafficLightManager->OnTrafficPhaseChanged.AddUObject(this, &AEnv_RoadLane::OnPhaseChanged);
        }
    }

    // 找到 World 里的 Traffic_AICarManagerComponent，绑定 RoadSurface 的 Overlap 事件
    TrafficManager = UTraffic_AICarManagerComponent::GetTrafficManager(this);
    if (TrafficManager)
    {
        // 开启 RoadSurface 的 overlap 检测（Pawn / Vehicle 通道）
        RoadSurface->SetGenerateOverlapEvents(true);
        RoadSurface->SetCollisionResponseToChannel(ECC_Pawn,    ECR_Overlap);
        RoadSurface->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);

        RoadSurface->OnComponentBeginOverlap.RemoveDynamic(this, &AEnv_RoadLane::OnLaneBeginOverlap);
        RoadSurface->OnComponentBeginOverlap.AddDynamic(this, &AEnv_RoadLane::OnLaneBeginOverlap);

        RoadSurface->OnComponentEndOverlap.RemoveDynamic(this, &AEnv_RoadLane::OnLaneEndOverlap);
        RoadSurface->OnComponentEndOverlap.AddDynamic(this, &AEnv_RoadLane::OnLaneEndOverlap);
    }
}

void AEnv_RoadLane::OnLaneBeginOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor*              OtherActor,
    UPrimitiveComponent* OtherComp,
    int32                OtherBodyIndex,
    bool                 bFromSweep,
    const FHitResult&    SweepResult)
{
    if (!OtherActor || OtherActor == this) { return; }
    if (IsAICar(OtherActor))              { return; }

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn || !TrafficManager)         { return; }

    if (!GetWorldTimerManager().IsTimerActive(LaneSetCooldownHandle))
    {
        TrafficManager->SetCurrentLane(this);

        GetWorldTimerManager().SetTimer(
            LaneSetCooldownHandle,
            [](){},
            0.3f,
            false
        );
    }
}

void AEnv_RoadLane::OnLaneEndOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor*              OtherActor,
    UPrimitiveComponent* OtherComp,
    int32                OtherBodyIndex)
{
    if (!OtherActor || OtherActor == this) { return; }
    if (IsAICar(OtherActor))              { return; }

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn || !TrafficManager)         { return; }

    TrafficManager->SetCurrentLane(nullptr);

    GetWorldTimerManager().ClearTimer(LaneSetCooldownHandle);
}

void AEnv_RoadLane::OnBrakeBeginOverlap(
     UPrimitiveComponent* OverlappedComp,
     AActor* OtherActor,
     UPrimitiveComponent* OtherComp,
     int32 OtherBodyIndex,
     bool bFromSweep,
     const FHitResult& SweepResult
)
{
    if (!OtherActor || OtherActor == this) return;

    ATraffic_AICar* Car = Cast<ATraffic_AICar>(OtherActor);
    if (!Car) return;

    CarsInBrakingArea.Add(Car);
    UpdateCarGoState(Car);
}

void AEnv_RoadLane::OnBrakeEndOverlap(
      UPrimitiveComponent* OverlappedComp,
      AActor* OtherActor,
      UPrimitiveComponent* OtherComp,
      int32 OtherBodyIndex
)
{
    if (!OtherActor || OtherActor == this) return;

    if (ATraffic_AICar* Car = Cast<ATraffic_AICar>(OtherActor))
    {
        Car->SetShouldGoIntersection(true);
        CarsInBrakingArea.Remove(Car);
    }
}

bool AEnv_RoadLane::IsAICar(AActor* OtherActor) const
{
    if (!OtherActor || OtherActor == this) return false;
    return OtherActor->IsA(ATraffic_AICar::StaticClass());
}

void AEnv_RoadLane::OnPhaseChanged(EGlobalTrafficPhase NewPhase)
{
    CarsInBrakingArea.Remove(nullptr);

    for (const TWeakObjectPtr<ATraffic_AICar>& W : CarsInBrakingArea)
    {
        if (ATraffic_AICar* Car = W.Get())
        {
            UpdateCarGoState(Car);
        }
    }
}

void AEnv_RoadLane::UpdateCarGoState(ATraffic_AICar* Car)
{
    if (!Car || !TrafficLightManager) return;

    const auto Phase       = TrafficLightManager->GetCurrentPhase();
    const auto CarLaneAxis = Car->GetCurrentLane()->LaneAxis;
    const auto CarLaneIndex = Car->GetLaneIndex();

    bool bGo = false;

    switch (Phase)
    {
        case EGlobalTrafficPhase::NS_Straight_Ped_Green:
            bGo = (CarLaneAxis == ETrafficAxis::NorthSouth && CarLaneIndex >= 1);
            break;
        case EGlobalTrafficPhase::EW_Straight_Ped_Green:
            bGo = (CarLaneAxis == ETrafficAxis::EastWest && CarLaneIndex >= 1);
            break;
        case EGlobalTrafficPhase::NS_Left_Green:
            bGo = (CarLaneAxis == ETrafficAxis::NorthSouth && CarLaneIndex == 0);
            break;
        case EGlobalTrafficPhase::EW_Left_Green:
            bGo = (CarLaneAxis == ETrafficAxis::EastWest && CarLaneIndex == 0);
            break;
        default:
            bGo = false;
            break;
    }
    
    if (!bGo)
    {
        const FVector CarLoc   = Car->GetActorLocation();
        const float Remaining  = GetRemainingToStopLine(CarLoc);
        if (Remaining <= CommitDist) { bGo = true; }
        Car->SetDesiredSpeed(6000.f);
    }
    else
    {
        Car->SetDesiredSpeed(2000.f);
    }

    Car->SetShouldGoIntersection(bGo);
}

void AEnv_RoadLane::UpdateGeometryFromParams()
{
    RoadWidth         = FMath::Max(10.f, RoadWidth);
    RoadLength        = FMath::Max(10.f, RoadLength);
    RoadSideThickness = FMath::Max(1.f,  RoadSideThickness);
    RoadSideHeight    = FMath::Max(10.f, RoadSideHeight);
    LaneNumber        = FMath::Max(1,    LaneNumber);

    const float HalfLength = RoadLength * 0.5f;
    const float HalfWidth  = RoadWidth  * 0.5f;

    RoadSurface->SetBoxExtent(FVector(HalfLength, HalfWidth, 50.f), true);

    const float SideHalfThickness = RoadSideThickness * 0.5f;
    const float SideHalfHeight    = RoadSideHeight    * 0.5f;
    const FVector SideExtent(HalfLength, SideHalfThickness, SideHalfHeight);

    RoadSideLeft->SetBoxExtent(SideExtent, true);
    RoadSideRight->SetBoxExtent(SideExtent, true);

    const float YLeft      = -HalfWidth - SideHalfThickness;
    const float YRight     = +HalfWidth + SideHalfThickness;
    const float ZSideCenter = SideHalfHeight;

    RoadSideLeft->SetRelativeLocation(FVector(0.f, YLeft,  ZSideCenter));
    RoadSideRight->SetRelativeLocation(FVector(0.f, YRight, ZSideCenter));
    
    if (BrakingArea)
    {
        const float BrakingHalfWidth  = HalfWidth;
        const float BrakingHalfHeight = 100.f;
        BrakingArea->SetBoxExtent(FVector(50.0f, BrakingHalfWidth, BrakingHalfHeight), true);
        const float BrakingCenterX = HalfLength - BrakingAreaLength * 0.5f;
        BrakingArea->SetRelativeLocation(FVector(BrakingCenterX, 0.f, BrakingHalfHeight));
    }
    
    if (Spawner)
    {
        const float SpawnerHalfHeight = 100.f;
        float A = FMath::Clamp(FMath::Min(SpawnRange.X, SpawnRange.Y), 0.f, 1.f);
        float B = FMath::Clamp(FMath::Max(SpawnRange.X, SpawnRange.Y), 0.f, 1.f);
        const float Length    = HalfLength * 2.f;
        const float StartX    = -HalfLength + A * Length;
        const float EndX      = -HalfLength + B * Length;
        const float SpawnerHalfX = 0.5f * (EndX - StartX);
        const float CenterX   = 0.5f * (StartX + EndX);
        Spawner->SetBoxExtent(FVector(SpawnerHalfX, HalfWidth, SpawnerHalfHeight), true);
        Spawner->SetRelativeLocation(FVector(CenterX, 0.f, SpawnerHalfHeight));
    }
}

int32 AEnv_RoadLane::GetLaneIndexForLocation(const FVector& WorldLocation) const
{
    if (!RoadSurface) { return 0; }
    const FTransform RoadTransform = GetActorTransform();
    const FVector LocalPos = RoadTransform.InverseTransformPosition(WorldLocation);
    const FVector BoxExtent = RoadSurface->GetUnscaledBoxExtent();
    const float HalfWidth   = BoxExtent.Y;
    const int32 NumLanes    = FMath::Max(1, LaneNumber);
    const float TotalWidth  = HalfWidth * 2.f;
    const float LaneWidth   = TotalWidth / static_cast<float>(NumLanes);
    const float ShiftedY    = LocalPos.Y + HalfWidth;
    int32 LaneIndex = FMath::FloorToInt(ShiftedY / LaneWidth);
    LaneIndex = FMath::Clamp(LaneIndex, 0, NumLanes - 1);
    return LaneIndex;
}

float AEnv_RoadLane::GetForwardDistanceAlongLane(const FVector& WorldLocation, bool bClampToSegment) const
{
    if (!RoadSurface) { return 0.f; }
    const FTransform RoadTransform = GetActorTransform();
    const FVector LocalPos = RoadTransform.InverseTransformPosition(WorldLocation);
    const FVector BoxExtent  = RoadSurface->GetUnscaledBoxExtent();
    const float HalfLength   = BoxExtent.X;
    const float TotalLength  = HalfLength * 2.f;
    float Distance = LocalPos.X + HalfLength;
    if (bClampToSegment) { Distance = FMath::Clamp(Distance, 0.f, TotalLength); }
    UE_LOG(LogTemp, Display, TEXT("Local X is %f"), Distance);
    return Distance;
}

float AEnv_RoadLane::GetLateralOffsetFromCenter(const FVector& WorldLocation, bool bClampToLane) const
{
    if (!RoadSurface) { return 0.f; }
    const FTransform RoadTransform = RoadSurface->GetComponentTransform();
    const FVector LocalPos = RoadTransform.InverseTransformPosition(WorldLocation);
    const FVector BoxExtent = RoadSurface->GetUnscaledBoxExtent();
    const float HalfWidth   = BoxExtent.Y;
    float Offset = LocalPos.Y;
    if (bClampToLane) { Offset = FMath::Clamp(Offset, -HalfWidth, HalfWidth); }
    UE_LOG(LogTemp, Display, TEXT("Local Y is %f"), Offset);
    return Offset;
}

float AEnv_RoadLane::GetRemainingToStopLine(const FVector& WorldLoc) const
{
    const float S = GetForwardDistanceAlongLane(WorldLoc, true);
    UE_LOG(LogTemp, Warning, TEXT("[Lane] Distance relative %f"), S);
    const float StopLineS = RoadLength - StopLineOffsetFromEnd;
    UE_LOG(LogTemp, Warning, TEXT("[Lane] Distance to stop line %f"), StopLineS - S);
    return StopLineS - S;
}
