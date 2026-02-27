#pragma once

#include "CoreMinimal.h"
#include "Env_RoadLane.h"
#include "Traffic_AICarManagerComponent.h"
#include "Env_RoadLaneTransfer.generated.h"

class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnLoopTransferTriggered,
    AEnv_RoadLaneTransfer*, Transfer,
    AActor*, OtherActor,
    AEnv_RoadLane*, DestLane
);

UCLASS()
class LVL_AGENT_DEMO_API AEnv_RoadLaneTransfer : public AEnv_RoadLane
{
    GENERATED_BODY()

public:
    AEnv_RoadLaneTransfer();

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

public:
    // 从路段“起点”沿前进方向算起，LoopTrigger 距离起点的距离（cm）
    // 例如 RoadLength=6000，LoopTriggerLengthFromStart=5500 → 靠近路尾
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Road|Loop")
    float LoopTriggerLengthFromStart = 5000.f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Road|Loop")
    float NearDeadTriggerLengthFromLoop = 5000.f;
    
    UPROPERTY(BlueprintAssignable, Category="Transfer|Events")
    FOnLoopTransferTriggered OnLoopTransferTriggered;
    
    // 触发循环传送的 Box
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Road|Loop")
    UBoxComponent* LoopTrigger;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Road|DeadZone")
    UBoxComponent* NearDeadZone;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Road|DeadZone")
    UBoxComponent* EndDeadZone;

    // 传送的目标 RoadLane（场景中的具体实例）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Road|Loop")
    AEnv_RoadLane* TransferDestination;

protected:
    UFUNCTION()
    void OnLoopTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );
    
    UFUNCTION()
    void OnDeadZoneBeginOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    // 把一个 Actor 从当前 RoadLane 的本地坐标系，映射到目标 RoadLane 的世界坐标系
    void TeleportActorToDestinationLane(AActor* Actor);
    
    UPROPERTY()
    TSet<TWeakObjectPtr<AActor>> RecentlyTeleportedActors;
    
    UTraffic_AICarManagerComponent* AICar_Mgr;
    FTimerHandle LoopTriggerTimerHandle;
};
