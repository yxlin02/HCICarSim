// Copyright WorldBLD LLC. All rights reserved.

#include "ClipperUtils.h"
#include "clipper2/clipper.h"

namespace
{
	Clipper2Lib::PathD ToClipperPath(const TArray<FVector2D>& Points)
	{
		Clipper2Lib::PathD Result;
		Result.reserve(Points.Num());
		
		for (const FVector2D& Point : Points)
		{
			Result.push_back(Clipper2Lib::PointD(Point.X, Point.Y));
		}
		
		return Result;
	}

	Clipper2Lib::PathsD ToClipperPaths(const TArray<TArray<FVector2D>>& Polygons)
	{
		Clipper2Lib::PathsD Result;
		Result.reserve(Polygons.Num());
		
		for (const TArray<FVector2D>& Polygon : Polygons)
		{
			Result.push_back(ToClipperPath(Polygon));
		}
		
		return Result;
	}

	TArray<FVector2D> FromClipperPath(const Clipper2Lib::PathD& ClipperPath)
	{
		TArray<FVector2D> Result;
		Result.Reserve(ClipperPath.size());
		
		for (const Clipper2Lib::PointD& Point : ClipperPath)
		{
			Result.Add(FVector2D(Point.x, Point.y));
		}
		
		return Result;
	}

	TArray<TArray<FVector2D>> FromClipperPaths(const Clipper2Lib::PathsD& ClipperPaths)
	{
		TArray<TArray<FVector2D>> Result;
		Result.Reserve(ClipperPaths.size());
		
		for (const Clipper2Lib::PathD& Path : ClipperPaths)
		{
			Result.Add(FromClipperPath(Path));
		}
		
		return Result;
	}

	Clipper2Lib::FillRule ToClipperFillRule(EClipperFillRule FillRule)
	{
		switch (FillRule)
		{
		case EClipperFillRule::EvenOdd:
			return Clipper2Lib::FillRule::EvenOdd;
		case EClipperFillRule::NonZero:
			return Clipper2Lib::FillRule::NonZero;
		case EClipperFillRule::Positive:
			return Clipper2Lib::FillRule::Positive;
		case EClipperFillRule::Negative:
			return Clipper2Lib::FillRule::Negative;
		default:
			return Clipper2Lib::FillRule::NonZero;
		}
	}

	Clipper2Lib::JoinType ToClipperJoinType(EClipperJoinType JoinType)
	{
		switch (JoinType)
		{
		case EClipperJoinType::Square:
			return Clipper2Lib::JoinType::Square;
		case EClipperJoinType::Bevel:
			return Clipper2Lib::JoinType::Bevel;
		case EClipperJoinType::Round:
			return Clipper2Lib::JoinType::Round;
		case EClipperJoinType::Miter:
			return Clipper2Lib::JoinType::Miter;
		default:
			return Clipper2Lib::JoinType::Round;
		}
	}

	Clipper2Lib::EndType ToClipperEndType(EClipperEndType EndType)
	{
		switch (EndType)
		{
		case EClipperEndType::Polygon:
			return Clipper2Lib::EndType::Polygon;
		case EClipperEndType::Joined:
			return Clipper2Lib::EndType::Joined;
		case EClipperEndType::Butt:
			return Clipper2Lib::EndType::Butt;
		case EClipperEndType::Square:
			return Clipper2Lib::EndType::Square;
		case EClipperEndType::Round:
			return Clipper2Lib::EndType::Round;
		default:
			return Clipper2Lib::EndType::Polygon;
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////

	Clipper2Lib::PathD ConvertToClipperPath(const TArray<FVector>& Points)
	{
		Clipper2Lib::PathD Result;
		Result.reserve(Points.Num());

		for (const FVector& Point : Points)
		{
			Result.push_back(Clipper2Lib::PointD(Point.X, Point.Y));
		}

		return Result;
	}

	TArray<FVector> ConvertFromClipperPath(const Clipper2Lib::PathD& ClipperPath, const TArray<FVector>& SourcePoints)
	{
		TArray<FVector> Result;

		if (ClipperPath.empty() || SourcePoints.Num() == 0)
		{
			return Result;
		}

		Result.Reserve(ClipperPath.size());

		// Build cumulative distance array for source points to help with Z interpolation
		TArray<double> SourceCumulativeDistances;
		SourceCumulativeDistances.Reserve(SourcePoints.Num());
		SourceCumulativeDistances.Add(0.0);

		for (int32 i = 1; i < SourcePoints.Num(); i++)
		{
			double Distance = FVector::Dist2D(SourcePoints[i - 1], SourcePoints[i]);
			SourceCumulativeDistances.Add(SourceCumulativeDistances[i - 1] + Distance);
		}

		double TotalSourceLength = SourceCumulativeDistances.Last();

		// Convert each Clipper point back to FVector with interpolated Z
		for (const Clipper2Lib::PointD& ClipperPoint : ClipperPath)
		{
			FVector NewPoint(ClipperPoint.x, ClipperPoint.y, 0.0);

			// Find the closest point on the source path to interpolate Z value
			double MinDistSquared = TNumericLimits<double>::Max();
			int32 ClosestIndex = 0;

			for (int32 i = 0; i < SourcePoints.Num(); i++)
			{
				double DistSquared = FVector::DistSquared2D(NewPoint, SourcePoints[i]);
				if (DistSquared < MinDistSquared)
				{
					MinDistSquared = DistSquared;
					ClosestIndex = i;
				}
			}

			// For more accurate Z interpolation, check the segment the point is closest to
			if (ClosestIndex < SourcePoints.Num() - 1)
			{
				const FVector& P0 = SourcePoints[ClosestIndex];
				const FVector& P1 = SourcePoints[ClosestIndex + 1];

				// Project the point onto the segment to find interpolation factor
				FVector SegmentDir = P1 - P0;
				double SegmentLength2D = FVector::Dist2D(P0, P1);

				if (SegmentLength2D > KINDA_SMALL_NUMBER)
				{
					FVector ToPoint = NewPoint - P0;
					double ProjectionLength = (ToPoint.X * SegmentDir.X + ToPoint.Y * SegmentDir.Y) / SegmentLength2D;
					double T = FMath::Clamp(ProjectionLength / SegmentLength2D, 0.0, 1.0);
					NewPoint.Z = FMath::Lerp(P0.Z, P1.Z, T);
				}
				else
				{
					NewPoint.Z = P0.Z;
				}
			}
			else
			{
				// Use the closest point's Z directly
				NewPoint.Z = SourcePoints[ClosestIndex].Z;
			}

			Result.Add(NewPoint);
		}

		return Result;
	}

}

TArray<TArray<FVector2D>> UClipperUtils::UnionPolygon2D(
	const TArray<FVector2D>& Points,
	EClipperFillRule FillRule)
{
	if (Points.Num() < 3)
	{
		return TArray<TArray<FVector2D>>();
	}

	// Convert to Clipper path
	Clipper2Lib::PathsD SubjectPaths;
	SubjectPaths.push_back(ToClipperPath(Points));

	// Perform Union operation (resolves self-intersections)
	Clipper2Lib::PathsD SolutionPaths = Clipper2Lib::Union(
		SubjectPaths,
		ToClipperFillRule(FillRule)
	);

	return FromClipperPaths(SolutionPaths);
}

TArray<TArray<FVector2D>> UClipperUtils::UnionPolygons2D(
	const TArray<TArray<FVector2D>>& Polygons,
	EClipperFillRule FillRule)
{
	if (Polygons.Num() == 0)
	{
		return TArray<TArray<FVector2D>>();
	}

	// Convert to Clipper paths
	Clipper2Lib::PathsD SubjectPaths = ToClipperPaths(Polygons);

	// Perform Union operation
	Clipper2Lib::PathsD SolutionPaths = Clipper2Lib::Union(
		SubjectPaths,
		ToClipperFillRule(FillRule)
	);

	return FromClipperPaths(SolutionPaths);
}

TArray<TArray<FVector2D>> UClipperUtils::OffsetPolygon2D(
	const TArray<FVector2D>& Points,
	double Delta,
	EClipperJoinType JoinType,
	double MiterLimit,
	double ArcTolerance)
{
	if (Points.Num() < 3 || FMath::IsNearlyZero(Delta))
	{
		// Return original polygon if no offset needed
		if (Points.Num() >= 3)
		{
			TArray<TArray<FVector2D>> Result;
			Result.Add(Points);
			return Result;
		}
		return TArray<TArray<FVector2D>>();
	}

	// Convert to Clipper path
	Clipper2Lib::PathsD SubjectPaths;
	SubjectPaths.push_back(ToClipperPath(Points));

	// Perform offset operation
	Clipper2Lib::PathsD SolutionPaths = Clipper2Lib::InflatePaths(
		SubjectPaths,
		Delta,
		ToClipperJoinType(JoinType),
		Clipper2Lib::EndType::Polygon,
		MiterLimit,
		2,  // precision
		ArcTolerance
	);

	return FromClipperPaths(SolutionPaths);
}

TArray<TArray<FVector2D>> UClipperUtils::OffsetPolygons2D(
	const TArray<TArray<FVector2D>>& Polygons,
	double Delta,
	EClipperJoinType JoinType,
	EClipperEndType EndType,
	double MiterLimit,
	double ArcTolerance)
{
	if (Polygons.Num() == 0)
	{
		return TArray<TArray<FVector2D>>();
	}

	if (FMath::IsNearlyZero(Delta))
	{
		// Return original polygons if no offset needed
		return Polygons;
	}

	// Convert to Clipper paths
	Clipper2Lib::PathsD SubjectPaths = ToClipperPaths(Polygons);

	// Perform offset operation
	Clipper2Lib::PathsD SolutionPaths = Clipper2Lib::InflatePaths(
		SubjectPaths,
		Delta,
		ToClipperJoinType(JoinType),
		ToClipperEndType(EndType),
		MiterLimit,
		2,  // precision
		ArcTolerance
	);

	return FromClipperPaths(SolutionPaths);
}


void UClipperUtils::OffsetPolyline(
	const TArray<FVector2d>& Polyline,
	float Delta,
	TArray<FVector2d>& Result,
	EClipperJoinType JoinType,
	EClipperEndType EndType,
	double MiterLimit,
	double ArcTolerance)
{
	Result.Empty();
	if (Polyline.Num() < 2 || FMath::IsNearlyZero(Delta))
	{
		return;
	}
	
	// Convert to Clipper path
	Clipper2Lib::PathsD SubjectPaths;
	SubjectPaths.push_back(ToClipperPath(Polyline));

	Clipper2Lib::PathsD SolutionPaths = Clipper2Lib::InflatePaths(
		SubjectPaths,
		Delta,
		ToClipperJoinType(JoinType),
		ToClipperEndType(EndType),
		MiterLimit,
		2,  // precision
		ArcTolerance);

	// Return the first result polygon
	if (!SolutionPaths.empty())
	{
		Result = FromClipperPath(SolutionPaths[0]);
	}
}

bool UClipperUtils::RemoveSelfIntersections(const TArray<FVector>& AllOuterPoints, TArray<FVector>& CorrectedOuterPoints)
{

	// Convert to Clipper path (closed polygon)
	Clipper2Lib::PathD OuterClipperPath = ConvertToClipperPath(AllOuterPoints);

	bool bClipperSuccess = false;
	if (!OuterClipperPath.empty())
	{
		// Use Union operation on the path with itself to resolve self-intersections
		// Union will "fix" the self-intersecting polygon by resolving overlaps using the NonZero fill rule
		Clipper2Lib::PathsD SubjectPaths;
		SubjectPaths.push_back(OuterClipperPath);

		// Perform Union operation - this resolves self-intersections
		Clipper2Lib::PathsD SolutionPaths = Clipper2Lib::Union(
			SubjectPaths,
			Clipper2Lib::FillRule::NonZero  // NonZero fill rule for self-intersecting polygons
		);

		if (!SolutionPaths.empty())
		{
			// The union might produce multiple polygons if the original was highly self-intersecting
			// Use the largest one (by point count, or we could use area)
			size_t LargestIdx = 0;
			size_t MaxPoints = SolutionPaths[0].size();

			for (size_t i = 1; i < SolutionPaths.size(); i++)
			{
				if (SolutionPaths[i].size() > MaxPoints)
				{
					MaxPoints = SolutionPaths[i].size();
					LargestIdx = i;
				}
			}

			CorrectedOuterPoints = ConvertFromClipperPath(SolutionPaths[LargestIdx], AllOuterPoints);

			if (CorrectedOuterPoints.Num() >= 3)
			{
				bClipperSuccess = true;
				UE_LOG(LogTemp, Log, TEXT("GenerateSidewalkGeometryForPerimeterLoop: Clipper SUCCESS - Corrected outer points: %d (original: %d), Result polygons: %d"),
					CorrectedOuterPoints.Num(), AllOuterPoints.Num(), (int32)SolutionPaths.size());

				/*
				// Visualize corrected outer edges in magenta above the yellow original edges
				if (GetWorld())
				{
					const float CorrectedDebugHeight = 600.0f; // Higher than yellow (500)
					const FColor CorrectedEdgeColor = FColor::Magenta;

					for (int32 i = 0; i < CorrectedOuterPoints.Num(); i++)
					{
						FVector P1 = CorrectedOuterPoints[i] + FVector(0, 0, CorrectedDebugHeight);
						FVector P2 = CorrectedOuterPoints[(i + 1) % CorrectedOuterPoints.Num()] + FVector(0, 0, CorrectedDebugHeight);

						DrawDebugLine(GetWorld(), P1, P2, CorrectedEdgeColor, true, -1.0f, 0, 5.0f);
					}

					UE_LOG(LogTemp, Log, TEXT("GenerateSidewalkGeometryForPerimeterLoop: Magenta debug visualization enabled for corrected edges"));
				}
				*/
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GenerateSidewalkGeometryForPerimeterLoop: Clipper Union returned empty result"));
		}
	}
	return bClipperSuccess;
}

bool UClipperUtils::OffsetPolygon3D(
	const TArray<FVector>& Points,
	double Delta,
	TArray<FVector>& OutOffsetPoints,
	EClipperJoinType JoinType,
	double MiterLimit,
	double ArcTolerance)
{
	// Clear output array
	OutOffsetPoints.Empty();

	// Step 2.1: Input Validation
	if (Points.Num() < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("OffsetPolygon3D: Invalid input - requires at least 3 points, got %d"), Points.Num());
		return false;
	}

	if (FMath::IsNearlyZero(Delta))
	{
		// No offset needed, copy input to output
		OutOffsetPoints = Points;
		UE_LOG(LogTemp, Log, TEXT("OffsetPolygon3D: Delta is nearly zero, returning original polygon with %d points"), Points.Num());
		return true;
	}

	// Step 2.2: Convert to 2D
	TArray<FVector2D> Points2D;
	Points2D.Reserve(Points.Num());
	for (const FVector& Point : Points)
	{
		Points2D.Add(FVector2D(Point.X, Point.Y));
	}

	// Step 2.3: Perform Offset Operation
	TArray<TArray<FVector2D>> OffsetResults = OffsetPolygon2D(Points2D, Delta, JoinType, MiterLimit, ArcTolerance);

	if (OffsetResults.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("OffsetPolygon3D: Clipper offset operation returned empty result (Delta: %.2f, Input points: %d)"), Delta, Points.Num());
		return false;
	}

	// Step 2.4: Select Largest Result Polygon
	int32 LargestIdx = 0;
	int32 MaxPoints = OffsetResults[0].Num();

	if (OffsetResults.Num() > 1)
	{
		for (int32 i = 1; i < OffsetResults.Num(); i++)
		{
			if (OffsetResults[i].Num() > MaxPoints)
			{
				MaxPoints = OffsetResults[i].Num();
				LargestIdx = i;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("OffsetPolygon3D: Multiple polygons produced (%d), selected largest with %d points"), OffsetResults.Num(), MaxPoints);
	}

	// Step 2.5: Convert Back to 3D with Z-Interpolation
	// Convert the selected 2D polygon to Clipper PathD format
	Clipper2Lib::PathD ClipperPath = ToClipperPath(OffsetResults[LargestIdx]);

	// Use existing ConvertFromClipperPath to restore Z coordinates with interpolation
	OutOffsetPoints = ConvertFromClipperPath(ClipperPath, Points);

	// Step 2.6: Validation and Logging
	if (OutOffsetPoints.Num() >= 3)
	{
		UE_LOG(LogTemp, Log, TEXT("OffsetPolygon3D: SUCCESS - Input: %d points, Output: %d points, Delta: %.2f, Result polygons: %d"),
			Points.Num(), OutOffsetPoints.Num(), Delta, OffsetResults.Num());
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("OffsetPolygon3D: FAILED - Output polygon has insufficient points (%d), requires at least 3"), OutOffsetPoints.Num());
		OutOffsetPoints.Empty();
		return false;
	}
}