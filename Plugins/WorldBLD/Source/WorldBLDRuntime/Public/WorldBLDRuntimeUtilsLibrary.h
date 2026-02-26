// Copyright WorldBLD LLC. All rights reserved. 

#pragma once

#include "Components/SplineComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Curves/CurveFloat.h"
#include "Engine/StaticMeshSocket.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "WorldBLDRuntimeUtilsLibrary.generated.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

WORLDBLDRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogWorldBLD, Log, All);

#ifndef WORLDBLD_LOG
#define WORLDBLD_LOG(CategoryName, Result, Verbosity, Format, ...) \
	{\
		UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__); \
		if (ELogVerbosity::Verbosity == ELogVerbosity::Error) \
		{ \
			Result.Errors.Add(FString::Printf(Format, ##__VA_ARGS__)); \
		} \
	}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

UENUM(BlueprintType)
enum class ESplineSampleMode : uint8
{
	// We uniformly traverse the distance of the spline.
	Uniform = 0,

	// We calculate the curvature of the spline while we traverse the spline.
	CurvatureThreshold,
};

USTRUCT(BlueprintType)
struct FSplineSamplingParams
{
	GENERATED_BODY()

	// How we sample the spline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines")
	ESplineSampleMode SamplingMode {ESplineSampleMode::CurvatureThreshold};

	// [Uniform] We increment the distance by this value when returning samples back.
	// [CurvatureThreshold] We increment the distance by this value while we calculate curvature values. Smaller means we get a more accurate curvature.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines")
	float SamplingDistance {5.0f};

	// Accumulate curvature before we sample a point (measure in Degrees).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines", Meta=(EditCondition="SamplingMode==ESplineSampleMode::CurvatureThreshold", EditConditionHides))
	float CurvatureThreshold {2.0f};

	// [CurvatureThreshold] The max distance we are allowed to travel along a "straight" line before forcing a sample to happen.
	// The allows you to intermix curvature threshold and uniform sampling.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines", Meta=(EditCondition="SamplingMode==ESplineSampleMode::CurvatureThreshold", EditConditionHides))
	float ForceMaxTravelDistance {-1.0f};

	/// @TODO: Experimental
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines", Meta=(EditCondition="SamplingMode==ESplineSampleMode::CurvatureThreshold", EditConditionHides))
	bool bQuickSampleLinearNeighbors {false};

	// Whether or not to remove self-intersections as we sample the spline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines")
	bool bRemoveSelfIntersections {false};

	// The height offset between two "intersecting" segments that must be met in order for us to consider it a self-intersection.
	// If this value is < 0 then it doesn't take effect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines", Meta=(EditCondition="bRemoveSelfIntersections", EditConditionHides))
	float SelfIntersectionZHeightThreshold {-1.0f};

	// The winding value threshold, used when considering whether a loop is "insignificant". Think of this like the area of the loop. 
	// Loops with an area smaller than this are considered super tiny and not part of the normal spline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines", Meta=(EditCondition="bRemoveSelfIntersections", EditConditionHides))
	float SelfIntersectionWindingThreshold {0.0f};

	// The minimum distance between placed points before we consider placing it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines")
	float NewPointDistanceThreshold {-1.0f};

	// (Optional)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines")
	float StartDistance {-1.0f};

	// (Optional)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines")
	float EndDistance {-1.0f};
};


USTRUCT(BlueprintType)
struct FSplineCopyOffsetParams
{
	GENERATED_BODY()

	// The offset to apply to the source spline along the Left/Right direction relative to the spline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines")
	float OffsetRight {0.0f};

	// The offset to apply to the source spline along the Up/Down direction relative to the spline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines")
	float OffsetUp {0.0f};

	// (C++ Only) An Offset function as we sample from the start to the end of the spline. This overrides OffsetRight and OffsetUp.
	TFunction<void(float /* Distance */, float& /* OutOffsetRight */, float& /* OutOffsetUp*/)> OffsetFunc {nullptr};

	// Whether to ignore the first and last points of the spline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines")
	bool bTrimEdgePoints {false};

	// Whether to move the owning actor of the Destination spline to be the same location as the Source spline's actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines")
	bool bMoveDestActorToRootOfSpline {true};

	// Whether or not to simply copy the spline before applying the offset. Setting this to true ignors the 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines")
	bool bSimpleCopyOnly {false};

	// [bSimpleCopyOnly=false] How to resample the source spline for making the copy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines")
	FSplineSamplingParams SplineSampleSettings;
};

USTRUCT(BlueprintType)
struct FSplineCopyOffsetOutput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Splines")
	bool bSuccess {false};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Splines")
	TArray<FString> Errors;

	// The chirality of the source spline.
	// INTERNAL NOTE: you MUST set this before calling FixupLinearSplineSelfIntersections()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines")
	bool bSourceClockwiseWinding {false};

	// The winding value of the source spline.
	// INTERNAL NOTE: you MUST set this before calling FixupLinearSplineSelfIntersections()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splines")
	float SourceWindingValue {0.0f};
};

USTRUCT(BlueprintType)
struct FInnerRectangleGenParams
{
	GENERATED_BODY()

	// About how far the inner and outer splines are from one another.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	float OffsetDistanceHint {100.0f};

	// Minimum distance between sample lot edge points.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	float MinDistanceDifference {30.0f};

	// The range of allowed lot lengths (along the inner spline).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	FFloatRange LotLengthRange;

	// Whether or not smaller lots are merged into the previous lot (within a % tolerance).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	bool bMergeSmallerLots {true};

	// How we sample along the splines as we create lot boundaries. SamplingMode must be set to Curvature!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	FSplineSamplingParams SplineSampleSettings;
};

USTRUCT(BlueprintType)
struct FInnerRectangleDescription
{
	GENERATED_BODY()

	// The intended length of the Lot (randomly chosen value).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	float DesiredLotLength {0.0f};

	// Where along the inner spline the Lot starts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	float InnerStartDistance {0.0f};

	// The sampled distances along the inner spline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	TArray<float> InnerDistances;

	// The total distance sampled along the inner spline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	float TotalInnerDistance {0.0f};

	// The sampled distances along the outer spline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	TArray<float> OuterDistances;

	// The total distance sampled along the outer spline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	float TotalOuterDistance {0.0f};

	// The largest encountered angle while traversing the inner spline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	float LargestInnerBendAngle {0.0f};

	// The largest encountered angle while traversing the outer spline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	float LargestOuterBendAngle {0.0f};
};

USTRUCT(BlueprintType)
struct FInnerRectangleGenOutput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	bool bSuccess {false};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	TArray<FString> Errors;

	// The lots generated rectangles (lots) from GenerateInnerRectanglesBetweenSplines().
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	TArray<FInnerRectangleDescription> CalculatedLots;
};

USTRUCT(BlueprintType)
struct FFillLotParams
{
	GENERATED_BODY()

	// If the lot output points should be flattened (Z = average spline point height).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	bool bFlattenLot {true};

	// If bFlattenLot=true, where the lots sits relative to the min/average/max.
	// [-1] = min
	// [ 0] = avg
	// [+1] = max
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	float ZAnchor {1.0f};

	// RNG seed when computing offset variances.
	// Only used when `InnerOffsetVariance` is bounded.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	int32 VarianceSeed {-1};

	// Random variance (in Unreal units) from the inner spline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	FFloatRange InnerOffsetVariance;

	// Largest acceptable angle along inner and outer splines for us to allow offset variance.
	// Only used when `InnerOffsetVariance` is bounded.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	float VarianceAngleThreshold {30.0f};
};

USTRUCT(BlueprintType)
struct FFillLotResult
{
	GENERATED_BODY()

	// Whether or not the spline point at the given index belongs to the outer spline that generated it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	TArray<bool> IsOuterArray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lots")
	TArray<int32> RemovedPointsDuringCleanup;
};

USTRUCT(BlueprintType)
struct FEdgeLoopTriangulationResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	TArray<int32> TriangleIndices;
};

USTRUCT(BlueprintType)
struct FCleanupSplineParams
{
	GENERATED_BODY()

	/////////////////////////////////////////////////////////////////

	// Whether to remove linearly redundant points along the spline.
	// For a point to be linearly redundant, it needs to exist between two other linear points
	// and the angle of deflection must be 0 degrees. The tolerance of this is controlled by
	// `LinearPointDeflectionThreshold`.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Params")
	bool bRemoveRedundantLinearPoints {true};
	
	// Linear deflection threshold used when bRemoveRedundantLinearPoints=true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Params")
	float LinearPointDeflectionThreshold {0.1f};

	/////////////////////////////////////////////////////////////////

	// If a corner has too many points close together, simplify them into a single point.
	// Controlled by `MultipointCornerRadius`.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Params")
	bool bRemoveMultipointCorners {true};

	// Distance between points used when bRemoveMultipointCorners=true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Params")
	float MultipointCornerRadius {10.0f};

	/////////////////////////////////////////////////////////////////

	// Whether to simply collapse points that are too close together. SEE: CollapseRadius
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Params")
	bool bCollapseNearbyPoints {false};

	// Linear distance between points before they get collapsed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Params")
	float CollapseRadius {1.0f};
	
	/////////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Params")
	bool bRemoveQuickReversals {false};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Params")
	float QuickReversalDistanceThreshold {10.0f};

	/////////////////////////////////////////////////////////////////

};

USTRUCT(BlueprintType)
struct FCleanupSplineResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	TArray<int32> RemovedPoints;
};

struct FSplineSample
{
	float Distance {0.0f};
	FVector Location {FVector::ZeroVector};
	bool bIsLinearCorner {false};
	FVector Inset {FVector::ZeroVector};
};

USTRUCT(BlueprintType)
struct FExtractCurveParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Params")
	FVector2D CurveScale {1.0f, 1.0f};

	/// @TODO: Add sample settings specifically for Cubic points in a curve
};

USTRUCT(BlueprintType)
struct FSplineIntersectionTestParams
{
	GENERATED_BODY()

	// Sample settings used for curved segments.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Params")
	FSplineSamplingParams SampleSettings;

	// Z Distance check between segments that still allows for an intersection test to pass.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Params")
	float ZDistanceThreshold {100.0f};
};

USTRUCT(BlueprintType)
struct FSplineDistancePair
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Values")
	float DistanceSplineA {0.0f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Values")
	float DistanceSplineB {0.0f};
};

USTRUCT(BlueprintType)
struct FSelfIntersectionParams
{
	GENERATED_BODY()

	// What Z distances are considered for collapsing. If the distance is less than this threshold then we will de-intersect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Params")
	float ZHeightThreshold {-1.0f};

	// What winding we consider "insignificant" and should be preferred to be cut.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Params")
	float WindingThreshold {0.0f};

	// The max distance we will consider between points before you de-intersect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Params")
	float MaxDistanceBetweenPoints {-1.0f};
};

USTRUCT(BlueprintType)
struct FSplineIntersectionTestOutput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	TArray<FSplineDistancePair> Intersections;
};

USTRUCT(BlueprintType)
struct FSplineMeshData
{
	GENERATED_BODY()

	// The mesh used by this spline mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Mesh")
	class UStaticMesh* Mesh {nullptr};

	// The distance along the spline we start this mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Mesh")
	double StartDistance {0.0};

	// The distance along the spline we end this mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline Mesh")
	double EndDistance {0.0};
};

UCLASS(Blueprintable, BlueprintType, Abstract)
class WORLDBLDRUNTIME_API UPolylineModifierModule : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Modify")
	void ApplyModifier(FVector SweepMinWS, bool bMinEdgeIsPrimary, 
			FVector SweepMaxWS, bool bMaxEdgeIsPrimary,
			UPARAM(ref) TArray<FVector2D>& InOutLocalSweepEdge) const;

protected:
	virtual void ApplyModifier_Implementation(FVector SweepMinWS, bool bMinEdgeIsPrimary, 
			FVector SweepMaxWS, bool bMaxEdgeIsPrimary,
			UPARAM(ref) TArray<FVector2D>& InOutLocalSweepEdge) const {}
};

UCLASS(Blueprintable, BlueprintType)
class UFlattenPolylineAlongPrimaryEdge : public UPolylineModifierModule
{
	GENERATED_BODY()
protected:
	virtual void ApplyModifier_Implementation(FVector SweepMinWS, bool bMinEdgeIsPrimary, 
			FVector SweepMaxWS, bool bMaxEdgeIsPrimary,
			UPARAM(ref) TArray<FVector2D>& InOutLocalSweepEdge) const override;
};

USTRUCT(BlueprintType)
struct FPolylineModifierSpec 
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TSubclassOf<UActorComponent> OverlappingComponentClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TSubclassOf<UPolylineModifierModule> ModifierModuleClass {nullptr};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float TraceRadius {10.0f};
};



struct FSplineSpitShineCleanupParameters
{
	USplineComponent* ExistingSpline {nullptr};
	bool bCleanup {true};
	FCleanupSplineParams CleanupParams;
	bool bRemoveSelfIntersection {true};
	FSelfIntersectionParams IntersectionParams;
	bool bAutoCloseLoop {false};
};

USTRUCT(BlueprintType)
struct FSpitShineMySplineParams
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bCleanup {true};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FCleanupSplineParams CleanupParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bRemoveSelfIntersection {true};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FSelfIntersectionParams IntersectionParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bAutoCloseLoop {false};
};

///////////////////////////////////////////////////////////////////////////////////////////////////

// General utilities used by WorldBLD tools
UCLASS()
class WORLDBLDRUNTIME_API UWorldBLDRuntimeUtilsLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Modular Building Utilities

	//Adds an instancedMeshComponent to any given actor
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static UInstancedStaticMeshComponent* AddInstancedMeshComponentToActor(AActor* Actor);

	//Removes all InstancedMeshComponents from a given actor for cleanup
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void RemoveInstancedStaticMeshComponents(AActor* TargetActor);

	//This shows us all the InstancedStaticMeshComponents that this actor has
	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static TArray<UInstancedStaticMeshComponent*> GetInstancedMeshComponentsFromActor(const AActor* Actor);

	// Determines if the provided number is within one of the ranges
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils|Misc")
	static bool IsValueInRanges(const TArray<FVector2D>& Ranges, double ValueToCheck);

	//This adds a temporary spline from an array of vectors, for use with building generation logic
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static USplineComponent* AddClosedLinearSplineToActor(AActor* Actor, const TArray<FVector>& Points);

	//This cleans up any spline components on the actor
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void RemoveAllSplineComponentsFromActor(AActor* Actor);

	//Replaces a linear spline point with a circle. Spline is the spline component to use, CornerIndex is the index of the spline point to round out
	UFUNCTION(BlueprintCallable, Category = "Spline")
	static void RoundSplineCorner(USplineComponent* Spline, int32 CornerIndex, float Radius);

	//Gets the distance we need to offset a linear spline point to avoid points overlapping when the spline point is converted to a circular turn.
	UFUNCTION(BlueprintCallable, Category = "Spline")
	static float GetCircularTurnDistanceOffset(FVector PreviousPoint, FVector CornerPoint, FVector NextPoint, float Radius);

	//This returns us a list of all the sockets on the static mesh used by this InstancedStaticMeshComponent.
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static TArray<FName> GetSocketNamesFromInstancedMesh(const UInstancedStaticMeshComponent* InstancedMeshComponent);

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static TArray<UStaticMeshSocket*> GetSocketsFromInstancedMesh(const UInstancedStaticMeshComponent* InstancedMeshComponent);

	//This will convert an array of vectors from World Space to an actor's Local Space
	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static TArray<FVector> ConvertWorldVectorsToLocal(const TArray<FVector>& WorldVectors, const FTransform& ActorTransform);

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// General Spline Utilities
	


	//This will smooth a spline along the Z axis
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void SmoothSplineVertically(USplineComponent* SplineComponent, float SmoothingValue);

	//Reverse the order and direction of a spline in-place
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void ReverseSpline(USplineComponent* Spline);


	//This will subtract distance ranges where we DON'T want to generate anything from an overall spline, returning only the allowed ranges.
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static TArray<FVector2D> CalulateAllowedDistanceRanges(float TotalSplineLength, const TArray<FVector2D>& ForbiddenCuts);

	//Calculate bridge cuts by getting the ranges along a spline where we have a blocking hit when we run a trace
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static TArray<FVector2D> GetBlockingHitRanges(
	USplineComponent* SplineComponent,
	int32 NumSamples,
	float TraceDistance,
	ECollisionChannel TraceChannel,
	bool bDrawDebug,
	float DebugDrawTime, TArray<AActor*> ActorsToIgnore);

	// Stores a copy-offset of `SourceSpline` into `DestSpline` using `Params`.
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void CopyAndOffsetSpline(class USplineComponent* SourceSpline, const FSplineCopyOffsetParams& Params, class USplineComponent* DestSpline,
			FSplineCopyOffsetOutput& OutResult);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void CleanupSpline(class USplineComponent* Spline, const FCleanupSplineParams& Params, FCleanupSplineResult& OutResult);

	// Recreates the spline in place, clearing out any potential dead data.
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void RebuildSpline(class USplineComponent* Spline);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void SpitShineMySpline(class USplineComponent* Spline, const FSpitShineMySplineParams& Params);

	// Samples the spline dynamically given the sample parameters.
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void SampleSplineToTransforms(class USplineComponent* Spline, FSplineSamplingParams SweepPathSampleParams, TArray<FTransform>& OutTransforms);

	// Returns the wrapped distance around Spline. If the spline is a closed loop, the value
	// is wrapped from the end to the start and vice-versa. If the spline is not a closed loop,
	// then the value is simply clamped to [0, Spline Len].
	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static float ComputeSplineWrappedDistance(class USplineComponent* Spline, float Distance);

	

	///////////////////////////////////////////////////////////////////////////////////////////////
	//  Blueprint Exposed Utils

	// Emits a Delaunay triangulation of the input EdgeLoop. This only works on the top-down projection
	// of the points (Z is ignored). So this won't work for vertical walls or general 3D shapes.
	//UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	//static void TriangulateEdgeLoop(const TArray<FVector>& EdgeLoop, FEdgeLoopTriangulationResult& OutResult, int32 StartIdx = 0, int32 Length = -1);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void ConvertStaticMeshesToInstances(AActor* TargetActor, const TArray<class UStaticMeshComponent*>& StaticMeshes, 
			bool bDestroyStaticMeshes, TArray<class UHierarchicalInstancedStaticMeshComponent*>& OutHISMs);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void CopyOverHismProperties(class UHierarchicalInstancedStaticMeshComponent* Source, class UHierarchicalInstancedStaticMeshComponent* Dest);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void GetKeyValuePairsFromCurve(UCurveFloat* Curve, TArray<FVector2D>& OutPairs);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void ExtractCurveSamples(UCurveFloat* Curve, FExtractCurveParams Params, TArray<FVector2D>& OutPairs);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void FindIntersectionsBetweenSplines(class USplineComponent* SplineA, class USplineComponent* SplineB, 
			FSplineIntersectionTestParams Params, FSplineIntersectionTestOutput& OutOutput);

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static bool SegmentIntersection2D(const FVector& SegmentStartA, const FVector& SegmentEndA, const FVector& SegmentStartB, const FVector& SegmentEndB, FVector& OutIntersectionPoint);

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static float ApproximateSurfaceAreaOfClosedSpline(class USplineComponent* Spline);
	

	//UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	//static TArray<float> CalculateAdaptivePolylineUVs(TArray<FTransform> SweepPath);
	
 //Function to invert an array of distance ranges along a spline
 //Returns all the distance ranges that are NOT covered by the input array
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static TArray<FVector2D> InvertSplineDistanceRanges(const TArray<FVector2D>& InRanges, float SplineLength);

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static void GetLocalBoundsOfComponent(USceneComponent* Component, FVector& OutOrigin, FVector& OutExtent, float& OutSphereRadius, FTransform Transform, bool bCached = true);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void FixupLinearSplineEndTangent(USplineComponent* Spline);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void SanitizeSplineComponent(USplineComponent* Spline, bool bUpdate = true);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void MakeCircularSpline(USplineComponent* Spline, FVector Center, float Radius, int32 NumPoints=4);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static double GetAngleAtSplinePoint(const USplineComponent* Spline, int32 Index);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void SetAllSplinePointTypes(USplineComponent* Spline, ESplinePointType::Type SplinePointType = ESplinePointType::Linear);

	// Places spline mesh components along a spline using the provided static mesh
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void PlaceSplineMeshesAlongSpline(USplineComponent* Spline, UStaticMesh* Mesh, AActor* OwnerActor);

	// Places spline meshes with higher fidelity by adaptively segmenting the spline by curvature and/or max segment length
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void PlaceSplineMeshesAlongSplineAdvanced(USplineComponent* Spline, UStaticMesh* Mesh, AActor* OwnerActor, float MaxSegmentLength = 0.0f, float CurvatureAngleThresholdDeg = 0.0f);

	// Helper function to calculate spline mesh data based on mesh bounds
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static TArray<FSplineMeshData> CalculateSplineMeshData(USplineComponent* Spline, UStaticMesh* Mesh);

	// Helper function to spawn spline mesh components based on calculated data
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void SpawnSplineMeshes(const TArray<FSplineMeshData>& MeshData, USplineComponent* Spline, AActor* OwnerActor);

	///////////////////////////////////////////////////////////////////////////////////////////////
	
	// Ported from CityBLD_SplineUtils Blueprint Function Library 

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static void  GetLengthOfSegment(USplineComponent* Spline, int32 Segment, float& Length);

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static void GetMidpointOfSegment(USplineComponent* Spline, int32 Segment, ESplineCoordinateSpace::Type CoordinateSpace, FTransform& Transform, float& Distance);

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static void ConvertSplineSampleFramesToUVs(const TArray<FTransform>& Frames, double StartDistance, double EndDistance, TArray<double>& VCoordinates);
		
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void SetAllSplinePointTypesToLinear(USplineComponent* Spline);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void GetNearestOfVectorArray(const TArray<FVector>& InputArray, const FVector& InputLocation, FVector& NearestVector, double& DistanceOfNearestVector, int32& NearestIndex);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void GetAngleOfLinearSplinePoint(const USplineComponent* Spline, int32 PointIndex, float& Angle);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void GetAngleBetweenForwardVectors(const FVector& VectorA, const FVector& VectorB, float& Angle);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void SortSplinePointsClockwise(USplineComponent* Spline);

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static void GetSplinePointsVectors(USplineComponent* Spline, ESplineCoordinateSpace::Type CoordinateSpace, TArray<FVector>& OutLocations);

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static void GetSplinePointsVectors2d(const USplineComponent* Spline, ESplineCoordinateSpace::Type CoordinateSpace, TArray<FVector2D>& OutLocations);

	///////////////////////////////////////////////////////////////////////////////////////////////

	static float FindNextDistanceThatMeetsCurvatureThreshold(USplineComponent* Spline,
		float StartingDistance, float DirectionDelta, float CutoffDistance, FSplineSamplingParams SampleSettings, bool* bAtCornerRef = nullptr);

	static void VisitPointsAlongSpline(USplineComponent* Spline, float StartingDistance, float CutoffDistance,
			const FSplineSamplingParams& Params, TFunction<bool(float)> DistanceCallback);

	static void FixupLinearSplineSelfIntersections(USplineComponent* Spline, const FSelfIntersectionParams& Params, FSplineCopyOffsetOutput& InOutResult);

	static bool CalculateSplineWinding(USplineComponent* BorderSpline, float& OutWindingValue);
	static bool CalculateSplineWinding(USplineComponent* Spline, int32 StartIdx, int32 Len, const TArray<FVector>& AdditionalPoints, float& OutWindingValue);
	static bool CalculateEdgeLoopWinding(const TArray<FVector>& EdgeLoop, float& OutWindingValue);

	static void CopySplineIntoSpline(USplineComponent* DestinationSpline, USplineComponent* SourceSpline, TArray<FVector>* OptionalOutPoints = nullptr);
	static void GetFlattenedSplineCurve(USplineComponent* Spline, FSplineCurves& OutFlattened);

	static bool SplineSegmentHasAnyCrossings(USplineComponent* Spline, float ZHeightThreshold, int32 StartIdx, int32 Len);

	static void CalculateLinearBendRightVectorAtDistance(class USplineComponent* Spline, float Distance, float SampleDelta, float& OutBendAngle,
			FVector& OutLocation, FVector& OutDirection, ESplineCoordinateSpace::Type CoordinateSpace = ESplineCoordinateSpace::Local);

	static void OffsetSpline_Internal(class USplineComponent* Spline, const FSplineCopyOffsetParams& OffsetParams, ESplineCoordinateSpace::Type CoordSpace, TArray<FSplineSample>& OutSamplePoints);

	///////////////////////////////////////////////////////////////////////////////////////////////

	static void SplineCurves_AddPoint(struct FSplineCurves& SplineCurves, const FVector& Position, const FQuat& Rotation = FQuat::Identity, const FVector& Scale = FVector(1), bool bLinear = true);
	static void SetSplineCurvesOnComponent(class USplineComponent* Spline, const struct FSplineCurves& SplineCurves);
	static void SplineCurves_Sanitize(struct FSplineCurves& SplineCurves);
	
	static void SpitShineMySplineCurves(struct FSplineCurves& SplineCurves, const FSplineSpitShineCleanupParameters& Params);
	static void ReverseSplineCurves(struct FSplineCurves& SplineCurves);

	///////////////////////////////////////////////////////////////////////////////////////////////

	static void SampleAdaptive(
		double Begin, double End,
		TArray<FVector>& OutPoints,
		TArray<double>& OutDistances,
		TFunctionRef<FVector(double Distance)> GetPositionAt,
		double MaxSquareDist = 0.05,
		double MinSquareSegLen = 1.0,
		double MaxSquareSegLen = 1e5);

	static void SampleAdaptiveToTransforms(
		double Begin,
		double End,
		TArray<FTransform>& OutSamples,
		TArray<double>& OutDistances,
		TFunctionRef<FTransform(double Distance)> GetPositionAt,
		double MaxSquareDist = 0.05,
		double MinSquareSegLen = 1.0,
		double MaxSquareSegLen = 1e5);

};

///////////////////////////////////////////////////////////////////////////////////////////////////
