#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MozaInputModifier.h"
#include "Sys_MozaInputComponent.generated.h"

class ALvl_agent_demoPawn;
class UChaosWheeledVehicleMovementComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LVL_AGENT_DEMO_API USys_MozaInputComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USys_MozaInputComponent();

    UPROPERTY(EditAnywhere, Category = "Moza Input")
    float MaxSteeringAngle = 1080.0f;

    UPROPERTY(EditAnywhere, Category = "Moza Input")
    bool bEnableMozaInput = true;

    UPROPERTY(EditAnywhere, Category = "Moza Input")
    float PedalDeadZone = 0.02f;

    UPROPERTY(EditAnywhere, Category = "Moza Input|Calibration")
    int32 CalibrationFrames = 30;

    UPROPERTY(EditAnywhere, Category = "Moza Input|Calibration")
    float RestTolerance = 2000.0f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY() UMozaInputBroadcaster* MozaBroadcaster;
    UPROPERTY() ALvl_agent_demoPawn* OwnerPawn;
    UPROPERTY() UChaosWheeledVehicleMovementComponent* VehicleMovement;

    float CurrentThrottle = 0.0f;
    float CurrentBrake = 0.0f;
    float LastBrakeRaw = 0.0f;

    bool  bCalibrated = false;
    int32 CalibrationCount = 0;
    float ThrottleRestSum = 0.0f;
    float BrakeRestSum = 0.0f;
    float ThrottleRestRaw = 0.0f;
    float BrakeRestRaw = 0.0f;

    void ApplyThrottleBrake();
    UFUNCTION() void OnMozaSteeringChanged(float SteeringAngle);
    UFUNCTION() void OnMozaThrottleChanged(float ThrottleValue);
    UFUNCTION() void OnMozaBrakeChanged(float BrakeValue);
    UFUNCTION() void OnMozaBrakeTriggered(float BrakeValue);
    UFUNCTION() void OnMozaBrakeStarted();
    UFUNCTION() void OnMozaBrakeCompleted();
};