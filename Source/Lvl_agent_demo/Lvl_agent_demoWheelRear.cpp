// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lvl_agent_demoWheelRear.h"
#include "UObject/ConstructorHelpers.h"

ULvl_agent_demoWheelRear::ULvl_agent_demoWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}