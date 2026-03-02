// Copyright WorldBLD LLC. All rights reserved. 

#include "WorldBLDRuntimeUtilsLibrary.h"
#include "IWorldBLDKitElementInterface.h"
#include "WorldBLDKitElementUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Algo/Reverse.h"
#include "Polygon2.h"
#include "Curve/GeneralPolygon2.h"
#include "Curve/CurveUtil.h"
#include "Generators/FlatTriangulationMeshGenerator.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/SplineMeshComponent.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_LOG_CATEGORY(LogWorldBLD);

///////////////////////////////////////////////////////////////////////////////////////////////////

UInstancedStaticMeshComponent* UWorldBLDRuntimeUtilsLibrary::AddInstancedMeshComponentToActor(AActor* Actor)
{
	// Check if the Actor is valid
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddInstancedMeshComponentToActor: Invalid Actor provided"));
		return nullptr;
	}

	// Check if we're in the editor and not playing or simulating
	if (!Actor->GetWorld() || !Actor->GetWorld()->IsEditorWorld() || Actor->GetWorld()->IsPlayInEditor())
	{
		UE_LOG(LogTemp, Warning, TEXT("AddInstancedMeshComponentToActor: This function should only be called in the editor, not at runtime"));
		return nullptr;
	}

	// Generate a unique name for the new component
	FName ComponentName = *FString::Printf(TEXT("InstancedStaticMeshComponent_%s"), *FGuid::NewGuid().ToString());

	// Create a new UInstancedStaticMeshComponent
	UInstancedStaticMeshComponent* NewComponent = NewObject<UInstancedStaticMeshComponent>(Actor, UInstancedStaticMeshComponent::StaticClass(), ComponentName, RF_Transactional);

	// Check if the component was created successfully
	if (!NewComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("AddInstancedMeshComponentToActor: Failed to create UInstancedStaticMeshComponent"));
		return nullptr;
	}

	// Add the component to the Actor
	Actor->AddInstanceComponent(NewComponent);
	// HLOD building only considers static primitives. Since this utility is explicitly editor-only,
	// default the new instanced component to Static mobility so it can participate in HLODs.
	NewComponent->SetMobility(EComponentMobility::Static);
	NewComponent->bEnableAutoLODGeneration = true;
	NewComponent->OnComponentCreated();
	NewComponent->RegisterComponent();

	// Attach the component to the Actor's root component
	if (Actor->GetRootComponent())
	{
		NewComponent->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}
	else
	{
		Actor->SetRootComponent(NewComponent);
	}

	// Mark the actor's package as dirty
	(void)Actor->MarkPackageDirty();

#if WITH_EDITOR
	// Notify the editor that the actor has been modified
	if (GEditor)
	{
		GEditor->Modify();
		GEditor->RedrawLevelEditingViewports();
	}
#endif

	// Return the newly created and added component
	return NewComponent;
}

void UWorldBLDRuntimeUtilsLibrary::RemoveInstancedStaticMeshComponents(AActor* TargetActor)
{
#if WITH_EDITOR
	if (!GEditor)
	{
		return;
	}
#endif // WITH_EDITOR

	// Early return if actor is invalid
	if (!TargetActor)
	{
		return;
	}

	// Get all components of the actor
	TArray<UActorComponent*> Components;
	TargetActor->GetComponents(Components);

	// Array to store components that need to be removed
	TArray<UInstancedStaticMeshComponent*> ComponentsToRemove;

	// Find all InstancedStaticMesh components
	for (UActorComponent* Component : Components)
	{
		if (UInstancedStaticMeshComponent* ISMComponent = Cast<UInstancedStaticMeshComponent>(Component))
		{
			ComponentsToRemove.Add(ISMComponent);
		}
	}


	// Remove each component
	for (UInstancedStaticMeshComponent* ComponentToRemove : ComponentsToRemove)
	{
		// Notify the component it will be destroyed
		ComponentToRemove->Modify();
		ComponentToRemove->DestroyComponent(true);
	}

	// Mark the actor's package as dirty
	(void)TargetActor->MarkPackageDirty();
}

TArray<UInstancedStaticMeshComponent*> UWorldBLDRuntimeUtilsLibrary::GetInstancedMeshComponentsFromActor(const AActor* Actor)
{
	
	TArray<UInstancedStaticMeshComponent*> InstancedComponents;

	
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetInstancedMeshComponentsFromActor: Invalid Actor provided"));
		return InstancedComponents;
	}

	
	Actor->GetComponents<UInstancedStaticMeshComponent>(InstancedComponents);

	return InstancedComponents;
}

bool UWorldBLDRuntimeUtilsLibrary::IsValueInRanges(const TArray<FVector2D>& Ranges, double ValueToCheck)
{

	// Iterate through each range in the array
	for (const FVector2D& Range : Ranges)
	{
		// Get min and max values from the Vector2D
		double MinValue = Range.X;
		double MaxValue = Range.Y;

		// Check if the value is within the current range (inclusive)
		if (ValueToCheck >= MinValue && ValueToCheck <= MaxValue)
		{
			return true; // Value is within this range, so return true immediately
		}
	}

	// If we've checked all ranges and found no match, return false
	return false;
}

USplineComponent* UWorldBLDRuntimeUtilsLibrary::AddClosedLinearSplineToActor(AActor* Actor, const TArray<FVector>& Points)
{
	// Validate input parameters
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddClosedLinearSplineToActor: Invalid Actor provided"));
		return nullptr;
	}

	if (Points.Num() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddClosedLinearSplineToActor: At least 2 points are required to create a spline"));
		return nullptr;
	}

	// Check if we're in the editor and not playing or simulating
	if (!Actor->GetWorld() || !Actor->GetWorld()->IsEditorWorld() || Actor->GetWorld()->IsPlayInEditor())
	{
		UE_LOG(LogTemp, Warning, TEXT("AddClosedLinearSplineToActor: This function should only be called in the editor, not at runtime"));
		return nullptr;
	}

	// Create a unique name for the spline component
	FName ComponentName = *FString::Printf(TEXT("SplineComponent_%s_%d"),
		*Actor->GetName(), Actor->GetComponents().Num());

	// Create the spline component
	USplineComponent* SplineComponent = NewObject<USplineComponent>(Actor,
		USplineComponent::StaticClass(), ComponentName, RF_Transactional);

	if (!SplineComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("AddClosedLinearSplineToActor: Failed to create SplineComponent"));
		return nullptr;
	}

	// Configure the spline component
	SplineComponent->bEditableWhenInherited = true;
	SplineComponent->SetClosedLoop(true);
	SplineComponent->ClearSplinePoints();

	// Add points to the spline - using the proper point addition method
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		// Add the point to the spline - no need to capture return value
		SplineComponent->AddSplinePoint(Points[i], ESplineCoordinateSpace::Local, false);

		// Configure the point to be linear type
		SplineComponent->SetSplinePointType(i, ESplinePointType::Linear, false);

		
	}

	// Update the spline after adding all points
	SplineComponent->UpdateSpline();

	// Register and attach the component
	Actor->AddInstanceComponent(SplineComponent);
	SplineComponent->OnComponentCreated();
	SplineComponent->RegisterComponent();

	// Attach to root component if it exists
	if (Actor->GetRootComponent())
	{
		SplineComponent->AttachToComponent(Actor->GetRootComponent(),
			FAttachmentTransformRules::KeepRelativeTransform);
	}
	else
	{
		Actor->SetRootComponent(SplineComponent);
	}

	// Mark the actor's package as dirty
	(void)Actor->MarkPackageDirty();

#if WITH_EDITOR
	// Update editor viewports
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(false);
	}
#endif

	return SplineComponent;
}

void UWorldBLDRuntimeUtilsLibrary::RemoveAllSplineComponentsFromActor(AActor* Actor)
{
	// First validate our input actor
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveAllSplineComponentsFromActor: Invalid Actor provided"));
		return;
	}

	// Check if we're in the editor and not playing or simulating
	// This is important because component removal in runtime vs editor requires different handling
	if (!Actor->GetWorld() || !Actor->GetWorld()->IsEditorWorld() || Actor->GetWorld()->IsPlayInEditor())
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveAllSplineComponentsFromActor: This function should only be called in the editor, not at runtime"));
		return;
	}

	// Get all spline components from the actor
	TArray<USplineComponent*> SplineComponents;
	Actor->GetComponents<USplineComponent>(SplineComponents);

	// If we found any spline components, we'll need to handle them properly
	if (SplineComponents.Num() > 0)
	{
		// Keep track if this actor needs a new root component
		bool bNeedsNewRoot = false;

		// Iterate through all found spline components
		for (USplineComponent* SplineComp : SplineComponents)
		{
			if (SplineComp)
			{
				// Check if this component is the root component
				if (Actor->GetRootComponent() == SplineComp)
				{
					bNeedsNewRoot = true;
				}

				// Detach from parent if attached
				SplineComp->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);

				// Deregister the component
				SplineComp->DestroyComponent(true);
			}
		}

		// If we removed the root component, we should try to set a new one
		if (bNeedsNewRoot)
		{
			// Try to find any other suitable component to be the root
			TArray<USceneComponent*> SceneComponents;
			Actor->GetComponents<USceneComponent>(SceneComponents);

			if (SceneComponents.Num() > 0)
			{
				Actor->SetRootComponent(SceneComponents[0]);
			}
		}

		// Mark the actor's package as dirty since we've modified it
		(void)Actor->MarkPackageDirty();

		// Update the editor viewports to reflect our changes
#if WITH_EDITOR
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(false);
		}
#endif
	}
}

void UWorldBLDRuntimeUtilsLibrary::RoundSplineCorner(USplineComponent* Spline, int32 CornerIndex, float Radius)
{
	if (!Spline || CornerIndex <= 0 || CornerIndex >= Spline->GetNumberOfSplinePoints() - 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid Spline!"));
		return;
	}

	if (CornerIndex <= 0 || CornerIndex >= Spline->GetNumberOfSplinePoints() - 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid Corner Index!"));
		return;
	}

	FVector PreviousPoint = Spline->GetLocationAtSplinePoint(CornerIndex - 1, ESplineCoordinateSpace::World);
	FVector CornerPoint = Spline->GetLocationAtSplinePoint(CornerIndex, ESplineCoordinateSpace::World);
	FVector NextPoint = Spline->GetLocationAtSplinePoint(CornerIndex + 1, ESplineCoordinateSpace::World);

	FVector PreviousDirection = (PreviousPoint - CornerPoint).GetSafeNormal();
	FVector NextDirection = (NextPoint - CornerPoint).GetSafeNormal();
	FVector Bisector = (PreviousDirection + NextDirection).GetSafeNormal();

	float HalfAngle = FMath::Acos(FVector::DotProduct(PreviousDirection, NextDirection)) / 2.0f;
	float Distance = Radius / FMath::Sin(HalfAngle);


	FVector CircleCenter = CornerPoint + Bisector * Distance;


	FVector Normal = FVector::CrossProduct(PreviousDirection, NextDirection).GetSafeNormal();
	FVector PreviousDirectionTangent = FVector::CrossProduct(PreviousDirection, Normal).GetSafeNormal();
	FVector NextDirectionTangent = FVector::CrossProduct(Normal, NextDirection).GetSafeNormal();

	FVector NewPoint0 = CircleCenter + PreviousDirectionTangent * Radius;
	FVector NewPoint1 = CircleCenter + NextDirectionTangent * Radius;



	int32 InsertIndex = CornerIndex;
	Spline->RemoveSplinePoint(CornerIndex, true);
	Spline->AddSplinePointAtIndex(NewPoint0, InsertIndex, ESplineCoordinateSpace::World, true);
	Spline->AddSplinePointAtIndex(NewPoint1, InsertIndex + 1, ESplineCoordinateSpace::World, true);

	float CircleSegment45Degrees = PI / 4.0f;
	float CircleSegment90Degrees = PI / 2.0f;

	// 1.12f and 1.5f are magic numbers discovered through trial and error. I'd rather use a more precise formula, but it appears one doesn't exist.
	float CurveAdjustment = FMath::GetMappedRangeValueUnclamped(FVector2D(CircleSegment45Degrees, CircleSegment90Degrees), FVector2D(1.12f, 1.5f), 2.0f * HalfAngle);
	CurveAdjustment = (CurveAdjustment != 0.0f) ? CurveAdjustment : 0.001f;

	FVector Tangent0 = (CornerPoint - NewPoint0) * HalfAngle * PI / CurveAdjustment;
	FVector Tangent1 = (NewPoint1 - CornerPoint) * HalfAngle * PI / CurveAdjustment;

	Spline->SetTangentAtSplinePoint(InsertIndex, Tangent0, ESplineCoordinateSpace::World);
	Spline->SetTangentAtSplinePoint(InsertIndex + 1, Tangent1, ESplineCoordinateSpace::World);


	Spline->UpdateSpline();
}

float UWorldBLDRuntimeUtilsLibrary::GetCircularTurnDistanceOffset(FVector PreviousPoint, FVector CornerPoint, FVector NextPoint, float Radius)
{

	FVector PreviousDirection = (PreviousPoint - CornerPoint).GetSafeNormal();
	FVector NextDirection = (NextPoint - CornerPoint).GetSafeNormal();
	FVector Bisector = (PreviousDirection + NextDirection).GetSafeNormal();

	float HalfAngle = FMath::Acos(FVector::DotProduct(PreviousDirection, NextDirection)) / 2.0f;
	float Distance = Radius / FMath::Sin(HalfAngle);

	return Distance;
}

TArray<FName> UWorldBLDRuntimeUtilsLibrary::GetSocketNamesFromInstancedMesh(const UInstancedStaticMeshComponent* InstancedMeshComponent)
{
	// Initialize our return array
	TArray<FName> SocketNames;

	// First, validate our input component
	if (!InstancedMeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetSocketNamesFromInstancedMesh: Invalid InstancedStaticMeshComponent provided"));
		return SocketNames;
	}

	// Get the static mesh asset from the component
	UStaticMesh* StaticMesh = InstancedMeshComponent->GetStaticMesh();
	if (!StaticMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetSocketNamesFromInstancedMesh: No StaticMesh asset found in component"));
		return SocketNames;
	}

	// Get all socket names from the static mesh
	const TArray<TObjectPtr<UStaticMeshSocket>>& Sockets = StaticMesh->Sockets;
	for (const TObjectPtr<UStaticMeshSocket> Socket : Sockets)
	{
		if (Socket)
		{
			SocketNames.Add(Socket->SocketName);
		}
	}

	return SocketNames;
}

TArray<UStaticMeshSocket*> UWorldBLDRuntimeUtilsLibrary::GetSocketsFromInstancedMesh(const UInstancedStaticMeshComponent* InstancedMeshComponent)
{
	// Initialize our return array for the socket pointers
	TArray<UStaticMeshSocket*> SocketRefs;

	// Validate our input component - error handling is important since we're dealing with pointers
	if (!InstancedMeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetSocketsFromInstancedMesh: Invalid InstancedStaticMeshComponent provided"));
		return SocketRefs;
	}

	// Get the static mesh asset - we need this to access its sockets
	UStaticMesh* StaticMesh = InstancedMeshComponent->GetStaticMesh();
	if (!StaticMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetSocketsFromInstancedMesh: No StaticMesh asset found in component"));
		return SocketRefs;
	}

	// Simply return the array of socket pointers
	// The Sockets array is already a TArray<UStaticMeshSocket*>, exactly what we want
	return StaticMesh->Sockets;
}

TArray<FVector> UWorldBLDRuntimeUtilsLibrary::ConvertWorldVectorsToLocal(const TArray<FVector>& WorldVectors, const FTransform& ActorTransform)
{
	// Initialize our output array for the local space vectors
	TArray<FVector> LocalVectors;

	// Reserve space in our output array to avoid reallocations
	// This is a small optimization that helps when working with larger arrays
	LocalVectors.Reserve(WorldVectors.Num());

	// Create an inverse transform that we'll use to convert from world to local space
	// We only need to calculate this once, rather than for each vector
	const FTransform InverseTransform = ActorTransform.Inverse();

	// Process each world vector
	for (const FVector& WorldPos : WorldVectors)
	{
		// TransformPosition handles the full transformation including rotation, translation, and scale
		// It's more accurate than doing the math manually
		FVector LocalPos = InverseTransform.TransformPosition(WorldPos);
		LocalVectors.Add(LocalPos);
	}

	return LocalVectors;
}

void UWorldBLDRuntimeUtilsLibrary::SmoothSplineVertically(USplineComponent* SplineComponent, float SmoothingValue)
{
	if (!SplineComponent || SmoothingValue <= 0.0f)
	{
		return;
	}

	// Get the number of spline points
	const int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();
	if (NumPoints < 3)  // Need at least 3 points for meaningful smoothing
	{
		return;
	}


	// Create arrays to store original and smoothed positions
	TArray<FVector> OriginalPositions;
	TArray<FVector> SmoothedPositions;
	OriginalPositions.Reserve(NumPoints);
	SmoothedPositions.Reserve(NumPoints);

	// Store original positions
	for (int32 i = 0; i < NumPoints; i++)
	{
		OriginalPositions.Add(SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
	}

	// Calculate smoothed positions
	for (int32 i = 0; i < NumPoints; i++)
	{
		// Initialize with original position
		FVector CurrentPos = OriginalPositions[i];
		float TotalWeight = 1.0f;
		float SmoothedZ = CurrentPos.Z;

		// Apply smoothing based on distance to neighboring points
		for (int32 j = 0; j < NumPoints; j++)
		{
			if (i != j)  // Skip current point
			{
				// Calculate horizontal distance (ignoring Z)
				FVector Delta = OriginalPositions[j] - CurrentPos;
				Delta.Z = 0;  // Only consider horizontal distance
				float Distance = Delta.Size();

				// Calculate weight based on distance and smoothing value
				// Higher smoothing values increase the influence of distant points
				float Weight = FMath::Exp(-Distance / (SmoothingValue * 100.0f));

				// Add weighted contribution to Z coordinate
				SmoothedZ += OriginalPositions[j].Z * Weight;
				TotalWeight += Weight;
			}
		}

		// Calculate final smoothed position, ensuring we only move downward
		FVector SmoothedPos = CurrentPos;
		float NormalizedSmoothedZ = SmoothedZ / TotalWeight;

		// Only apply the smoothed Z if it's lower than the original position
		SmoothedPos.Z = FMath::Min(CurrentPos.Z, NormalizedSmoothedZ);
		SmoothedPositions.Add(SmoothedPos);
	}

	// Update spline points with new positions
	// Maintain original tangents and other properties
	for (int32 i = 0; i < NumPoints; i++)
	{
		FSplinePoint Point = SplineComponent->GetSplinePointAt(i, ESplineCoordinateSpace::World);
		Point.Position = SmoothedPositions[i];
		SplineComponent->SetLocationAtSplinePoint(i, Point.Position, ESplineCoordinateSpace::World);
	}

	// Update spline
	SplineComponent->UpdateSpline();

	// Mark component as modified for undo/redo support
	SplineComponent->MarkPackageDirty();
}

TArray<FVector2D> UWorldBLDRuntimeUtilsLibrary::CalulateAllowedDistanceRanges(float TotalSplineLength,
	const TArray<FVector2D>& ForbiddenCuts)
{
	// Step 1: Create a copy of intersection ranges for processing
    TArray<FVector2D> SortedRanges = ForbiddenCuts;
    
    // Step 2: Sort the ranges by their starting points (X values)
    SortedRanges.Sort([](const FVector2D& A, const FVector2D& B) {
        return A.X < B.X;
    });
    
    // Step 3: Merge overlapping ranges
    TArray<FVector2D> MergedRanges;
    
    if (SortedRanges.Num() > 0)
    {
        FVector2D CurrentRange = SortedRanges[0];
        
        for (int32 i = 1; i < SortedRanges.Num(); i++)
        {
            // If current range overlaps with the next range
            if (CurrentRange.Y >= SortedRanges[i].X)
            {
                // Extend the current range if needed
                CurrentRange.Y = FMath::Max(CurrentRange.Y, SortedRanges[i].Y);
            }
            else
            {
                // No overlap, so add the current range to results and start a new one
                MergedRanges.Add(CurrentRange);
                CurrentRange = SortedRanges[i];
            }
        }
        
        // Add the last range
        MergedRanges.Add(CurrentRange);
    }
    
    // Step 4: Calculate the complementary ranges (allowed road areas)
    TArray<FVector2D> AllowedRanges;
    
    // If there are no intersection ranges, the entire spline is available
    if (MergedRanges.Num() == 0)
    {
        AllowedRanges.Add(FVector2D(0.0f, TotalSplineLength));
        return AllowedRanges;
    }
    
    // Check if there's space before the first intersection
    if (MergedRanges[0].X > 0.0f)
    {
        AllowedRanges.Add(FVector2D(0.0f, MergedRanges[0].X));
    }
    
    // Add ranges between intersections
    for (int32 i = 0; i < MergedRanges.Num() - 1; i++)
    {
        AllowedRanges.Add(FVector2D(MergedRanges[i].Y, MergedRanges[i+1].X));
    }
    
    // Check if there's space after the last intersection
    if (MergedRanges.Last().Y < TotalSplineLength)
    {
        AllowedRanges.Add(FVector2D(MergedRanges.Last().Y, TotalSplineLength));
    }
    
    return AllowedRanges;
}

TArray<FVector2D> UWorldBLDRuntimeUtilsLibrary::GetBlockingHitRanges(
    USplineComponent* SplineComponent,
    int32 NumSamples,
    float TraceDistance,
    ECollisionChannel TraceChannel,
    bool bDrawDebug,
    float DebugDrawTime, TArray<AActor*> ActorsToIgnore)
{
    // Validate input parameters
    if (!SplineComponent || NumSamples <= 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetBlockingHitRanges: Invalid input parameters"));
        return TArray<FVector2D>();
    }

    TArray<FVector2D> BlockingRanges;
    const float SplineLength = SplineComponent->GetSplineLength();
    
    // If spline has no length, return empty array
    if (SplineLength <= 0.0f)
    {
        return BlockingRanges;
    }

    // Initialize variables for tracking blocking hit ranges
    bool bInBlockingRange = false;
    float RangeStart = 0.0f;
    float DistanceBetweenSamples = SplineLength / (NumSamples - 1);
    
    // Setup collision parameters
    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = true;
	QueryParams.AddIgnoredActors(ActorsToIgnore);
    
    // Get world pointer
    UWorld* World = SplineComponent->GetOwner()->GetWorld();
    if (!World)
    {
        return BlockingRanges;
    }

    // Sample the spline and perform line traces
    for (int32 i = 0; i < NumSamples; ++i)
    {
        // Calculate distance along spline
        float Distance = i * DistanceBetweenSamples;
        
        // Get location and up vector at this distance
        FVector SampleLocation = SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
        FVector UpVector = SplineComponent->GetUpVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
        
        // Start trace from sample point
        FVector TraceStart = SampleLocation;
        FVector TraceEnd = SampleLocation - (UpVector.GetSafeNormal() * TraceDistance);
        
        // Perform line trace
        FHitResult HitResult;
        bool bHit = World->LineTraceSingleByChannel(
            HitResult,
            TraceStart,
            TraceEnd,
            TraceChannel,
            QueryParams
        );
        
        // Draw debug line if requested
        if (bDrawDebug)
        {
            FColor LineColor = bHit ? FColor::Green : FColor::Red;
            DrawDebugLine(World, TraceStart, TraceEnd, LineColor, false, DebugDrawTime, 0, 1.0f);
            
            if (bHit)
            {
                DrawDebugPoint(World, HitResult.ImpactPoint, 10.0f, FColor::Blue, false, DebugDrawTime);
            }
        }
        
        // Process the hit result
        if (bHit && HitResult.bBlockingHit)
        {
            // If this is the first hit in a potential range, mark the start
            if (!bInBlockingRange)
            {
                bInBlockingRange = true;
                RangeStart = Distance;
            }
        }
        else
        {
            // If we were in a blocking range but no longer have a hit,
            // add the completed range and reset tracking
            if (bInBlockingRange)
            {
                BlockingRanges.Add(FVector2D(RangeStart, Distance));
                bInBlockingRange = false;
            }
        }
    }
    
    // If we're still in a blocking range at the end of sampling,
    // add the final range
    if (bInBlockingRange)
    {
        BlockingRanges.Add(FVector2D(RangeStart, SplineLength));
    }
    
    return BlockingRanges;
}

void UWorldBLDRuntimeUtilsLibrary::CopyAndOffsetSpline(
	class USplineComponent* SourceSpline, 
	const FSplineCopyOffsetParams& Params, 
	class USplineComponent* DestSpline,
	FSplineCopyOffsetOutput& OutResult)
{
	OutResult = FSplineCopyOffsetOutput();

	if (!IsValid(SourceSpline) || !IsValid(DestSpline))
	{
		WORLDBLD_LOG(LogWorldBLD, OutResult, Error, TEXT("One of SourceSpline or DestSpline are missing"));
		return;
	}

	OutResult.bSuccess = true;
	
	if (Params.bMoveDestActorToRootOfSpline && DestSpline->GetOwner() && SourceSpline->GetOwner())
	{
		DestSpline->GetOwner()->SetActorLocation(SourceSpline->GetOwner()->GetActorLocation());
	}

	const bool bInPlaceModify = (SourceSpline == DestSpline) && IsValid(SourceSpline);
	if (!bInPlaceModify)
	{
		DestSpline->ClearSplinePoints();
	}
	OutResult.bSourceClockwiseWinding = CalculateSplineWinding(SourceSpline, OutResult.SourceWindingValue);

	if (Params.bSimpleCopyOnly)
	{
		// Simple Copy
		if (!bInPlaceModify)
		{
			CopySplineIntoSpline(DestSpline, SourceSpline);
		}

		int32 Inset = Params.bTrimEdgePoints ? 1 : 0;
		for (int32 Idx = Inset; Idx < SourceSpline->GetNumberOfSplinePoints() - Inset; Idx += 1)
		{
			FVector Point = SourceSpline->GetLocationAtSplinePoint(Idx, ESplineCoordinateSpace::Local);
			FVector Direction = SourceSpline->GetDirectionAtSplinePoint(Idx, ESplineCoordinateSpace::Local).GetSafeNormal();
			FVector Up = SourceSpline->GetUpVectorAtSplinePoint(Idx, ESplineCoordinateSpace::Local).GetSafeNormal();
			FVector Right = FVector::CrossProduct(Direction, Up).GetSafeNormal();

			float OffsetRight = Params.OffsetRight;
			float OffsetUp = Params.OffsetUp;
			if (Params.OffsetFunc)
			{
				Params.OffsetFunc(SourceSpline->GetDistanceAlongSplineAtSplinePoint(Idx), OffsetRight, OffsetUp);
			}

			DestSpline->SetLocationAtSplinePoint(Idx, 
					Point + OffsetRight * Right + OffsetUp * Up, 
					ESplineCoordinateSpace::Local, /* Update? */ false);
		}
	}
	else
	{
		// Edge Resample
		int32 LocalSplineIdx = -1;
		TArray<FSplineSample> SamplePoints;
		OffsetSpline_Internal(SourceSpline, Params, ESplineCoordinateSpace::World, SamplePoints);
		const bool bSourceIsLoop = SourceSpline->IsClosedLoop();
		if (bInPlaceModify)
		{
			DestSpline->ClearSplinePoints();
		}
		for (const FSplineSample& Sample : SamplePoints)
		{
			LocalSplineIdx += 1;
			DestSpline->AddSplinePoint(Sample.Location + Sample.Inset, ESplineCoordinateSpace::World, /* Update? */ false);
			DestSpline->SetSplinePointType(LocalSplineIdx, ESplinePointType::Linear, /* Update? */ false);
		}

		if (Params.SplineSampleSettings.bRemoveSelfIntersections)
		{
			FSelfIntersectionParams FixupParams;
			FixupParams.ZHeightThreshold = Params.SplineSampleSettings.SelfIntersectionZHeightThreshold;
			FixupParams.WindingThreshold = Params.SplineSampleSettings.SelfIntersectionWindingThreshold;
			FixupLinearSplineSelfIntersections(DestSpline, FixupParams, OutResult);
		}

		DestSpline->SetClosedLoop(bSourceIsLoop);
	}

	DestSpline->UpdateSpline();
}

void UWorldBLDRuntimeUtilsLibrary::OffsetSpline_Internal(
	class USplineComponent* Spline,
	const FSplineCopyOffsetParams& OffsetParams,
	ESplineCoordinateSpace::Type CoordSpace,
	TArray<FSplineSample>& OutSamplePoints)
{
	// Edge Resample
	float StartDistance = (OffsetParams.SplineSampleSettings.StartDistance >= 0.0f) ? OffsetParams.SplineSampleSettings.StartDistance : 0.0f;
	float EndDistance = (OffsetParams.SplineSampleSettings.EndDistance >= 0.0f) ? OffsetParams.SplineSampleSettings.EndDistance : Spline->GetSplineLength();
	int32 LocalSplineIdx = -1;

	if (OffsetParams.bTrimEdgePoints && (Spline->GetNumberOfSplinePoints() > 2))
	{
		StartDistance = Spline->GetDistanceAlongSplineAtSplinePoint(1);
		EndDistance = Spline->GetDistanceAlongSplineAtSplinePoint(Spline->GetNumberOfSplinePoints() - 2);
	}

	const float MinNewPointThreshold = OffsetParams.SplineSampleSettings.NewPointDistanceThreshold * OffsetParams.SplineSampleSettings.NewPointDistanceThreshold;
	const bool bHasMinPointTheshold = OffsetParams.SplineSampleSettings.NewPointDistanceThreshold > 0.0f;
	FVector LastPlacedPosition {FVector::ZeroVector};

	bool bDone = false;
	bool bAtLinearCorner = false;
	bool bLastSample = false;
	float Distance = StartDistance;
	while (!bDone || bLastSample)
	{
		float OffsetRight = OffsetParams.OffsetRight;
		float OffsetUp = OffsetParams.OffsetUp;
		if (OffsetParams.OffsetFunc)
		{
			OffsetParams.OffsetFunc(Distance, OffsetRight, OffsetUp);
		}
			
		FVector Point, Right;
		float BendAngle;
		CalculateLinearBendRightVectorAtDistance(Spline, Distance,
				OffsetParams.SplineSampleSettings.SamplingDistance, BendAngle, Point, Right, CoordSpace);
		FVector Up = Spline->GetUpVectorAtDistanceAlongSpline(Distance, CoordSpace).GetSafeNormal();
		FVector Direction = Spline->GetDirectionAtDistanceAlongSpline(Distance, CoordSpace).GetSafeNormal();
		if (OffsetParams.SplineSampleSettings.bQuickSampleLinearNeighbors)
		{
			Right *= -1.0;
		}
		else
		{
			Right = FVector::CrossProduct(Direction, Up).GetSafeNormal();
		}
		FVector Inset = OffsetRight * Right + OffsetUp * Up;
		FVector FinalLocation = Point + Inset;
			
		bool bHasSevereBend = false;
		if (bHasMinPointTheshold && (OffsetParams.SplineSampleSettings.SamplingMode == ESplineSampleMode::CurvatureThreshold))
		{
			bHasSevereBend = BendAngle > OffsetParams.SplineSampleSettings.CurvatureThreshold;
		}
			
		if ((LocalSplineIdx < 0) || !bHasMinPointTheshold || bHasSevereBend ||
			((LocalSplineIdx >= 0) && ((LastPlacedPosition - FinalLocation).SquaredLength() > MinNewPointThreshold)))
		{
			LocalSplineIdx += 1;
			
			FSplineSample& NewSample = OutSamplePoints.AddDefaulted_GetRef();
			NewSample.bIsLinearCorner = bAtLinearCorner;
			NewSample.Distance = Distance;
			NewSample.Inset = Inset;
			NewSample.Location = Point;
			LastPlacedPosition = FinalLocation;
		}

		if (bLastSample)
		{
			break;
		}

		float DistanceBefore = Distance;
		float DistanceDelta = ((EndDistance > StartDistance) ? 1.0f : -1.0f) * OffsetParams.SplineSampleSettings.SamplingDistance;
		switch (OffsetParams.SplineSampleSettings.SamplingMode)
		{
		case ESplineSampleMode::Uniform: {
			Distance += DistanceDelta;
			break; }
		case ESplineSampleMode::CurvatureThreshold: {
			Distance = FindNextDistanceThatMeetsCurvatureThreshold(Spline, Distance, DistanceDelta, EndDistance, OffsetParams.SplineSampleSettings, &bAtLinearCorner);
			break; }
		}
		bDone = (EndDistance > StartDistance) ? (Distance >= EndDistance) : (Distance <= EndDistance);
		if (bDone)
		{
			bLastSample = true;
			// Fix an edge case right at the end where we sample the same point again the same way
			if (OffsetParams.SplineSampleSettings.SamplingMode == ESplineSampleMode::CurvatureThreshold)
			{
				Distance -= DistanceDelta;
			}
		}
	}
}

float UWorldBLDRuntimeUtilsLibrary::FindNextDistanceThatMeetsCurvatureThreshold(
	USplineComponent* Spline,
	float StartingDistance,
	float DirectionDelta,
	float CutoffDistance,
	FSplineSamplingParams SampleParams,
	bool* bAtCornerRef)
{
	float Current = StartingDistance;
	bool bDone = false, bEarlyExit = false;
	float AccumulatedDeflection = 0.0f;
	const float DeflectionThreshold = SampleParams.CurvatureThreshold;
	bool bGoingStraight = false;
	const float SplineLen = Spline->GetSplineLength();
	const bool bIsClosedLoop = Spline->IsClosedLoop();
	const int32 NumSplinePoints = Spline->GetNumberOfSplinePoints();

	auto LoopedDistance = [&]() -> float
	{
		return FMath::IsNearlyZero(SplineLen) ? 0.0f : (bIsClosedLoop ? FMath::Fmod(Current + SplineLen, SplineLen) : Current);
	};
	auto LoopedSplinePoint = [&](int32 Idx) -> int32
	{
		return bIsClosedLoop ? FMath::Clamp(Idx, 0, NumSplinePoints - 1) : ((Idx + NumSplinePoints) % NumSplinePoints);
	};

	if (bAtCornerRef)
	{
		*bAtCornerRef = false;
	}
	
	float LastSample = Current;
	FVector LastDirection = -Spline->GetDirectionAtDistanceAlongSpline(LoopedDistance(), ESplineCoordinateSpace::Local).GetSafeNormal();
	if (LastDirection.IsNearlyZero())
	{
		return CutoffDistance;
	}
	
	if (SampleParams.bQuickSampleLinearNeighbors)
	{
		float SplinePointProgress = Spline->GetInputKeyValueAtDistanceAlongSpline(LoopedDistance());
		int32 NearestSplinePoint = FMath::RoundToInt(SplinePointProgress);
		int32 CurrentSplinePoint = FMath::Floor(SplinePointProgress);
		bool bAllNearbyPointsAreLinear =
			(Spline->GetSplinePointType(LoopedSplinePoint(NearestSplinePoint - 1)) == ESplinePointType::Linear) &&
			(Spline->GetSplinePointType(LoopedSplinePoint(NearestSplinePoint + 0)) == ESplinePointType::Linear) &&
			(Spline->GetSplinePointType(LoopedSplinePoint(NearestSplinePoint + 1)) == ESplinePointType::Linear);

		if (bAllNearbyPointsAreLinear && (NearestSplinePoint != NumSplinePoints - 1) && (NearestSplinePoint != 0))
		{
			Current = Spline->GetDistanceAlongSplineAtSplinePoint(LoopedSplinePoint(NearestSplinePoint + 1)) + DirectionDelta;
			if (bAtCornerRef)
			{
				*bAtCornerRef = true;
			}
			bEarlyExit = true;
		}
	}

	while (!bDone && !bEarlyExit)
	{
		FVector InnerDirection = -Spline->GetDirectionAtDistanceAlongSpline(LoopedDistance(), ESplineCoordinateSpace::Local).GetSafeNormal();
		if (InnerDirection.IsNearlyZero())
		{
			bDone = true;
			break;
		}
		float ThisDeflection = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(LastDirection, InnerDirection)));
		if (FMath::IsNearlyEqual(ThisDeflection, 180.0f, 0.1f))
		{
			FString OwnerContext = UKismetSystemLibrary::GetDisplayName(IsValid(Spline->GetOwner()) ? (UObject*)Spline->GetOwner() : Spline);
			
		}
		AccumulatedDeflection += ThisDeflection;

		// If we have deflected too much, exit
		if (AccumulatedDeflection > DeflectionThreshold)
		{
			bEarlyExit = true;
			break;
		}

		// Detect if we are "going straight". If we detect a "kink" after going straight, then place a sample there.
		if (!FMath::IsNearlyEqual(LastSample, Current) && FMath::IsNearlyEqual(ThisDeflection, 0.0f, 0.0001f))
		{
			bGoingStraight = true;
		}
		else if (bGoingStraight)
		{
			bEarlyExit = true;
			break;
		}

		LastSample = Current;
		Current += DirectionDelta;
		// Check to see if we have moved too far.
		bDone = (DirectionDelta < 0.0f) ? (Current <= CutoffDistance) : (Current >= CutoffDistance);
		LastDirection = InnerDirection;
	}

	float FinalDistance = bDone ? CutoffDistance : (Current - DirectionDelta);
	if (FMath::IsNearlyEqual(StartingDistance, FinalDistance))
	{
		FinalDistance += DirectionDelta;
	}
	return FinalDistance;
}

void UWorldBLDRuntimeUtilsLibrary::FixupLinearSplineSelfIntersections(
	USplineComponent* Spline,
	const FSelfIntersectionParams& Params,
	FSplineCopyOffsetOutput& InOutResult)
{
	InOutResult.bSuccess = true;
	if (Spline->GetNumberOfSplinePoints() < 3)
	{
		return;
	}

	bool bMadeACut {true};
	while (bMadeACut)
	{
		bMadeACut = false;
		int32 SplineLen = Spline->GetNumberOfSplinePoints();
		for (int32 Idx = 0; Idx < SplineLen; Idx += 1)
		{
			int32 Idx2 = (Idx + 1) % SplineLen;
			FVector CurrentStart = Spline->GetLocationAtSplinePoint(Idx, ESplineCoordinateSpace::Local);
			FVector CurrentEnd = Spline->GetLocationAtSplinePoint(Idx2, ESplineCoordinateSpace::Local);
			
			int32 CheckLen = Spline->IsClosedLoop() ? (SplineLen - 2) : (SplineLen - Idx - 2);
			for (int32 OOff = 1; OOff < CheckLen; OOff += 1)
			{
				int32 OIdx = (Idx + 1 + OOff) % SplineLen;
				int32 OIdx2 = (OIdx + 1) % SplineLen;
				FVector OtherStart = Spline->GetLocationAtSplinePoint(OIdx, ESplineCoordinateSpace::Local);
				FVector OtherEnd = Spline->GetLocationAtSplinePoint(OIdx2, ESplineCoordinateSpace::Local);

				// If the Z values are too far apart, don't worry about self-intersection.
				if ((Params.ZHeightThreshold > 0.0f) &&
					(FMath::Abs(OtherStart.Z - CurrentStart.Z) >= Params.ZHeightThreshold) &&
					(FMath::Abs(CurrentEnd.Z - OtherEnd.Z) >= Params.ZHeightThreshold))
				{
					continue;
				}

				// If the distances are too far apart, don't worry about self-intersection.
				if (Params.MaxDistanceBetweenPoints > 0.0f)
				{
					float Dist1 = Spline->GetDistanceAlongSplineAtSplinePoint(Idx), Dist2 = Spline->GetDistanceAlongSplineAtSplinePoint(Idx2);
					float ODist1 = Spline->GetDistanceAlongSplineAtSplinePoint(OIdx), ODist2 = Spline->GetDistanceAlongSplineAtSplinePoint(OIdx2);
					float SmallestDist = FMath::Min(
							FMath::Min(FMath::Abs(Dist2 - ODist2), FMath::Abs(Dist2 - ODist1)), 
							FMath::Min(FMath::Abs(Dist1 - ODist2), FMath::Abs(Dist1 - ODist1)));
					if (SmallestDist > Params.MaxDistanceBetweenPoints)
					{
						continue;
					}
				}

				// Parallel lines should be ignored for intersection tests.
				if (FMath::IsNearlyEqual(
					FMath::Abs(
						FVector::DotProduct(
							(OtherEnd - OtherStart).GetSafeNormal(), 
							(CurrentEnd - CurrentStart).GetSafeNormal())), 1.0f, 0.0001f))
				{
					continue;
				}

				FVector IntersectionPoint;
				// This function projects the points into the XY plane and checks for an intersection. The resulting value though includes proper Z value.
				if (FMath::SegmentIntersection2D(CurrentStart, CurrentEnd, OtherStart, OtherEnd, IntersectionPoint))
				{
					// By default, reject the next part of the loop.
					int32 CutEnd = Idx2; // the piece we move
					int32 CutStart = Idx2 + 1; // where we start deleting
					int32 NextLoopLen = (OIdx - Idx);
					if (OIdx < Idx) // NOTE: this will only happen on closed loops
					{
						NextLoopLen = SplineLen - (Idx - OIdx);
					}
					int32 PrevLoopLen = SplineLen - NextLoopLen;
					int32 CutLength = NextLoopLen - 1; // Length of the cut

					// If we have a closed loop, we need to make sure we maintain our winding.
					if (Spline->IsClosedLoop())
					{
						// Because there is a crossing, we expect each side to have a different winding (or the Winding value is zero).
						float PrevLoopWinding {0.0f}, NextLoopWinding {0.0f};
						bool bPrevLoopClockwise = CalculateSplineWinding(Spline, OIdx2, PrevLoopLen, {IntersectionPoint}, PrevLoopWinding);
						bool bPrevHasCrossings = SplineSegmentHasAnyCrossings(Spline, Params.ZHeightThreshold, OIdx2, PrevLoopLen);
						bool bNextLoopClockwise = CalculateSplineWinding(Spline, Idx2, NextLoopLen, {IntersectionPoint}, NextLoopWinding);
						bool bNextHasCrossings = SplineSegmentHasAnyCrossings(Spline, Params.ZHeightThreshold, Idx2, NextLoopLen);
						
						auto WindingIsInsignificant = [&](float WindingValue) -> bool
						{
							// Old logic: FMath::IsNearlyZero(WindingValue)
							return FMath::IsWithinInclusive(FMath::Abs(WindingValue), -SMALL_NUMBER, Params.WindingThreshold + SMALL_NUMBER);
						};

						// Previous winding being zero means it is a very tiny "loop" (overlapping points)
						bool bConditionA = WindingIsInsignificant(PrevLoopWinding);
						// Previous loop is "perfect" and is oppositely wound
						bool bConditionB = (!bPrevHasCrossings && !WindingIsInsignificant(PrevLoopWinding) && (bPrevLoopClockwise != InOutResult.bSourceClockwiseWinding));
						// If the next loop's winding matches, then the previous one should be deleted
						bool bConditionC = (!WindingIsInsignificant(NextLoopWinding) && (bNextLoopClockwise == InOutResult.bSourceClockwiseWinding)) &&
								(FMath::Abs(PrevLoopWinding) < FMath::Abs(NextLoopWinding));

						// Use our winding to figure out which part of the cross loop to keep.
						if (bConditionA || bConditionB || bConditionC)
						{
							// Cut the previous part of the loop out.
							CutEnd = OIdx2;
							CutStart = OIdx2 + 1;
							CutLength = PrevLoopLen - 1;
						}

						// Degenerate case... note sure where to make the cut (the curve must be really deformed!)
						if (!WindingIsInsignificant(PrevLoopWinding) && !WindingIsInsignificant(NextLoopWinding) && (bPrevLoopClockwise == bNextLoopClockwise))
						{
							// We hit the degenerate case when there are lots of crossings messing up our winding calculation. However, if our next loop
							// doesn't have any crossings, then if we got here then it is oppositely wound and should be cut. Otherwise we just need to
							// skip and hope at another iteration we will eventually fix up this intersection.
							if (bNextHasCrossings)
							{
								continue;
							}
						}
					}
					
					CutEnd = CutEnd % SplineLen;
					CutStart = CutStart % SplineLen;

					// Move CurrentEnd to the intersection point and remove all points between Idx + 1 and OIdx + 1.
					Spline->SetLocationAtSplinePoint(CutEnd, IntersectionPoint, ESplineCoordinateSpace::Local, /* Update? */ false);

					// Actually make the cut!
					for (int32 RIdx = 0; RIdx < CutLength; RIdx += 1)
					{
						// If that index is no longer value, remove from beginning
						if (CutStart >= Spline->GetNumberOfSplinePoints())
						{
							CutStart = 0;
						}
						Spline->RemoveSplinePoint(CutStart, /* Update? */ false);
					}

					bMadeACut = true;
					break;
				}
			} // for (other)

			// If we made a cut, then start searching for a crossing again from the beginning.
			if (bMadeACut)
			{
				break;
			}
		} // for (current)
	} // while (cut)

	// Closed loops should maintain their chirality.
	//if (Spline->IsClosedLoop())
	{
		float FinalWinding;
		bool bFinalIsClockwise = CalculateSplineWinding(Spline, FinalWinding);

		if (bFinalIsClockwise != InOutResult.bSourceClockwiseWinding)
		{
			InOutResult.bSuccess = false;
			WORLDBLD_LOG(LogWorldBLD, InOutResult, Error, TEXT("Final curve is inverted."));
		}
	}
	//else
	{
		/// @TODO: How do we check if a line inverts?
	}
}

bool UWorldBLDRuntimeUtilsLibrary::SplineSegmentHasAnyCrossings(
	USplineComponent* Spline,
	float ZHeightThreshold,
	int32 StartIdx,
	int32 Len)
{
	bool bHasCrossing = false;

	int32 SplineLen = Spline->GetNumberOfSplinePoints();
	for (int32 Offs = 0; Offs < Len; Offs += 1)
	{
		int32 Idx = (StartIdx + Offs) % SplineLen;
		int32 Idx2 = (Idx + 1) % SplineLen;
		FVector CurrentStart = Spline->GetLocationAtSplinePoint(Idx, ESplineCoordinateSpace::Local);
		FVector CurrentEnd = Spline->GetLocationAtSplinePoint(Idx2, ESplineCoordinateSpace::Local);
			
		for (int32 OOff = 1; OOff < Len - 2; OOff += 1)
		{
			int32 OIdx = (Idx + 1 + OOff) % SplineLen;
			int32 OIdx2 = (OIdx + 1) % SplineLen;
			FVector OtherStart = Spline->GetLocationAtSplinePoint(OIdx, ESplineCoordinateSpace::Local);
			FVector OtherEnd = Spline->GetLocationAtSplinePoint(OIdx2, ESplineCoordinateSpace::Local);
			
			// If the Z values are too far apart, don't worry about self-intersection.
			if ((ZHeightThreshold > 0.0f) && 
				(FMath::Abs(OtherStart.Z - CurrentStart.Z) >= ZHeightThreshold) &&
				(FMath::Abs(CurrentEnd.Z - OtherEnd.Z) >= ZHeightThreshold))
			{
				continue;
			}

			// Parallel lines should be ignored for intersection tests.
			if (FMath::IsNearlyEqual(
				FMath::Abs(
					FVector::DotProduct(
						(OtherEnd - OtherStart).GetSafeNormal(), 
						(CurrentEnd - CurrentStart).GetSafeNormal())), 1.0f, 0.0001f))
			{
				continue;
			}

			FVector IntersectionPoint;
			// This function projects the points into the XY plane and checks for an intersection. The resulting value though includes proper Z value.
			if (FMath::SegmentIntersection2D(CurrentStart, CurrentEnd, OtherStart, OtherEnd, IntersectionPoint))
			{
				bHasCrossing = true;
				break;
			}
		}

		if (bHasCrossing)
		{
			break;
		}
	}

	return bHasCrossing;
}


void UWorldBLDRuntimeUtilsLibrary::ConvertStaticMeshesToInstances(
	AActor* TargetActor,
	const TArray<class UStaticMeshComponent*>& StaticMeshes,
	bool bDestroyStaticMeshes,
	TArray<class UHierarchicalInstancedStaticMeshComponent*>& OutHISMs)
{
	OutHISMs.Empty();

	struct FMatInfo
	{
		class UMaterialInterface* Material {nullptr};
		FName Slot {NAME_None};

		FMatInfo() {}
		FMatInfo(class UMaterialInterface* InMaterial, FName InSlot) : Material(InMaterial), Slot(InSlot) {}

		bool operator==(const FMatInfo& Other) const
		{
			return (Material == Other.Material) && (Slot == Other.Slot);
		}
	};
	struct FMeshMatSet
	{
		class UStaticMesh* Mesh {nullptr};
		TArray<FMatInfo> Materials;
		TArray<UStaticMeshComponent*> AssociatedComponents;
		UHierarchicalInstancedStaticMeshComponent* ExistingHISM {nullptr};

		bool operator==(const FMeshMatSet& Other) const
		{
			return (Mesh == Other.Mesh) && (Materials == Other.Materials);
		}
	};
	
	TArray<FMeshMatSet> UniqueMeshMats;

	for (UStaticMeshComponent* SM : StaticMeshes)
	{
		FMeshMatSet NewSet;
		NewSet.Mesh = SM->GetStaticMesh();

		const auto& Slots = SM->GetMaterialSlotNames();
		const auto& Mats = SM->GetMaterials();
		for (int32 Idx = 0; Idx < Mats.Num(); Idx += 1)
		{
			NewSet.Materials.Add(FMatInfo(Mats[Idx], Slots[Idx]));
		}

		int32 AddIdx = UniqueMeshMats.AddUnique(NewSet);
		UniqueMeshMats[AddIdx].AssociatedComponents.Add(SM);
	}
	
	TArray<UHierarchicalInstancedStaticMeshComponent*> ExistingHISMs;
	if (TargetActor)
	{
		TargetActor->ForEachComponent(false, [&](UActorComponent* ActorComp)
		{
			if (auto* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(ActorComp))
			{
				FMeshMatSet NewSet;
				NewSet.Mesh = HISM->GetStaticMesh();

				const auto& Slots = HISM->GetMaterialSlotNames();
				const auto& Mats = HISM->GetMaterials();
				for (int32 Idx = 0; Idx < Mats.Num(); Idx += 1)
				{
					NewSet.Materials.Add(FMatInfo(Mats[Idx], Slots[Idx]));
				}

				int32 Index = UniqueMeshMats.Find(NewSet);
				if (Index != INDEX_NONE)
				{
					UniqueMeshMats[Index].ExistingHISM = HISM;
				}
			}
		});
	}

	for (const FMeshMatSet& Set : UniqueMeshMats)
	{
		UHierarchicalInstancedStaticMeshComponent* HISM = Set.ExistingHISM;
		if (!IsValid(HISM))
		{
			if (IsValid(TargetActor) && !TargetActor->HasAnyFlags(RF_ClassDefaultObject))
			{
				HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(
					TargetActor->AddComponentByClass(UHierarchicalInstancedStaticMeshComponent::StaticClass(),
						/* Manual Attachment? */ false, FTransform(), /* Deferred? */ false));
			}
			// Allow this case for when we are converting a Blueprint CDO to use HISMs instead of SMs
			else
			{
				HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>();
			}
		}

		if (ensure(IsValid(HISM)))
		{
			HISM->SetStaticMesh(Set.Mesh);
			for (int32 Idx = 0; Idx < Set.Materials.Num(); Idx += 1)
			{
				HISM->SetMaterial(Idx, Set.Materials[Idx].Material);
			}
			OutHISMs.Add(HISM);

			for (UStaticMeshComponent* SM : Set.AssociatedComponents)
			{
				HISM->AddInstance(SM->GetComponentTransform(), /* World Space? */ true);
				if (bDestroyStaticMeshes)
				{
					SM->DestroyComponent();
				}
			}
		}
	}
}

void UWorldBLDRuntimeUtilsLibrary::CopyOverHismProperties(class UHierarchicalInstancedStaticMeshComponent* Source, class UHierarchicalInstancedStaticMeshComponent* Dest)
{
	if (!IsValid(Source) || !IsValid(Dest))
	{
		return;
	}

	Dest->SetStaticMesh(Source->GetStaticMesh());
	for (int32 Idx = 0; Idx < Source->GetNumMaterials(); Idx += 1)
	{
		Dest->SetMaterial(Idx, Source->GetMaterial(Idx));
	}
	
	for (int32 Idx = 0; Idx < Source->GetInstanceCount(); Idx += 1)
	{
		FTransform InstanceXform;
		Source->GetInstanceTransform(Idx, InstanceXform);
		Dest->AddInstance(InstanceXform);
	}
}

void UWorldBLDRuntimeUtilsLibrary::GetKeyValuePairsFromCurve(
	UCurveFloat* Curve, 
	TArray<FVector2D>& OutPairs)
{
	OutPairs.Empty();
	if (!IsValid(Curve))
	{
		return;
	}

	for (const FRichCurveKey& Key : Curve->FloatCurve.Keys)
	{
		OutPairs.Add(FVector2D(Key.Time, Key.Value));
	}
}

void UWorldBLDRuntimeUtilsLibrary::ExtractCurveSamples(UCurveFloat* Curve, FExtractCurveParams Params, TArray<FVector2D>& OutPairs)
{
	OutPairs.Empty();
	if (!IsValid(Curve))
	{
		return;
	}

	for (int32 Idx = 0; Idx < Curve->FloatCurve.Keys.Num(); Idx += 1)
	{
		const FRichCurveKey& Key = Curve->FloatCurve.Keys[Idx];
		switch (Key.InterpMode)
		{
			case RCIM_Linear:
				OutPairs.Add(FVector2D(Key.Time, Key.Value));
				break;

			case RCIM_Constant: {
				OutPairs.Add(FVector2D(Key.Time, Key.Value));
				if (Idx < Curve->FloatCurve.Keys.Num() - 1)
				{
					const FRichCurveKey& NextKey = Curve->FloatCurve.Keys[Idx + 1];
					OutPairs.Add(FVector2D(NextKey.Time, Key.Value));
				}
				break; }
			
			case RCIM_Cubic:
				/// @TODO: Do something smart here.
				OutPairs.Add(FVector2D(Key.Time, Key.Value));
				break;

			case RCIM_None:
				// Skip!
			default:
				break;
		}
	}

	// Apply transformation
	for (FVector2D& Sample : OutPairs)
	{
		Sample = Sample * Params.CurveScale;
	}
}

void UWorldBLDRuntimeUtilsLibrary::FindIntersectionsBetweenSplines(
	class USplineComponent* SplineA, 
	class USplineComponent* SplineB,
	FSplineIntersectionTestParams Params, 
	FSplineIntersectionTestOutput& OutOutput)
{
	OutOutput = FSplineIntersectionTestOutput();
	if (!IsValid(SplineA) || !IsValid(SplineB) || (SplineA->GetNumberOfSplinePoints() == 0) || (SplineB->GetNumberOfSplinePoints() == 0))
	{
		return;
	}

	FSplineCopyOffsetParams OffsetParams;
	OffsetParams.bMoveDestActorToRootOfSpline = false;
	OffsetParams.bSimpleCopyOnly = false;
	OffsetParams.bTrimEdgePoints = false;
	OffsetParams.SplineSampleSettings = Params.SampleSettings;

	// NOTE: use flattened splines to figure out nearest distances along curve to intersections.
	FSplineCurves FlattenedSplineA;
	FSplineCurves FlattenedSplineB;
	GetFlattenedSplineCurve(SplineA, FlattenedSplineA);
	GetFlattenedSplineCurve(SplineB, FlattenedSplineB);

	TArray<FSplineSample> SplineASamples;
	TArray<FSplineSample> SplineBSamples;
	OffsetSpline_Internal(SplineA, OffsetParams, ESplineCoordinateSpace::World, SplineASamples);
	OffsetSpline_Internal(SplineB, OffsetParams, ESplineCoordinateSpace::World, SplineBSamples);

	int32 SplineALen   = SplineASamples.Num();
	int32 SplineARange = SplineALen + (SplineA->IsClosedLoop() ? 1 : 0) - 1;
	int32 SplineBLen   = SplineBSamples.Num();
	int32 SplineBRange = SplineBLen + (SplineB->IsClosedLoop() ? 1 : 0) - 1;

	for (int32 Idx = 0; Idx < SplineARange; Idx += 1)
	{
		const FVector& CurrentStart = SplineASamples[Idx % SplineALen].Location;
		const FVector& CurrentEnd   = SplineASamples[(Idx + 1) % SplineALen].Location;
		for (int32 OIdx = 0; OIdx < SplineBRange; OIdx += 1)
		{
			const FVector& OtherStart = SplineBSamples[OIdx % SplineBLen].Location;
			const FVector& OtherEnd   = SplineBSamples[(OIdx + 1) % SplineBLen].Location;

			// If the Z values are too far apart, don't worry about self-intersection.
			if ((Params.ZDistanceThreshold > 0.0f) &&
				(FMath::Abs(OtherStart.Z - CurrentStart.Z) >= Params.ZDistanceThreshold) &&
				(FMath::Abs(CurrentEnd.Z - OtherEnd.Z) >= Params.ZDistanceThreshold))
			{
				continue;
			}

			// Parallel lines should be ignored for intersection tests.
			if (FMath::IsNearlyEqual(
				FMath::Abs(
					FVector::DotProduct(
						(OtherEnd - OtherStart).GetSafeNormal(), 
						(CurrentEnd - CurrentStart).GetSafeNormal())), 1.0f, 0.0001f))
			{
				continue;
			}

			FVector IntersectionPoint;
			// This function projects the points into the XY plane and checks for an intersection. The resulting value though includes proper Z value.
			if (FMath::SegmentIntersection2D(CurrentStart, CurrentEnd, OtherStart, OtherEnd, IntersectionPoint))
			{
				FSplineDistancePair& Intersection = OutOutput.Intersections.AddDefaulted_GetRef();
				FVector IntersectionPointA = SplineA->GetComponentToWorld().InverseTransformPosition(IntersectionPoint);
				FVector IntersectionPointB = SplineB->GetComponentToWorld().InverseTransformPosition(IntersectionPoint);

				IntersectionPointA.Z = 0.0f;
				IntersectionPointB.Z = 0.0f;

				float TmpDistance;
				// We can probably find this analytically... but eh
				Intersection.DistanceSplineA = SplineA->GetDistanceAlongSplineAtSplineInputKey(
						FlattenedSplineA.Position.InaccurateFindNearest(IntersectionPointA, TmpDistance));
				Intersection.DistanceSplineB = SplineB->GetDistanceAlongSplineAtSplineInputKey(
						FlattenedSplineB.Position.InaccurateFindNearest(IntersectionPointB, TmpDistance));
			}
		}
	}
}

bool UWorldBLDRuntimeUtilsLibrary::SegmentIntersection2D(
	const FVector& SegmentStartA, 
	const FVector& SegmentEndA, 
	const FVector& SegmentStartB, 
	const FVector& SegmentEndB, 
	FVector& OutIntersectionPoint)
{
	if (FMath::IsNearlyEqual(
		FMath::Abs(
			FVector::DotProduct(
				(SegmentEndA - SegmentStartA).GetSafeNormal(), 
				(SegmentEndB - SegmentStartB).GetSafeNormal())), 1.0f, 0.0001f))
	{
		return false;
	}

	return FMath::SegmentIntersection2D(SegmentStartA, SegmentEndA, SegmentStartB, SegmentEndB, OutIntersectionPoint);
}

float UWorldBLDRuntimeUtilsLibrary::ApproximateSurfaceAreaOfClosedSpline(class USplineComponent* Spline)
{
	float SurfaceArea = 0.0f;
	CalculateSplineWinding(Spline, SurfaceArea);
	return FMath::Abs(SurfaceArea);
}

void UFlattenPolylineAlongPrimaryEdge::ApplyModifier_Implementation(
	FVector SweepMinWS, 
	bool bMinEdgeIsPrimary, 
	FVector SweepMaxWS, 
	bool bMaxEdgeIsPrimary,
	TArray<FVector2D>& InOutLocalSweepEdge) const
{
	float MinValue = bMinEdgeIsPrimary ? 0.0f : 1.0f;
	float MaxValue = bMaxEdgeIsPrimary ? 0.0f : 1.0f;
	FVector2D StartPoint = (InOutLocalSweepEdge.Num() > 0)? InOutLocalSweepEdge[0] : FVector2D::ZeroVector;
	FVector2D EndPoint = (InOutLocalSweepEdge.Num() > 0)? InOutLocalSweepEdge.Last() : FVector2D::ZeroVector;
	for (FVector2D& EdgePoint : InOutLocalSweepEdge)
	{
		float Alpha = (EdgePoint - StartPoint).X / FMath::Abs(EndPoint.X - StartPoint.X);
		float Modifier = FMath::LerpStable(MinValue, MaxValue, Alpha);
		EdgePoint.Y *= Modifier;
	}
}



TArray<FVector2D> UWorldBLDRuntimeUtilsLibrary::InvertSplineDistanceRanges(const TArray<FVector2D>& InRanges, float SplineLength)
{
	TArray<FVector2D> ResultRanges;
    
	
	if (InRanges.Num() == 0)
	{
		// If no ranges are provided, return the entire spline length
		ResultRanges.Add(FVector2D(0.0f, SplineLength));
		return ResultRanges;
	}
    
	// Sort the input ranges by their start distances
	TArray<FVector2D> SortedRanges = InRanges;
	SortedRanges.Sort([](const FVector2D& A, const FVector2D& B) {
		return A.X < B.X;
	});
    
	// Check if the first range starts after the beginning of the spline
	if (SortedRanges[0].X > 0.0f)
	{
		ResultRanges.Add(FVector2D(0.0f, SortedRanges[0].X));
	}
    
	// Check for gaps between consecutive ranges
	for (int32 i = 0; i < SortedRanges.Num() - 1; i++)
	{
		const float CurrentRangeEnd = SortedRanges[i].Y;
		const float NextRangeStart = SortedRanges[i + 1].X;
        
		// If there's a gap, add it to the result
		if (NextRangeStart > CurrentRangeEnd)
		{
			ResultRanges.Add(FVector2D(CurrentRangeEnd, NextRangeStart));
		}
	}
    
	// Check if the last range ends before the end of the spline
	const FVector2D& LastRange = SortedRanges.Last();
	if (LastRange.Y < SplineLength)
	{
		ResultRanges.Add(FVector2D(LastRange.Y, SplineLength));
	}
    
	return ResultRanges;
}

#if 0
		auto CalcBendVector = [](USplineComponent* Spline, float Dist, float DistDelta) -> FVector
		{
			FVector Points[3] = {
				Spline->GetLocationAtDistanceAlongSpline(Dist - DistDelta, ESplineCoordinateSpace::Local),
				Spline->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::Local),
				Spline->GetLocationAtDistanceAlongSpline(Dist + DistDelta, ESplineCoordinateSpace::Local),
			};
			FVector BA = (Points[0] - Points[1]).GetSafeNormal();
			FVector BC = (Points[2] - Points[1]).GetSafeNormal();
			// NOTE: We don't actually need to convert between radians and degrees, but it is useful for debugging.
			float AngleBA = FMath::RadiansToDegrees(FMath::Atan2(BA.Y, BA.X));
			float AngleBC = FMath::RadiansToDegrees(FMath::Atan2(BC.Y, BC.X));
			float AngleBetween = AngleBC - AngleBA;
			FVector LocalUp = FVector::CrossProduct(BA, BC).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
			FVector Result = FQuat(LocalUp, FMath::DegreesToRadians(0.5f * AngleBetween)).RotateVector(BA);
			FVector RightVectorForComparison = Spline->GetRightVectorAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::Local);
			float Dots = FVector::DotProduct(RightVectorForComparison, Result);
			//Result = Result * FMath::Sign(Dots);
			if (Dots < 0.6f)
			{
				UE_LOG(LogTemp, Verbose, TEXT("BreakpointSpot"));
			}
			return Result;
		};
#endif

void UWorldBLDRuntimeUtilsLibrary::FixupLinearSplineEndTangent(USplineComponent* Spline)
{
	// NOTE: This issue appears to be a bug in earlier engine versions.
		/// @FIXME: For some reason the last point is messed up (bad tagent?). Specifically this only happens if the last point is Linear
		if ((Spline->GetNumberOfSplinePoints() > 1) && !Spline->IsClosedLoop() &&
			(Spline->GetSplinePointType(Spline->GetNumberOfSplinePoints() - 1) == ESplinePointType::Linear))
		{
			Spline->SetSplinePointType(Spline->GetNumberOfSplinePoints() - 1, ESplinePointType::Curve);
#if 0
			Spline->SetTangentAtSplinePoint(Spline->GetNumberOfSplinePoints() - 1,
				Spline->GetTangentAtSplinePoint(Spline->GetNumberOfSplinePoints() - 2, ESplineCoordinateSpace::World),
				ESplineCoordinateSpace::World);
#endif
		}
}

void UWorldBLDRuntimeUtilsLibrary::SpitShineMySplineCurves(
	struct FSplineCurves& SplineCurves, 
	const FSplineSpitShineCleanupParameters& Params)
{
	// It's possible the swept edge is self-intersecting. Try and fix that up.
	USplineComponent* TempSpline = IsValid(Params.ExistingSpline) ? Params.ExistingSpline : NewObject<USplineComponent>();
	TempSpline->SplineCurves = SplineCurves;
	TempSpline->SetClosedLoop(SplineCurves.Position.bIsLooped);
	TempSpline->UpdateSpline();

	if (Params.bAutoCloseLoop && (TempSpline->GetNumberOfSplinePoints() > 1))
	{
		FVector Start = TempSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);
		FVector End = TempSpline->GetLocationAtSplinePoint(TempSpline->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::Local);
		// Auto-close loop the spline if it isn't already
		if ((FVector::Dist(Start, End) < 100.0f) && !TempSpline->IsClosedLoop())
		{
			TempSpline->RemoveSplinePoint(TempSpline->GetNumberOfSplinePoints() - 1);
			TempSpline->SetClosedLoop(true);
			SplineCurves = TempSpline->SplineCurves;
		}
	}
	
	// Do any requested general cleanup.
	if (Params.bCleanup)
	{
		FCleanupSplineResult CleanupOutput;
		UWorldBLDRuntimeUtilsLibrary::CleanupSpline(TempSpline, Params.CleanupParams, CleanupOutput);

		FixupLinearSplineEndTangent(TempSpline);
		SplineCurves = TempSpline->SplineCurves;
	}
	
	// Fix self-intersections, if any.
	if (Params.bRemoveSelfIntersection)
	{
		FSplineCopyOffsetOutput Output;
		Output.bSourceClockwiseWinding = UWorldBLDRuntimeUtilsLibrary::CalculateSplineWinding(TempSpline, Output.SourceWindingValue);
		UWorldBLDRuntimeUtilsLibrary::FixupLinearSplineSelfIntersections(TempSpline, Params.IntersectionParams, Output);

		if (Output.bSuccess)
		{
			FixupLinearSplineEndTangent(TempSpline);
			SplineCurves = TempSpline->SplineCurves;
		}
	}
		
	if (!IsValid(Params.ExistingSpline))
	{
		TempSpline->DestroyComponent();
	}
}

void UWorldBLDRuntimeUtilsLibrary::SpitShineMySpline(class USplineComponent* Spline, const FSpitShineMySplineParams& Params)
{
	FSplineSpitShineCleanupParameters InternalParams;
	InternalParams.bAutoCloseLoop = Params.bAutoCloseLoop;
	InternalParams.bCleanup = Params.bCleanup;
	InternalParams.CleanupParams = Params.CleanupParams;
	InternalParams.ExistingSpline = Spline;
	InternalParams.bRemoveSelfIntersection = Params.bRemoveSelfIntersection;
	InternalParams.IntersectionParams = Params.IntersectionParams;
	SpitShineMySplineCurves(Spline->SplineCurves, InternalParams);
}

void UWorldBLDRuntimeUtilsLibrary::GetLocalBoundsOfComponent(
	USceneComponent* Component,
	FVector& OutOrigin, 
	FVector& OutExtent, 
	float& OutSphereRadius, 
	FTransform Transform, 
	bool bCached)
{
	if (IsValid(Component))
	{
		FBoxSphereBounds Bounds = bCached ? Component->CalcBounds(Transform) : Component->Bounds.TransformBy(Transform.Inverse());
		OutOrigin = Bounds.GetBox().GetCenter();
		OutExtent = Bounds.GetBox().GetExtent();
		OutSphereRadius = Bounds.GetSphere().W;
	}
	else
	{
		OutOrigin = FVector::ZeroVector;
		OutExtent = FVector::ZeroVector;
		OutSphereRadius = 0.0f;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void UWorldBLDRuntimeUtilsLibrary::VisitPointsAlongSpline(
	USplineComponent* Spline,
	float StartingDistance,
	float CutoffDistance,
	const FSplineSamplingParams& Params, 
	TFunction<bool(float)> DistanceCallback)
{
	bool bUserKeepGoing = true;
	float CurrentDistance = StartingDistance;
	float LastSampledDistance = StartingDistance;
	const float SampleSign = FMath::Sign(Params.SamplingDistance);

	if (FMath::Max(FMath::Abs(StartingDistance), FMath::Abs(CutoffDistance)) > 1e9f)
	{
		UE_LOG(LogWorldBLD, Error, TEXT("[VisitPointsAlongSpline] Sampling a spline (owner %s) with a huge distance bounds"),
				*UKismetSystemLibrary::GetDisplayName(Spline->GetOwner()));
		return;
	}

	switch (Params.SamplingMode)
	{
		case ESplineSampleMode::Uniform: {
			for (; (SampleSign * CurrentDistance) < (SampleSign * CutoffDistance); CurrentDistance += Params.SamplingDistance)
			{
				bUserKeepGoing = DistanceCallback(CurrentDistance);
				LastSampledDistance = CurrentDistance;
				if (!bUserKeepGoing)
				{
					break;
				}
			}
			break; }
		
		case ESplineSampleMode::CurvatureThreshold: {
			auto CanProceed = [&]() -> bool
			{
				if (SampleSign > 0.0f)
				{
					return CurrentDistance < CutoffDistance;
				}
				else
				{
					return CurrentDistance > CutoffDistance;
				}
			};
			while (CanProceed())
			{
				bUserKeepGoing = DistanceCallback(CurrentDistance);
				if (!bUserKeepGoing)
				{
					break;
				}

				LastSampledDistance = CurrentDistance;
				float TrueCutoff = CutoffDistance;
				if (Params.ForceMaxTravelDistance > 0.0f)
				{
					if (SampleSign > 0.0f)
					{
						TrueCutoff = FMath::Min(LastSampledDistance + Params.ForceMaxTravelDistance, CutoffDistance);
					}
					else
					{
						TrueCutoff = FMath::Max(LastSampledDistance - Params.ForceMaxTravelDistance, CutoffDistance);
					}
				}
				CurrentDistance = FindNextDistanceThatMeetsCurvatureThreshold(Spline, CurrentDistance, 
						Params.SamplingDistance, TrueCutoff, Params);
			}
			break; }
	}

	if (bUserKeepGoing && !FMath::IsNearlyEqual(CurrentDistance, LastSampledDistance, Params.SamplingDistance))
	{
		bUserKeepGoing = DistanceCallback(CutoffDistance);
	} 
}

bool UWorldBLDRuntimeUtilsLibrary::CalculateSplineWinding(
	USplineComponent* Spline, 
	int32 StartIdx, 
	int32 Len, 
	const TArray<FVector>& AdditionalPoints, 
	float& OutWindingValue)
{
	int32 TotalNumberPoints = Spline->GetNumberOfSplinePoints();
	TArray<FVector> Locations;
	for (int32 Offs = 0; Offs < Len; Offs += 1)
	{
		int32 Idx = (StartIdx + Offs) % TotalNumberPoints;
		Locations.Add(Spline->GetLocationAtSplinePoint(Idx, ESplineCoordinateSpace::Local));
	}
	Locations.Append(AdditionalPoints);

	return CalculateEdgeLoopWinding(Locations, OutWindingValue);
}

bool UWorldBLDRuntimeUtilsLibrary::CalculateEdgeLoopWinding(const TArray<FVector>& EdgeLoop, float& OutWindingValue)
{
	float Accumulator = 0.0f;
	for (int32 Idx = 0; Idx < EdgeLoop.Num(); Idx += 1)
	{
		FVector P1 = EdgeLoop[(Idx + 0) % EdgeLoop.Num()];
		FVector P2 = EdgeLoop[(Idx + 1) % EdgeLoop.Num()];
		float Value = (P2.X - P1.X) * (P2.Y + P1.Y);
		Accumulator += Value;
	}
	Accumulator *= -1.0f;

	// If Accumulator > 0.0 the curve points are clockwise, otherwise they are counter-clockwise.
	OutWindingValue = Accumulator;
	
	return Accumulator > 0.0f;
}

bool UWorldBLDRuntimeUtilsLibrary::CalculateSplineWinding(USplineComponent* BorderSpline, float& OutWindingValue)
{
	return CalculateSplineWinding(BorderSpline, 0, BorderSpline->GetNumberOfSplinePoints(), {}, OutWindingValue);
}

void UWorldBLDRuntimeUtilsLibrary::CopySplineIntoSpline(USplineComponent* DestinationSpline, USplineComponent* SourceSpline, TArray<FVector>* OptionalOutPoints)
{
	const int32 NumSplinePoints = SourceSpline->GetNumberOfSplinePoints();
	int32 LocalSplineIdx = -1;
	for (int32 Idx = 0; Idx < NumSplinePoints; Idx += 1)
	{
		FVector SplineWorldLoc = SourceSpline->GetLocationAtSplinePoint(Idx, ESplineCoordinateSpace::World);
		if (OptionalOutPoints)
		{
			OptionalOutPoints->Add(SourceSpline->GetLocationAtSplinePoint(Idx, ESplineCoordinateSpace::Local));
		}

		LocalSplineIdx += 1;
		DestinationSpline->AddSplinePoint(SplineWorldLoc, ESplineCoordinateSpace::World, /* Update? */ false);
		ESplinePointType::Type PointType = SourceSpline->GetSplinePointType(Idx);
		FVector ArriveTangent = SourceSpline->GetArriveTangentAtSplinePoint(Idx, ESplineCoordinateSpace::World);
		FVector LeaveTangent = SourceSpline->GetLeaveTangentAtSplinePoint(Idx, ESplineCoordinateSpace::World);
		DestinationSpline->SetTangentsAtSplinePoint(LocalSplineIdx, ArriveTangent, LeaveTangent, ESplineCoordinateSpace::World, /* Update? */ false);
		DestinationSpline->SetSplinePointType(LocalSplineIdx, PointType, /* Update? */ false);
	}
	DestinationSpline->SetClosedLoop(SourceSpline->IsClosedLoop());
	DestinationSpline->UpdateSpline();
}

void UWorldBLDRuntimeUtilsLibrary::GetFlattenedSplineCurve(
	USplineComponent* Spline,
	FSplineCurves& OutFlattened)
{
	OutFlattened = Spline->SplineCurves;
	int32 TotalNumberPoints = Spline->GetNumberOfSplinePoints();
	for (int32 Idx = 0; Idx < TotalNumberPoints; Idx += 1)
	{
		OutFlattened.Position.Points[Idx].OutVal.Z = 0.0f;
	}
}

void UWorldBLDRuntimeUtilsLibrary::CalculateLinearBendRightVectorAtDistance(
	class USplineComponent* Spline,
	float Distance,
	float SampleDelta,
	float& OutBendAngle,
	FVector& OutLocation,
	FVector& OutDirection,
	ESplineCoordinateSpace::Type CoordinateSpace)
{
	OutBendAngle = 0.0f;
	OutLocation = Spline->GetLocationAtDistanceAlongSpline(Distance, CoordinateSpace);

	// NOTE: Can't make the delta too small or it won't show any differences
	const float Delta = FMath::Min(5.0f, SampleDelta) * 0.2f;
	if (!Spline->IsClosedLoop() && ((Distance <= 0.0f + Delta) || (Distance >= Spline->GetSplineLength() - Delta)))
	{
		float ClampedDistance = FMath::Clamp(Distance, 0.0f, Spline->GetSplineLength());
		OutDirection = Spline->GetRightVectorAtDistanceAlongSpline(ClampedDistance, CoordinateSpace);
	}
	else
	{
		FVector RightBefore = Spline->GetRightVectorAtDistanceAlongSpline(Distance - Delta, CoordinateSpace);
		FVector RightAfter  = Spline->GetRightVectorAtDistanceAlongSpline(Distance + Delta, CoordinateSpace);
		float AngleBetween = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(RightBefore, RightAfter)));
		if (FMath::IsNearlyEqual(FMath::Abs(AngleBetween), 0.0f, 0.001f))
		{
			OutDirection = RightAfter;
		}
		else
		{
			// We always want to rotate using right-handed chirality.
			if (AngleBetween < 0.0f)
			{
				AngleBetween += 360.0f;
			}
			FVector Axis = FVector::CrossProduct(RightBefore, RightAfter).GetSafeNormal();
			OutDirection = RightBefore.RotateAngleAxis(0.5f * AngleBetween, Axis);
			OutBendAngle = AngleBetween;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void UWorldBLDRuntimeUtilsLibrary::CleanupSpline(
	class USplineComponent* Spline, 
	const FCleanupSplineParams& Params, 
	FCleanupSplineResult& OutResult)
{
	OutResult = FCleanupSplineResult();

	if (!IsValid(Spline))
	{
		/// @TODO: errors
		return;
	}

	int32 OrigPointCount = Spline->GetNumberOfSplinePoints();
	TArray<int32> PointRemap;
	TArray<int32> IdentityMap;
	for (int32 Idx = 0; Idx < Spline->GetNumberOfSplinePoints(); Idx += 1)
	{
		PointRemap.Add(Idx);
	}
	IdentityMap = PointRemap;

	auto IndexWrapped = [&](int32 Idx, bool bReversed = false) -> int32
	{
		int32 Points = Spline->GetNumberOfSplinePoints();
		int32 NewIdx = (Idx + Points) % Points;
		if (bReversed)
		{
			NewIdx = Points - NewIdx - 1;
		}
		return NewIdx;
	};

	if (Params.bCollapseNearbyPoints)
	{
		int32 EndOffset = Spline->IsClosedLoop() ? 1 : -1;
		int32 NumPoints = Spline->GetNumberOfSplinePoints();
		TArray<int32> PointsToRemove;
		for (int32 Idx = 0; (NumPoints >= 2) && (Idx < (NumPoints + EndOffset)); Idx += 1)
		{
			int32 Ai = IndexWrapped(Idx + 0), Bi = IndexWrapped(Idx + 1);
			{
				FVector Locations[2] = {
					Spline->GetLocationAtSplinePoint(Ai, ESplineCoordinateSpace::Local), // A
					Spline->GetLocationAtSplinePoint(Bi, ESplineCoordinateSpace::Local), // B
				};

				FVector AB = (Locations[1] - Locations[0]);
				if (AB.SquaredLength() <= (Params.CollapseRadius * Params.CollapseRadius))
				{
					PointsToRemove.Add(Bi);
				}
			}
		}

		// Remove points in reverse
		if (Spline->IsClosedLoop())
		{
			PointsToRemove.Sort(); // Do this for when we have a closed loop
		}
		for (int32 Idx = PointsToRemove.Num() - 1; Idx >= 0; Idx -= 1)
		{
			PointRemap.RemoveAt(PointsToRemove[Idx]);
			Spline->RemoveSplinePoint(PointsToRemove[Idx], false);
		}

		if (PointsToRemove.Num() > 0)
		{
			Spline->UpdateSpline();
		}
	}

	if (Params.bRemoveRedundantLinearPoints)
	{
		int32 EndOffset = Spline->IsClosedLoop() ? 1 : -2;
		int32 NumPoints = Spline->GetNumberOfSplinePoints();
		TArray<int32> PointsToRemove;
		for (int32 Idx = 0; (NumPoints >= 3) && (Idx < (NumPoints + EndOffset)); Idx += 1)
		{
			int32 Ai = IndexWrapped(Idx + 0), Bi = IndexWrapped(Idx + 1), Ci = IndexWrapped(Idx + 2);
			if ((Spline->GetSplinePointType(Ai) == Spline->GetSplinePointType(Bi)) &&
				(Spline->GetSplinePointType(Bi) == Spline->GetSplinePointType(Ci)) &&
				(Spline->GetSplinePointType(Ai) == ESplinePointType::Linear))
			{
				FVector Locations[3] = {
					Spline->GetLocationAtSplinePoint(Ai, ESplineCoordinateSpace::Local), // A
					Spline->GetLocationAtSplinePoint(Bi, ESplineCoordinateSpace::Local), // B
					Spline->GetLocationAtSplinePoint(Ci, ESplineCoordinateSpace::Local), // C
				};

				FVector AB = (Locations[1] - Locations[0]).GetSafeNormal();
				FVector BC = (Locations[2] - Locations[1]).GetSafeNormal();
				float AngleBetween = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(AB, BC)));
				if (FMath::Abs(AngleBetween) <= Params.LinearPointDeflectionThreshold)
				{
					PointsToRemove.AddUnique(Bi);
				}
			}
		}

		// Remove points in reverse
		if (Spline->IsClosedLoop())
		{
			PointsToRemove.Sort(); // Do this for when we have a closed loop
		}
		for (int32 Idx = PointsToRemove.Num() - 1; Idx >= 0; Idx -= 1)
		{
			PointRemap.RemoveAt(PointsToRemove[Idx]);
			Spline->RemoveSplinePoint(PointsToRemove[Idx], false);
		}

		if (PointsToRemove.Num() > 0)
		{
			Spline->UpdateSpline();
		}
	}

	if (Params.bRemoveMultipointCorners)
	{
		int32 EndOffset = Spline->IsClosedLoop() ? 2 : -3;
		for (int32 Idx = 0; (Spline->GetNumberOfSplinePoints() >= 4) && (Idx < Spline->GetNumberOfSplinePoints() + EndOffset); Idx += 1)
		{
			int32 Ai = IndexWrapped(Idx + 0), Bi = IndexWrapped(Idx + 1), Ci = IndexWrapped(Idx + 2), Di = IndexWrapped(Idx + 3);
			if ((Spline->GetSplinePointType(Ai) == Spline->GetSplinePointType(Bi)) &&
				(Spline->GetSplinePointType(Bi) == Spline->GetSplinePointType(Ci)) &&
				(Spline->GetSplinePointType(Ci) == Spline->GetSplinePointType(Di)) &&
				(Spline->GetSplinePointType(Ai) == ESplinePointType::Linear))
			{
				TArray<FVector> Locations = {
					Spline->GetLocationAtSplinePoint(Ai, ESplineCoordinateSpace::Local), // A
					Spline->GetLocationAtSplinePoint(Bi, ESplineCoordinateSpace::Local), // B
					Spline->GetLocationAtSplinePoint(Ci, ESplineCoordinateSpace::Local), // C
					Spline->GetLocationAtSplinePoint(Di, ESplineCoordinateSpace::Local), // D
				};

				float Distance = (Locations[2] - Locations[1]).Length();
				if (Distance <= Params.MultipointCornerRadius)
				{
#if 1
					// Simply average the points together.
					FVector NewCorner = 0.5f * (Locations[2] + Locations[1]);
					Spline->SetLocationAtSplinePoint(Ci, NewCorner, ESplineCoordinateSpace::Local);
					Spline->RemoveSplinePoint(Bi);
					PointRemap.RemoveAt(Bi);
					Idx -= 1;
#else
					// For geometric thinking, see: https://www.notion.so/WorldBLDRuntimeUtilsLibrary-CleanupSpline-Multi-point-corners-e374a6cfd4a442ca9a0067cadedd301e?pvs=4
					FVector BA = (Locations[0] - Locations[1]).GetSafeNormal();
					FVector CD = (Locations[3] - Locations[2]).GetSafeNormal();
					float AngleBetween = FMath::Acos(FVector::DotProduct(BA, CD));
					// If the angle is nearly zero, then the point we might place could go really far out in the distance (avoid that!).
					if (FMath::Abs(AngleBetween) > 0.001f)
					{
						float Extension = (0.5f * Distance) / FMath::Sin(AngleBetween / 2.0f);
						FVector NewCorner = Locations[2] - CD * Extension; // Move C to the new spot.
						FVector NewCorner = 0.5f * (Locations[2] + Locations[1]);
						Spline->SetLocationAtSplinePoint(Ci, NewCorner, ESplineCoordinateSpace::Local);
						Spline->RemoveSplinePoint(Bi);
						PointRemap.RemoveAt(Bi);
						Idx -= 1;
					}
#endif
				}
			}
		}
	}

	if (Params.bRemoveQuickReversals)
	{
		for (int32 IterIdx = 0; IterIdx < 2; IterIdx += 1)
		{
			int32 EndOffset = Spline->IsClosedLoop() ? 1 : -2;
			int32 NumPoints = Spline->GetNumberOfSplinePoints();
			TArray<int32> PointsToRemove;
			bool bReversed = IterIdx != 0;

			for (int32 Idx = 0; (NumPoints >= 3) && (Idx < (NumPoints + EndOffset)); Idx += 1)
			{
				int32 Ai = IndexWrapped(Idx + 0, bReversed), Bi = IndexWrapped(Idx + 1, bReversed), Ci = IndexWrapped(Idx + 2, bReversed);
				{
					FVector Locations[3] = {
						Spline->GetLocationAtSplinePoint(Ai, ESplineCoordinateSpace::Local), // A
						Spline->GetLocationAtSplinePoint(Bi, ESplineCoordinateSpace::Local), // B
						Spline->GetLocationAtSplinePoint(Ci, ESplineCoordinateSpace::Local), // C
					};

					FVector AB = (Locations[1] - Locations[0]);
					FVector BC = (Locations[2] - Locations[1]);
					bool bABisSmall = (AB.SquaredLength() <= (Params.QuickReversalDistanceThreshold * Params.QuickReversalDistanceThreshold));
					bool bBCisSmall = (BC.SquaredLength() <= (Params.QuickReversalDistanceThreshold * Params.QuickReversalDistanceThreshold));
					if ((bABisSmall || bBCisSmall) && (bABisSmall != bBCisSmall) && FMath::Sign(FVector::DotProduct(AB.GetSafeNormal(), BC.GetSafeNormal())) < 0.0f)
					{
						PointsToRemove.AddUnique(bABisSmall ? Bi : Ci);
					}
				}
			}

			// Remove points in reverse
			if (Spline->IsClosedLoop())
			{
				PointsToRemove.Sort(); // Do this for when we have a closed loop
			}
			for (int32 Idx = PointsToRemove.Num() - 1; Idx >= 0; Idx -= 1)
			{
				PointRemap.RemoveAt(PointsToRemove[Idx]);
				Spline->RemoveSplinePoint(PointsToRemove[Idx], false);
			}

			if (PointsToRemove.Num() > 0)
			{
				Spline->UpdateSpline();
			}
		}
	}

	for (int32 Idx : IdentityMap)
	{
		if (!PointRemap.Contains(Idx))
		{
			OutResult.RemovedPoints.Add(Idx);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void UWorldBLDRuntimeUtilsLibrary::RebuildSpline(class USplineComponent* Spline)
{
	const int32 NumSplinePoints = Spline->GetNumberOfSplinePoints();
	struct FLocalPointSet {
		FVector WorldLoc {FVector::ZeroVector};
		ESplinePointType::Type PointType {ESplinePointType::Linear};
		FVector ArriveTangent {FVector::ZeroVector};
		FVector LeaveTangent {FVector::ZeroVector};
	};
	
	TArray<FLocalPointSet> PointData;
	for (int32 Idx = 0; Idx < NumSplinePoints; Idx += 1)
	{
		FLocalPointSet& Data = PointData.AddDefaulted_GetRef();
		Data.WorldLoc = Spline->GetLocationAtSplinePoint(Idx, ESplineCoordinateSpace::World);
		Data.PointType = Spline->GetSplinePointType(Idx);
		Data.ArriveTangent = Spline->GetArriveTangentAtSplinePoint(Idx, ESplineCoordinateSpace::World);
		Data.LeaveTangent = Spline->GetLeaveTangentAtSplinePoint(Idx, ESplineCoordinateSpace::World);
	}

	Spline->SplineCurves.Position.Points.Reset(NumSplinePoints);
	Spline->SplineCurves.Rotation.Points.Reset(NumSplinePoints);
	Spline->SplineCurves.Scale.Points.Reset(NumSplinePoints);
	
	for (int32 Idx = 0; Idx < NumSplinePoints; Idx += 1)
	{
		const FLocalPointSet& Data = PointData[Idx];
		Spline->AddSplinePoint(Data.WorldLoc, ESplineCoordinateSpace::World, /* Update? */ false);
		Spline->SetTangentsAtSplinePoint(Idx, Data.ArriveTangent, Data.LeaveTangent, ESplineCoordinateSpace::World, /* Update? */ false);
		Spline->SetSplinePointType(Idx, Data.PointType, /* Update? */ false);
	}

	Spline->UpdateSpline();
}

void UWorldBLDRuntimeUtilsLibrary::SampleSplineToTransforms(class USplineComponent* Spline, FSplineSamplingParams SweepPathSampleParams, TArray<FTransform>& OutTransforms)
{
	OutTransforms.Empty();
	const float SweepPathLength =  Spline->GetSplineLength();
	VisitPointsAlongSpline(Spline, 
			(SweepPathSampleParams.StartDistance >= 0.0f) ? SweepPathSampleParams.StartDistance : 0.0f, 
			(SweepPathSampleParams.EndDistance >= 0.0f) ? SweepPathSampleParams.EndDistance : SweepPathLength, 
			SweepPathSampleParams, [&](float CurrentDistance)
	{
		OutTransforms.Add(Spline->GetTransformAtDistanceAlongSpline(CurrentDistance, ESplineCoordinateSpace::Local));
		return true;
	});
}

float UWorldBLDRuntimeUtilsLibrary::ComputeSplineWrappedDistance(class USplineComponent* Spline, float Distance)
{
	float WrappedDistance = Distance;
	if (IsValid(Spline))
	{
		const float SplineLen = Spline->GetSplineLength();
		if (Spline->IsClosedLoop())
		{
			WrappedDistance = (Distance < 0.0f) ? 
					(SplineLen - FMath::Fmod(-Distance, SplineLen)) : 
					FMath::Fmod(Distance, SplineLen);
		}
		else
		{
			WrappedDistance = FMath::Clamp(Distance, 0.0f, SplineLen);
		}
	}
	return WrappedDistance;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void UWorldBLDRuntimeUtilsLibrary::SplineCurves_AddPoint(struct FSplineCurves& SplineCurves, const FVector& Position, const FQuat& Rotation, const FVector& Scale, bool bLinear)
{
	const float InKey = SplineCurves.Position.Points.Num() ? SplineCurves.Position.Points.Last().InVal + 1.0f : 0.0f;
	SplineCurves.Position.Points.Emplace(InKey, Position, FVector::ZeroVector, FVector::ZeroVector, bLinear ? CIM_Linear : CIM_CurveAuto);
	SplineCurves.Rotation.Points.Emplace(InKey, Rotation, FQuat::Identity, FQuat::Identity, bLinear ? CIM_Linear : CIM_CurveAuto);
	SplineCurves.Scale.Points.Emplace(InKey, Scale, FVector::ZeroVector, FVector::ZeroVector, bLinear ? CIM_Linear : CIM_CurveAuto);
}

void UWorldBLDRuntimeUtilsLibrary::SetSplineCurvesOnComponent(class USplineComponent* Spline, const struct FSplineCurves& SplineCurves)
{
	Spline->SplineCurves = SplineCurves;
	SplineCurves_Sanitize(Spline->SplineCurves);
	Spline->SetClosedLoop(SplineCurves.Position.bIsLooped);
	FixupLinearSplineEndTangent(Spline);
}

void UWorldBLDRuntimeUtilsLibrary::SplineCurves_Sanitize(struct FSplineCurves& SplineCurves)
{
	if ((SplineCurves.Position.Points.Num() != SplineCurves.Rotation.Points.Num()) || (SplineCurves.Position.Points.Num() != SplineCurves.Scale.Points.Num()))
	{
		// Fix up rotations
		while (SplineCurves.Rotation.Points.Num() < SplineCurves.Position.Points.Num())
		{
			SplineCurves.Rotation.AddPoint(SplineCurves.Position.Points[SplineCurves.Rotation.Points.Num()].InVal, FQuat::Identity);
		}
		while (SplineCurves.Rotation.Points.Num() > SplineCurves.Position.Points.Num())
		{
			SplineCurves.Rotation.Points.RemoveAt(SplineCurves.Rotation.Points.Num() - 1);
		}

		// Fix up scales
		while (SplineCurves.Scale.Points.Num() < SplineCurves.Position.Points.Num())
		{
			SplineCurves.Scale.AddPoint(SplineCurves.Position.Points[SplineCurves.Scale.Points.Num()].InVal, FVector(1));
		}
		while (SplineCurves.Scale.Points.Num() > SplineCurves.Position.Points.Num())
		{
			SplineCurves.Scale.Points.RemoveAt(SplineCurves.Scale.Points.Num() - 1);
		}
	}
}

void UWorldBLDRuntimeUtilsLibrary::SanitizeSplineComponent(USplineComponent* Spline, bool bUpdate)
{
	if (IsValid(Spline))
	{
		SplineCurves_Sanitize(Spline->SplineCurves);
		if (bUpdate)
		{
			Spline->UpdateSpline();
		}
	}
}

void UWorldBLDRuntimeUtilsLibrary::MakeCircularSpline(USplineComponent* Spline, FVector Center, float Radius, int32 NumPoints)
{
	if (IsValid(Spline))
	{		
		NumPoints = FMath::Max(4, NumPoints);
		Spline->ClearSplinePoints(false);

		float Yaw = 360.0 / NumPoints;
		float TangLen = Radius * FMath::Abs((4.0 / 3.0) * FMath::Tan(FMath::DegreesToRadians(Yaw) / 4.0)) * 3.0;
		
		for (int i = 0; i < NumPoints; i++)
		{			
			FVector Point = Center + FRotator(0, Yaw * i, 0).Vector() * Radius;
			FVector Tang = FRotator(0, Yaw * i + 90.0, 0).Vector().GetSafeNormal() * TangLen;
			Spline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
			Spline->SetTangentAtSplinePoint(i, Tang, ESplineCoordinateSpace::Local, false);
		}

		Spline->SetClosedLoop(true, true);		
	}	
}

void UWorldBLDRuntimeUtilsLibrary::ReverseSplineCurves(struct FSplineCurves& SplineCurves)
{
	SplineCurves_Sanitize(SplineCurves);

	TArray<float> InVals;
	InVals.Reserve(SplineCurves.Position.Points.Num());
	for (const auto& InterpPoint : SplineCurves.Position.Points)
	{
		InVals.Add(InterpPoint.InVal);
	}
	Algo::Reverse(SplineCurves.Position.Points);
	Algo::Reverse(SplineCurves.Rotation.Points);
	Algo::Reverse(SplineCurves.Scale.Points);

	for (int32 Idx = 0; Idx < InVals.Num(); Idx += 1)
	{
		SplineCurves.Position.Points[Idx].InVal = InVals[Idx];
		SplineCurves.Rotation.Points[Idx].InVal = InVals[Idx];
		SplineCurves.Scale.Points[Idx].InVal = InVals[Idx];
		/// @TODO: Do we need to reverse tangents or rotations?
	}
}

void UWorldBLDRuntimeUtilsLibrary::ReverseSpline(USplineComponent* Spline)
{
    if (!IsValid(Spline) || Spline->GetNumberOfSplinePoints() < 2)
    {
        return;
    }

    // Work directly on the curves to preserve all attributes
    FSplineCurves& Curves = Spline->SplineCurves;
    ReverseSplineCurves(Curves);

    // Swap and negate tangents per point to preserve shape with reversed traversal
    for (int32 i = 0; i < Curves.Position.Points.Num(); ++i)
    {
        auto& Pt = Curves.Position.Points[i];
        const FVector Tmp = Pt.ArriveTangent;
        Pt.ArriveTangent = -Pt.LeaveTangent;
        Pt.LeaveTangent = -Tmp;
    }

    // Maintain closed state and refresh
    const bool bClosed = Spline->IsClosedLoop();
    Spline->UpdateSpline();
    if (bClosed)
    {
        Spline->SetClosedLoop(true, true);
    }
}

double UWorldBLDRuntimeUtilsLibrary::GetAngleAtSplinePoint(const USplineComponent* Spline, int32 Index)
{
	if (!IsValid(Spline) || Spline->GetNumberOfSplinePoints() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWorldBLDRuntimeUtilsLibrary::GetAngleAtSplinePoint: Invalid Spline"));
		return 0;
	}

	int NumPoints = Spline->GetNumberOfSplinePoints();

	Index = (Index + NumPoints) % NumPoints;
	int IPrev = (Index - 1 + NumPoints) % NumPoints;
	int INext = (Index + 1 + NumPoints) % NumPoints;

	const TArray <FInterpCurvePoint<FVector>>& Points = Spline->SplineCurves.Position.Points;

	const FVector& CurrPoint = Points[Index].OutVal;
	const FVector& PrevPoint = Points[IPrev].OutVal;
	const FVector& NextPoint = Points[INext].OutVal;

	FVector VIn = (CurrPoint - PrevPoint).GetSafeNormal();
	FVector VOut = (NextPoint - CurrPoint).GetSafeNormal();
	if (!Spline->IsClosedLoop())
	{
		if (Index == 0) VIn = VOut * 1.0f;
		if (Index == NumPoints - 1)  VOut = VIn * -1.0f;
	}

	return FQuat::FindBetweenNormals(VIn, VOut).GetAngle();
}

void UWorldBLDRuntimeUtilsLibrary::SetAllSplinePointTypes(USplineComponent* Spline, ESplinePointType::Type SplinePointType)
{
	if (!IsValid(Spline))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWorldBLDRuntimeUtilsLibrary::SetAllSplinePointTypes: Invalid Spline"));
		return;
	}

	bool bChanged = false;
	for (int i = 0; i < Spline->GetNumberOfSplinePoints(); i++)
	{
		if (Spline->GetSplinePointType(i) != SplinePointType)
		{
			Spline->SetSplinePointType(i, SplinePointType, false);
			bChanged = true;
		}

	}
	if (bChanged)
	{
		Spline->UpdateSpline();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////

// Ported from CityBLD_SplineUtils Blueprint Function Library 

void UWorldBLDRuntimeUtilsLibrary::GetLengthOfSegment(USplineComponent* Spline, int32 Segment, float& Length)
{
	if (!IsValid(Spline))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWorldBLDRuntimeUtilsLibrary::GetLengthOfSegment: Invalid Spline"));
		Length = 0.0;
	}
	Length = Spline->SplineCurves.GetSegmentLength(Segment, 1.0f);
}

void UWorldBLDRuntimeUtilsLibrary::GetMidpointOfSegment(USplineComponent* Spline, int32 Segment, ESplineCoordinateSpace::Type CoordinateSpace, FTransform& Transform, float& Distance)
{
	if (!IsValid(Spline))
	{		
		UE_LOG(LogTemp, Warning, TEXT("UWorldBLDRuntimeUtilsLibrary::GetLengthOfSegment: Invalid Spline"));
		Distance = 0.0;
	}
	
	float SegLen = Spline->SplineCurves.GetSegmentLength(Segment, .5f);
	Distance = Spline->GetDistanceAlongSplineAtSplinePoint(Segment) + SegLen;
	Transform = Spline->GetTransformAtDistanceAlongSpline(Distance, CoordinateSpace);
}

void UWorldBLDRuntimeUtilsLibrary::ConvertSplineSampleFramesToUVs(const TArray<FTransform>& Frames, double StartDistance, double EndDistance, TArray<double>& VCoordinates)
{
	for (const FTransform& Frame : Frames)
	{
		int MaxIdx = 0;
		float MaxVal = FMath::Max(VCoordinates, &MaxIdx);
		float Dist = (EndDistance - StartDistance) / 1000.0 + MaxVal;
		VCoordinates.Push(Dist);
	}
}

void UWorldBLDRuntimeUtilsLibrary::SetAllSplinePointTypesToLinear(USplineComponent* Spline)
{
	if (!IsValid(Spline))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWorldBLDRuntimeUtilsLibrary::SetAllSplinePointTypes: Invalid Spline"));
		return;
	}

	for (int i = 0; i < Spline->GetNumberOfSplinePoints(); i++)
	{
		Spline->SetSplinePointType(i, ESplinePointType::Linear, false);
	}
	Spline->UpdateSpline();
}

void UWorldBLDRuntimeUtilsLibrary::GetNearestOfVectorArray(const TArray<FVector>& InputArray, const FVector& InputLocation, FVector& NearestVector, double& DistanceOfNearestVector, int32& NearestIndex)
{
	using namespace UE::Geometry;
	if (!InputArray.IsEmpty())
	{
		TArrayView<const FVector> View = InputArray;
		NearestIndex = CurveUtil::FindNearestIndex<float, FVector>(View, InputLocation);
		NearestVector = InputArray[NearestIndex];
		DistanceOfNearestVector = FVector::Distance(NearestVector, InputLocation);
	}
}

void UWorldBLDRuntimeUtilsLibrary::GetAngleOfLinearSplinePoint(const USplineComponent* Spline, int32 PointIndex, float& Angle)
{
	if (!IsValid(Spline))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWorldBLDRuntimeUtilsLibrary::GetAngleOfLinearSplinePoint: Invalid Spline"));
		Angle = 0;
	}

	float Dist = Spline->GetDistanceAlongSplineAtSplinePoint(PointIndex);
	FVector VPrev = Spline->GetTangentAtDistanceAlongSpline(Dist - 1.0f, ESplineCoordinateSpace::Local).GetSafeNormal();
	FVector VNext = Spline->GetTangentAtDistanceAlongSpline(Dist + 1.0f, ESplineCoordinateSpace::Local).GetSafeNormal();

	/*double Rad = FQuat::FindBetweenNormals(VPrev, VNext).GetAngle();
	Angle = UKismetMathLibrary::RadiansToDegrees(Rad);*/
	
	GetAngleBetweenForwardVectors(VPrev, VNext, Angle);
}

void UWorldBLDRuntimeUtilsLibrary::GetAngleBetweenForwardVectors(const FVector& VectorA, const FVector& VectorB, float& Angle)
{
	float Dir = FVector::CrossProduct(VectorA, VectorB).Z > 0 ? 1.0 : -1.0;
	double Dot = FVector::DotProduct(VectorA, VectorB);
	Angle = UKismetMathLibrary::DegAcos(Dot) * Dir;
}

void UWorldBLDRuntimeUtilsLibrary::SortSplinePointsClockwise(USplineComponent* Spline)
{
	if (!IsValid(Spline))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWorldBLDRuntimeUtilsLibrary::SortSplinePointsClockwise: Invalid Spline"));
		return;
	}

	int32 NumPoints = Spline->GetNumberOfSplinePoints();
	TArray<FVector> Points;
	Points.Reserve(NumPoints);
	double SumOverEdges = 0.0;

	// NumPoints - 1?
	for (int32 Index = 0; Index < NumPoints; Index++)
	{
		const FVector PointA = Spline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::Local);
		const FVector PointB = Spline->GetLocationAtSplinePoint(Index + 1, ESplineCoordinateSpace::Local);
		Points.Add(PointA);
		SumOverEdges += ((PointB.X - PointA.X) * (PointB.Y + PointA.Y));
	}

	Points.Add(Spline->GetLocationAtSplinePoint(NumPoints - 1, ESplineCoordinateSpace::Local));

	if (SumOverEdges < 0.0)
	{
		Spline->ClearSplinePoints(true);
		for (int32 i = Points.Num() - 1; i >= 0; i--)
		{
			Spline->AddSplinePoint(Points[i], ESplineCoordinateSpace::Local, false);
		}
		Spline->UpdateSpline();
	}
}

void UWorldBLDRuntimeUtilsLibrary::GetSplinePointsVectors(USplineComponent* Spline, ESplineCoordinateSpace::Type CoordinateSpace, TArray<FVector>& OutLocations)
{
	if (!IsValid(Spline))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWorldBLDRuntimeUtilsLibrary::GetSplinePointsVectors: Invalid Spline"));
		return;
	}

	for (int i = 0; i < Spline->GetNumberOfSplinePoints(); i++)
	{
		FVector Location = Spline->GetLocationAtSplinePoint(i, CoordinateSpace);
		OutLocations.Push(Location);
	}
}

void UWorldBLDRuntimeUtilsLibrary::GetSplinePointsVectors2d(const USplineComponent* Spline, ESplineCoordinateSpace::Type CoordinateSpace, TArray<FVector2D>& OutLocations)
{
	if (!IsValid(Spline))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWorldBLDRuntimeUtilsLibrary::GetSplinePointsVectors: Invalid Spline"));
		return;
	}

	for (int i = 0; i < Spline->GetNumberOfSplinePoints(); i++)
	{
		FVector2D Location = (FVector2D)Spline->GetLocationAtSplinePoint(i, CoordinateSpace);
		OutLocations.Push(Location);
	}
}

void UWorldBLDRuntimeUtilsLibrary::PlaceSplineMeshesAlongSpline(USplineComponent* Spline, UStaticMesh* Mesh, AActor* OwnerActor)
{
	if (!IsValid(Spline))
	{
		UE_LOG(LogWorldBLD, Warning, TEXT("PlaceSplineMeshesAlongSpline: Invalid Spline provided"));
		return;
	}

	if (!IsValid(Mesh))
	{
		UE_LOG(LogWorldBLD, Warning, TEXT("PlaceSplineMeshesAlongSpline: Invalid Mesh provided"));
		return;
	}

	if (!IsValid(OwnerActor))
	{
		UE_LOG(LogWorldBLD, Warning, TEXT("PlaceSplineMeshesAlongSpline: Invalid OwnerActor provided"));
		return;
	}

	// Calculate the spline mesh data
	TArray<FSplineMeshData> MeshData = CalculateSplineMeshData(Spline, Mesh);
	
	// Spawn the spline mesh components
	SpawnSplineMeshes(MeshData, Spline, OwnerActor);
}

static void BuildAdaptiveSplineMeshData(USplineComponent* Spline, UStaticMesh* Mesh, float MaxSegmentLength, float CurvatureAngleThresholdDeg, TArray<FSplineMeshData>& OutData)
{
    OutData.Reset();
    if (!IsValid(Spline) || !IsValid(Mesh))
    {
        return;
    }

    const float TotalLen = Spline->GetSplineLength();
    if (TotalLen <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    // Base segment length from mesh bounds
    FBoxSphereBounds MeshBounds = Mesh->GetBounds();
    float BaseLen = FMath::Max(1.0f, MeshBounds.BoxExtent.X * 2.0f);

    // If explicit MaxSegmentLength is provided and smaller, use it
    if (MaxSegmentLength > 1.0f)
    {
        BaseLen = FMath::Min(BaseLen, MaxSegmentLength);
    }

    const float AngleThreshold = CurvatureAngleThresholdDeg > 0.0f ? CurvatureAngleThresholdDeg : 7.5f; // default tighter for corners

    float CurrentStart = 0.0f;
    while (CurrentStart < TotalLen - KINDA_SMALL_NUMBER)
    {
        float CurrentEnd = FMath::Min(CurrentStart + BaseLen, TotalLen);

        // Refine end by curvature: march within [CurrentStart, CurrentStart+BaseLen]
        float Step = BaseLen / 5.0f; // sample a few times per base segment
        float LastAngle = 0.0f;
        float CandidateEnd = CurrentEnd;
        for (float d = CurrentStart + Step; d <= CurrentEnd; d += Step)
        {
            const FVector T0 = Spline->GetTangentAtDistanceAlongSpline(d - Step, ESplineCoordinateSpace::World).GetSafeNormal();
            const FVector T1 = Spline->GetTangentAtDistanceAlongSpline(d, ESplineCoordinateSpace::World).GetSafeNormal();
            const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(T0, T1), -1.0f, 1.0f)));
            LastAngle += AngleDeg;
            if (LastAngle >= AngleThreshold)
            {
                CandidateEnd = d;
                break;
            }
        }

        FSplineMeshData Data;
        Data.Mesh = Mesh;
        Data.StartDistance = CurrentStart;
        Data.EndDistance = CandidateEnd;
        OutData.Add(Data);

        CurrentStart = CandidateEnd;
    }
}

void UWorldBLDRuntimeUtilsLibrary::PlaceSplineMeshesAlongSplineAdvanced(USplineComponent* Spline, UStaticMesh* Mesh, AActor* OwnerActor, float MaxSegmentLength, float CurvatureAngleThresholdDeg)
{
    if (!IsValid(Spline) || !IsValid(Mesh) || !IsValid(OwnerActor))
    {
        UE_LOG(LogWorldBLD, Warning, TEXT("PlaceSplineMeshesAlongSplineAdvanced: Invalid inputs"));
        return;
    }

    TArray<FSplineMeshData> MeshData;
    BuildAdaptiveSplineMeshData(Spline, Mesh, MaxSegmentLength, CurvatureAngleThresholdDeg, MeshData);
    if (MeshData.Num() == 0)
    {
        // Fallback to default placement
        PlaceSplineMeshesAlongSpline(Spline, Mesh, OwnerActor);
        return;
    }
    SpawnSplineMeshes(MeshData, Spline, OwnerActor);
}

TArray<FSplineMeshData> UWorldBLDRuntimeUtilsLibrary::CalculateSplineMeshData(USplineComponent* Spline, UStaticMesh* Mesh)
{
	TArray<FSplineMeshData> MeshDataArray;
	
	if (!IsValid(Spline) || !IsValid(Mesh))
	{
		UE_LOG(LogWorldBLD, Warning, TEXT("CalculateSplineMeshData: Invalid Spline or Mesh provided"));
		return MeshDataArray;
	}

	// Get the mesh bounds to determine the length of each segment
	FBoxSphereBounds MeshBounds = Mesh->GetBounds();
	float MeshLength = MeshBounds.BoxExtent.X * 2.0f; // X is typically the length axis for spline meshes
	
	if (MeshLength <= 0.0f)
	{
		UE_LOG(LogWorldBLD, Warning, TEXT("CalculateSplineMeshData: Invalid mesh length (%.2f), using default value"), MeshLength);
		MeshLength = 100.0f; // Default fallback
	}

	// Get the total length of the spline
	float TotalSplineLength = Spline->GetSplineLength();
	
	if (TotalSplineLength <= 0.0f)
	{
		UE_LOG(LogWorldBLD, Warning, TEXT("CalculateSplineMeshData: Invalid spline length (%.2f)"), TotalSplineLength);
		return MeshDataArray;
	}

	// Calculate how many mesh segments we need
	int32 NumSegments = FMath::CeilToInt(TotalSplineLength / MeshLength);
	MeshDataArray.Reserve(NumSegments);

	// Create mesh data for each segment
	for (int32 i = 0; i < NumSegments; ++i)
	{
		FSplineMeshData MeshData;
		MeshData.Mesh = Mesh;
		MeshData.StartDistance = i * MeshLength;
		MeshData.EndDistance = FMath::Min((i + 1) * MeshLength, TotalSplineLength);
		
		MeshDataArray.Add(MeshData);
	}

	return MeshDataArray;
}

void UWorldBLDRuntimeUtilsLibrary::SpawnSplineMeshes(const TArray<FSplineMeshData>& MeshData, USplineComponent* Spline, AActor* OwnerActor)
{
	if (!IsValid(Spline) || !IsValid(OwnerActor))
	{
		UE_LOG(LogWorldBLD, Warning, TEXT("SpawnSplineMeshes: Invalid Spline or OwnerActor provided"));
		return;
	}

	if (MeshData.Num() == 0)
	{
		UE_LOG(LogWorldBLD, Warning, TEXT("SpawnSplineMeshes: No mesh data provided"));
		return;
	}

    // Offset component naming to avoid collisions across multiple calls for the same OwnerActor
    TArray<USplineMeshComponent*> PreExistingSplineMeshes;
    OwnerActor->GetComponents<USplineMeshComponent>(PreExistingSplineMeshes);
    const int32 BaseIndex = PreExistingSplineMeshes.Num();

    // Create spline mesh components for each mesh data entry
    for (int32 i = 0; i < MeshData.Num(); ++i)
	{
		const FSplineMeshData& Data = MeshData[i];
		
		if (!IsValid(Data.Mesh))
		{
			UE_LOG(LogWorldBLD, Warning, TEXT("SpawnSplineMeshes: Invalid mesh at index %d"), i);
			continue;
		}

        // Create a unique name for the spline mesh component
        FName ComponentName = *FString::Printf(TEXT("SplineMeshComponent_%d"), BaseIndex + i);
		
        // Create the spline mesh component
        USplineMeshComponent* SplineMeshComponent = NewObject<USplineMeshComponent>(OwnerActor, USplineMeshComponent::StaticClass(), ComponentName, RF_Transactional);
		
		if (!SplineMeshComponent)
		{
			UE_LOG(LogWorldBLD, Error, TEXT("SpawnSplineMeshes: Failed to create SplineMeshComponent at index %d"), i);
			continue;
		}

        // Set mobility BEFORE attaching/registration to avoid static-to-movable attach warnings
        SplineMeshComponent->SetMobility(EComponentMobility::Movable);

        // Set the static mesh
        SplineMeshComponent->SetStaticMesh(Data.Mesh);

        // Get start and end positions and tangents from the spline in WORLD space,
        // then convert to OwnerActor's local space so the component-local values are correct
        const FVector StartPosWS = Spline->GetLocationAtDistanceAlongSpline(Data.StartDistance, ESplineCoordinateSpace::World);
        const FVector StartTangentWS = Spline->GetTangentAtDistanceAlongSpline(Data.StartDistance, ESplineCoordinateSpace::World);
        const FVector EndPosWS = Spline->GetLocationAtDistanceAlongSpline(Data.EndDistance, ESplineCoordinateSpace::World);
        const FVector EndTangentWS = Spline->GetTangentAtDistanceAlongSpline(Data.EndDistance, ESplineCoordinateSpace::World);

        const FTransform OwnerXform = OwnerActor->GetActorTransform();
        const FVector StartPos = OwnerXform.InverseTransformPosition(StartPosWS);
        const FVector StartTangent = OwnerXform.InverseTransformVectorNoScale(StartTangentWS);
        const FVector EndPos = OwnerXform.InverseTransformPosition(EndPosWS);
        const FVector EndTangent = OwnerXform.InverseTransformVectorNoScale(EndTangentWS);

        // Attach to the root component prior to registration
        if (OwnerActor->GetRootComponent())
        {
            SplineMeshComponent->AttachToComponent(OwnerActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        }
        else
        {
            OwnerActor->SetRootComponent(SplineMeshComponent);
        }

        // Apply spline params after attach, before register
        SplineMeshComponent->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent);

        // Add the component to the actor and register
        OwnerActor->AddInstanceComponent(SplineMeshComponent);
        SplineMeshComponent->OnComponentCreated();
        SplineMeshComponent->RegisterComponent();

		OwnerActor->MarkPackageDirty();

#if WITH_EDITOR
		if (GEditor)
		{
			GEditor->Modify();
			GEditor->RedrawLevelEditingViewports();
		}
#endif
	}

	UE_LOG(LogWorldBLD, Log, TEXT("SpawnSplineMeshes: Successfully created %d spline mesh components"), MeshData.Num());
}

void UWorldBLDRuntimeUtilsLibrary::SampleAdaptive(
	double Begin, double End,
	TArray<FVector>& OutPoints,
	TArray<double>& OutDistances,
	TFunctionRef<FVector(double Distance)> GetPositionAt,
	double MaxSquareDist,
	double MinSquareSegLen,
	double MaxSquareSegLen)
{
	if (End < Begin)
	{
		Swap(Begin, End);
	}
	if (End - Begin <= 0.0f)
	{
		return;
	}

	MaxSquareDist = FMath::Max(0.0, MaxSquareDist);
	MinSquareSegLen = FMath::Max(0.0, MinSquareSegLen);
	MaxSquareSegLen = FMath::Max(0.0, MaxSquareSegLen);

	const FVector2d Range{ Begin, End };
	FVector A = GetPositionAt(Range.X);
	FVector B = GetPositionAt(Range.Y);

	OutPoints.Push(A);
	OutPoints.Push(B);
	OutDistances.Reset();
	OutDistances.Push(Range.X);
	OutDistances.Push(Range.Y);

	TArray<TTuple<FVector2D, FVector, FVector>> SegQueue{ {Range, A, B} };

	while (true)
	{
		auto SegTuple = SegQueue.Pop();

		const FVector2D& Rng = SegTuple.Get<0>();
		const FVector& PointA = SegTuple.Get<1>();
		const FVector& PointB = SegTuple.Get<2>();
		const double Len = Rng.Y - Rng.X;
		double DistMid = Rng.X + Len * 0.5;
		const double DistMidL = Rng.X + Len * 0.25;
		const double DistMidR = Rng.X + Len * 0.75;

		FVector PointMid = GetPositionAt(DistMid);
		FVector PointMidL = GetPositionAt(DistMidL);
		FVector PointMidR = GetPositionAt(DistMidR);

		const double DistSquaredL = FMath::PointDistToSegmentSquared(PointMidL, PointA, PointMid);
		const double DistSquaredR = FMath::PointDistToSegmentSquared(PointMidR, PointMid, PointB);

		int InsertIdx = 0;
		for (int i = 0; i < OutDistances.Num(); i++)
		{
			if (OutDistances[i] > DistMid)
			{
				InsertIdx = i;
				break;
			}
		}

		OutDistances.Insert(DistMid, InsertIdx);
		OutPoints.Insert(PointMid, InsertIdx);

		/*double LenA = FVector::DistSquared(PointA, PointMid);
		double LenB = FVector::DistSquared(PointB, PointMid);*/

		const double LenA = DistMid - Rng.X;
		const double LenB = Rng.Y - DistMid;

		//if ((DistSquaredL > MaxSquareDist && LenA > MinSquareSegLen) || LenA > MaxSquareSegLen)

		if ((DistSquaredL > MaxSquareDist || LenA > MaxSquareSegLen) && LenA * 0.5 > MinSquareSegLen)
		{
			SegQueue.Push({ FVector2D(Rng.X, DistMid), PointA, PointMid });
		}
		if ((DistSquaredR > MaxSquareDist || LenB > MaxSquareSegLen) && LenB * 0.5 > MinSquareSegLen)
		{
			SegQueue.Push({ FVector2D(DistMid, Rng.Y), PointMid, PointB });
		}

		if (SegQueue.IsEmpty())
		{
			break;
		}
	}
}

void UWorldBLDRuntimeUtilsLibrary::SampleAdaptiveToTransforms(
	double Begin,
	double End,
	TArray<FTransform>& OutSamples,
	TArray<double>& OutDistances,
	TFunctionRef<FTransform(double Distance)> GetTransformAt,
	double MaxSquareDist,
	double MinSquareSegLen,
	double MaxSquareSegLen)
{
	if (End < Begin)
	{
		Swap(Begin, End);
	}
	if (End - Begin <= 0.0f)
	{
		return;
	}

	MaxSquareDist = FMath::Max(0.0, MaxSquareDist);
	MinSquareSegLen = FMath::Max(0.0, MinSquareSegLen);
	MaxSquareSegLen = FMath::Max(0.0, MaxSquareSegLen);

	const FVector2d Range{ Begin, End };
	FTransform A = GetTransformAt(Range.X);
	FTransform B = GetTransformAt(Range.Y);

	OutSamples.Push(A);
	OutSamples.Push(B);
	OutDistances.Reset();
	OutDistances.Push(Range.X);
	OutDistances.Push(Range.Y);

	TArray<TTuple<FVector2D, FVector, FVector>> SegQueue{ {Range, A.GetLocation(), B.GetLocation()} };

	while (true)
	{
		auto SegTuple = SegQueue.Pop();

		const FVector2D& Rng = SegTuple.Get<0>();
		const FVector& PointA = SegTuple.Get<1>();
		const FVector& PointB = SegTuple.Get<2>();
		const double Len = Rng.Y - Rng.X;
		double DistMid = Rng.X + Len * 0.5;
		const double DistMidL = Rng.X + Len * 0.25;
		const double DistMidR = Rng.X + Len * 0.75;

		FTransform MidSample = GetTransformAt(DistMid);
		FVector PointMid = MidSample.GetLocation();
		FVector PointMidL = GetTransformAt(DistMidL).GetLocation();
		FVector PointMidR = GetTransformAt(DistMidR).GetLocation();

		const double DistSquaredL = FMath::PointDistToSegmentSquared(PointMidL, PointA, PointMid);
		const double DistSquaredR = FMath::PointDistToSegmentSquared(PointMidR, PointMid, PointB);

		int InsertIdx = 0;
		for (int i = 0; i < OutDistances.Num(); i++)
		{
			if (OutDistances[i] > DistMid)
			{
				InsertIdx = i;
				break;
			}
		}

		OutDistances.Insert(DistMid, InsertIdx);
		OutSamples.Insert(MidSample, InsertIdx);

		const double LenA = DistMid - Rng.X;
		const double LenB = Rng.Y - DistMid;

		if ((DistSquaredL > MaxSquareDist || LenA > MaxSquareSegLen) && LenA * 0.5 > MinSquareSegLen)
		{
			SegQueue.Push({ FVector2D(Rng.X, DistMid), PointA, PointMid });
		}
		if ((DistSquaredR > MaxSquareDist || LenB > MaxSquareSegLen) && LenB * 0.5 > MinSquareSegLen)
		{
			SegQueue.Push({ FVector2D(DistMid, Rng.Y), PointMid, PointB });
		}

		if (SegQueue.IsEmpty())
		{
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////
