// RoadLane.cpp

#include "Env_RoadLane.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Env_TrafficLightTypes.h"
#include "Traffic_AICar.h"

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
            // UE_LOG(LogTemp, Warning, TEXT("Found Traffic Manager!"));
            BrakingArea->OnComponentBeginOverlap.RemoveDynamic(this, &AEnv_RoadLane::OnBrakeBeginOverlap);
            BrakingArea->OnComponentBeginOverlap.AddDynamic(this, &AEnv_RoadLane::OnBrakeBeginOverlap);

            BrakingArea->OnComponentEndOverlap.RemoveDynamic(this, &AEnv_RoadLane::OnBrakeEndOverlap);
            BrakingArea->OnComponentEndOverlap.AddDynamic(this, &AEnv_RoadLane::OnBrakeEndOverlap);
            TrafficLightManager->OnTrafficPhaseChanged.AddUObject(this, &AEnv_RoadLane::OnPhaseChanged);
        }
    }
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

    const auto Phase = TrafficLightManager->GetCurrentPhase();
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
        const FVector CarLoc = Car->GetActorLocation();
        const float Remaining = GetRemainingToStopLine(CarLoc);

        if (Remaining <= CommitDist)
        {
            bGo = true;
        }
    }
    else
    {
        Car->SetDesiredSpeed(2000.f);
    }

    Car->SetShouldGoIntersection(bGo);
}

//

void AEnv_RoadLane::UpdateGeometryFromParams()
{
    RoadWidth         = FMath::Max(10.f, RoadWidth);
    RoadLength        = FMath::Max(10.f, RoadLength);
    RoadSideThickness = FMath::Max(1.f,  RoadSideThickness);
    RoadSideHeight    = FMath::Max(10.f, RoadSideHeight);
    LaneNumber        = FMath::Max(1,    LaneNumber);

    const float HalfLength = RoadLength * 0.5f;
    const float HalfWidth  = RoadWidth  * 0.5f;

    RoadSurface->SetBoxExtent(
        FVector(HalfLength, HalfWidth, 50.f),
        true
    );
    // RoadSurface->SetRelativeLocation(FVector::ZeroVector);

    const float SideHalfThickness = RoadSideThickness * 0.5f;
    const float SideHalfHeight    = RoadSideHeight   * 0.5f;

    const FVector SideExtent(HalfLength, SideHalfThickness, SideHalfHeight);

    RoadSideLeft->SetBoxExtent(SideExtent, true);
    RoadSideRight->SetBoxExtent(SideExtent, true);

    const float YLeft  = -HalfWidth - SideHalfThickness;
    const float YRight = +HalfWidth + SideHalfThickness;
    const float ZSideCenter = SideHalfHeight;

    RoadSideLeft->SetRelativeLocation(FVector(0.f, YLeft,  ZSideCenter));
    RoadSideRight->SetRelativeLocation(FVector(0.f, YRight, ZSideCenter));
    
    if (BrakingArea)
    {
        const float BrakingHalfLength = BrakingAreaLength * 0.5f;
        const float BrakingHalfWidth  = HalfWidth;
        const float BrakingHalfHeight = 100.f;

        BrakingArea->SetBoxExtent(
            FVector(50.0f, BrakingHalfWidth, BrakingHalfHeight),
            true
        );

        const float BrakingCenterX = HalfLength - BrakingHalfLength;
        const float BrakingCenterZ = BrakingHalfHeight;

        BrakingArea->SetRelativeLocation(FVector(BrakingCenterX, 0.f, BrakingCenterZ));
    }
    
    if (Spawner)
    {
        const float SpawnerHalfWidth  = HalfWidth;
        const float SpawnerHalfHeight = 100.f;

        // 保证顺序、夹紧到 [0,1]
        float A = FMath::Clamp(FMath::Min(SpawnRange.X, SpawnRange.Y), 0.f, 1.f);
        float B = FMath::Clamp(FMath::Max(SpawnRange.X, SpawnRange.Y), 0.f, 1.f);

        const float Length = HalfLength * 2.f;

        // 以 Lane 本地 X：[-HalfLength, +HalfLength] 为坐标系
        const float StartX = -HalfLength + A * Length;
        const float EndX   = -HalfLength + B * Length;

        const float SpawnerHalfX = 0.5f * (EndX - StartX);
        const float CenterX      = 0.5f * (StartX + EndX);

        // X=覆盖区间半长，Y=道路半宽，Z=高度
        Spawner->SetBoxExtent(
            FVector(SpawnerHalfX, SpawnerHalfWidth, SpawnerHalfHeight),
            true
        );

        // 放到区间中心（相对 Lane / Root 的本地坐标）
        const float CenterZ = SpawnerHalfHeight;
        Spawner->SetRelativeLocation(FVector(CenterX, 0.f, CenterZ));
    }

    // RoadMesh->SetRelativeLocation(FVector::ZeroVector);

    // const float ScaleX = RoadLength / 100.f;
    // const float ScaleY = RoadWidth  / 100.f;
    // RoadMesh->SetRelativeScale3D(FVector(ScaleX, ScaleY, 1.f));
}

int32 AEnv_RoadLane::GetLaneIndexForLocation(const FVector& WorldLocation) const
{
    if (!RoadSurface)
    {
        return 0;
    }

    const FTransform RoadTransform = GetActorTransform();
    const FVector LocalPos = RoadTransform.InverseTransformPosition(WorldLocation);

    // RoadSurface
    const FVector BoxExtent = RoadSurface->GetUnscaledBoxExtent();
    const float HalfWidth = BoxExtent.Y;
    const int32 NumLanes  = FMath::Max(1, LaneNumber);

    const float TotalWidth = HalfWidth * 2.f;
    const float LaneWidth  = TotalWidth / static_cast<float>(NumLanes);

    // [-HalfWidth, +HalfWidth] → [0, TotalWidth]
    const float ShiftedY = LocalPos.Y + HalfWidth;

    // index
    int32 LaneIndex = FMath::FloorToInt(ShiftedY / LaneWidth);

    // Clamp
    LaneIndex = FMath::Clamp(LaneIndex, 0, NumLanes - 1);

    return LaneIndex;
}

float AEnv_RoadLane::GetForwardDistanceAlongLane(const FVector& WorldLocation, bool bClampToSegment) const
{
    if (!RoadSurface)
    {
        return 0.f;
    }

    const FTransform RoadTransform = GetActorTransform();
    const FVector LocalPos = RoadTransform.InverseTransformPosition(WorldLocation);

    const FVector BoxExtent = RoadSurface->GetUnscaledBoxExtent();
    const float HalfLength = BoxExtent.X;
    const float TotalLength = HalfLength * 2.f;

    // Local X: [-HalfLength, +HalfLength] → [0, TotalLength]
    float Distance = LocalPos.X + HalfLength;

    if (bClampToSegment)
    {
        Distance = FMath::Clamp(Distance, 0.f, TotalLength);
    }
    
    UE_LOG(LogTemp, Display, TEXT("Local X is %f"), Distance);

    return Distance; // cm
}

float AEnv_RoadLane::GetLateralOffsetFromCenter(const FVector& WorldLocation, bool bClampToLane) const
{
    if (!RoadSurface)
    {
        return 0.f;
    }

    const FTransform RoadTransform = RoadSurface->GetComponentTransform();
    const FVector LocalPos = RoadTransform.InverseTransformPosition(WorldLocation);

    // Box Y 方向是道路宽度
    const FVector BoxExtent = RoadSurface->GetUnscaledBoxExtent();
    const float HalfWidth = BoxExtent.Y;

    // Local Y: 左负右正（UE 坐标系）
    float Offset = LocalPos.Y;

    if (bClampToLane)
    {
        Offset = FMath::Clamp(Offset, -HalfWidth, HalfWidth);
    }
    
    UE_LOG(LogTemp, Display, TEXT("Local Y is %f"), Offset);
    return Offset; // cm, 中心线为 0
}

float AEnv_RoadLane::GetRemainingToStopLine(const FVector& WorldLoc) const
{
    const float S = GetForwardDistanceAlongLane(WorldLoc, true); // clamp
    UE_LOG(LogTemp, Warning, TEXT("[Lane] Distance relative %f"), S);
    const float StopLineS = RoadLength - StopLineOffsetFromEnd;
    UE_LOG(LogTemp, Warning, TEXT("[Lane] Distance to stop line %f"), StopLineS - S);
    return StopLineS - S; // >0 还没到线；<=0 已越线
}
