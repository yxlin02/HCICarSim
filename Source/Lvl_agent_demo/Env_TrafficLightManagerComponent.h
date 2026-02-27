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

    /** 每个 Phase 持续时间（秒） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traffic Timing")
    float StraightDuration = 10.f;   // forwardGo 秒

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Traffic Timing")
    float LeftDuration = 5.f;        // leftGo 秒

    /** 内部计时器 */
    float PhaseTimeAccumulated;

    /** 被管理的所有红绿灯 Actor */
    UPROPERTY()
    TArray<AEnv_TrafficLight*> RegisteredLights;

    void SwitchPhase();
    void BroadcastPhaseToAll();
    float GetCurrentPhaseDuration() const;
};
