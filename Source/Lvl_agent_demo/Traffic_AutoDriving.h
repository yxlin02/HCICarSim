#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Traffic_AutoDriving.generated.h"

class ALvl_agent_demoPawn;
class UChaosWheeledVehicleMovementComponent;
class AEnv_RoadLane;
class ATraffic_AICar;
class UTraffic_AICarManagerComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LVL_AGENT_DEMO_API UTraffic_AutoDriving : public UActorComponent
{
    GENERATED_BODY()

public:
    UTraffic_AutoDriving();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

public:
    void SetCurrentLane(AEnv_RoadLane* Lane);
    bool IsFrontBlockedInCurrentLane() const;

    UFUNCTION(BlueprintCallable, Category = "AutoDrive|LaneChange")
    bool RequestLaneChange(int32 DeltaLaneIdx);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive")
    bool bGo = true;

    void SetGo(bool bInGo);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive")
    bool bAutoDriveEnabled = true;

    UFUNCTION()
    int32 GetCurrentLaneIdx() const { return CurrentLaneIdx; }

    UFUNCTION(BlueprintCallable)
    void TurnAtIntersection(float TurnDir);

    UFUNCTION()
    bool SetIsInIntersection(bool bInIntersection) { bInIntersectionArea = bInIntersection; return bInIntersectionArea; }

    UFUNCTION()
    void GetIsInIntersection(bool& bOutInIntersection) const { bOutInIntersection = bInIntersectionArea; }

    UFUNCTION()
    AEnv_RoadLane* GetCurrentDrivingLane() const { return CurrentDrivingLane; }

    UPROPERTY(EditAnywhere, Category = "AutoDrive")
    bool bUseSwitchLane = true;

    void NotifyTeleportedToLane(AEnv_RoadLane* ExpectedLane);

protected:
    void UpdateFrontDetection();
    void UpdateSideDetection();          // ← 新增：变道/转弯时的侧方持续检测
    void UpdateLaneIndexFromPosition();
    void TryRequestLaneChange(float DeltaTime);
    void UpdateTargetSpeed(float DeltaTime);
    void UpdateSteering(float DeltaTime);
    void ApplyControl();
    void UpdateLaneChangeState(float DeltaTime);

    bool TraceForAICar(
        const FVector& Start,
        const FVector& End,
        FHitResult& OutHit) const;

    bool TraceFanForAICar(
        const FVector& Start,
        const FVector& Forward,
        float Distance,
        float HalfDeg,
        int32 RayCount,
        FHitResult& OutBestHit,
        FColor DebugColor = FColor::Green) const;

    bool IsFanRegionOccupiedByAICar(
        const FVector& Start,
        const FVector& Forward,
        float Distance,
        float HalfDeg,
        int32 RayCount) const;

    bool IsLaneOccupied(int32 CandidateLaneIdx) const;

    bool GetLaneGeometry(
        const AEnv_RoadLane* Lane,
        float& OutHalfY,
        int32& OutNumLanes,
        float& OutLaneWidth) const;

    FVector GetLaneCenterPoint(
        const AEnv_RoadLane* Lane,
        int32 LaneIdx,
        float ForwardOffset) const;

    int32 GetNearestLaneIndexOnCurrentLane() const;

    UFUNCTION()
    void ApplyStop();

    bool ForceRequestLaneChange(int32 TargetLaneIdx);

    bool bInIntersectionArea = false;

protected:
    UPROPERTY()
    ALvl_agent_demoPawn* OwnerPawn = nullptr;

    UPROPERTY()
    UChaosWheeledVehicleMovementComponent* VehicleMovement = nullptr;

    UPROPERTY()
    AEnv_RoadLane* CurrentDrivingLane = nullptr;

    UPROPERTY()
    ATraffic_AICar* FrontBlockingCar = nullptr;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Debug")
    bool bDrawDebug = true;
    bool ShouldAvoidLeftTurnLane() const;
    void HandleLeftTurnLaneReservation();

protected:
    // ── 前方检测 ──────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|Front", meta = (ClampMin = "1.0"))
    float ForwardDetectDistance = 2000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|Front", meta = (ClampMin = "0.0"))
    float FrontDetectHalfDeg = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|Front", meta = (ClampMin = "1"))
    int32 FrontDetectRayCount = 5;

protected:
    // ── 变道/转弯时侧方持续检测 ────────────────────────────────
    /** 变道或转弯进行中，向侧前方扫描的距离 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|Side", meta = (ClampMin = "1.0"))
    float SideOngoingDetectDistance = 1000.f;

    /** 侧方扫描的半角（度） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|Side", meta = (ClampMin = "0.0"))
    float SideOngoingDetectHalfDeg = 45.f;

    /** 侧方扫描射线数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|Side", meta = (ClampMin = "1"))
    int32 SideOngoingDetectRayCount = 7;

    /** 侧方检测到 AICar 时开始减速的距离 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|Side", meta = (ClampMin = "1.0"))
    float SideSlowDistance = 800.f;

    /** 侧方检测到 AICar 时完全停车的距离 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|Side", meta = (ClampMin = "1.0"))
    float SideStopDistance = 250.f;

protected:
    // ── 变道决策前检测（现有） ────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|LaneCheckFront", meta = (ClampMin = "1.0"))
    float SideCheckForwardDistance = 1200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|LaneCheckFront", meta = (ClampMin = "0.0"))
    float SideFrontDetectHalfDeg = 6.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|LaneCheckFront", meta = (ClampMin = "1"))
    int32 SideFrontDetectRayCount = 5;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|LaneCheckRear", meta = (ClampMin = "1.0"))
    float SideCheckBackwardDistance = 1200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|LaneCheckRear", meta = (ClampMin = "0.0"))
    float RearDetectHalfDeg = 12.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|LaneCheckRear", meta = (ClampMin = "1"))
    int32 RearDetectRayCount = 7;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|LaneCheckSide", meta = (ClampMin = "1.0"))
    float SideCrossCheckDistance = 450.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|LaneCheckSide", meta = (ClampMin = "0.0"))
    float SideDetectHalfDeg = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Detect|LaneCheckSide", meta = (ClampMin = "1"))
    int32 SideDetectRayCount = 5;

protected:
    // ── 速度控制 ──────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Speed")
    float CruiseSpeed = 1400.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Speed")
    float SlowDistance = 1200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Speed")
    float StopDistance = 350.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Speed")
    float SpeedDeadband = 30.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Speed")
    float KpThrottle = 0.0025f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Speed")
    float KpBrake = 0.0040f;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Steering")
    float SteeringLookAhead = 700.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Steering")
    float LaneChangeSteeringLookAhead = 550.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Steering")
    float SteeringTargetGain = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Steering")
    float SteeringHeadingGain = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|Steering")
    float LaneChangeCompleteTolerance = 80.f;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|LaneChange")
    float LaneChangeCooldown = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoDrive|LaneChange")
    float LaneChangeCooldownTimer = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoDrive|LaneChange", meta = (ClampMin = "0.0"))
    float LaneChangeRequestCooldown = 5.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoDrive|LaneChange")
    float LaneChangeRequestCooldownTimer = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoDrive|LaneChange")
    bool bLaneChangeInProgress = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoDrive|LaneChange")
    int32 CurrentLaneIdx = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoDrive|LaneChange")
    int32 DesiredLaneIdx = 0;

    UPROPERTY(EditAnywhere, Category = "AutoDrive|LaneChange")
    float LaneChangeSideFrontCheckDist = 1200.f;

    UPROPERTY(EditAnywhere, Category = "AutoDrive|LaneChange")
    float LaneChangeSideRearCheckDist = 800.f;

    UPROPERTY(EditAnywhere, Category = "AutoDrive|LaneChange")
    float LaneChangeLaneTolerance = 0.45f;

    UPROPERTY(EditAnywhere, Category = "AutoDrive|LaneChange")
    float MaxLaneChangeBlockedTime = 2.0f;

    UPROPERTY(VisibleAnywhere, Category = "AutoDrive|LaneChange")
    bool bLaneChangeBlocked = false;

    UPROPERTY(VisibleAnywhere, Category = "AutoDrive|LaneChange")
    float LaneChangeBlockedTimer = 0.f;

    bool IsLaneChangePathBlocked(int32 TargetLaneIdx) const;
    void UpdateLaneChangeBlockState(float DeltaTime);

protected:
    // ── 运行时状态 ────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoDrive|State")
    bool bFrontBlocked = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoDrive|State")
    float FrontDistance = TNumericLimits<float>::Max();

    /** 变道/转弯时侧方是否检测到 AICar */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoDrive|State")
    bool bSideBlocked = false;

    /** 侧方最近 AICar 的距离 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoDrive|State")
    float SideDistance = TNumericLimits<float>::Max();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoDrive|State")
    float ThrottleCmd = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoDrive|State")
    float BrakeCmd = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AutoDrive|State")
    bool bHandbrakeCmd = false;

    UTraffic_AICarManagerComponent* TrafficManager;

private:
    bool HasPassedSafeTurnDist() const;
    bool IsInCurrentLaneBrakingArea() const;
    int32 GetCenterLaneIndex(const AEnv_RoadLane* Lane) const;
    void UpdateTurnState(float DeltaTime);
    void FinishIntersectionTurn();
    void ClearTurnRequest();

private:
    bool bTurnRequested = false;
    bool bTurnInProgress = false;
    float PendingTurnDir = 0.f;
    AEnv_RoadLane* PendingTurnTargetLane = nullptr;

    UPROPERTY()
    AEnv_RoadLane* TurnStartLane = nullptr;

    FTimerHandle TurnCooldownTimerHandle;

    UPROPERTY(EditAnywhere, Category = "Traffic|Turn")
    float TurnCooldown = 5.f;

    UPROPERTY(EditAnywhere, Category = "Traffic|Turn")
    float TurnForwardAlignToleranceDeg = 12.f;

    UPROPERTY(EditAnywhere, Category = "Traffic|Turn")
    float TurnSteeringLookAhead = 500.f;

    UPROPERTY()
    AEnv_RoadLane* TeleportExpectedLane = nullptr;

    float TeleportLockTimer = 0.f;

    UPROPERTY(EditAnywhere, Category = "AutoDrive|Teleport")
    float TeleportLockDuration = 1.5f;

    // ── 变道保持时间 ──────────────────────────────────────────
    UPROPERTY(EditAnywhere, Category = "AutoDrive|LaneChange")
    float LaneChangeMinHoldTime = 1.5f;

    UPROPERTY(VisibleAnywhere, Category = "AutoDrive|LaneChange")
    float LaneChangeHoldTimer = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "AutoDrive|LaneChange")
    bool bForcedLaneChange = false;   // ← 新增：标记是否为强制变道（不受 block 检测影响）
};