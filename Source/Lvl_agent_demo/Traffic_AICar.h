// Traffic_AICar.h

#pragma once

#include "CoreMinimal.h"
#include "SportsCar/Lvl_agent_demoSportsCar.h"
#include "Traffic_AICar.generated.h"

class AEnv_RoadLane;
class UBoxComponent;
class UChaosWheeledVehicleMovementComponent;

/**
 * 简单前向跟车 / 红灯响应的 AI 车
 */

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

    /** 前向感知盒，用来检测前车 / Pawn / 障碍物 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI|Sensor")
    UBoxComponent* FrontSensorBox;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Sensor")
    float SafeDist = 2000.f;

    /** GameMode / TrafficManager 希望这辆车跑到的理想速度（cm/s） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Speed")
    float DesiredSpeed = 800.f;

    /** 在当前环境约束下允许的目标速度（cm/s），= min(DesiredSpeed, 安全速度) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI|Speed")
    float TargetSpeed = 0.f;

    /** 当前实际执行中的线速度（cm/s），逐渐逼近 TargetSpeed */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI|Speed")
    float CurrentSpeed = 0.f;

    /** 从当前速度渐变到 TargetSpeed 所需的大致时间（秒） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Speed", meta=(ClampMin="0.01"))
    float SpeedChangeTime = 1.0f;

public:

    /** 设置理想速度，GameMode 或 TrafficManager 可以定期调用 */
    UFUNCTION(BlueprintCallable, Category="AI|Speed")
    void SetDesiredSpeed(float NewDesiredSpeed);

    UFUNCTION(BlueprintPure, Category="AI|Speed")
    float GetDesiredSpeed() const { return DesiredSpeed; }

    UFUNCTION(BlueprintPure, Category="AI|Speed")
    float GetTargetSpeed() const { return TargetSpeed; }

    UFUNCTION(BlueprintPure, Category="AI|Speed")
    float GetCurrentSpeed() const { return CurrentSpeed; }
    
    UFUNCTION()
    void SetShouldGoIntersection(bool bShouldGo);
    
    UPROPERTY(EditAnywhere, Category="AI|Axis")
    AEnv_RoadLane* CurrentDrivingLane = nullptr;
    
    UPROPERTY(EditAnywhere, Category="AI|Axis")
    int32 CurrentDrivingLaneIndex = INDEX_NONE;
    
    void SetCurrentLane(AEnv_RoadLane* Lane);
    void SetLaneIndex(int32 LaneIndex);
    int32 GetLaneIndex() const;
    AEnv_RoadLane* GetCurrentLane() const;

    void SetPawnApproachingEffect(float Effect);

protected:
    /** 根据前方是否有车 / 红灯状态更新 TargetSpeed（不做插值） */
    UFUNCTION()
    void UpdateTargetSpeed();

    /** 让 CurrentSpeed 在 SpeedChangeTime 内平滑逼近 TargetSpeed */
    UFUNCTION()
    void UpdateSpeed(float DeltaTime);

    /** 把 CurrentSpeed 转换为油门 / 刹车输入，驱动 Chaos 车辆组件 */
    UFUNCTION()
    void ApplySpeedToVehicle();

    /** 前方感知盒开始 overlap 的回调 */
    UFUNCTION()
    void OnFrontSensorBeginOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    /** 前方感知盒结束 overlap 的回调 */
    UFUNCTION()
    void OnFrontSensorEndOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex
    );

    float PawnApproachingEffect = 1.0;

private:
    /** 是否前方存在潜在阻挡（车 / Pawn / 障碍物） */
    bool bHasBlockingActorAhead = false;
    bool bForceBrake = false;

    /** 前方最近的阻挡 Actor（后面可以用来算车距 / 前车速度） */
    TWeakObjectPtr<AActor> BlockingActorAhead;

    /** 缓存 Chaos 车辆 movement 组件 */
    UPROPERTY()
    UChaosWheeledVehicleMovementComponent* CachedVehicleMovement = nullptr;
    
    UPROPERTY()
    TSet<TWeakObjectPtr<AActor>> FrontSensorOverlaps;
    
    bool IsDesignatedBlockingActor(AActor* OtherActor) const;
    void UpdateBlockingActorAheadFromOverlaps();
    bool bShouldGoIntersection = true;
};
