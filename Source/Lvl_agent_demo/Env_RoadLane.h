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

protected:
    UPROPERTY(EditAnywhere, Category="Traffic|StopLine")
    float CommitDist = 300.f;

    UPROPERTY(EditAnywhere, Category="Traffic|StopLine")
    float StopLineOffsetFromEnd = 0.f;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Road")
    UBoxComponent* RoadSideLeft;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Road")
    UBoxComponent* RoadSideRight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Road|BrakingArea")
    UBoxComponent* BrakingArea;

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

private:
    void UpdateGeometryFromParams();
    void UpdateCarGoState(ATraffic_AICar* Car);
    void OnPhaseChanged(EGlobalTrafficPhase NewPhase);
    
    UEnv_TrafficLightManagerComponent* TrafficLightManager;
    bool IsAICar(AActor* OtherActor) const;

public:
    UFUNCTION(BlueprintCallable, Category="Road|Lane")
    int32 GetLaneIndexForLocation(const FVector& WorldLocation) const;

    UFUNCTION(BlueprintCallable, Category="Road|Lane")
    float GetForwardDistanceAlongLane(const FVector& WorldLocation, bool bClampToSegment = true) const;
    
    UFUNCTION(BlueprintCallable, Category="Road|Lane")
    float GetLateralOffsetFromCenter(const FVector& WorldLocation, bool bClampToLane) const;
    
    float GetRemainingToStopLine(const FVector& WorldLoc) const;
};
