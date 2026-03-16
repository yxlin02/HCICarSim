#include "Traffic_AutoDriving.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Env_RoadLane.h"
#include "Lvl_agent_demoPawn.h"
#include "Traffic_AICar.h"
#include "RecommendationManager.h"
#include "Components/BoxComponent.h"
#include "Traffic_AICarManagerComponent.h"

UTraffic_AutoDriving::UTraffic_AutoDriving()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UTraffic_AutoDriving::BeginPlay()
{
    Super::BeginPlay();

    if (!bAutoDriveEnabled) return;

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("[Auto Input] World is null"));
        return;
    }

    UGameInstance* GI = World->GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogTemp, Error, TEXT("[Auto Input] GameInstance is null"));
        return;
    }

    URecommendationManager* RecMgr = GI->GetSubsystem<URecommendationManager>();
    if (!RecMgr)
    {
        UE_LOG(LogTemp, Error, TEXT("[Auto Inputt] RecommendationManager not found"));
        return;
    }
    else
    {
        if (RecMgr->GetCurrentModeID() != 2)
        {
            UE_LOG(LogTemp, Log, TEXT("[Auto Input] Using Moza mode, Auto disabled."));
            return;
        }
    }

    bAutoDriveEnabled = true;
    OwnerPawn = Cast<ALvl_agent_demoPawn>(GetOwner());
    if (!OwnerPawn)
    {
        UE_LOG(LogTemp, Error, TEXT("[Traffic_AutoDriving] Owner is not ALvl_agent_demoPawn"));
        return;
    }

    VehicleMovement = Cast<UChaosWheeledVehicleMovementComponent>(OwnerPawn->GetVehicleMovement());
    if (!VehicleMovement)
    {
        UE_LOG(LogTemp, Error, TEXT("[Traffic_AutoDriving] Failed to get vehicle movement component"));
        return;
    }

    TrafficManager = UTraffic_AICarManagerComponent::GetTrafficManager(this);
}

void UTraffic_AutoDriving::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 递减 Teleport 保护计时器
    if (TeleportLockTimer > 0.f)
    {
        TeleportLockTimer = FMath::Max(0.f, TeleportLockTimer - DeltaTime);
        if (TeleportLockTimer <= 0.f)
        {
            TeleportExpectedLane = nullptr;
            UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] Teleport lock expired"));
        }
    }

    if (!bAutoDriveEnabled || !OwnerPawn || !VehicleMovement)
    {
        return;
    }

    if (!bGo)
    {
        ApplyStop();
        return;
    }

    OwnerPawn->DoHandbrakeStop();

    LaneChangeCooldownTimer = FMath::Max(0.f, LaneChangeCooldownTimer - DeltaTime);
    LaneChangeRequestCooldownTimer = FMath::Max(0.f, LaneChangeRequestCooldownTimer - DeltaTime);

    UpdateFrontDetection();
    UpdateSideDetection();
    UpdateLaneChangeState(DeltaTime);

    if (CurrentDrivingLane)
    {
        UpdateLaneIndexFromPosition();

        UpdateTurnState(DeltaTime);
        HandleLeftTurnLaneReservation();

        if (!bTurnRequested && !bTurnInProgress)
        {
            TryRequestLaneChange(DeltaTime);
        }

        UpdateSteering(DeltaTime);
    }
    else
    {
        OwnerPawn->DoSteering(0.f);
    }

    UpdateTargetSpeed(DeltaTime);
    ApplyControl();

    UE_LOG(LogTemp, Warning,
        TEXT("[AutoDrive] CurrentLaneIdx: %d, TargetLaneIdx: %d, InProgress=%d, Cooldown=%.2f, SideBlocked=%d SideDist=%.0f"),
        CurrentLaneIdx,
        DesiredLaneIdx,
        bLaneChangeInProgress ? 1 : 0,
        LaneChangeRequestCooldownTimer,
        bSideBlocked ? 1 : 0,
        SideDistance);
}

void UTraffic_AutoDriving::NotifyTeleportedToLane(AEnv_RoadLane* ExpectedLane)
{
    TeleportExpectedLane = ExpectedLane;
    TeleportLockTimer = TeleportLockDuration;

    UE_LOG(LogTemp, Warning,
        TEXT("[AutoDrive] NotifyTeleportedToLane: locking to %s for %.1f s"),
        ExpectedLane ? *ExpectedLane->GetName() : TEXT("None"),
        TeleportLockDuration);
}

void UTraffic_AutoDriving::SetCurrentLane(AEnv_RoadLane* Lane)
{
    // ── Teleport 保护 ─────────────────────────────────────────
    if (TeleportLockTimer > 0.f && IsValid(TeleportExpectedLane))
    {
        if (Lane != TeleportExpectedLane)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[AutoDrive] SetCurrentLane BLOCKED by teleport lock: rejected %s, expecting %s (%.2f s remaining)"),
                Lane ? *Lane->GetName() : TEXT("None"),
                *TeleportExpectedLane->GetName(),
                TeleportLockTimer);
            return;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] Turn SetCurrentLane called. Old=%s  New=%s"),
        CurrentDrivingLane ? *CurrentDrivingLane->GetName() : TEXT("None"),
        Lane ? *Lane->GetName() : TEXT("None"));

    AEnv_RoadLane* OldLane = CurrentDrivingLane;
    CurrentDrivingLane = Lane;

    if (!IsValid(CurrentDrivingLane))
    {
        FrontBlockingCar = nullptr;
        bFrontBlocked = false;
        FrontDistance = TNumericLimits<float>::Max();

        CurrentLaneIdx = 0;
        DesiredLaneIdx = 0;
        bLaneChangeInProgress = false;

        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] SetCurrentLane invalid -> reset to 0,0"));
        return;
    }

    const int32 NearestIdx = GetNearestLaneIndexOnCurrentLane();
    CurrentLaneIdx = NearestIdx;

    if (!IsValid(OldLane))
    {
        DesiredLaneIdx = NearestIdx;
        bLaneChangeInProgress = false;
    }
    else
    {
        float HalfY = 0.f;
        int32 NumLanes = 1;
        float LaneWidth = 1.f;
        if (GetLaneGeometry(CurrentDrivingLane, HalfY, NumLanes, LaneWidth))
        {
            DesiredLaneIdx = FMath::Clamp(DesiredLaneIdx, 0, NumLanes - 1);
        }
        else
        {
            DesiredLaneIdx = NearestIdx;
            bLaneChangeInProgress = false;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] SetCurrentLane nearest idx = %d, desired = %d"),
        CurrentLaneIdx, DesiredLaneIdx);
}

// ── 侧方持续检测 ──────────────────────────────────────────────
void UTraffic_AutoDriving::UpdateSideDetection()
{
    // 只在变道或转弯进行中时激活
    const bool bChangingLane = bLaneChangeInProgress || (DesiredLaneIdx != CurrentLaneIdx);
    const bool bActive = bChangingLane || bTurnInProgress;

    if (!bActive || !IsValid(OwnerPawn))
    {
        bSideBlocked = false;
        SideDistance = TNumericLimits<float>::Max();
        return;
    }

    bSideBlocked = false;
    SideDistance = TNumericLimits<float>::Max();

    const FVector Start = OwnerPawn->GetActorLocation() + FVector(0.f, 0.f, 50.f);

    // 转弯时用目标 lane 朝向；变道时用当前朝向
    FVector Forward;
    if (bTurnInProgress && IsValid(PendingTurnTargetLane) && PendingTurnTargetLane->Spawner)
    {
        Forward = PendingTurnTargetLane->Spawner->GetForwardVector().GetSafeNormal();
    }
    else
    {
        Forward = OwnerPawn->GetActorForwardVector().GetSafeNormal();
    }

    if (Forward.IsNearlyZero()) return;

    // 变道方向：向 DesiredLane 偏移的方向
    FVector ScanForward = Forward;
    if (bChangingLane && IsValid(CurrentDrivingLane))
    {
        const FVector DesiredCenter = GetLaneCenterPoint(CurrentDrivingLane, DesiredLaneIdx, 0.f);
        const FVector CurrentCenter = GetLaneCenterPoint(CurrentDrivingLane, CurrentLaneIdx, 0.f);
        FVector LateralDir = (DesiredCenter - CurrentCenter);
        LateralDir.Z = 0.f;
        if (!LateralDir.IsNearlyZero())
        {
            // 将扫描方向朝变道侧偏斜 30 度
            const FVector BiasDir = (Forward + LateralDir.GetSafeNormal() * 0.6f).GetSafeNormal();
            ScanForward = BiasDir;
        }
    }

    FHitResult BestHit;
    const bool bHit = TraceFanForAICar(
        Start,
        ScanForward,
        SideOngoingDetectDistance,
        SideOngoingDetectHalfDeg,
        SideOngoingDetectRayCount,
        BestHit,
        FColor::Orange);

    if (!bHit) return;

    AActor* HitActor = BestHit.GetActor();
    if (!IsValid(HitActor)) return;

    if (!Cast<ATraffic_AICar>(HitActor)) return;

    bSideBlocked = true;
    SideDistance = BestHit.Distance;

    if (bDrawDebug)
    {
        DrawDebugLine(
            GetWorld(),
            Start,
            BestHit.Location,
            FColor::Orange,
            false, 0.f, 0, 4.f);
        DrawDebugSphere(
            GetWorld(),
            BestHit.Location,
            40.f, 8,
            FColor::Orange,
            false, 0.f);
    }
}

bool UTraffic_AutoDriving::RequestLaneChange(int32 DeltaLaneIdx)
{
    if (!bUseSwitchLane) return false;

    if (!IsValid(CurrentDrivingLane) || !IsValid(OwnerPawn))
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] RequestLaneChange failed: invalid lane or owner"));
        return false;
    }

    if (DeltaLaneIdx != -1 && DeltaLaneIdx != 1)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[AutoDrive] RequestLaneChange failed: DeltaLaneIdx must be -1 or 1, got %d"),
            DeltaLaneIdx);
        return false;
    }

    if (bLaneChangeInProgress || DesiredLaneIdx != CurrentLaneIdx)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] RequestLaneChange rejected: already changing lane"));
        return false;
    }

    if (LaneChangeRequestCooldownTimer > 0.f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[AutoDrive] RequestLaneChange rejected: cooldown %.2f s remaining"),
            LaneChangeRequestCooldownTimer);
        return false;
    }

    float HalfY = 0.f;
    int32 NumLanes = 1;
    float LaneWidth = 1.f;
    if (!GetLaneGeometry(CurrentDrivingLane, HalfY, NumLanes, LaneWidth))
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] RequestLaneChange failed: GetLaneGeometry failed"));
        return false;
    }

    const int32 CandidateLaneIdx = CurrentLaneIdx + DeltaLaneIdx;

    if (CandidateLaneIdx < 0 || CandidateLaneIdx >= NumLanes)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[AutoDrive] RequestLaneChange rejected: target lane out of range, current=%d delta=%d num=%d"),
            CurrentLaneIdx, DeltaLaneIdx, NumLanes);
        return false;
    }

    if (IsLaneOccupied(CandidateLaneIdx))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[AutoDrive] RequestLaneChange rejected: target lane %d occupied"),
            CandidateLaneIdx);
        return false;
    }

    DesiredLaneIdx = CandidateLaneIdx;
    bLaneChangeInProgress = true;
    bForcedLaneChange = false;         // ← 普通变道
    LaneChangeCooldownTimer = LaneChangeCooldown;

    UE_LOG(LogTemp, Warning,
        TEXT("[AutoDrive] RequestLaneChange ACCEPTED: current=%d target=%d"),
        CurrentLaneIdx, DesiredLaneIdx);

    return true;
}

void UTraffic_AutoDriving::UpdateFrontDetection()
{
    FrontBlockingCar = nullptr;
    bFrontBlocked = false;
    FrontDistance = TNumericLimits<float>::Max();

    if (!IsValid(OwnerPawn))
    {
        return;
    }

    const FVector Start = OwnerPawn->GetActorLocation() + FVector(0.f, 0.f, 50.f);

    FVector Forward;
    if (bTurnInProgress && IsValid(PendingTurnTargetLane) && PendingTurnTargetLane->Spawner)
    {
        Forward = PendingTurnTargetLane->Spawner->GetForwardVector().GetSafeNormal();
    }
    else
    {
        Forward = OwnerPawn->GetActorForwardVector().GetSafeNormal();
    }

    if (Forward.IsNearlyZero())
    {
        return;
    }

    FHitResult BestHit;
    const bool bHit = TraceFanForAICar(
        Start,
        Forward,
        ForwardDetectDistance,
        FrontDetectHalfDeg,
        FrontDetectRayCount,
        BestHit,
        FColor::Green);

    if (!bHit)
    {
        return;
    }

    AActor* HitActor = BestHit.GetActor();
    if (!IsValid(HitActor))
    {
        return;
    }

    ATraffic_AICar* HitCar = Cast<ATraffic_AICar>(HitActor);
    if (!HitCar)
    {
        return;
    }

    FrontBlockingCar = HitCar;
    bFrontBlocked = true;
    FrontDistance = BestHit.Distance;

    if (bDrawDebug)
    {
        DrawDebugLine(
            GetWorld(),
            Start,
            BestHit.Location,
            FColor::Blue,
            false,
            0.f,
            0,
            3.f);
    }
}

void UTraffic_AutoDriving::UpdateLaneIndexFromPosition()
{
    if (!CurrentDrivingLane) return;

    const int32 NearestIdx = GetNearestLaneIndexOnCurrentLane();

    if (!bLaneChangeInProgress)
    {
        CurrentLaneIdx = NearestIdx;

        if (bUseSwitchLane)
        {
            // 避让模式下，不允许 DesiredLaneIdx 被重置回 lane 0
            const bool bAvoidLane0 = ShouldAvoidLeftTurnLane();
            if (bAvoidLane0 && NearestIdx == 0)
            {
                // 保持 DesiredLaneIdx 不变，让 HandleLeftTurnLaneReservation 去处理
            }
            else
            {
                DesiredLaneIdx = CurrentLaneIdx;
            }
        }
        return;
    }

    const FVector DesiredCenter = GetLaneCenterPoint(CurrentDrivingLane, DesiredLaneIdx, 0.f);
    const FVector PawnPos = OwnerPawn->GetActorLocation();
    const FVector LaneRight = CurrentDrivingLane->Spawner
        ? CurrentDrivingLane->Spawner->GetRightVector().GetSafeNormal()
        : CurrentDrivingLane->GetActorRightVector().GetSafeNormal();

    const float LateralErr = FMath::Abs(FVector::DotProduct(PawnPos - DesiredCenter, LaneRight));

    if (LateralErr < LaneChangeCompleteTolerance)
    {
        CurrentLaneIdx = DesiredLaneIdx;
    }
}

void UTraffic_AutoDriving::TryRequestLaneChange(float DeltaTime)
{
    if (!bUseSwitchLane)
    {
        DesiredLaneIdx = CurrentLaneIdx;
        return;
    }

    if (!IsValid(CurrentDrivingLane) || !IsValid(OwnerPawn))
    {
        return;
    }

    if (bLaneChangeInProgress)
    {
        return;
    }

    if (LaneChangeRequestCooldownTimer > 0.f)
    {
        return;
    }

    if (LaneChangeCooldownTimer > 0.f)
    {
        return;
    }

    float HalfY = 0.f;
    int32 NumLanes = 1;
    float LaneWidth = 1.f;
    if (!GetLaneGeometry(CurrentDrivingLane, HalfY, NumLanes, LaneWidth))
    {
        return;
    }

    const bool bAvoidLane0 = ShouldAvoidLeftTurnLane() && NumLanes > 1;

    if (!bFrontBlocked)
    {
        if (!(bAvoidLane0 && CurrentLaneIdx == 0))
        {
            DesiredLaneIdx = CurrentLaneIdx;
        }
        return;
    }

    if (DesiredLaneIdx != CurrentLaneIdx)
    {
        return;
    }

    const bool bHasLeftLane = (CurrentLaneIdx > 0);
    const bool bLeftAllowed = bHasLeftLane && !(bAvoidLane0 && (CurrentLaneIdx - 1) == 0);
    const bool bHasRightLane = (CurrentLaneIdx < NumLanes - 1);

    const bool bLeftFree = bLeftAllowed ? !IsLaneOccupied(CurrentLaneIdx - 1) : false;
    const bool bRightFree = bHasRightLane ? !IsLaneOccupied(CurrentLaneIdx + 1) : false;

    int32 NewDesiredLaneIdx = CurrentLaneIdx;

    if (bLeftFree && bRightFree)
    {
        NewDesiredLaneIdx = FMath::RandBool()
            ? (CurrentLaneIdx - 1)
            : (CurrentLaneIdx + 1);
    }
    else if (bLeftFree)
    {
        NewDesiredLaneIdx = CurrentLaneIdx - 1;
    }
    else if (bRightFree)
    {
        NewDesiredLaneIdx = CurrentLaneIdx + 1;
    }
    else
    {
        DesiredLaneIdx = CurrentLaneIdx;
        return;
    }

    if (NewDesiredLaneIdx != CurrentLaneIdx)
    {
        const int32 DeltaLaneIdx = NewDesiredLaneIdx - CurrentLaneIdx;
        RequestLaneChange(DeltaLaneIdx);
    }
}

void UTraffic_AutoDriving::UpdateLaneChangeState(float DeltaTime)
{
    if (!IsValid(CurrentDrivingLane) || !IsValid(OwnerPawn))
    {
        bLaneChangeInProgress = false;
        bForcedLaneChange = false;
        return;
    }

    if (bTurnInProgress)
    {
        bLaneChangeInProgress = false;
        bForcedLaneChange = false;
        return;
    }

    if (!bLaneChangeInProgress)
    {
        return;
    }

    const FVector DesiredCenter = GetLaneCenterPoint(CurrentDrivingLane, DesiredLaneIdx, 0.f);
    const FVector PawnPos = OwnerPawn->GetActorLocation();
    const FVector LaneRight = CurrentDrivingLane->Spawner
        ? CurrentDrivingLane->Spawner->GetRightVector().GetSafeNormal()
        : CurrentDrivingLane->GetActorRightVector().GetSafeNormal();

    const float LateralErr = FMath::Abs(FVector::DotProduct(PawnPos - DesiredCenter, LaneRight));

    if (LateralErr <= LaneChangeCompleteTolerance)
    {
        CurrentLaneIdx = DesiredLaneIdx;
        bLaneChangeInProgress = false;
        bForcedLaneChange = false;

        if (!bTurnRequested)
        {
            LaneChangeRequestCooldownTimer = LaneChangeRequestCooldown;
        }

        UE_LOG(LogTemp, Warning,
            TEXT("[AutoDrive] Lane change completed. Current=%d, cooldown=%.2f"),
            CurrentLaneIdx,
            LaneChangeRequestCooldown);
    }
}

void UTraffic_AutoDriving::UpdateTargetSpeed(float DeltaTime)
{
    const float CurrentSpeed = VehicleMovement
        ? FMath::Abs(VehicleMovement->GetForwardSpeed())
        : 0.f;

    float TargetSpeed = CruiseSpeed;

    if (bFrontBlocked)
    {
        if (FrontDistance <= StopDistance)
        {
            TargetSpeed = 0.f;
        }
        else if (FrontDistance <= SlowDistance)
        {
            const float Ratio = FMath::Clamp(
                (FrontDistance - StopDistance) / FMath::Max(1.f, SlowDistance - StopDistance),
                0.f, 1.f);
            TargetSpeed = CruiseSpeed * Ratio;
        }
    }

    if (bSideBlocked)
    {
        float SideTargetSpeed = CruiseSpeed;

        if (SideDistance <= SideStopDistance)
        {
            SideTargetSpeed = 0.f;
        }
        else if (SideDistance <= SideSlowDistance)
        {
            const float Ratio = FMath::Clamp(
                (SideDistance - SideStopDistance) / FMath::Max(1.f, SideSlowDistance - SideStopDistance),
                0.f, 1.f);
            SideTargetSpeed = CruiseSpeed * Ratio;
        }

        TargetSpeed = FMath::Min(TargetSpeed, SideTargetSpeed);
    }

    const bool bChangingLane = bLaneChangeInProgress || (DesiredLaneIdx != CurrentLaneIdx);
    const bool bFrontTooClose = bFrontBlocked && (FrontDistance < SlowDistance * 0.6f);

    if (bChangingLane && !bFrontTooClose)
    {
        TargetSpeed = FMath::Clamp(TargetSpeed, 0.f, 800.f);
    }

    const float SpeedError = TargetSpeed - CurrentSpeed;

    ThrottleCmd = 0.f;
    BrakeCmd = 0.f;
    bHandbrakeCmd = false;

    if (SpeedError > SpeedDeadband)
    {
        ThrottleCmd = FMath::Clamp(SpeedError * KpThrottle, 0.f, 1.f);
    }
    else if (SpeedError < -SpeedDeadband)
    {
        BrakeCmd = FMath::Clamp((-SpeedError) * KpBrake, 0.f, 1.f);
    }

    if (TargetSpeed <= 1.f && (FrontDistance < StopDistance * 0.7f || SideDistance < SideStopDistance * 0.7f))
    {
        ThrottleCmd = 0.f;
        BrakeCmd = 1.f;
        bHandbrakeCmd = true;
    }

    if (BrakeCmd > 0.f)
    {
        ThrottleCmd = 0.f;
    }
}

void UTraffic_AutoDriving::UpdateSteering(float DeltaTime)
{
    if (!IsValid(CurrentDrivingLane) || !IsValid(OwnerPawn)) return;

    const FVector PawnPos = OwnerPawn->GetActorLocation();
    const FVector PawnFwd = OwnerPawn->GetActorForwardVector().GetSafeNormal();
    const FVector Up = OwnerPawn->GetActorUpVector().GetSafeNormal();
    const FVector LaneFwd = CurrentDrivingLane->Spawner
        ? CurrentDrivingLane->Spawner->GetForwardVector().GetSafeNormal()
        : CurrentDrivingLane->GetActorForwardVector().GetSafeNormal();

    const bool bChangingLane = bLaneChangeInProgress || (DesiredLaneIdx != CurrentLaneIdx);

    if (!bTurnRequested && !bTurnInProgress)
    {
        if (!bForcedLaneChange)
        {
            UpdateLaneChangeBlockState(DeltaTime);
        }
        else
        {
            bLaneChangeBlocked = false;
            LaneChangeBlockedTimer = 0.f;
        }
    }
    else
    {
        bLaneChangeBlocked = false;
        LaneChangeBlockedTimer = 0.f;
    }

    FVector TargetPoint = FVector::ZeroVector;
    float LookAhead = SteeringLookAhead;

    if (bTurnInProgress && IsValid(PendingTurnTargetLane))
    {
        LookAhead = TurnSteeringLookAhead;
        const int32 CenterIdx = GetCenterLaneIndex(PendingTurnTargetLane);
        TargetPoint = GetLaneCenterPoint(PendingTurnTargetLane, CenterIdx, LookAhead);
    }
    else
    {
        LookAhead = bChangingLane ? LaneChangeSteeringLookAhead : SteeringLookAhead;

        const int32 SteeringLaneIdx =
            (bLaneChangeInProgress && bLaneChangeBlocked)
            ? CurrentLaneIdx
            : DesiredLaneIdx;

        TargetPoint = GetLaneCenterPoint(CurrentDrivingLane, SteeringLaneIdx, LookAhead);
    }

    FVector ToTarget = (TargetPoint - PawnPos);
    ToTarget.Z = 0.f;
    ToTarget = ToTarget.GetSafeNormal();

    if (ToTarget.IsNearlyZero())
    {
        OwnerPawn->DoSteering(0.f);
        return;
    }

    const float DotTarget = FVector::DotProduct(PawnFwd, ToTarget);
    const float CrossTargetZ = FVector::DotProduct(FVector::CrossProduct(PawnFwd, ToTarget), Up);
    const float TargetAngle = FMath::Atan2(CrossTargetZ, DotTarget);

    const float DotHeading = FVector::DotProduct(PawnFwd, LaneFwd);
    const float CrossHeadingZ = FVector::DotProduct(FVector::CrossProduct(PawnFwd, LaneFwd), Up);
    const float HeadingAngle = FMath::Atan2(CrossHeadingZ, DotHeading);

    float Steering = 0.f;

    if (bChangingLane || bTurnInProgress)
    {
        Steering =
            10.0f * TargetAngle +
            0.3f * HeadingAngle;
    }
    else
    {
        Steering =
            SteeringTargetGain * TargetAngle +
            SteeringHeadingGain * HeadingAngle;
    }

    Steering = FMath::Clamp(Steering, -1.f, 1.f);

    OwnerPawn->DoSteering(Steering);

    if (bDrawDebug)
    {
        DrawDebugSphere(GetWorld(), TargetPoint, 30.f, 12, FColor::Cyan, false, 0.f);
        DrawDebugLine(GetWorld(), PawnPos + FVector(0, 0, 40), TargetPoint, FColor::Blue, false, 0.f, 0, 2.f);
    }
}

void UTraffic_AutoDriving::ApplyControl()
{
    if (!VehicleMovement)
    {
        return;
    }

    VehicleMovement->SetThrottleInput(FMath::Clamp(ThrottleCmd, 0.f, 1.f));
    VehicleMovement->SetBrakeInput(FMath::Clamp(BrakeCmd, 0.f, 1.f));
    VehicleMovement->SetHandbrakeInput(bHandbrakeCmd);
}

bool UTraffic_AutoDriving::TraceForAICar(
    const FVector& Start,
    const FVector& End,
    FHitResult& OutHit) const
{
    if (!GetWorld() || !OwnerPawn) return false;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(TrafficAutoDriveTrace), false);
    Params.AddIgnoredActor(OwnerPawn);

    TArray<FHitResult> Hits;
    const bool bHitAnything = GetWorld()->LineTraceMultiByChannel(
        Hits,
        Start,
        End,
        ECC_Vehicle,
        Params);

    if (!bHitAnything) return false;

    float BestDist = TNumericLimits<float>::Max();
    bool bFound = false;

    for (const FHitResult& Hit : Hits)
    {
        AActor* A = Hit.GetActor();
        if (!A) continue;

        ATraffic_AICar* AICar = Cast<ATraffic_AICar>(A);
        if (!AICar) continue;

        const FVector Dir = (A->GetActorLocation() - Start).GetSafeNormal();
        const float ForwardDot = FVector::DotProduct(OwnerPawn->GetActorForwardVector(), Dir);
        if (ForwardDot < 0.35f) continue;

        if (Hit.Distance < BestDist)
        {
            BestDist = Hit.Distance;
            OutHit = Hit;
            bFound = true;
        }
    }

    return bFound;
}

bool UTraffic_AutoDriving::IsLaneOccupied(int32 CandidateLaneIdx) const
{
    UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] >>> IsLaneOccupied start, Candidate=%d"), CandidateLaneIdx);

    if (!IsValid(CurrentDrivingLane) || !IsValid(OwnerPawn))
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] IsLaneOccupied early return: invalid lane or owner"));
        return true;
    }

    float HalfY = 0.f;
    int32 NumLanes = 1;
    float LaneWidth = 1.f;
    if (!GetLaneGeometry(CurrentDrivingLane, HalfY, NumLanes, LaneWidth))
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] IsLaneOccupied early return: GetLaneGeometry failed"));
        return true;
    }

    if (CandidateLaneIdx < 0 || CandidateLaneIdx >= NumLanes)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] IsLaneOccupied early return: invalid candidate idx"));
        return true;
    }

    const FVector ZOffset(0.f, 0.f, 40.f);

    const FVector PawnPos = OwnerPawn->GetActorLocation();
    const FVector LaneFwd = CurrentDrivingLane->Spawner
        ? CurrentDrivingLane->Spawner->GetForwardVector().GetSafeNormal()
        : CurrentDrivingLane->GetActorForwardVector().GetSafeNormal();

    if (LaneFwd.IsNearlyZero())
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] IsLaneOccupied early return: LaneFwd zero"));
        return true;
    }

    const FVector CandidateCenter = GetLaneCenterPoint(CurrentDrivingLane, CandidateLaneIdx, 0.f);

    FVector SideVec = CandidateCenter - PawnPos;
    SideVec.Z = 0.f;

    const float CandidateLateralDist = SideVec.Size2D();
    const FVector SideDir = SideVec.GetSafeNormal();

    if (bDrawDebug)
    {
        DrawDebugSphere(GetWorld(), CandidateCenter + ZOffset, 35.f, 12, FColor::Yellow, false, 1.0f);

        DrawDebugDirectionalArrow(
            GetWorld(),
            CandidateCenter + ZOffset,
            CandidateCenter + ZOffset + LaneFwd * 300.f,
            60.f,
            FColor::Blue,
            false,
            1.0f,
            0,
            3.f);

        DrawDebugDirectionalArrow(
            GetWorld(),
            CandidateCenter + ZOffset,
            CandidateCenter + ZOffset - LaneFwd * 300.f,
            60.f,
            FColor::Red,
            false,
            1.0f,
            0,
            3.f);

        if (!SideDir.IsNearlyZero())
        {
            DrawDebugDirectionalArrow(
                GetWorld(),
                PawnPos + ZOffset,
                PawnPos + ZOffset + SideDir * FMath::Min(CandidateLateralDist, 300.f),
                60.f,
                FColor::Cyan,
                false,
                1.0f,
                0,
                3.f);
        }
    }

    FHitResult DummyHit;

    // 1) 目标车道前方
    if (TraceFanForAICar(
        CandidateCenter + ZOffset,
        LaneFwd,
        SideCheckForwardDistance,
        SideFrontDetectHalfDeg,
        SideFrontDetectRayCount,
        DummyHit,
        FColor::Blue))
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] Candidate lane occupied at FRONT"));
        return true;
    }

    // 2) 目标车道后方
    if (TraceFanForAICar(
        CandidateCenter + ZOffset,
        -LaneFwd,
        SideCheckBackwardDistance,
        RearDetectHalfDeg,
        RearDetectRayCount,
        DummyHit,
        FColor::Red))
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] Candidate lane occupied at REAR"));
        return true;
    }

    // 3) 本车切向目标车道横向区域
    if (!SideDir.IsNearlyZero() && CandidateLateralDist > 50.f)
    {
        const float CrossCheckDist = FMath::Max(SideCrossCheckDistance, CandidateLateralDist);

        if (TraceFanForAICar(
            PawnPos + ZOffset,
            SideDir,
            CrossCheckDist,
            SideDetectHalfDeg,
            SideDetectRayCount,
            DummyHit,
            FColor::Cyan))
        {
            UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] Candidate lane occupied at SIDE"));
            return true;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] Candidate lane is FREE"));
    return false;
}

bool UTraffic_AutoDriving::IsFrontBlockedInCurrentLane() const
{
    return bFrontBlocked;
}

bool UTraffic_AutoDriving::GetLaneGeometry(
    const AEnv_RoadLane* Lane,
    float& OutHalfY,
    int32& OutNumLanes,
    float& OutLaneWidth) const
{
    if (!Lane) return false;
    if (!Lane->Spawner) return false;

    const int32 NumLanesLocal = FMath::Max(1, Lane->LaneNumber);

    const FVector Ext = Lane->Spawner->GetUnscaledBoxExtent();
    const float HalfY = Ext.Y;

    OutHalfY = HalfY;
    OutNumLanes = NumLanesLocal;
    OutLaneWidth = (HalfY * 2.f) / static_cast<float>(NumLanesLocal);

    return true;
}

FVector UTraffic_AutoDriving::GetLaneCenterPoint(
    const AEnv_RoadLane* Lane,
    int32 LaneIdx,
    float ForwardOffset) const
{
    if (!Lane || !OwnerPawn)
    {
        return FVector::ZeroVector;
    }

    float HalfY = 0.f;
    int32 NumLanes = 1;
    float LaneWidth = 1.f;
    if (!GetLaneGeometry(Lane, HalfY, NumLanes, LaneWidth))
    {
        return OwnerPawn->GetActorLocation();
    }

    LaneIdx = FMath::Clamp(LaneIdx, 0, NumLanes - 1);

    if (!Lane->Spawner)
    {
        return OwnerPawn->GetActorLocation();
    }

    const FVector LaneOrigin = Lane->Spawner->GetComponentLocation();
    const FVector LaneFwd = Lane->Spawner->GetForwardVector().GetSafeNormal();
    const FVector LaneRight = Lane->Spawner->GetRightVector().GetSafeNormal();

    const FVector PawnPos = OwnerPawn->GetActorLocation();

    const float Along = FVector::DotProduct(PawnPos - LaneOrigin, LaneFwd);

    const float LateralOffset =
        -HalfY + LaneWidth * (static_cast<float>(LaneIdx) + 0.5f);

    return LaneOrigin
        + LaneFwd * (Along + ForwardOffset)
        + LaneRight * LateralOffset;
}

int32 UTraffic_AutoDriving::GetNearestLaneIndexOnCurrentLane() const
{
    if (!CurrentDrivingLane || !OwnerPawn || !CurrentDrivingLane->Spawner)
    {
        return 0;
    }

    float HalfY = 0.f;
    int32 NumLanes = 1;
    float LaneWidth = 1.f;
    if (!GetLaneGeometry(CurrentDrivingLane, HalfY, NumLanes, LaneWidth))
    {
        return 0;
    }

    const FVector Origin = CurrentDrivingLane->Spawner->GetComponentLocation();
    const FVector Right = CurrentDrivingLane->Spawner->GetRightVector().GetSafeNormal();
    const FVector PawnPos = OwnerPawn->GetActorLocation();

    const float Lateral = FVector::DotProduct(PawnPos - Origin, Right);

    UE_LOG(LogTemp, Warning,
        TEXT("[AutoDrive] Lateral: %.1f  HalfY: %.1f  LaneWidth: %.1f"),
        Lateral, HalfY, LaneWidth);

    int32 BestIdx = 0;
    float BestAbs = TNumericLimits<float>::Max();

    for (int32 i = 0; i < NumLanes; ++i)
    {
        const float CenterOffset = -HalfY + LaneWidth * (static_cast<float>(i) + 0.5f);
        const float Err = FMath::Abs(Lateral - CenterOffset);

        if (Err < BestAbs)
        {
            BestAbs = Err;
            BestIdx = i;
        }
    }

    return BestIdx;
}

bool UTraffic_AutoDriving::TraceFanForAICar(
    const FVector& Start,
    const FVector& Forward,
    float Distance,
    float HalfDeg,
    int32 RayCount,
    FHitResult& OutBestHit,
    FColor DebugColor) const
{
    OutBestHit = FHitResult();

    if (!GetWorld() || !IsValid(OwnerPawn))
    {
        return false;
    }

    const FVector BaseForward = Forward.GetSafeNormal();
    if (BaseForward.IsNearlyZero())
    {
        return false;
    }

    Distance = FMath::Max(1.f, Distance);
    HalfDeg = FMath::Max(0.f, HalfDeg);
    RayCount = FMath::Max(1, RayCount);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(TrafficAutoDriveFanTrace), false);
    Params.AddIgnoredActor(OwnerPawn);

    const FVector Up = FVector::UpVector;

    bool bFound = false;
    float BestDist = TNumericLimits<float>::Max();

    for (int32 i = 0; i < RayCount; ++i)
    {
        const float Alpha = (RayCount == 1)
            ? 0.5f
            : static_cast<float>(i) / static_cast<float>(RayCount - 1);

        const float AngleDeg = FMath::Lerp(-HalfDeg, HalfDeg, Alpha);
        const FVector Dir = BaseForward.RotateAngleAxis(AngleDeg, Up).GetSafeNormal();

        if (Dir.IsNearlyZero())
        {
            continue;
        }

        const FVector End = Start + Dir * Distance;

        if (bDrawDebug)
        {
            DrawDebugLine(
                GetWorld(),
                Start,
                End,
                DebugColor,
                false,
                0.f,
                0,
                1.5f);
        }

        TArray<FHitResult> Hits;
        const bool bHitAnything = GetWorld()->LineTraceMultiByChannel(
            Hits,
            Start,
            End,
            ECC_Vehicle,
            Params);

        if (!bHitAnything)
        {
            continue;
        }

        for (const FHitResult& Hit : Hits)
        {
            AActor* HitActor = Hit.GetActor();
            if (!IsValid(HitActor))
            {
                continue;
            }

            if (HitActor == OwnerPawn)
            {
                continue;
            }

            ATraffic_AICar* AICar = Cast<ATraffic_AICar>(HitActor);
            if (!AICar)
            {
                continue;
            }

            if (Hit.Distance < BestDist)
            {
                BestDist = Hit.Distance;
                OutBestHit = Hit;
                bFound = true;
            }
        }
    }

    return bFound;
}

bool UTraffic_AutoDriving::IsFanRegionOccupiedByAICar(
    const FVector& Start,
    const FVector& Forward,
    float Distance,
    float HalfDeg,
    int32 RayCount) const
{
    FHitResult DummyHit;
    return TraceFanForAICar(Start, Forward, Distance, HalfDeg, RayCount, DummyHit, FColor::Green);
}

bool UTraffic_AutoDriving::IsLaneChangePathBlocked(int32 TargetLaneIdx) const
{
    if (!IsValid(CurrentDrivingLane) || !IsValid(OwnerPawn) || !TrafficManager)
    {
        return false;
    }

    float HalfY = 0.f;
    int32 NumLanes = 1;
    float LaneWidth = 1.f;
    if (!GetLaneGeometry(CurrentDrivingLane, HalfY, NumLanes, LaneWidth))
    {
        return false;
    }

    const FVector LaneOrigin = CurrentDrivingLane->GetActorLocation();
    const FVector LaneFwd = CurrentDrivingLane->GetActorForwardVector().GetSafeNormal();
    const FVector LaneRight = CurrentDrivingLane->GetActorRightVector().GetSafeNormal();

    const FVector MyPos = OwnerPawn->GetActorLocation();
    const float MyAlong = FVector::DotProduct(MyPos - LaneOrigin, LaneFwd);

    const float TargetLaneCenterOffset =
        -HalfY + LaneWidth * (0.5f + TargetLaneIdx);

    TArray<ATraffic_AICar*> Cars;

    if (TrafficManager)
    {
        TrafficManager->GetAliveCars(Cars);
    }

    if (Cars.Num() == 0)
    {
        return false;
	}

    for (ATraffic_AICar* OtherCar : Cars)
    {
        if (!IsValid(OtherCar) || OtherCar == OwnerPawn)
        {
            continue;
        }

        const FVector OtherPos = OtherCar->GetActorLocation();

        const float OtherAlong = FVector::DotProduct(OtherPos - LaneOrigin, LaneFwd);
        const float AlongDelta = OtherAlong - MyAlong;

        if (AlongDelta > LaneChangeSideFrontCheckDist ||
            AlongDelta < -LaneChangeSideRearCheckDist)
        {
            continue;
        }

        const float OtherLateral = FVector::DotProduct(OtherPos - LaneOrigin, LaneRight);
        const float LateralDelta = FMath::Abs(OtherLateral - TargetLaneCenterOffset);

        if (LateralDelta <= LaneWidth * LaneChangeLaneTolerance)
        {
            return true;
        }
    }

    return false;
}

void UTraffic_AutoDriving::UpdateLaneChangeBlockState(float DeltaTime)
{
    if (!bLaneChangeInProgress || DesiredLaneIdx == CurrentLaneIdx)
    {
        bLaneChangeBlocked = false;
        LaneChangeBlockedTimer = 0.f;
        return;
    }

    bLaneChangeBlocked = IsLaneChangePathBlocked(DesiredLaneIdx);

    if (bLaneChangeBlocked)
    {
        LaneChangeBlockedTimer += DeltaTime;

        if (LaneChangeBlockedTimer >= MaxLaneChangeBlockedTime)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[AutoDrive] Lane change canceled: blocked too long, back to current lane %d"),
                CurrentLaneIdx);

            DesiredLaneIdx = CurrentLaneIdx;
            bLaneChangeInProgress = false;
            bLaneChangeBlocked = false;
            LaneChangeBlockedTimer = 0.f;
        }
    }
    else
    {
        LaneChangeBlockedTimer = 0.f;
    }
}

void UTraffic_AutoDriving::SetGo(bool bInGo)
{
	bGo = bInGo;
}

void UTraffic_AutoDriving::ApplyStop()
{
    OwnerPawn->DoThrottle(0.f);
    OwnerPawn->DoBrake(1.f);
    OwnerPawn->DoHandbrakeStart();
}

void UTraffic_AutoDriving::TurnAtIntersection(float TurnDir)
{
    if (!IsValid(CurrentDrivingLane) || !IsValid(OwnerPawn))
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] TurnAtIntersection rejected: invalid lane or owner"));
        return;
    }

    // 如果已过 SafeTurnDist，只有在 BrakingArea 内停车等灯的情况下仍允许注册
    // （Pawn 停在路口但还没真正通过路口）
    if (HasPassedSafeTurnDist() && !IsInCurrentLaneBrakingArea())
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] TurnAtIntersection ignored: already passed SafeTurnDist and not in braking area"));
        return;
    }

    if (TurnDir == 0.f)
    {
        ClearTurnRequest();
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] TurnAtIntersection canceled"));
        return;
    }

    if (bTurnInProgress)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] TurnAtIntersection rejected: turn already in progress"));
        return;
    }

    if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(TurnCooldownTimerHandle))
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] TurnAtIntersection rejected: in turn cooldown"));
        return;
    }

    const float Sign = (TurnDir > 0.f) ? 1.f : ((TurnDir < 0.f) ? -1.f : 0.f);
    if (Sign == 0.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] TurnAtIntersection rejected: TurnDir must be -1 or 1"));
        return;
    }

    AEnv_RoadLane* TargetLane = (Sign < 0.f)
        ? CurrentDrivingLane->LeftTurnTargetLane
        : CurrentDrivingLane->RightTurnTargetLane;

    if (!IsValid(TargetLane))
    {
        UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] TurnAtIntersection rejected: target lane is null"));
        return;
    }

    PendingTurnDir = Sign;
    PendingTurnTargetLane = TargetLane;
    bTurnRequested = true;
    bTurnInProgress = false;

    LaneChangeRequestCooldownTimer = 0.f;
    LaneChangeCooldownTimer = 0.f;

    UE_LOG(LogTemp, Warning,
        TEXT("[AutoDrive] Turn requested: Dir=%s TargetLane=%s"),
        (PendingTurnDir < 0.f ? TEXT("LEFT") : TEXT("RIGHT")),
        PendingTurnTargetLane ? *PendingTurnTargetLane->GetName() : TEXT("None"));
}

bool UTraffic_AutoDriving::HasPassedSafeTurnDist() const
{
    if (!IsValid(CurrentDrivingLane) || !IsValid(OwnerPawn) || !CurrentDrivingLane->Spawner)
    {
        return true;
    }

    const FVector LaneOrigin = CurrentDrivingLane->Spawner->GetComponentLocation();
    const FVector LaneFwd = CurrentDrivingLane->Spawner->GetForwardVector().GetSafeNormal();
    const FVector PawnPos = OwnerPawn->GetActorLocation();

    const float Along = FVector::DotProduct(PawnPos - LaneOrigin, LaneFwd);

    return Along > CurrentDrivingLane->SafeTurnDist;
}

bool UTraffic_AutoDriving::IsInCurrentLaneBrakingArea() const
{
    if (!IsValid(CurrentDrivingLane) || !IsValid(OwnerPawn))
    {
        return false;
    }

    if (!IsValid(CurrentDrivingLane->TurnArea))
    {
        return false;
    }

    return CurrentDrivingLane->TurnArea->IsOverlappingActor(OwnerPawn);
}

int32 UTraffic_AutoDriving::GetCenterLaneIndex(const AEnv_RoadLane* Lane) const
{
    if (!Lane)
    {
        return 0;
    }

    const int32 NumLanes = FMath::Max(1, Lane->LaneNumber);
    return NumLanes / 2;
}

void UTraffic_AutoDriving::ClearTurnRequest()
{
    bTurnRequested = false;
    bTurnInProgress = false;
    PendingTurnDir = 0.f;
    PendingTurnTargetLane = nullptr;
    TurnStartLane = nullptr;
	UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] Turn request cleared"));
}

void UTraffic_AutoDriving::FinishIntersectionTurn()
{
    bTurnRequested = false;
    bTurnInProgress = false;
    PendingTurnDir = 0.f;
    PendingTurnTargetLane = nullptr;

    bLaneChangeInProgress = false;
    bLaneChangeBlocked = false;
    LaneChangeBlockedTimer = 0.f;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            TurnCooldownTimerHandle,
            []() {},
            TurnCooldown,
            false);
    }

    UE_LOG(LogTemp, Warning, TEXT("[AutoDrive] Turn finished. Lane change logic re-enabled."));
}

void UTraffic_AutoDriving::UpdateTurnState(float DeltaTime)
{
    if (!IsValid(CurrentDrivingLane) || !IsValid(OwnerPawn))
    {
        ClearTurnRequest();
        return;
    }

    if (!bTurnRequested && !bTurnInProgress)
    {
        return;
    }

    // 如果只是请求了，但还没开始实际转弯：
    if (bTurnRequested && !bTurnInProgress)
    {
        float HalfY = 0.f;
        int32 NumLanes = 1;
        float LaneWidth = 1.f;
        if (!GetLaneGeometry(CurrentDrivingLane, HalfY, NumLanes, LaneWidth))
        {
            ClearTurnRequest();
            return;
        }

        const int32 PrepLaneIdx = (PendingTurnDir < 0.f) ? 0 : (NumLanes - 1);

        // 强制切到准备车道（不受 bUseSwitchLane 影响）
        if (CurrentLaneIdx != PrepLaneIdx || DesiredLaneIdx != PrepLaneIdx)
        {
            ForceRequestLaneChange(PrepLaneIdx);
        }

        if (bGo && IsInCurrentLaneBrakingArea())
        {
            UE_LOG(LogTemp, Display, TEXT("[AutoDrive] Turn hit go and braking"))
            if (CurrentLaneIdx == PrepLaneIdx)
            {
                bTurnInProgress = true;
                bLaneChangeInProgress = false;
                TurnStartLane = CurrentDrivingLane;

                UE_LOG(LogTemp, Warning,
                    TEXT("[AutoDrive] Turn started: Dir=%s, TargetLane=%s"),
                    (PendingTurnDir < 0.f ? TEXT("LEFT") : TEXT("RIGHT")),
                    PendingTurnTargetLane ? *PendingTurnTargetLane->GetName() : TEXT("None"));
            }
        }

        return;
    }

    // 已经开始实际转弯：
    if (bTurnInProgress)
    {
        if (!IsValid(PendingTurnTargetLane))
        {
            ClearTurnRequest();
            return;
        }

        // 必须等 CurrentDrivingLane 切换到目标 lane 的轴向后，才检查朝向对齐
        // 这样可以防止刚起步时 CurrentDrivingLane 还是原 lane，朝向已对齐就被误判结束
        if (CurrentDrivingLane == TurnStartLane)
        {
            // 还没离开起始 lane，继续转向目标 lane，不做结束判断
            return;
        }

        if (CurrentDrivingLane->LaneAxis == PendingTurnTargetLane->LaneAxis)
        {
            const FVector PawnFwd = OwnerPawn->GetActorForwardVector().GetSafeNormal();
            const FVector LaneFwd = CurrentDrivingLane->Spawner
                ? CurrentDrivingLane->Spawner->GetForwardVector().GetSafeNormal()
                : CurrentDrivingLane->GetActorForwardVector().GetSafeNormal();

            const float Dot = FVector::DotProduct(PawnFwd, LaneFwd);
            const float AngleDeg = FMath::RadiansToDegrees(
                FMath::Acos(FMath::Clamp(Dot, -1.f, 1.f)));

            if (AngleDeg <= TurnForwardAlignToleranceDeg)
            {
                CurrentLaneIdx = GetNearestLaneIndexOnCurrentLane();
                DesiredLaneIdx = CurrentLaneIdx;
                TurnStartLane = nullptr;
                FinishIntersectionTurn();
            }
        }
    }
}

bool UTraffic_AutoDriving::ShouldAvoidLeftTurnLane() const
{
    if (!IsValid(CurrentDrivingLane))
    {
        return false;
    }

    // 已经 assign 了左转请求，不需要避让
    if (bTurnRequested || bTurnInProgress)
    {
        return false;
    }

    // 过了 SafeTurnDist 才需要避让
    return HasPassedSafeTurnDist();
}

void UTraffic_AutoDriving::HandleLeftTurnLaneReservation()
{
    if (!ShouldAvoidLeftTurnLane())
    {
        return;
    }

    float HalfY = 0.f;
    int32 NumLanes = 1;
    float LaneWidth = 0.f;

    if (!GetLaneGeometry(CurrentDrivingLane, HalfY, NumLanes, LaneWidth))
    {
        return;
    }

    if (NumLanes <= 1)
    {
        return;
    }

    if (CurrentLaneIdx == 0 || DesiredLaneIdx == 0)
    {
		bUseSwitchLane = true;
        const int32 TargetIdx = FMath::Clamp(1, 0, NumLanes - 1);
        if (!bLaneChangeInProgress || DesiredLaneIdx != TargetIdx)
        {
            ForceRequestLaneChange(TargetIdx);
            UE_LOG(LogTemp, Warning,
                TEXT("[AutoDrive] HandleLeftTurnLaneReservation: forcing to lane %d"), TargetIdx);
        }
    }
}

bool UTraffic_AutoDriving::ForceRequestLaneChange(int32 TargetLaneIdx)
{
    if (!IsValid(CurrentDrivingLane) || !IsValid(OwnerPawn))
    {
        return false;
    }

    float HalfY = 0.f;
    int32 NumLanes = 1;
    float LaneWidth = 1.f;
    if (!GetLaneGeometry(CurrentDrivingLane, HalfY, NumLanes, LaneWidth))
    {
        return false;
    }

    TargetLaneIdx = FMath::Clamp(TargetLaneIdx, 0, NumLanes - 1);

    if (TargetLaneIdx == CurrentLaneIdx && !bLaneChangeInProgress)
    {
        DesiredLaneIdx = TargetLaneIdx;
        bForcedLaneChange = false;
        return true;
    }

    DesiredLaneIdx = TargetLaneIdx;
    bLaneChangeInProgress = true;
    bForcedLaneChange = true;
    LaneChangeCooldownTimer = LaneChangeCooldown;

    UE_LOG(LogTemp, Warning,
        TEXT("[AutoDrive] ForceRequestLaneChange: current=%d target=%d"),
        CurrentLaneIdx, TargetLaneIdx);

    return true;
}
