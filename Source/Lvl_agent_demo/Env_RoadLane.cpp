// RoadLane.cpp

#include "Env_RoadLane.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Env_TrafficLightTypes.h"
#include "Traffic_AICar.h"
#include "Traffic_AICarManagerComponent.h"
#include "Traffic_AutoDriving.h"
#include "DecisionTrigger.h"
#include "Kismet/GameplayStatics.h"
#include "Env_RoadLaneTransfer.h"
#include "RecommendationManager.h"

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

	TurnArea = CreateDefaultSubobject<UBoxComponent>(TEXT("TurnArea"));
	TurnArea->SetupAttachment(RoadSurface);
	TurnArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TurnArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	TurnArea->SetCollisionObjectType(ECC_WorldDynamic);
	TurnArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TurnArea->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
	TurnArea->SetGenerateOverlapEvents(true);

    RoadRangeBox = CreateDefaultSubobject<UBoxComponent>(TEXT("RoadRangeBox"));
    RoadRangeBox->SetupAttachment(RoadSurface);
    RoadRangeBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    RoadRangeBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    RoadRangeBox->SetCollisionObjectType(ECC_WorldDynamic);
    RoadRangeBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    RoadRangeBox->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
    RoadRangeBox->SetGenerateOverlapEvents(true);
    
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

    TrafficManager = UTraffic_AICarManagerComponent::GetTrafficManager(this);
    if (TrafficManager)
    {
        //UE_LOG(LogTemp, Warning, TEXT("[RoadLane] Found TrafficManager: %s"), *TrafficManager->GetName());

        RoadRangeBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        RoadRangeBox->SetCollisionObjectType(ECC_WorldDynamic);
        RoadRangeBox->SetCollisionResponseToAllChannels(ECR_Ignore);
        RoadRangeBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        RoadRangeBox->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
        RoadRangeBox->SetGenerateOverlapEvents(true);

        RoadRangeBox->OnComponentBeginOverlap.RemoveDynamic(this, &AEnv_RoadLane::OnLaneBeginOverlap);
        RoadRangeBox->OnComponentBeginOverlap.AddDynamic(this, &AEnv_RoadLane::OnLaneBeginOverlap);

        RoadRangeBox->OnComponentEndOverlap.RemoveDynamic(this, &AEnv_RoadLane::OnLaneEndOverlap);
        RoadRangeBox->OnComponentEndOverlap.AddDynamic(this, &AEnv_RoadLane::OnLaneEndOverlap);

        FTimerHandle Handle;

        GetWorldTimerManager().SetTimer(
            Handle,
            this,
            &AEnv_RoadLane::DelayedUpdateOverlaps,
            1.f,
            false
        );
    }

    if (DecisionTriggerClass)
    {
        FVector SpawnLocation = GetActorLocation()
            + GetActorForwardVector() * (DecisionTriggerInitRatio * LaneLength);

        DecisionTrigger = GetWorld()->SpawnActor<ADecisionTrigger>(
            DecisionTriggerClass,
            SpawnLocation,
            GetActorRotation()
        );

        if (DecisionTrigger)
        {
            DecisionTrigger->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
            SetDecisionTriggerAtRatio(DecisionTriggerInitRatio);
        }
    }

    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnv_RoadLaneTransfer::StaticClass(), Found);

    for (AActor* A : Found)
    {
        if (AEnv_RoadLaneTransfer* T = Cast<AEnv_RoadLaneTransfer>(A))
        {
            T->OnLoopTransferTriggered.AddDynamic(this, &AEnv_RoadLane::OnAnyLoopTransferTriggered);
            // UE_LOG(LogTemp, Display, TEXT("[Traffic Manager] Found Road Transfer"));
        }
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("[Road Lane] World is null"));
        return;
    }

    UGameInstance* GI = World->GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogTemp, Error, TEXT("[Road Lane] GameInstance is null"));
        return;
    }

    RecMgr = GI->GetSubsystem<URecommendationManager>();
    if (!RecMgr)
    {
        UE_LOG(LogTemp, Error, TEXT("[Road Lane] RecommendationManager not found"));
        return;
    }
}

void AEnv_RoadLane::DelayedUpdateOverlaps()
{
    if (!RoadRangeBox) return;

    UE_LOG(LogTemp, Warning, TEXT("[RoadLane] DelayedUpdateOverlaps: %s"), *GetName());

    RoadRangeBox->UpdateOverlaps();

    TArray<AActor*> OverlappingActors;
    RoadRangeBox->GetOverlappingActors(OverlappingActors);

    UE_LOG(LogTemp, Warning, TEXT("[RoadLane] Found %d overlapping actors in RoadRangeBox"), OverlappingActors.Num());

    for (AActor* OtherActor : OverlappingActors)
    {
        if (!OtherActor || OtherActor == this) continue;

        APawn* Pawn = Cast<APawn>(OtherActor);
        if (!Pawn || !TrafficManager) continue;

        UE_LOG(LogTemp, Warning, TEXT("[RoadLane] Initial overlap found: %s"), *OtherActor->GetName());

        Auto = Pawn->FindComponentByClass<UTraffic_AutoDriving>();
        if (Auto)
        {
            Auto->SetCurrentLane(this);
        }

        if (!GetWorldTimerManager().IsTimerActive(LaneSetCooldownHandle))
        {
            TrafficManager->SetCurrentLane(this);
            GetWorldTimerManager().SetTimer(LaneSetCooldownHandle, []() {}, 0.3f, false);
        }
    }
}

void AEnv_RoadLane::OnLaneBeginOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32                OtherBodyIndex,
    bool                 bFromSweep,
    const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this) { return; }
    if (IsAICar(OtherActor))
    {
        FTimerHandle TempHandle;

        TWeakObjectPtr<AActor> WeakActor = OtherActor;

        FTimerDelegate TimerDel;
        TimerDel.BindLambda([this, WeakActor]()
            {
                if (WeakActor.IsValid())
                {
                    OnAICarEnterLane(WeakActor.Get());
                }
            });

        GetWorld()->GetTimerManager().SetTimer(
            TempHandle,
            TimerDel,
            AICarRecoverSpeedTime,
            false
        );

        return;
    }

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn || !TrafficManager) { return; }

    IsPawnInBrakingArea = BrakingArea->IsOverlappingActor(Pawn);

    UTraffic_AutoDriving* NewAuto = Pawn->FindComponentByClass<UTraffic_AutoDriving>();
    if (NewAuto && NewAuto->bAutoDriveEnabled)
    {
        // 更新 Auto 引用到当前 lane 的 auto
        Auto = NewAuto;

        Auto->SetCurrentLane(this);

        if (IsPawnInBrakingArea && TrafficLightManager)
        {
            UpdateAutoDrivingGoState(Auto);
        }

        Auto->bUseSwitchLane = false;
        Auto->SetIsInIntersection(false);
    }

    if (!GetWorldTimerManager().IsTimerActive(LaneSetCooldownHandle))
    {
        TrafficManager->SetCurrentLane(this);

        GetWorldTimerManager().SetTimer(
            LaneSetCooldownHandle,
            []() {},
            0.3f,
            false
        );
    }
}

void AEnv_RoadLane::OnLaneEndOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    UE_LOG(LogTemp, Warning, TEXT("[RoadLane] OnLaneEndOverlap: Lane=%s OtherActor=%s"),
        *GetName(),
        OtherActor ? *OtherActor->GetName() : TEXT("None"));

    if (!OtherActor || OtherActor == this) return;
    if (IsAICar(OtherActor)) return;

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn || !TrafficManager) return;

    if (TrafficManager->GetCurrentLane() != this)
    {
        return;
    }

    TrafficManager->SetCurrentLane(nullptr);

    if (Auto && Auto->bAutoDriveEnabled)
    {
        // 关键保护：只有当 Auto 当前记录的 lane 仍然是本 lane 时，才执行副作用
        // 若 Auto 已经切换到新 lane（Teleport 情况），不做任何修改
        if (Auto->GetCurrentDrivingLane() != this)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[RoadLane] OnLaneEndOverlap: skipping side effects, Auto already on lane %s"),
                Auto->GetCurrentDrivingLane() ? *Auto->GetCurrentDrivingLane()->GetName() : TEXT("None"));
            Auto = nullptr;
            return;
        }

        Auto->SetGo(true);
        Auto->SetIsInIntersection(true);
        IsPawnInBrakingArea = false;
        Auto = nullptr;
    }
}

void AEnv_RoadLane::OnAICarEnterLane(AActor* OtherActor)
{
    if (ATraffic_AICar* AICar = Cast<ATraffic_AICar>(OtherActor))
    {
        AICar->SetDesiredSpeed(AICar->GetInitialDesiredSpeed());
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

    // --- AICar 路径 ---
    ATraffic_AICar* Car = Cast<ATraffic_AICar>(OtherActor);
    if (Car)
    {
        CarsInBrakingArea.Add(Car);
        UpdateCarGoState(Car);
        return;
    }

    // --- Pawn（AutoDriving）路径 ---
    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn) return;

    // 如果 Auto 还是 null（Pawn 比 RoadRangeBox 先进 BrakingArea），尝试补充赋值
    if (!Auto)
    {
        Auto = Pawn->FindComponentByClass<UTraffic_AutoDriving>();
    }

    if (Auto && Auto->bAutoDriveEnabled)
    {
        IsPawnInBrakingArea = true;
        UpdateAutoDrivingGoState(Auto);
    }
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

    if (Auto && Auto->bAutoDriveEnabled)
    {
        // 同样只在 Auto 的当前 lane 是本 lane 时才操作
        if (Auto->GetCurrentDrivingLane() == this || Auto->GetCurrentDrivingLane() == nullptr)
        {
            Auto->SetGo(true);
        }
        IsPawnInBrakingArea = false;
        Auto = nullptr;
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

    if (Auto && Auto->bAutoDriveEnabled)
    {
        UpdateAutoDrivingGoState(Auto);
    }
}

// 修复 C4458：参数名从 Auto 改为 InAuto，避免与类成员 Auto 同名
void AEnv_RoadLane::UpdateAutoDrivingGoState(UTraffic_AutoDriving* InAuto)
{
    if (!InAuto || !TrafficLightManager || !IsPawnInBrakingArea) return;

    if (InAuto->GetCurrentDrivingLane() != this) return;

    const auto Phase = TrafficLightManager->GetCurrentPhase();
    const auto PawnCurrentLaneAxis = InAuto->GetCurrentDrivingLane()->LaneAxis;
    const auto CarLaneIndex = InAuto->GetCurrentLaneIdx();

    bool bGo = false;
    switch (Phase)
    {
    case EGlobalTrafficPhase::NS_Straight_Ped_Green:
        bGo = (PawnCurrentLaneAxis == ETrafficAxis::NorthSouth && CarLaneIndex >= 1);
        break;
    case EGlobalTrafficPhase::EW_Straight_Ped_Green:
        bGo = (PawnCurrentLaneAxis == ETrafficAxis::EastWest && CarLaneIndex >= 1);
        break;
    case EGlobalTrafficPhase::NS_Left_Green:
        bGo = (PawnCurrentLaneAxis == ETrafficAxis::NorthSouth && CarLaneIndex == 0);
        break;
    case EGlobalTrafficPhase::EW_Left_Green:
        bGo = (PawnCurrentLaneAxis == ETrafficAxis::EastWest && CarLaneIndex == 0);
        break;
    case EGlobalTrafficPhase::All_Red:
        bGo = false;
        break;
    default:
        bGo = false;
        break;
    }

    InAuto->SetGo(bGo);
}

void AEnv_RoadLane::UpdateCarGoState(ATraffic_AICar* Car)
{
    if (!Car || !TrafficLightManager) return;

    const auto Phase        = TrafficLightManager->GetCurrentPhase();
    const auto CarLaneAxis  = Car->GetCurrentLane()->LaneAxis;
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
        case EGlobalTrafficPhase::All_Red:
            bGo = false;
            break;
        default:
            bGo = false;
            break;
    }

    if (!bGo)
    {
        const FVector CarLoc  = Car->GetActorLocation();
        const float Remaining = GetRemainingToStopLine(CarLoc);
        if (Remaining <= CommitDist) { 
            bGo = true; 
            Car->SetDesiredSpeed(2000.f);
        }
    }

    if (bGo)
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

    if (RoadRangeBox)
    {
        const float RangeHalfHeight = 250.f;
        const float RangeCenterZ = 100.f;
        RoadRangeBox->SetBoxExtent(FVector(HalfLength, HalfWidth - SideHalfThickness, RangeHalfHeight), true);
        RoadRangeBox->SetRelativeLocation(FVector(0.f, 0.f, RangeCenterZ));
    }

    if (TurnArea) 
    {
        TurnArea->SetBoxExtent(FVector(50.0f, HalfWidth, 100.f), true);
        const float BrakingCenterX = HalfLength + 200.f;
        TurnArea->SetRelativeLocation(FVector(BrakingCenterX, 0.f, 100.f));
    }
    
	SafeTurnDist = HalfLength*0.75;
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

// =======================
// Decision trigger
// =======================
FVector AEnv_RoadLane::GetPointAlongLane(float DistanceAlongLane) const
{
    const FVector LaneStart =
        GetActorLocation() - GetActorForwardVector() * (RoadLength * 0.5f);

    const FVector LaneForward = GetActorForwardVector().GetSafeNormal();

    return LaneStart + LaneForward * DistanceAlongLane;
}

void AEnv_RoadLane::SetDecisionTriggerAtDistance(float DistanceAlongLane)
{
    if (!DecisionTrigger)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RoadLane] DecisionTrigger is null on %s"), *GetName());
        return;
    }

    const float ClampedDistance = FMath::Clamp(DistanceAlongLane, 0.f, RoadLength);
    FVector NewLocation = GetPointAlongLane(ClampedDistance);

    FRotator TriggerRotation = GetActorRotation();

    TriggerRotation.Yaw += 90.f;
    DecisionTrigger->SetActorLocationAndRotation(NewLocation, TriggerRotation);
	/*DrawDebugBox(GetWorld(), NewLocation, FVector(50.f, 50.f, 100.f), FColor::Green, false, 10.f);

    UBoxComponent* Box = DecisionTrigger->FindComponentByClass<UBoxComponent>();

    if (Box)
    {
        FVector Center = Box->GetComponentLocation();
        FVector Extent = Box->GetScaledBoxExtent();
        FQuat Rotation = Box->GetComponentQuat();

        DrawDebugBox(
            GetWorld(),
            Center,
            Extent,
            Rotation,
            FColor::Red,
            false, 
            10
        );
    }*/

    UE_LOG(LogTemp, Log, TEXT("[RoadLane] Move DecisionTrigger to %.1f / %.1f on %s"),
        ClampedDistance, RoadLength, *GetName());
}

void AEnv_RoadLane::SetDecisionTriggerAtRatio(float Ratio)
{
    const float ClampedRatio = FMath::Clamp(Ratio, 0.f, 1.f);
    SetDecisionTriggerAtDistance(ClampedRatio * RoadLength);
}

void AEnv_RoadLane::OnAnyLoopTransferTriggered(
    AEnv_RoadLaneTransfer* Transfer,
    AActor* OtherActor,
    AEnv_RoadLane* DestLane)
{
    if (!RecMgr || !DecisionTrigger)
    {
        return;
    }

    int32 Category = 0;
    int32 Subcategory = 0;
    int32 Item = 0;

    const int32 EncodeId = RecMgr->GetCurrentRowId();
    RecMgr->DecomposeRecommendationID(EncodeId, Category, Subcategory, Item);

    if (Category == 1 && Subcategory >= 1 && Subcategory <= 5)
    {
        SetDecisionTriggerAtRatio(0.45f);
    }
    else if (Category == 2 && (Subcategory == 1 || Subcategory == 2))
    {
        SetDecisionTriggerAtRatio(DecisionTriggerInitRatio);
    }
    else if (Category == 2 && Subcategory == 3)
    {
        SetDecisionTriggerAtRatio(0.75f);
    }
    else if (Category == 2 && Subcategory == 4)
    {
        SetDecisionTriggerAtRatio(0.65f);
    }
    else if (Category == 2 && Subcategory == 5)
    {
        SetDecisionTriggerAtRatio(0.5f);
    }
    else
    {
        SetDecisionTriggerAtRatio(DecisionTriggerInitRatio);
	}
}