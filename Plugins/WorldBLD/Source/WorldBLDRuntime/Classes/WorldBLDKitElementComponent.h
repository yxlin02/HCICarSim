// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "IWorldBLDKitElementInterface.h"
#include "WorldBLDKitBase.h"
#include "WorldBLDKitElementComponent.generated.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWorldBLDKitElementEvent, class UWorldBLDKitElementComponent*, Element);

// Component that maintains references between various elements present in a CityKit.
UCLASS(BlueprintType, Blueprintable, ClassGroup = (WorldBLD), meta = (BlueprintSpawnableComponent))
class WORLDBLDRUNTIME_API UWorldBLDKitElementComponent : public UActorComponent, public IWorldBLDKitElementInterface
{
	GENERATED_BODY()

public:
	///////////////////////////////////////////////////////////////////////////////////////////////

	// Whether or not to use the properties on the parent actor (it implements IWorldBLDKitElementInterface) or to use the properties
	// directly from this component (default).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WorldBLDKitElement")
	bool bUseOwningActorTraits {false};

	// The properties of this element. Used for filtering and comparing elements.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement", Meta=(EditCondition="!bUseOwningActorTraits"))
	FWorldBLDKitElementProperties ElementProperties;

	// The parent actor to this element.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement", Meta=(EditCondition="!bUseOwningActorTraits"))
	AActor* ElementParent {nullptr};

	// The child actors of this element.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement", Meta=(EditCondition="!bUseOwningActorTraits"))
	TArray<AActor*> ElementChildren;

	// The edges of this element.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement", Meta=(EditCondition="!bUseOwningActorTraits"))
	TArray<FWorldBLDKitElementEdge> ElementEdges;

	// The points of this element.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement", Meta=(EditCondition="!bUseOwningActorTraits"))
	TArray<FWorldBLDKitElementPoint> ElementPoints;

	// Which styles are allowed to be assigned to this element.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLDKitElement", Meta=(Categories="WorldBLD.Style"))
	FGameplayTagContainer AllowedElementStyles;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WorldBLDKitElement")
	bool bAutoAssignOwnerToFolder {true};

	///////////////////////////////////////////////////////////////////////////////////////////////

	// Called when WorldBLDKitElement_UpdateRebuild() is triggered.
	UPROPERTY(BlueprintAssignable)
	FWorldBLDKitElementEvent OnUpdateRebuild;

	// Called when WorldBLDKitElement_Demolish() is triggered.
	UPROPERTY(BlueprintAssignable)
	FWorldBLDKitElementEvent OnDemolished;

	// Called when SetAssignedStyle() actually changes the style.
	UPROPERTY(BlueprintAssignable)
	FWorldBLDKitElementEvent OnStyleAssigned;

	///////////////////////////////////////////////////////////////////////////////////////////////

	// Allocates a new element edge.
	UFUNCTION(BlueprintCallable, Category="WorldBLDKitElement")
	FWorldBLDKitElementEdge& AllocateElementEdge(FName OptionalId = NAME_None);

	UFUNCTION(BlueprintCallable, Category="WorldBLDKitElement")
	int32 AllocateElementEdgeIndexed(FName OptionalId = NAME_None);

	// Allocates an edge along a swept polyline.
	UFUNCTION(BlueprintCallable, Category="WorldBLDKitElement")
	FWorldBLDKitElementEdge& AllocateElementEdgeAlongExtrudedPolyline(FName OptionalId, UPARAM(meta=(Categories="WorldBLD.Detail.Edge")) FGameplayTagContainer Tags, 
			const TArray<FTransform>& LocalSweepPath, UCurveFloat* SweepShape, float Scale, bool bAlongMax, int32& OutIndex);

	// Allocates a new element point.
	UFUNCTION(BlueprintCallable, Category="WorldBLDKitElement")
	FWorldBLDKitElementPoint& AllocateElementPoint(FName OptionalId = NAME_None);

	// Allocates a new element point returns its index.
	UFUNCTION(BlueprintCallable, Category="WorldBLDKitElement")
	int32 AllocateElementPointIndexed(FName OptionalId = NAME_None);

	///////////////////////////////////////////////////////////////////////////////////////////////

	// Goes through the immediate children and verifies that they have their parent set to the owning actor.
	UFUNCTION(BlueprintCallable, Category="WorldBLDKitElement")
	void ValidateChildAssociations(TArray<AActor*>& OutInvalidChildren);

	UFUNCTION(CallInEditor, Category="WorldBLDKitElement|Util")
	void RerunOwnerConstructionScript();

	///////////////////////////////////////////////////////////////////////////////////////////////

	// Sets the assigned style storing a local copy of it, calling `OnStyleAssigned` when this happens. 
	// Returns false if the style could not be accepted (SEE: AllowedElementStyles). By default all styles
	// are duplicated as sharing a unique id. 
	UFUNCTION(BlueprintCallable, Category="WorldBLDKitElement")
	bool SetAssignedStyle(UWorldBLDKitStyleBase* Style, bool bForce = false);

	// Retrieves the assigned style.
	UFUNCTION(BlueprintPure, Category="WorldBLDKitElement")
	UWorldBLDKitStyleBase* GetAssignedStyle() const;

	///////////////////////////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category="WorldBLDKitElement")
	void WorldBLDKitElement_DemolishRoads();

	UFUNCTION(BlueprintCallable, Category="WorldBLDKitElement")
	void WorldBLDKitElement_DemolishBuildings();

	///////////////////////////////////////////////////////////////////////////////////////////////

	// >> UActorComponent
	virtual void OnRegister() override;
	// << UActorComponent

protected:
	// > IWorldBLDKitElementInterface
	virtual FWorldBLDKitElementProperties WorldBLDKitElement_Properties_Implementation() const override;
	virtual void WorldBLDKitElement_UpdateRebuild_Implementation(bool bRerunConstructionScripts) override;
	virtual void WorldBLDKitElement_Demolish_Implementation() override;
	virtual AActor* WorldBLDKitElement_ParentActor_Implementation() const override;
	virtual TArray<AActor*> WorldBLDKitElement_Children_Implementation(bool bIncludeSubChildren) const override;
	virtual void WorldBLDKitElement_RetrieveEdges_Implementation(TArray<FWorldBLDKitElementEdge>& OutEdges) const override;
	virtual void WorldBLDKitElement_RetrievePoints_Implementation(TArray<FWorldBLDKitElementPoint>& OutPoints) const override;
	// < IWorldBLDKitElementInterface

	UPROPERTY(EditAnywhere, Instanced, Category="WorldBLDKitElement", Meta=(TitleProperty="[{StyleType}] {StyleText}"))
	UWorldBLDKitStyleBase* AssignedStyle;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
