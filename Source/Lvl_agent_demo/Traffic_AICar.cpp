// Traffic_AICar.cpp

#include "Traffic_AICar.h"

#include "Components/PrimitiveComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Traffic_AICarManagerComponent.h"
#include "DrawDebugHelpers.h"

ATraffic_AICar::ATraffic_AICar()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ATraffic_AICar::BeginPlay()
{
    Super::BeginPlay();

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
    TargetSpeed = DesiredSpeed;
    CurrentSpeed = 0.f;
}

void ATraffic_AICar::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 1) Raycast 更新前方障碍物信息
    UpdateRaycast();

    // 2) 计算当前环境允许的 TargetSpeed（前车、红灯等）
    UpdateTargetSpeed();

    // 3) CurrentSpeed 平滑逼近 TargetSpeed
    UpdateSpeed(DeltaTime);

    // 4) 把 CurrentSpeed 作用到 Chaos 车辆组件
    ApplySpeedToVehicle();
}

// ---------- Raycast ----------

void ATraffic_AICar::UpdateRaycast()
{
    UWorld* World = GetWorld();
    if (!World) return;

    const FTransform ActorTransform = GetActorTransform();
    const FVector Origin = ActorTransform.TransformPosition(RaycastOriginOffset);
    const FVector Fwd = GetActorForwardVector();
    const FVector Right = GetActorRightVector();

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = false;

    // 重置状态
    bHasBlockingActorAhead = false;
    BlockingActorAhead = nullptr;
    RaycastHitDistance = TNumericLimits<float>::Max();

    // 计算每条射线的偏角（度）
    // RaycastCount == 1 时只发中心线
    const int32 NumRays = FMath::Max(1, RaycastCount);
    const float HalfAngle = (NumRays > 1) ? RaycastHalfAngleDeg : 0.f;

    for (int32 i = 0; i < NumRays; ++i)
    {
        // 在 [-HalfAngle, +HalfAngle] 均匀分布
        const float AngleDeg = (NumRays > 1)
            ? FMath::Lerp(-HalfAngle, HalfAngle, static_cast<float>(i) / static_cast<float>(NumRays - 1))
            : 0.f;

        const float AngleRad = FMath::DegreesToRadians(AngleDeg);
        const FVector RayDir = (Fwd * FMath::Cos(AngleRad) + Right * FMath::Sin(AngleRad)).GetSafeNormal();
        const FVector RayEnd = Origin + RayDir * RaycastMaxDistance;

        FHitResult Hit;
        const bool bHit = World->LineTraceSingleByChannel(
            Hit,
            Origin,
            RayEnd,
            ECC_Vehicle,   // 只检测 Vehicle 通道；如需也检测 Pawn 改为 ECC_Pawn 或用 ObjectQuery
            QueryParams
        );

        // 调试绘制（需要 DrawDebugHelpers.h）
        // DrawDebugLine(World, Origin, bHit ? Hit.ImpactPoint : RayEnd,
        //     bHit ? FColor::Red : FColor::Green, false, -1.f, 0, 3.f);

        if (!bHit) continue;

        AActor* HitActor = Hit.GetActor();
        if (!IsDesignatedBlockingActor(HitActor)) continue;

        // 取所有射线中最近的命中
        if (Hit.Distance < RaycastHitDistance)
        {
            RaycastHitDistance = Hit.Distance;
            BlockingActorAhead = HitActor;
            bHasBlockingActorAhead = true;
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[Raycast] bHas=%d  Dist=%.1f  Actor=%s"),
        bHasBlockingActorAhead ? 1 : 0,
        RaycastHitDistance,
        *GetNameSafe(BlockingActorAhead.Get()));
}

// ---------- 速度逻辑 ----------

void ATraffic_AICar::SetDesiredSpeed(float NewDesiredSpeed)
{
    DesiredSpeed = FMath::Max(0.f, NewDesiredSpeed);
}

void ATraffic_AICar::UpdateTargetSpeed()
{
    // 红绿灯 / 路口逻辑保留不变
    bForceBrake = !bShouldGoIntersection;
    bHardStop = false;

    if (bHasBlockingActorAhead && BlockingActorAhead.IsValid())
    {
        // 用 RaycastHitDistance 直接作为前向距离，无需再 DotProduct
        const float ForwardDist = RaycastHitDistance;

        const float StopDist = SafeDist * 3.0f;
        const float HardStopDist = SafeDist * 0.8f;

        if (ForwardDist < StopDist)
        {
            bForceBrake = true;
        }

        if (ForwardDist < HardStopDist)
        {
            bHardStop = true;
        }
    }

    TargetSpeed = bForceBrake ? 0.f : DesiredSpeed * PawnApproachingEffect;
}

void ATraffic_AICar::UpdateSpeed(float DeltaTime)
{
    //if (SpeedChangeTime <= 0.f)
    //{
    //    CurrentSpeed = TargetSpeed;
    //    return;
    //}

    //// 通过 Lerp 在 SpeedChangeTime 内趋近 TargetSpeed
    //const float Alpha = FMath::Clamp(DeltaTime / SpeedChangeTime, 0.f, 1.f);
    //CurrentSpeed = FMath::Lerp(CurrentSpeed, TargetSpeed, Alpha);
}

void ATraffic_AICar::ApplySpeedToVehicle()
{
    if (!CachedVehicleMovement) return;

    const float Speed = CachedVehicleMovement->GetForwardSpeed();

    // ---------- 强制停车 ----------
    if (bForceBrake)
    {
        CachedVehicleMovement->SetThrottleInput(0.f);
        CachedVehicleMovement->SetBrakeInput(1.f);
        CachedVehicleMovement->SetHandbrakeInput(true);
        return;
    }

    CachedVehicleMovement->SetHandbrakeInput(false);

    const float SpeedError = TargetSpeed - Speed;

    float Throttle = 0.f;
    float Brake = 0.f;

    constexpr float Deadband = 50.f;

    if (SpeedError > Deadband)
    {
        Throttle = 0.6f;
    }
    else if (SpeedError < -Deadband)
    {
        Brake = 0.5f;
    }

    CachedVehicleMovement->SetThrottleInput(Throttle);
    CachedVehicleMovement->SetBrakeInput(Brake);
}

// ---------- 工具函数 ----------

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
    if (bShouldGoIntersection == bShouldGo) return;
    bShouldGoIntersection = bShouldGo;
}

void ATraffic_AICar::SetCurrentLane(AEnv_RoadLane* Lane) { CurrentDrivingLane = Lane; }
void ATraffic_AICar::SetLaneIndex(int32 LaneIndex) { CurrentDrivingLaneIndex = LaneIndex; }
int32 ATraffic_AICar::GetLaneIndex() const { return CurrentDrivingLaneIndex; }
AEnv_RoadLane* ATraffic_AICar::GetCurrentLane() const { return CurrentDrivingLane; }
void ATraffic_AICar::SetPawnApproachingEffect(float Effect) { PawnApproachingEffect = Effect; }