// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lvl_agent_demoGameMode.h"
#include "Lvl_agent_demoPlayerController.h"

ALvl_agent_demoGameMode::ALvl_agent_demoGameMode()
{
	PlayerControllerClass = ALvl_agent_demoPlayerController::StaticClass();
}
