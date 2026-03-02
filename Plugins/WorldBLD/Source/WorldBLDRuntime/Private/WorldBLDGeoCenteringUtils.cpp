// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDGeoCenteringUtils.h"

#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"

bool FWorldBLDGeoCenteringUtils::CenterActorOnSplineBoundsXY(
	AActor* ActorToMove,
	USplineComponent* BoundsSpline,
	const TArray<USplineComponent*>& SplinesToRebase)
{
	if (!IsValid(ActorToMove) || !IsValid(BoundsSpline))
	{
		return false;
	}

	const int32 NumBoundsPoints = BoundsSpline->GetNumberOfSplinePoints();
	if (NumBoundsPoints <= 0)
	{
		return false;
	}

	// Compute world-space bounds of the bounds spline's control points.
	FBox WorldBounds(ForceInit);
	for (int32 i = 0; i < NumBoundsPoints; ++i)
	{
		WorldBounds += BoundsSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
	}

	if (!WorldBounds.IsValid)
	{
		return false;
	}

	const FVector OldActorLocation = ActorToMove->GetActorLocation();
	const FVector CenterWorld = WorldBounds.GetCenter();
	const FVector NewActorLocation(CenterWorld.X, CenterWorld.Y, OldActorLocation.Z);
	const FVector DeltaXY(NewActorLocation.X - OldActorLocation.X, NewActorLocation.Y - OldActorLocation.Y, 0.0);

	if (DeltaXY.IsNearlyZero(KINDA_SMALL_NUMBER))
	{
		return false;
	}

	// Cache world-space spline point locations for every spline we intend to rebase.
	struct FSplineWorldPoints
	{
		TObjectPtr<USplineComponent> Spline = nullptr;
		TArray<FVector> WorldPoints;
	};

	TArray<FSplineWorldPoints> CachedSplines;
	CachedSplines.Reserve(SplinesToRebase.Num());

	for (USplineComponent* Spline : SplinesToRebase)
	{
		if (!IsValid(Spline))
		{
			continue;
		}

		const int32 NumPoints = Spline->GetNumberOfSplinePoints();
		FSplineWorldPoints Entry;
		Entry.Spline = Spline;
		Entry.WorldPoints.Reserve(NumPoints);

		for (int32 i = 0; i < NumPoints; ++i)
		{
			Entry.WorldPoints.Add(Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
		}

		CachedSplines.Add(MoveTemp(Entry));
	}

	// Move actor (XY only; preserve Z). We'll rebase spline points afterwards so the world-space shape remains identical.
	ActorToMove->SetActorLocation(NewActorLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// Rebase points for each spline to keep their world-space positions unchanged.
	for (const FSplineWorldPoints& Entry : CachedSplines)
	{
		USplineComponent* Spline = Entry.Spline.Get();
		if (!IsValid(Spline))
		{
			continue;
		}

		const FTransform ComponentTransform = Spline->GetComponentTransform();
		const int32 NumPoints = Spline->GetNumberOfSplinePoints();
		const int32 NumCached = Entry.WorldPoints.Num();
		const int32 N = FMath::Min(NumPoints, NumCached);

		for (int32 i = 0; i < N; ++i)
		{
			const FVector NewLocal = ComponentTransform.InverseTransformPosition(Entry.WorldPoints[i]);
			Spline->SetLocationAtSplinePoint(i, NewLocal, ESplineCoordinateSpace::Local, false);
		}

		Spline->UpdateSpline();
	}

	return true;
}


