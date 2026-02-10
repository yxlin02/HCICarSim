// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Lvl_agent_demoPawn.h"
#include "Lvl_agent_demoSportsCar.generated.h"

/**
 *  Sports car wheeled vehicle implementation
 */
UCLASS(abstract)
class ALvl_agent_demoSportsCar : public ALvl_agent_demoPawn
{
	GENERATED_BODY()
	
public:

	ALvl_agent_demoSportsCar();
};
