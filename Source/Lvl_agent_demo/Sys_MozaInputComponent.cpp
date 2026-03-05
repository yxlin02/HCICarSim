#include "Sys_MozaInputComponent.h"
#include "Lvl_agent_demoPawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Math/UnrealMathUtility.h"

USys_MozaInputComponent::USys_MozaInputComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void USys_MozaInputComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!bEnableMozaInput) { return; }

    OwnerPawn = Cast<ALvl_agent_demoPawn>(GetOwner());
    if (!OwnerPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MozaComp] Owner is not ALvl_agent_demoPawn."));
        return;
    }

    VehicleMovement = Cast<UChaosWheeledVehicleMovementComponent>(OwnerPawn->GetVehicleMovement());
    if (!VehicleMovement)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MozaComp] VehicleMovement not found."));
        return;
    }

    // 强制清零，防止任何初始输入
    VehicleMovement->SetHandbrakeInput(false);
    VehicleMovement->SetBrakeInput(0.0f);
    VehicleMovement->SetThrottleInput(0.0f);

    MozaBroadcaster = NewObject<UMozaInputBroadcaster>(GetOwner());

    MozaBroadcaster->OnSteeringChanged.AddDynamic(this, &USys_MozaInputComponent::OnMozaSteeringChanged);
    MozaBroadcaster->OnThrottleChanged.AddDynamic(this, &USys_MozaInputComponent::OnMozaThrottleChanged);
    MozaBroadcaster->OnBrakeChanged.AddDynamic(this, &USys_MozaInputComponent::OnMozaBrakeChanged);
    MozaBroadcaster->OnBrakeStarted.AddDynamic(this, &USys_MozaInputComponent::OnMozaBrakeStarted);
    MozaBroadcaster->OnBrakeCompleted.AddDynamic(this, &USys_MozaInputComponent::OnMozaBrakeCompleted);

    MozaBroadcaster->StartPolling();

    UE_LOG(LogTemp, Warning, TEXT("[MozaComp] Started. Calibrating for %d frames..."), CalibrationFrames);
}

void USys_MozaInputComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
}

void USys_MozaInputComponent::OnMozaSteeringChanged(float SteeringAngle)
{
    if (!VehicleMovement) { return; }
    const float Normalized = FMath::Clamp(SteeringAngle / MaxSteeringAngle, -1.0f, 1.0f);
    VehicleMovement->SetSteeringInput(Normalized);
}

void USys_MozaInputComponent::OnMozaThrottleChanged(float ThrottleValue)
{
    if (!OwnerPawn) { return; }

    if (!bCalibrated)
    {
        ThrottleRestSum += ThrottleValue;
        BrakeRestSum += LastBrakeRaw;
        CalibrationCount++;

        VehicleMovement->SetHandbrakeInput(false);
        VehicleMovement->SetBrakeInput(0.0f);
        VehicleMovement->SetThrottleInput(0.0f);

        if (CalibrationCount >= CalibrationFrames)
        {
            ThrottleRestRaw = ThrottleRestSum / CalibrationFrames;
            BrakeRestRaw = BrakeRestSum / CalibrationFrames;
            bCalibrated = true;
            UE_LOG(LogTemp, Warning, TEXT("[MozaComp] Calibrated! ThrottleRest=%.0f BrakeRest=%.0f"),
                ThrottleRestRaw, BrakeRestRaw);
        }
        return;
    }

    const float ThrottleDelta = ThrottleValue - ThrottleRestRaw;
    float Norm = 0.0f;
    if (ThrottleDelta > RestTolerance)
    {
        const float Range = 32767.0f - ThrottleRestRaw;
        Norm = (Range > 1.0f) ? FMath::Clamp(ThrottleDelta / Range, 0.0f, 1.0f) : 0.0f;
    }

    CurrentThrottle = Norm < PedalDeadZone ? 0.0f : Norm;
    UE_LOG(LogTemp, Warning, TEXT("[MozaComp] Throttle raw=%.0f rest=%.0f delta=%.0f norm=%.3f"),
        ThrottleValue, ThrottleRestRaw, ThrottleDelta, CurrentThrottle);
    ApplyThrottleBrake();
}

void USys_MozaInputComponent::OnMozaBrakeChanged(float BrakeValue)
{
    if (!OwnerPawn) { return; }
    LastBrakeRaw = BrakeValue;

    if (!bCalibrated) { return; }

    const float BrakeDelta = BrakeValue - BrakeRestRaw;
    float Norm = 0.0f;
    if (BrakeDelta > RestTolerance)
    {
        const float Range = 32767.0f - BrakeRestRaw;
        Norm = (Range > 1.0f) ? FMath::Clamp(BrakeDelta / Range, 0.0f, 1.0f) : 0.0f;
    }

    CurrentBrake = Norm < PedalDeadZone ? 0.0f : Norm;
    UE_LOG(LogTemp, Warning, TEXT("[MozaComp] Brake raw=%.0f rest=%.0f delta=%.0f norm=%.3f"),
        BrakeValue, BrakeRestRaw, BrakeDelta, CurrentBrake);
    ApplyThrottleBrake();
}

void USys_MozaInputComponent::OnMozaBrakeTriggered(float BrakeValue)
{
    // 不再使用，保留空实现防止绑定报错
}

void USys_MozaInputComponent::OnMozaBrakeStarted()
{
    if (!OwnerPawn) { return; }
    OwnerPawn->DoBrakeStart();
}

void USys_MozaInputComponent::OnMozaBrakeCompleted()
{
    if (!OwnerPawn) { return; }
    OwnerPawn->DoBrakeStop();
}

void USys_MozaInputComponent::ApplyThrottleBrake()
{
    if (!OwnerPawn) { return; }

    VehicleMovement->SetHandbrakeInput(false);

    if (CurrentBrake > 0.0f)
    {
        OwnerPawn->DoThrottle(0.0f);
        OwnerPawn->DoBrake(CurrentBrake);
    }
    else
    {
        OwnerPawn->DoBrake(0.0f);
        OwnerPawn->DoThrottle(CurrentThrottle);
    }
}