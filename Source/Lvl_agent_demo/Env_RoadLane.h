// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Env_TrafficLightManagerComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Env_TrafficLightTypes.h"
#include "Env_RoadLane.generated.h"

class ATraffic_AICar;
class UBoxComponent;
class UStaticMeshComponent;
class UTraffic_AICarManagerComponent;
class UTraffic_AutoDriving;
class ADecisionTrigger;
class AEnv_RoadLaneTransfer;
class URecommendationManager;

UCLASS()
class LVL_AGENT_DEMO_API AEnv_RoadLane : public AActor
{
    GENERATED_BODY()
    
public:
    // Sets default values for this actor's properties
    AEnv_RoadLane();

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lane")
    float LaneLength = 2000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lane|Decision")
    TSubclassOf<ADecisionTrigger> DecisionTriggerClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lane|Decision")
    ADecisionTrigger* DecisionTrigger = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lane|Decision")
    float DecisionTriggerInitRatio = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lane|Decision")
    bool bPlaceDecisionTriggerOnBeginPlay = true;

    UFUNCTION(BlueprintCallable, Category = "Lane|Decision")
    void SetDecisionTriggerAtDistance(float DistanceAlongLane);

    UFUNCTION(BlueprintCallable, Category = "Lane|Decision")
    void SetDecisionTriggerAtRatio(float Ratio);

    UFUNCTION(BlueprintCallable, Category = "Lane|Decision")
    FVector GetPointAlongLane(float DistanceAlongLane) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Road")
    float RoadWidth = 1200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Road")
    float RoadLength = 5000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Road")
    int32 LaneNumber = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Road")
    float RoadSideThickness = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Road")
    float RoadSideHeight = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Road|BrakingArea")
    float BrakingAreaLength = 2000.f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Road",
        meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
    FVector2D SpawnRange = FVector2D(0.5f, 0.9f);
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Road")
    UBoxComponent* RoadSurface;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Intersection")
    AActor* Intersection;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Intersection")
    ETrafficAxis LaneAxis = ETrafficAxis::NorthSouth;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spawn")
    UBoxComponent* Spawner;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road|BrakingArea")
    UBoxComponent* BrakingArea;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road|TurnArea")
    UBoxComponent* TurnArea;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road")
    UBoxComponent* RoadRangeBox;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurnTargetLane")
	AEnv_RoadLane* LeftTurnTargetLane;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurnTargetLane")
    AEnv_RoadLane* RightTurnTargetLane;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurnTargetLane")
	float SafeTurnDist = 500.f;

protected:
    UPROPERTY(EditAnywhere, Category="Traffic|StopLine")
    float CommitDist = 300.f;

    UPROPERTY(EditAnywhere, Category="Traffic|AICarSpeedTime")
	float AICarRecoverSpeedTime = 2.f;

    UPROPERTY(EditAnywhere, Category="Traffic|StopLine")
    float StopLineOffsetFromEnd = 0.f;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Road")
    UBoxComponent* RoadSideLeft;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Road")
    UBoxComponent* RoadSideRight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Road")
    UStaticMeshComponent* RoadMesh;
    
    UPROPERTY()
    TSet<TWeakObjectPtr<ATraffic_AICar>> CarsInBrakingArea;
    
    UFUNCTION()
    void OnBrakeBeginOverlap(
         UPrimitiveComponent* OverlappedComp,
         AActor* OtherActor,
         UPrimitiveComponent* OtherComp,
         int32 OtherBodyIndex,
         bool bFromSweep,
         const FHitResult& SweepResult
    );
    
    UFUNCTION()
    void OnBrakeEndOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex
    );

    UFUNCTION()
    void OnLaneBeginOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    UFUNCTION()
    void OnLaneEndOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex
    );
    UFUNCTION()
    void OnAICarEnterLane(AActor* OtherActor);

    UFUNCTION()
    void DelayedUpdateOverlaps();

private:
    void UpdateGeometryFromParams();
    void UpdateCarGoState(ATraffic_AICar* Car);
    void OnPhaseChanged(EGlobalTrafficPhase NewPhase);
    
    UEnv_TrafficLightManagerComponent* TrafficLightManager = nullptr;

    /** 缓存找到的 Traffic Manager，用于设置 CurrentLane */
    UTraffic_AICarManagerComponent* TrafficManager = nullptr;

    /** 节流计时器：防止 Pawn 在同一帧重复触发 SetCurrentLane */
    FTimerHandle LaneSetCooldownHandle;

    bool IsAICar(AActor* OtherActor) const;
    bool IsPawnInBrakingArea = false;

        // 修复 C4458：声明参数名改为 InAuto
    void UpdateAutoDrivingGoState(UTraffic_AutoDriving* InAuto);

    UTraffic_AutoDriving* Auto = nullptr;

    UFUNCTION()
    void OnAnyLoopTransferTriggered(AEnv_RoadLaneTransfer* Transfer, AActor* OtherActor, AEnv_RoadLane* DestLane);
	URecommendationManager* RecMgr = nullptr;

public:
    UFUNCTION(BlueprintCallable, Category="Road|Lane")
    int32 GetLaneIndexForLocation(const FVector& WorldLocation) const;

    UFUNCTION(BlueprintCallable, Category="Road|Lane")
    float GetForwardDistanceAlongLane(const FVector& WorldLocation, bool bClampToSegment = true) const;
    
    UFUNCTION(BlueprintCallable, Category="Road|Lane")
    float GetLateralOffsetFromCenter(const FVector& WorldLocation, bool bClampToLane) const;
    
    float GetRemainingToStopLine(const FVector& WorldLoc) const;
};
