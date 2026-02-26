// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Components/SplineComponent.h"

#if WITH_EDITOR
# include "EditorUndoClient.h"
#endif

#include "WorldBLDKitElementUtils.generated.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

DECLARE_LOG_CATEGORY_EXTERN(LogWorldBLDKitTiming, Log, All);

WORLDBLDRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WorldBLD_Detail_Edge); // an edge-specific detail
WORLDBLDRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WorldBLD_Detail_Edge_Centerline); // this edge is a centerline
WORLDBLDRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WorldBLD_Detail_Edge_Border); // this edge is a border
WORLDBLDRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WorldBLD_Detail_Edge_Border_Left); // this edge is a border
WORLDBLDRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WorldBLD_Detail_Edge_Border_Right); // this edge is a border
WORLDBLDRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WorldBLD_Detail_Point); // a point-specific detail
WORLDBLDRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WorldBLD_Detail_Enter); // this element represents an entrance (things flow in)
WORLDBLDRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WorldBLD_Detail_Exit); // this element represents an exit (things flow out)
WORLDBLDRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WorldBLD_Detail_Junction); // multiple things can connect at this element

static inline FGameplayTagContainer operator|(const FGameplayTag& LHS, const FGameplayTag& RHS)
{
	FGameplayTagContainer Tags(LHS);
	Tags.AddTag(RHS);
	return Tags;
}

static inline FGameplayTagContainer operator|(const FGameplayTagContainer& LHS, const FGameplayTag& RHS)
{
	FGameplayTagContainer Tags(LHS);
	Tags.AddTag(RHS);
	return Tags;
}

static inline FGameplayTagContainer operator|(const FGameplayTag& LHS, const FGameplayTagContainer& RHS)
{
	FGameplayTagContainer Tags(RHS);
	Tags.AddTag(LHS);
	return Tags;
}

static inline FGameplayTagContainer operator|(const FGameplayTagContainer& LHS, const FGameplayTagContainer& RHS)
{
	FGameplayTagContainer Tags(LHS);
	Tags.AppendTags(RHS);
	return Tags;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: Use this to handle the case where we might be built outside of the editor.
#if WITH_EDITOR
class FCityBuildCommonUndoClient : public FEditorUndoClient { };
#else
class FCityBuildCommonUndoClient { };
#endif

USTRUCT()
struct FElementTimingEntry
{
	GENERATED_BODY()

	FString Context;
	UPROPERTY()
	UObject* ContextObject {nullptr};
	double CapturedTime {-1.0f};
	double CapturedDuration {-1.0f};
};

///////////////////////////////////////////////////////////////////////////////////////////////////

UCLASS(Blueprintable, BlueprintType)
class WORLDBLDRUNTIME_API UWorldBLDKitElementSubsystem : public UTickableWorldSubsystem, public FCityBuildCommonUndoClient
{
	GENERATED_BODY()
public:

	///////////////////////////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintPure, CallInEditor, Category = "WorldBLD")
	static UWorldBLDKitElementSubsystem* GetActiveWorldBLDKitElementSubsystem();

	UFUNCTION(BlueprintPure, CallInEditor, Category = "WorldBLD", Meta=(DisplayName="Get WorldBLD Kit Element Subsystem"))
	static UWorldBLDKitElementSubsystem* Get(UObject* WorldContextObject);

	///////////////////////////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "WorldBLD|Utils")
	void GetAllToolIgnorableActors(UPARAM(ref) TArray<AActor*>& Array);

	void CheckAddIgnorableActor(AActor* Actor);
	void CheckRemoveIgnorableActor(AActor* Actor);

	///////////////////////////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "WorldBLD|Utils")
	static void BeginTimingBlock(const FString& Context, UObject* ContextObject);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "WorldBLD|Utils")
	static void ContinueTimingBlock(const FString& NewContext, UObject* ContextObject);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "WorldBLD|Utils")
	static void EndTimingBlock();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "WorldBLD|Utils")
	static void BeginSlowTask(FText TaskTitle, int32 UnitsOfWork);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "WorldBLD|Utils")
	static void UpdateSlowTask(int32 WorkCompleted);

	UFUNCTION(BlueprintPure, CallInEditor, Category = "WorldBLD|Utils")
	static bool UserRequestedCancelSlowTask();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "WorldBLD|Utils")
	static void EndSlowTask();

	///////////////////////////////////////////////////////////////////////////////////////////////

public:
	// > USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// < USubsystem

	// > FTickableGameObject
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickableInEditor() const override {return true;}
	virtual TStatId GetStatId() const override;
	virtual void Tick(float DeltaTime) override;
	// < FTickableGameObject
	
	void OnActorSpawned(AActor* InActor);
	void OnActorDeleted(AActor* InActor);
	
	
	void OnLevelsChanged();

#if WITH_EDITOR
	void OnLevelActorListChanged();

	// > FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// < FEditorUndoClient
#endif

protected:
	// > UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	// < UWorldSubsystem
	
	UPROPERTY(Transient)
	TArray<FElementTimingEntry> TimingStack;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> ToolIgnoredActors;

	bool bPendingFullRebuild {true};
	void RebuildActorLists();

	TArray<TSubclassOf<AActor>> TraceIgnoreList;

	static TSharedPtr<struct FSlowTask> ActiveSlowTask;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

USTRUCT(BlueprintType)
struct FEdgeRefSet
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Edges")
	TArray<FWorldBLDKitElementEdgeRef> Refs;
};

USTRUCT(BlueprintType)
struct FElementFilterSpec
{
	GENERATED_BODY()

	// The element must have all of these tags or it will be filtered.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters", Meta=(Categories="WorldBLD.Detail"))
	FGameplayTagContainer RequiredTags;

	// The element must have at least one of these tags.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters", Meta=(Categories="WorldBLD.Detail"))
	FGameplayTagContainer RequestedTags;

	// If the element has this tag, it is completely ignored.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters", Meta=(Categories="WorldBLD.Detail"))
	FGameplayTagContainer IgnoredTags;

	bool ConditionsMet(const FGameplayTagContainer& Tags) const;
};

USTRUCT(BlueprintType)
struct FAutoExpandEdgeLoopParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters")
	float SphereTraceRadius {100.0f};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Parameters")
	FElementFilterSpec EdgeFilter;
};



///////////////////////////////////////////////////////////////////////////////////////////////////

UCLASS()
class WORLDBLDRUNTIME_API UWorldBLDKitElementUtils : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:

	///////////////////////////////////////////////////////////////////////////////////////////////
	
	// Call within WorldBLDKitElement_Children() to expand your known child array out fully.
	UFUNCTION(BlueprintCallable, Category="WorldBLD|Utils")
	static TArray<AActor*> WorldBLDKitElement_GetSubChildrenHelper(TArray<AActor*> ChildrenToExpand);

	// Returns an array containing Element and all of its children (assuming it is a WorldBLDKitElement).
	UFUNCTION(BlueprintCallable, Category="WorldBLD|Utils")
	static TArray<AActor*> TryGetExpandedWorldBLDKitElementChildren(AActor* Element);

	UFUNCTION(BlueprintCallable, Category="WorldBLD|Utils")
	static FWorldBLDKitElementProperties TryGetWorldBLDKitElementProperties(AActor* Element);

	UFUNCTION(BlueprintCallable, Category="WorldBLD|Utils")
	static UObject* FindWorldBLDKitElementImplementor(UObject* Source);

	// Tries to call Demolish() and then SafeDeleteActor()
	UFUNCTION(BlueprintCallable, Category="WorldBLD|Utils")
	static void WorldBLDKitElement_DemolishAndDestroy(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category="WorldBLD|Utils")
	static void WorldBLDKitElement_DemolishChildren(AActor* Actor);

	// Calls "UpdateRebuild()" followed by trying to re-run the construction script of this actor and its children.
	UFUNCTION(BlueprintCallable, Category="WorldBLD|Utils")
	static void WorldBLDKitElement_UpdateRebuild_Helper(AActor* Actor, bool bRerunConstructionScript = false);

	// Grabs the Edges for this element, if any. If Filter is non-empty, then all edges must contain all of the tags in Filter.
	UFUNCTION(BlueprintCallable, Category="WorldBLD|Utils")
	static void WorldBLDKitElement_GetEdges(AActor* Actor, FElementFilterSpec Filter, TArray<FWorldBLDKitElementEdge>& OutEdges);

	//Returns a point at the center of an edge spline.
	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static FVector FindMidpointOfEdge(const struct FWorldBLDKitElementEdge& Edge);

	// Grabs the Edges for this element, if any. If Filter is non-empty, then all points must contain all of the tags in Filter.
	UFUNCTION(BlueprintCallable, Category="WorldBLD|Utils")
	static void WorldBLDKitElement_GetPoints(AActor* Actor, FElementFilterSpec Filter, TArray<FWorldBLDKitElementPoint>& OutPoints);

	UFUNCTION(BlueprintPure, Category="WorldBLD|Utils")
	static void CreateElementPointTransformFromSplinePoint(class USplineComponent* Spline, int32 SplinePoint, 
			ESplineCoordinateSpace::Type CoordinateSpace, FTransform& Transform, float& TangentMagnitude);

	UFUNCTION(BlueprintPure, Category="WorldBLD|Utils")
	static bool GetElementPointWorldBasis(const FWorldBLDKitElementPoint& Point, float DefaultTangent, 
			FVector& OutLocation, FVector& OutForward, FVector& OutRight, FVector& OutTangent);

	UFUNCTION(BlueprintPure, Category="WorldBLD|Utils")
	static bool GetElementPointWorldTransform(const FWorldBLDKitElementPoint& Point, FTransform& OutTransform);

	UFUNCTION(BlueprintPure, Category="WorldBLD|Utils")
	static bool IsElementPointValid(const FWorldBLDKitElementPoint& Point);

	UFUNCTION(BlueprintCallable, Category="WorldBLD|Utils")
	static void WorldBLDKitElement_GatherPlacedSnapPoints(AActor* Actor, TArray<FWorldBLDKitElementPoint>& OutPoints, bool bEmptyFirst = true);

	// Exposes access to "UObject::MarkPackageDirty()"
	UFUNCTION(BlueprintCallable, Category="WorldBLD|Utils")
	static void MarkObjectPackageDirty(UObject* Object);

	///////////////////////////////////////////////////////////////////////////////////////////////

	// Spawns the actor giving it editor-only traits as appropriate.
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils", meta = (DeterminesOutputType = "ActorClass", KeyWords = "Transient"))
	static AActor* SpawnActorFromClassInMainWorld(TSubclassOf<class AActor> ActorClass, FVector Location, FRotator Rotation = FRotator::ZeroRotator, 
			bool bTransient = false, bool bTemporaryEditorActor = false, bool bHideFromSceneOutliner = false, bool bIncludeInTransactionBuffer = true);

	// Runs construction scripts for the provided actors if we are in the Editor.
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void TryForceRunConstructionScipts(TArray<AActor*> Actors);

	// Runs construction script for the provided actor if we are in the Editor.
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void RunConstructionScipt(AActor* Actor);

	// Runs construction scripts for the provided actors if we are in the Editor. (Alias for TryForceRunConstructionScripts())
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void RunConstructionScipts(TArray<AActor*> Actors);

	// Tries to delete Actor (that is the closest thing to hitting the "delete" key depending on your context).
	// This should fix issues where the undo/redo buffer doesn't seem to work correctly.
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void SafeDeleteActor(AActor* Actor);

	// [Editor Only!]
	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils|Editor")
	static bool UserIsDraggingGizmoInViewport();

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static int32 GetBlueprintMaximumLoopIterationCount();

	// Retrieves the mutable config of the provided class.
	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils", meta=(BlueprintAutocast, DeterminesOutputType="ConfigClass"))
	static UObject* GetMutableClassConfig(UClass* ConfigClass);

	// Saves the object as the config. This must be the CDO for the class or it will do nothing.
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void SaveConfigObject(UObject* ConfigObject, bool bClearDirtyFlag = true);

	///////////////////////////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static FWorldBLDKitElementEdgeRef MakeEdgeReference(const struct FWorldBLDKitElementEdge& InEdge);

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static void ResolveEdgeReference(FWorldBLDKitElementEdgeRef Reference, bool& bValid, struct FWorldBLDKitElementEdge& OutEdge);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void AddPointToElementEdge(UPARAM(Ref) struct FWorldBLDKitElementEdge& InEdge, const FVector& Location, const FQuat& Rotation, const FVector& Scale = FVector(1), bool bLinear = true);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void AssignElementEdgeToSpline(const struct FWorldBLDKitElementEdge& Edge, class USplineComponent* Spline);
	
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static FVector GetLocationAtInputKeyAlongElementEdge(const struct FWorldBLDKitElementEdge& Edge, double IKValue);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static TArray<FVector> SampleElementEdge(const struct FWorldBLDKitElementEdge& Edge, int32 SampleCount,  int32 SamplePrecision = 100);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static TArray<FTransform> SampleElementEdgeToTransforms(const struct FWorldBLDKitElementEdge& Edge, int32 SampleCount,  int32 SamplePrecision = 100);

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static bool FindMatchingEdge(const TArray<struct FWorldBLDKitElementEdge>& Edges, AActor* Actor, FName Id, struct FWorldBLDKitElementEdge& OutEdge);

	

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static bool FindMatchingEdgeFromReference(const TArray<struct FWorldBLDKitElementEdge>& Edges, FWorldBLDKitElementEdgeRef Reference, 
			struct FWorldBLDKitElementEdge& OutEdge);

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static bool FindMatchingEdgeReference(const TArray<struct FWorldBLDKitElementEdgeRef>& References, FWorldBLDKitElementEdgeRef Reference, int32& OutIndex);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils", meta = (CallableWithoutWorldContext, WorldContext="WorldContext"))
	static void AutoExpandEdgeLoop(const UObject* WorldContext, FAutoExpandEdgeLoopParams Params, const TArray<FWorldBLDKitElementEdge>& InEdges, 
			TArray<FWorldBLDKitElementEdge>& OutFoundEdges);

	static void CalculateEdgeAlongPolylinePath(const TArray<FTransform>& LocalSweepPath, UCurveFloat* SweepShape, float Scale, bool bAlongMax, 
			FWorldBLDKitElementEdge& OutEdge, bool bFlatten = false, const struct FCleanupSplineParams* OverrideCleanupParams = nullptr,
			const struct FSelfIntersectionParams* OverrideSelfIntersectionParams = nullptr, bool bIsClosedLoop = false);


	static TArray<FTransform> ConvertVectorsToTransforms(const TArray<FVector>& Locations);

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static bool FindMatchingPoint(const TArray<struct FWorldBLDKitElementPoint>& Points, AActor* Actor, FName Id, struct FWorldBLDKitElementPoint& OutPoint);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void AssignActorsToCityFolderPath(TArray<AActor*> Actors);

	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utils")
	static FVector FindElementEdgeClosestPoint(const FVector& WorldLocation, const FWorldBLDKitElementEdge& Edge, float& DistanceSq, float& InputKey);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void FilterPoints(TArray<FWorldBLDKitElementPoint>& Points, FElementFilterSpec Filter);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void FilterEdges(TArray<FWorldBLDKitElementEdge>& Edges, FElementFilterSpec Filter);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static void FilterEdgesByDistanceToLocation(
		const FVector& Location, 
		UPARAM(ref)TArray<FWorldBLDKitElementEdge>& Edges, 
		float MinDistance, 
		TArray<FVector>& OutPoints,
		TArray<float>& OutDistances,
		bool bSort=true);
	
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utils")
	static int32 FindClosestEdge(
		const FVector& Location,
		UPARAM(ref)TArray<FWorldBLDKitElementEdge>& Edges,
		FVector& EdgePoint,
		float& IK);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
