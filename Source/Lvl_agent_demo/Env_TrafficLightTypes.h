// Env_TrafficLightTypes.h
#pragma once

#include "Env_TrafficLightTypes.generated.h"

UENUM(BlueprintType)
enum class EGlobalTrafficPhase : uint8
{
    NS_Straight_Ped_Green   UMETA(DisplayName="NS Straight+Ped Green, EW Red"),
    NS_Left_Green           UMETA(DisplayName="NS Left Green, Others Red"),
    EW_Straight_Ped_Green   UMETA(DisplayName="EW Straight+Ped Green, NS Red"),
    EW_Left_Green           UMETA(DisplayName="EW Left Green, Others Red"),
};

UENUM(BlueprintType)
enum class ETrafficAxis : uint8
{
    NorthSouth,
    EastWest
};

UENUM(BlueprintType)
enum class ETrafficLightRole : uint8
{
    Vehicle,
    Pedestrian
};
