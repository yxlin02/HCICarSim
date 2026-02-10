// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lvl_agent_demoWheelFront.h"
#include "UObject/ConstructorHelpers.h"

ULvl_agent_demoWheelFront::ULvl_agent_demoWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
}