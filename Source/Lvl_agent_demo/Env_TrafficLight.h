// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Env_TrafficLightTypes.h"
#include "Env_TrafficLightManagerComponent.h"
#include "Env_TrafficLight.generated.h"

UCLASS()
class LVL_AGENT_DEMO_API AEnv_TrafficLight : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEnv_TrafficLight();
    
    UFUNCTION(BlueprintCallable, Category="Traffic Light")
    void ApplyPhase(EGlobalTrafficPhase NewPhase);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
    
    void TurnDirectionOff(class UMaterialInstanceDynamic* RedMID,
                              class UMaterialInstanceDynamic* YellowMID,
                              class UMaterialInstanceDynamic* GreenMID);
    void InitDynamicMaterials();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Traffic Light")
    USceneComponent* Root;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Traffic Light")
    UStaticMeshComponent* PoleMesh;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Traffic Light")
    UStaticMeshComponent* ForwardLightMesh;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Traffic Light")
    UStaticMeshComponent* LeftLightMesh;
    
    // Forward Direction 3 Materials Dynamtic Instance
    UPROPERTY()
    UMaterialInstanceDynamic* ForwardRedMID;

    UPROPERTY()
    UMaterialInstanceDynamic* ForwardYellowMID;

    UPROPERTY()
    UMaterialInstanceDynamic* ForwardGreenMID;
    
    // Left Direction 3 Materials Dynamtic Instance
    UPROPERTY()
    UMaterialInstanceDynamic* LeftRedMID;

    UPROPERTY()
    UMaterialInstanceDynamic* LeftYellowMID;

    UPROPERTY()
    UMaterialInstanceDynamic* LeftGreenMID;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Traffic Light")
    EGlobalTrafficPhase CurrentPhase;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traffic Light|Material")
    FName ParamName_OnOff;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traffic")
    ETrafficAxis Axis = ETrafficAxis::NorthSouth;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traffic")
    ETrafficLightRole TrafficRole = ETrafficLightRole::Vehicle;
    
    UPROPERTY()
    UEnv_TrafficLightManagerComponent* Env_TrafficLightManager;
    FTimerHandle YellowTimerHandle;
    
private:
    float YellowDuration = 2.f;
    
    void EnterForwardYellow();
    void EnterForwardRed();
    void EnterLeftYellow();
    void EnterLeftRed();
    
    void SetOn(UMaterialInstanceDynamic* MID);
    void SetOff(UMaterialInstanceDynamic* MID);
    
};
