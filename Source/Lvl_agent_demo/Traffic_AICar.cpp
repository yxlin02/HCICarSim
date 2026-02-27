// Traffic_AICar.cpp

#include "Traffic_AICar.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Traffic_AICarManagerComponent.h"

ATraffic_AICar::ATraffic_AICar()
{
    PrimaryActorTick.bCanEverTick = true;

    // 创建前向感知 Box
    FrontSensorBox = CreateDefaultSubobject<UBoxComponent>(TEXT("FrontSensorBox"));
    FrontSensorBox->SetupAttachment(GetRootComponent());

    // 根据车长/宽/高适当调整
    // FrontSensorBox->SetBoxExtent(FVector(400.f, 120.f, 80.f));
    // FrontSensorBox->SetRelativeLocation(FVector(400.f, 0.f, 50.f));

    FrontSensorBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    FrontSensorBox->SetCollisionObjectType(ECC_WorldDynamic);
    FrontSensorBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    FrontSensorBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
    FrontSensorBox->SetCollisionResponseToChannel(ECC_WorldStatic,  ECR_Ignore);
    
    FrontSensorBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    FrontSensorBox->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
    FrontSensorBox->SetGenerateOverlapEvents(true);
    // FrontSensorBox->SetHiddenInGame(false);
}

void ATraffic_AICar::BeginPlay()
{
    Super::BeginPlay();

    // 绑定 overlap 回调
    if (FrontSensorBox)
    {
        FrontSensorBox->OnComponentBeginOverlap.AddDynamic(
            this, &ATraffic_AICar::OnFrontSensorBeginOverlap);
        FrontSensorBox->OnComponentEndOverlap.AddDynamic(
            this, &ATraffic_AICar::OnFrontSensorEndOverlap);
    }

    // 缓存 Chaos 车辆 movement 组件
    CachedVehicleMovement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
    if (!CachedVehicleMovement)
    {
        UE_LOG(LogTemp, Error, TEXT("[Traffic_AICar] Failed to get Chaos vehicle movement component"));
    }
    
    // register this car
    if (UTraffic_AICarManagerComponent* Mgr =
        UTraffic_AICarManagerComponent::GetTrafficManager(this))
    {
        Mgr->RegisterCar(this);
    }

    // 初始状态：希望跑到 DesiredSpeed，当前速度为 0
    TargetSpeed  = DesiredSpeed;
    CurrentSpeed = 0.f;
    
    FrontSensorBox->UpdateOverlaps();

    TArray<AActor*> Initial;
    FrontSensorBox->GetOverlappingActors(Initial, AActor::StaticClass());

    for (AActor* A : Initial)
    {
        if (IsDesignatedBlockingActor(A))
        {
            FrontSensorOverlaps.Add(A);
        }
    }

    bHasBlockingActorAhead = FrontSensorOverlaps.Num() > 0;
    UpdateBlockingActorAheadFromOverlaps();
}

void ATraffic_AICar::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 1) 计算当前环境允许的 TargetSpeed（前车、红灯等）
    UpdateTargetSpeed();

    // 2) CurrentSpeed 平滑逼近 TargetSpeed
    UpdateSpeed(DeltaTime);

    // 3) 把 CurrentSpeed 作用到 Chaos 车辆组件
    ApplySpeedToVehicle();
}

// ---------- 速度逻辑 ----------

void ATraffic_AICar::SetDesiredSpeed(float NewDesiredSpeed)
{
    DesiredSpeed = FMath::Max(0.f, NewDesiredSpeed);
}

void ATraffic_AICar::UpdateTargetSpeed()
{
    // 先假设可以跑到理想速度
    float NewTargetSpeed = DesiredSpeed;

    // TODO: 在这里加入红绿灯逻辑
    // e.g. if (IsRedLightAheadAndClose()) { NewTargetSpeed = 0.f; }
    bForceBrake = !bShouldGoIntersection;

    // 如果前方有车 / Pawn / 障碍物，现在先简单处理：直接降为 0（停车）
    // 以后可以用车距 + 前车速度算一个安全速度
    if (bHasBlockingActorAhead && BlockingActorAhead.IsValid())
    {
        NewTargetSpeed = 0.f;
        const FVector CarForward = GetActorForwardVector();
        const FVector Delta = BlockingActorAhead->GetActorLocation() - GetActorLocation();
        
        const float d = FVector::DotProduct(Delta, CarForward);
        const float MaxDecel = 12000.f;
        const float v = FMath::Max(0.f, CachedVehicleMovement ? CachedVehicleMovement->GetForwardSpeed() : GetVelocity().Size());
        const float stopDist = (v*v) / (2.f*MaxDecel);

        bForceBrake = (stopDist >= (d - SafeDist));
    }
    
    TargetSpeed = FMath::Max(0.f, NewTargetSpeed);
}

void ATraffic_AICar::UpdateSpeed(float DeltaTime)
{
//    if (SpeedChangeTime <= 0.f)
//    {
//        CurrentSpeed = TargetSpeed;
//        return;
//    }
//
//    // 通过 Lerp 在 SpeedChangeTime 内趋近 TargetSpeed
//    const float Alpha = FMath::Clamp(DeltaTime / SpeedChangeTime, 0.f, 1.f);
//    CurrentSpeed = FMath::Lerp(CurrentSpeed, TargetSpeed, Alpha);
}

void ATraffic_AICar::ApplySpeedToVehicle()
{
    if (!CachedVehicleMovement) return;

    const float CurrentForwardSpeed = CachedVehicleMovement->GetForwardSpeed(); // cm/s

    const float SpeedError = TargetSpeed - CurrentForwardSpeed;

    float ThrottleInput = 0.f;
    float BrakeInput    = 0.f;

    constexpr float Deadband = 30.f;
    constexpr float KpAccel  = 0.0015f;
    constexpr float KpBrake  = 0.0035f;

    if (SpeedError > Deadband)
    {
        ThrottleInput = FMath::Clamp(SpeedError * KpAccel, 0.f, 1.f);
    }
    else if (SpeedError < -Deadband)
    {
        BrakeInput = FMath::Clamp((-SpeedError) * KpBrake, 0.f, 1.f);
    }

    if (bForceBrake)
    {
        ThrottleInput = 0.f;
        BrakeInput    = 1.f;
        CachedVehicleMovement->SetHandbrakeInput(true);
    }
    else
    {
        CachedVehicleMovement->SetHandbrakeInput(false);
    }

    CachedVehicleMovement->SetThrottleInput(ThrottleInput);
    CachedVehicleMovement->SetBrakeInput(BrakeInput);
}

// ---------- 感知回调 ----------
void ATraffic_AICar::OnFrontSensorBeginOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this)
    {
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("[Sensor] Overlap with %s (%s)"),
           *GetNameSafe(OtherActor),
           *GetNameSafe(OtherActor->GetClass()));

    if (!IsDesignatedBlockingActor(OtherActor))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Sensor] Not designated actor: %s"), *GetNameSafe(OtherActor));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[Sensor] Begin overlap: %s (%s)"),
           *GetNameSafe(OtherActor),
           *GetNameSafe(OtherActor->GetClass()));

    FrontSensorOverlaps.Add(OtherActor);

    // 更新阻挡状态
    bHasBlockingActorAhead = FrontSensorOverlaps.Num() > 0;

    // 选“最近的那个”作为 BlockingActorAhead（推荐）
    UpdateBlockingActorAheadFromOverlaps();
}

void ATraffic_AICar::OnFrontSensorEndOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    if (!OtherActor || OtherActor == this)
    {
        return;
    }

    if (FrontSensorOverlaps.Remove(OtherActor) > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Sensor] End overlap: %s"), *GetNameSafe(OtherActor));
    }

    // 清理失效指针（比如 actor 被销毁）
    FrontSensorOverlaps.Remove(nullptr);

    bHasBlockingActorAhead = FrontSensorOverlaps.Num() > 0;

    if (!bHasBlockingActorAhead)
    {
        BlockingActorAhead = nullptr;
        return;
    }

    UpdateBlockingActorAheadFromOverlaps();
}

void ATraffic_AICar::UpdateBlockingActorAheadFromOverlaps()
{
    const FVector MyPos = GetActorLocation();
    const FVector Fwd   = GetActorForwardVector();

    float BestDist = TNumericLimits<float>::Max();
    TWeakObjectPtr<AActor> BestActor = nullptr;

    for (const TWeakObjectPtr<AActor>& W : FrontSensorOverlaps)
    {
        AActor* A = W.Get();
        if (!IsValid(A)) continue;
        if (!IsDesignatedBlockingActor(A)) continue;

        const FVector To = A->GetActorLocation() - MyPos;
        const float ForwardDist = FVector::DotProduct(To, Fwd);

        // 只认“在前方”的（避免侧后方 overlap 也算）
        if (ForwardDist <= 0.f) continue;

        if (ForwardDist < BestDist)
        {
            BestDist = ForwardDist;
            BestActor = A;
        }
    }

    BlockingActorAhead = BestActor;
    bHasBlockingActorAhead = BlockingActorAhead.IsValid();

    UE_LOG(LogTemp, Verbose, TEXT("[Sensor] UpdateBlockingActorAhead => %s (bHas=%d)"),
           *GetNameSafe(BlockingActorAhead.Get()),
           bHasBlockingActorAhead ? 1 : 0);
}


bool ATraffic_AICar::IsDesignatedBlockingActor(AActor* OtherActor) const
{
    if (!OtherActor || OtherActor == this) return false;

    return OtherActor->IsA(ALvl_agent_demoPawn::StaticClass()) ||
           OtherActor->IsA(APawn::StaticClass()) ||
           OtherActor->IsA(ALvl_agent_demoSportsCar::StaticClass()) ||
           OtherActor->IsA(ATraffic_AICar::StaticClass());
}

void ATraffic_AICar::SetShouldGoIntersection(bool bShouldGo)
{
    if (bShouldGoIntersection == bShouldGo)
    {
        return;
    }

    bShouldGoIntersection = bShouldGo;
}

void ATraffic_AICar::SetCurrentLane(AEnv_RoadLane* Lane)
{
    CurrentDrivingLane = Lane;
}

void ATraffic_AICar::SetLaneIndex(int32 LaneIndex)
{
    CurrentDrivingLaneIndex = LaneIndex;
}

int32 ATraffic_AICar::GetLaneIndex() const { return CurrentDrivingLaneIndex; }
AEnv_RoadLane* ATraffic_AICar::GetCurrentLane() const { return CurrentDrivingLane; }
