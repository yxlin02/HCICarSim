// Traffic_AICar.h

#pragma once

#include "CoreMinimal.h"
#include "SportsCar/Lvl_agent_demoSportsCar.h"
#include "Traffic_AICar.generated.h"

class AEnv_RoadLane;
class UChaosWheeledVehicleMovementComponent;

UENUM(BlueprintType)
enum class EAICarTypes : uint8
{
    Left,
    Straight
};

UCLASS()
class LVL_AGENT_DEMO_API ATraffic_AICar : public ALvl_agent_demoSportsCar
{
    GENERATED_BODY()

public:
    ATraffic_AICar();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

protected:

    /** 期望与前车保持的最小间距（cm） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Sensor", meta = (ClampMin = "50.0"))
    float SafeDist = 300.f;

    /** Chaos 车辆实际最大减速度（cm/s²），用于计算制动距离，请根据实际车辆物理调整 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Sensor", meta = (ClampMin = "100.0"))
    float MaxDecel = 800.f;

    /** 制动距离安全系数，越大越保守 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Sensor", meta = (ClampMin = "1.0", ClampMax = "3.0"))
    float BrakeSafetyMargin = 1.5f;

    /**
     * Raycast 最远探测距离（cm）。
     * 车速越快时实际所需制动距离越长，建议设置为 DesiredSpeed 对应制动距离的 2~3 倍。
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Sensor", meta = (ClampMin = "100.0"))
    float RaycastMaxDistance = 2000.f;

    /**
     * 侧向扫射的射线数（总条数，含中心线）。
     * 奇数效果最好（中心 + 左右对称），建议 3 或 5。
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Sensor", meta = (ClampMin = "1", ClampMax = "9"))
    int32 RaycastCount = 3;

    /**
     * 侧向扫射最大半角（度）。
     * 例如 15°：左 -15°、中 0°、右 +15°。
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Sensor", meta = (ClampMin = "0.0", ClampMax = "60.0"))
    float RaycastHalfAngleDeg = 10.f;

    /** Raycast 起点相对于 Actor 根部的偏移（cm），用于避免射线从车身内部发出 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Sensor")
    FVector RaycastOriginOffset = FVector(200.f, 0.f, 50.f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Speed")
    float DesiredSpeed = 800.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Speed")
    float TargetSpeed = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Speed")
    float CurrentSpeed = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Speed", meta = (ClampMin = "0.01"))
    float SpeedChangeTime = 1.0f;

public:

    UFUNCTION(BlueprintCallable, Category = "AI|Speed")
    void SetDesiredSpeed(float NewDesiredSpeed);

    UFUNCTION(BlueprintPure, Category = "AI|Speed")
    float GetDesiredSpeed() const { return DesiredSpeed; }

    UFUNCTION(BlueprintPure, Category = "AI|Speed")
    float GetTargetSpeed() const { return TargetSpeed; }

    UFUNCTION(BlueprintPure, Category = "AI|Speed")
    float GetCurrentSpeed() const { return CurrentSpeed; }

    UFUNCTION()
    void SetShouldGoIntersection(bool bShouldGo);

    UPROPERTY(EditAnywhere, Category = "AI|Axis")
    AEnv_RoadLane* CurrentDrivingLane = nullptr;

    UPROPERTY(EditAnywhere, Category = "AI|Axis")
    int32 CurrentDrivingLaneIndex = INDEX_NONE;

    void SetCurrentLane(AEnv_RoadLane* Lane);
    void SetLaneIndex(int32 LaneIndex);
    int32 GetLaneIndex() const;
    AEnv_RoadLane* GetCurrentLane() const;

    void SetPawnApproachingEffect(float Effect);

    UFUNCTION()
    void SetInitialDesiredSpeed(float Speed) { DesiredSpeed = Speed; InitialDesiredSpeed = Speed; }

    UFUNCTION()
    float GetInitialDesiredSpeed() const { return InitialDesiredSpeed; }

protected:
    UFUNCTION()
    void UpdateTargetSpeed();

    UFUNCTION()
    void UpdateSpeed(float DeltaTime);

    UFUNCTION()
    void ApplySpeedToVehicle();

    /** 每帧执行 Raycast，更新 bHasBlockingActorAhead / BlockingActorAhead / RaycastHitDistance */
    void UpdateRaycast();

    bool IsDesignatedBlockingActor(AActor* OtherActor) const;

    float PawnApproachingEffect = 1.0f;
    float InitialDesiredSpeed = DesiredSpeed;
    bool bHardStop = false;

private:
    bool bHasBlockingActorAhead = false;
    bool bForceBrake = false;

    /** 最近命中的阻挡 Actor */
    TWeakObjectPtr<AActor> BlockingActorAhead;

    /** 最近一次 Raycast 命中的前向距离（cm） */
    float RaycastHitDistance = TNumericLimits<float>::Max();

    UPROPERTY()
    UChaosWheeledVehicleMovementComponent* CachedVehicleMovement = nullptr;

    bool bShouldGoIntersection = true;
};