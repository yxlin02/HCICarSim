// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDGeo.h"

AWorldBLDGeo::AWorldBLDGeo()
{
#if WITH_EDITOR
	// Prevent accidental movement or deletion in editor
	bLockLocation = true;
#endif
}
