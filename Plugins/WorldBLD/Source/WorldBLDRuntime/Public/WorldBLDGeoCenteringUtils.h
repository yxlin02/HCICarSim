// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class USplineComponent;

/**
 * Shared helpers for geometry-only actors whose "real" geometry is stored as component-space data
 * (mesh vertices, spline points, etc.) while the actor pivot sits at (0,0,0).
 *
 * These utilities recenter the actor pivot to the geometry bounds center (XY only by default) and
 * rebase the underlying vertex data so the geometry stays in the same world-space location.
 *
 * Primary motivation: World Partition streaming uses actor location, so geometry-only actors must have
 * a meaningful pivot location.
 */
class WORLDBLDRUNTIME_API FWorldBLDGeoCenteringUtils
{
public:
	/**
	 * Recenters an actor to the XY bounds center of a spline (using spline point positions), then rebases one or more
	 * spline components so their world-space shape remains unchanged.
	 *
	 * @param ActorToMove The actor whose pivot should be moved to the spline bounds center (XY only; actor Z preserved).
	 * @param BoundsSpline The spline used to compute bounds/center (typically the "outer" perimeter).
	 * @param SplinesToRebase Splines whose point positions should be adjusted to keep their world-space positions unchanged.
	 * @return True if a recenter was applied.
	 */
	static bool CenterActorOnSplineBoundsXY(AActor* ActorToMove, USplineComponent* BoundsSpline, const TArray<USplineComponent*>& SplinesToRebase);
};


