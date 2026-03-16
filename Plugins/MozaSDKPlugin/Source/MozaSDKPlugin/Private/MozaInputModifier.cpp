#include "MozaInputModifier.h" 
#include "MozaBlueprintLibrary.h" 
#include "Engine/World.h" 
#include "TimerManager.h" 
#include "mozaAPI.h"
#include "HAL/PlatformProcess.h" 
#include "Misc/DateTime.h" 
#include "Misc/OutputDeviceNull.h" 
#include "Logging/LogMacros.h" 
#include <cmath>

DEFINE_LOG_CATEGORY_STATIC(LogMozaInput, Log, All);

static const int16_t PEDAL_DISCONNECTED_A = static_cast<int16_t>(0x8000); // -32768
static const int16_t PEDAL_DISCONNECTED_B = static_cast<int16_t>(0x8001); // -32767

static bool IsPedalDisconnected(int16_t val)
{
    return val == PEDAL_DISCONNECTED_A || val == PEDAL_DISCONNECTED_B;
}

void UMozaInputBroadcaster::StartPolling()
{
    if (bIsPolling) { return; }
    bIsPolling = true;

    UWorld* World = GetWorld();
    if (!World) { return; }

    World->GetTimerManager().SetTimer(
        PollTimerHandle,
        this,
        &UMozaInputBroadcaster::PollMozaInput,
        PollingInterval,
        /*bLoop=*/true
    );
}

void UMozaInputBroadcaster::PollMozaInput()
{
    if (!bIsPolling) { return; }

    ERRORCODE err = NORMAL;
    const HIDData* data = moza::getHIDData(err);

    if (data)
    {
        if (!std::isnan(data->fSteeringWheelAngle))
        {
            LastValidSteering    = data->fSteeringWheelAngle;
            bSteeringInitialized = true;
        }

        LastValidThrottle    = static_cast<float>(data->throttle);
        bThrottleInitialized = true;

        LastValidBrake    = static_cast<float>(data->brake);
        bBrakeInitialized = true;
    }

    if (bSteeringInitialized) { OnSteeringChanged.Broadcast(LastValidSteering); }
    if (bThrottleInitialized) { OnThrottleChanged.Broadcast(LastValidThrottle); }
    if (bBrakeInitialized)    { OnBrakeChanged.Broadcast(LastValidBrake); }

    bool bBrakePressedNow = bBrakeInitialized && (LastValidBrake > 0.01f);
    if (bBrakePressedNow && !bBrakeWasPressed) { OnBrakeStarted.Broadcast(); }
    if (bBrakePressedNow)                      { OnBrakeTriggered.Broadcast(LastValidBrake); }
    if (!bBrakePressedNow && bBrakeWasPressed) { OnBrakeCompleted.Broadcast(); }
    bBrakeWasPressed = bBrakePressedNow;
}