// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ClipperUtils.generated.h"

enum class WORLDBLDRUNTIME_API EClipperJoinType : uint8
{
	Square = 0,
	Bevel = 1,
	Round = 2,
	Miter = 3
};

enum class WORLDBLDRUNTIME_API EClipperFillRule : uint8
{
	EvenOdd = 0,
	NonZero = 1,
	Positive = 2,
	Negative = 3
};

enum class WORLDBLDRUNTIME_API EClipperEndType : uint8
{
	Polygon = 0,
	Joined = 1,
	Butt = 2,
	Square = 3,
	Round = 4
};

/**
 * Utility class wrapping Clipper2 library functions for polygon operations.
 * Provides Unreal-friendly interface using TArray<FVector2D> instead of Clipper2 internal types.
 */
UCLASS()
class WORLDBLDRUNTIME_API UClipperUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Performs a Union operation on a single polygon to resolve self-intersections.
	 * @param Points - Input polygon points (2D)
	 * @param FillRule - Fill rule to use (default: NonZero)
	 * @return Array of result polygons (Union may split self-intersecting polygon into multiple)
	 */
	static TArray<TArray<FVector2D>> UnionPolygon2D(const TArray<FVector2D>& Points, EClipperFillRule FillRule = EClipperFillRule::NonZero);

	/**
	 * Performs a Union operation on multiple polygons.
	 * @param Polygons - Array of input polygons
	 * @param FillRule - Fill rule to use (default: NonZero)
	 * @return Array of result polygons
	 */
	static TArray<TArray<FVector2D>> UnionPolygons2D(const TArray<TArray<FVector2D>>& Polygons, EClipperFillRule FillRule = EClipperFillRule::NonZero);

	/**
	 * Offsets (inflates/deflates) a closed polygon path.
	 * @param Points - Input polygon points (2D)
	 * @param Delta - Offset amount (positive = inflate, negative = deflate)
	 * @param JoinType - How to handle corners (default: Round)
	 * @param MiterLimit - Limit for miter joins (default: 2.0)
	 * @param ArcTolerance - Tolerance for arc approximation (default: 0.0 = auto)
	 * @return Array of result polygons (offset may produce multiple polygons)
	 */
	static TArray<TArray<FVector2D>> OffsetPolygon2D(
		const TArray<FVector2D>& Points,
		double Delta,
		EClipperJoinType JoinType = EClipperJoinType::Round,
		double MiterLimit = 2.0,
		double ArcTolerance = 0.0);

	/**
	 * Offsets (inflates/deflates) multiple polygon paths.
	 * @param Polygons - Array of input polygons
	 * @param Delta - Offset amount (positive = inflate, negative = deflate)
	 * @param JoinType - How to handle corners (default: Round)
	 * @param EndType - How to handle open path ends (default: Polygon for closed paths)
	 * @param MiterLimit - Limit for miter joins (default: 2.0)
	 * @param ArcTolerance - Tolerance for arc approximation (default: 0.0 = auto)
	 * @return Array of result polygons
	 */
	static TArray<TArray<FVector2D>> OffsetPolygons2D(
		const TArray<TArray<FVector2D>>& Polygons,
		double Delta,
		EClipperJoinType JoinType = EClipperJoinType::Round,
		EClipperEndType EndType = EClipperEndType::Polygon,
		double MiterLimit = 2.0,
		double ArcTolerance = 0.0);

	static bool RemoveSelfIntersections(const TArray<FVector>& AllOuterPoints, TArray<FVector>& CorrectedOuterPoints);

	/**
	 * Offsets a 3D polygon while preserving Z coordinates through interpolation.
	 * Converts input to 2D, performs offset operation, then restores Z coordinates.
	 * If multiple polygons are produced, selects the largest by point count.
	 *
	 * @param Points - Input 3D polygon points
	 * @param Delta - Offset amount (positive = outward/inflate, negative = inward/inset)
	 * @param OutOffsetPoints - Output 3D polygon points with interpolated Z coordinates
	 * @param JoinType - How to handle corners (default: Round)
	 * @param MiterLimit - Limit for miter joins (default: 2.0)
	 * @param ArcTolerance - Tolerance for arc approximation (default: 0.0 = auto)
	 * @return true if successful, false if operation failed or produced empty result
	 */
	static bool OffsetPolygon3D(
		const TArray<FVector>& Points,
		double Delta,
		TArray<FVector>& OutOffsetPoints,
		EClipperJoinType JoinType = EClipperJoinType::Round,
		double MiterLimit = 2.0,
		double ArcTolerance = 0.0);

	/**
	 * Offsets a polyline (open path) to the left or right.
	 * @param Polyline - Input polyline points (2D, double precision)
	 * @param Delta - Offset amount and side (Positive = left, Negative = right, magnitude = distance)
	 * @param Result - Output polygon representing the offset area
	 * @param JoinType - How to handle corners (default: Round)
	 * @param EndType - How to handle open path ends (default: Round)
	 * @param MiterLimit - Limit for miter joins (default: 2.0)
	 * @param ArcTolerance - Tolerance for arc approximation (default: 0.0 = auto)
	 */
	static void OffsetPolyline(
		const TArray<FVector2d>& Polyline,
		float Delta,
		TArray<FVector2d>& Result,
		EClipperJoinType JoinType = EClipperJoinType::Bevel,
		EClipperEndType EndType = EClipperEndType::Butt,
		double MiterLimit = 2.0,
		double ArcTolerance = 0.0);

};

