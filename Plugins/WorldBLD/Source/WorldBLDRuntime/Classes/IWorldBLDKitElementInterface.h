// Copyright WorldBLD LLC. All rights reserved. 

#pragma once

#include "Components/SplineComponent.h"
#include "GameplayTagContainer.h"
#include "IWorldBLDKitElementInterface.generated.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

USTRUCT(BlueprintType)
struct FWorldBLDKitElementProperties
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLDKitElement")
	bool bValidElement {true};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLDKitElement")
	bool bIsBuilding {false};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLDKitElement")
	bool bIsRoad {false};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLDKitElement")
	bool bIsRoadProp {false};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLDKitElement")
	bool bIsBoundary {false};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLDKitElement")
	bool bIsVehicle {false};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLDKitElement")
	bool bIsHiddenElement {false};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLDKitElement")
	bool bIsPlaceholder {false};
};

// An edge to a WorldBLD Kit element.
USTRUCT(BlueprintType)
struct FWorldBLDKitElementEdge
{
	GENERATED_BODY()

	// Which actor owns this edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement")
	AActor* Owner {nullptr};
	
	// The unique identifier of this edge in Owner.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement")
	FName Id {NAME_None};

	// Tags for this edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement", Meta=(Categories="WorldBLD.Detail"))
	FGameplayTagContainer Tags;

	// The spline representing this edge (optional).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement")
	USplineComponent* Spline {nullptr};

	// The section representing this edge (optional).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLDKitElement")
	int32 RoadSectionIndex {0};

	// The transform of Spline/SplineCurve.
	/// @FIXME: This should probably be a local transform and we need to apply the Owner's ToWorld transform as well
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement")
	FTransform SplineCurveToWorldTransform;

	// Used if Spline is invalid.
	UPROPERTY()
	FSplineCurves SplineCurve;

	void WORLDBLDRUNTIME_API AddPoint(const FVector& Location, const FQuat& Rotation = FQuat::Identity, const FVector& Scale = FVector(1), bool bLinear = true);
	bool WORLDBLDRUNTIME_API operator==(const FWorldBLDKitElementEdge& RHS) const;
	bool WORLDBLDRUNTIME_API operator==(const struct FWorldBLDKitElementEdgeRef& RHS) const;
	
	
};

// A reference to an Edge.
USTRUCT(BlueprintType)
struct FWorldBLDKitElementEdgeRef
{
	GENERATED_BODY()

	// The owner of the referenced Edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement")
	AActor* Owner {nullptr};

	// The ID of the referenced Edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement")
	FName Id {NAME_None};

	// Tags for the referenced edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement")
	FGameplayTagContainer Tags;

	// Whether or not we are referring to the reversed version of the Edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement")
	bool bFlipped {false};

	FWorldBLDKitElementEdgeRef() {}
	FWorldBLDKitElementEdgeRef(const FWorldBLDKitElementEdge& Edge) : Owner(Edge.Owner), Id(Edge.Id), Tags(Edge.Tags) {}

	bool IsValid() const;
	FWorldBLDKitElementEdge* Get() const;

	bool WORLDBLDRUNTIME_API operator==(const FWorldBLDKitElementEdge& RHS) const;
	bool WORLDBLDRUNTIME_API operator==(const FWorldBLDKitElementEdgeRef& RHS) const;
};

USTRUCT(BlueprintType)
struct FWorldBLDKitElementPoint
{
	GENERATED_BODY()

	// Which actor owns this point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement")
	AActor* Owner {nullptr};
	
	// The unique identifier of this point in Owner.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement")
	FName Id {NAME_None};

	// Tags for this point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement", Meta=(Categories="WorldBLD.Detail"))
	FGameplayTagContainer Tags;

	// The location of this point in the local space of Owner.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement")
	FVector Location {FVector::ZeroVector};

	// The magnitude of the tangent at this point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement")
	float TangentMagnitude {0.0f};

	// The transform that orients this point in the Owner's local space. Apply the owner's transform to get the
	// world transform of this point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement")
	FTransform LocalTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLDKitElement")
	FVector2D Dimensions { FVector2D::ZeroVector };

	bool WORLDBLDRUNTIME_API operator==(const FWorldBLDKitElementPoint& RHS) const;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

UINTERFACE(MinimalAPI, BlueprintType)
class UWorldBLDKitElementInterface : public UInterface { GENERATED_BODY() };

// Abstracted version of a entity that is placeable from a WorldBLD Kit.
class WORLDBLDRUNTIME_API IWorldBLDKitElementInterface
{
	GENERATED_BODY()

public:

	// Gets the properties of this element.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, CallInEditor, Category="WorldBLDKitElement")
	FWorldBLDKitElementProperties WorldBLDKitElement_Properties() const;

	// Called when the underlying data of this element has changed and we need to rebuild or recalculate data.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, CallInEditor, Category="WorldBLDKitElement")
	void WorldBLDKitElement_UpdateRebuild(bool bRerunConstructionScripts);

	// Destroys this element's associated actors and detaches from other associated actors. This does
	// not destroy this actor (you must do that yourself).
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, CallInEditor, Category="WorldBLDKitElement")
	void WorldBLDKitElement_Demolish();

	// Returns the logical owning parent actor to this element.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, CallInEditor, Category="WorldBLDKitElement")
	AActor* WorldBLDKitElement_ParentActor() const;

	// Returns the child-spawned actors of this element.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, CallInEditor, Category="WorldBLDKitElement")
	TArray<AActor*> WorldBLDKitElement_Children(bool bIncludeSubChildren = false) const;

	// Grabs all of the edges of the element.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, CallInEditor, Category="WorldBLDKitElement")
	void WorldBLDKitElement_RetrieveEdges(TArray<FWorldBLDKitElementEdge>& OutEdges) const;

	// Grabs all of the points of the element.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, CallInEditor, Category="WorldBLDKitElement")
	void WorldBLDKitElement_RetrievePoints(TArray<FWorldBLDKitElementPoint>& OutPoints) const;

protected:
	virtual FWorldBLDKitElementProperties WorldBLDKitElement_Properties_Implementation() const {return FWorldBLDKitElementProperties();}
	virtual void WorldBLDKitElement_UpdateRebuild_Implementation(bool bRerunConstructionScripts) {}
	virtual void WorldBLDKitElement_Demolish_Implementation() {}
	virtual AActor* WorldBLDKitElement_ParentActor_Implementation() const {return nullptr;}
	virtual TArray<AActor*> WorldBLDKitElement_Children_Implementation(bool bIncludeSubChildren) const {return TArray<AActor*>();}
	virtual void WorldBLDKitElement_RetrieveEdges_Implementation(TArray<FWorldBLDKitElementEdge>& OutEdges) const {OutEdges.Empty();}
	virtual void WorldBLDKitElement_RetrievePoints_Implementation(TArray<FWorldBLDKitElementPoint>& OutPoints) const {OutPoints.Empty();}
};

///////////////////////////////////////////////////////////////////////////////////////////////////

