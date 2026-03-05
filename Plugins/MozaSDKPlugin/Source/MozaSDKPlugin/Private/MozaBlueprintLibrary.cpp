


#include "MozaBlueprintLibrary.h"
#include "mozaAPI.h"

float UMozaBlueprintLibrary::GetMozaSteeringAngle()

{

    ERRORCODE err = NORMAL;

    const HIDData* data = moza::getHIDData(err);



    if (data && !std::isnan(data->fSteeringWheelAngle))

    {

        return data->fSteeringWheelAngle;

    }



    return 0.0f; // fallback if data is invalid 

}

float UMozaBlueprintLibrary::GetAcceleratorValue()

{

    ERRORCODE err;

    const HIDData* data = moza::getHIDData(err);

    if (data && err == ERRORCODE::NORMAL)

    {
        return static_cast<float>(data->throttle + 32768) / 65535.0f;
    }

    return 0.0f;

}



float UMozaBlueprintLibrary::GetBrakeValue()

{

    ERRORCODE err;

    const HIDData* data = moza::getHIDData(err);

    if (data && err == ERRORCODE::NORMAL)

    {
        return static_cast<float>(data->brake + 32768) / 65535.0f;

    }

    return 0.0f;

}

