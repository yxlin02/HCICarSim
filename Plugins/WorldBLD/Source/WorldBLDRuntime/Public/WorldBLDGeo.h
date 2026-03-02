// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldBLDGeo.generated.h"

/**
 * Base class for WorldBLD geometry actors.
 * Provides a common foundation for all geometry-based actors in the WorldBLD system,
 * such as roads, lots, and other generated world elements.
 */
UCLASS()
class WORLDBLDRUNTIME_API AWorldBLDGeo : public AActor
{
	GENERATED_BODY()
	
public:
	AWorldBLDGeo();
};
