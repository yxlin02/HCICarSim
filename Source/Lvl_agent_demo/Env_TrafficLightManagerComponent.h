#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Env_TrafficLightTypes.h"
#include "Env_TrafficLightManagerComponent.generated.h"

class AEnv_TrafficLight;
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTrafficPhaseChanged, EGlobalTrafficPhase);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LVL_AGENT_DEMO_API UEnv_TrafficLightManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEnv_TrafficLightManagerComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 如果你不想自动搜，也可以在蓝图/代码里手动注册
    UFUNCTION(BlueprintCallable, Category="Traffic Light")
    void RegisterTrafficLight(AEnv_TrafficLight* TrafficLight);

    UFUNCTION(BlueprintCallable, Category="Traffic Light")
    void UnregisterTrafficLight(AEnv_TrafficLight* TrafficLight);

    // 允许蓝图强制切换 Phase（比如 Debug）
    UFUNCTION(BlueprintCallable, Category="Traffic Light")
    void ForceSetPhase(EGlobalTrafficPhase NewPhase, bool bResetTimer = true);
    
    float GetYellowDuration() const;
    
    UFUNCTION()
    EGlobalTrafficPhase GetCurrentPhase();
    
    FOnTrafficPhaseChanged OnTrafficPhaseChanged;

protected:
    /** 当前全局 Phase */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Traffic Light")
    EGlobalTrafficPhase CurrentPhase;

    /** 南北/东西直行绿灯持续时间（秒） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traffic Timing", meta=(ClampMin="1.0", UIMin="1.0"))
    float StraightDuration = 10.f;

    /** 南北/东西左转绿灯持续时间（秒） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traffic Timing", meta=(ClampMin="1.0", UIMin="1.0"))
    float LeftDuration = 5.f;

    /** 全红清场持续时间（秒），用于给路口车辆清场 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traffic Timing", meta=(ClampMin="0.5", UIMin="0.5"))
    float AllRedDuration = 3.f;

    /** 内部计时器 */
    float PhaseTimeAccumulated;

    /** 全红结束后切换到的下一个正式相位 */
    EGlobalTrafficPhase PendingPhase;

    /** 被管理的所有红绿灯 Actor */
    UPROPERTY()
    TArray<AEnv_TrafficLight*> RegisteredLights;

    void SwitchPhase();
    void BroadcastPhaseToAll();
    float GetCurrentPhaseDuration() const;
};
