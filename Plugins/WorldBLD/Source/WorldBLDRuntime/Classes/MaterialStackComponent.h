// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "MaterialStackComponent.generated.h"

// A snapshot of materials, usually for a specific component on an actor.
USTRUCT()
struct FMaterialSnapshot
{
	GENERATED_BODY()

	// Identifying key for this snapshot.
	UPROPERTY()
	FString Key;

	// Whether the materials use custom depth.
	UPROPERTY()
	bool bCustomRenderDepth {false};

	// The collection of materials.
	UPROPERTY()
	TArray<UMaterialInterface*> Materials;
};

USTRUCT()
struct FMaterialSnapshotArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMaterialSnapshot> Elements;
};

// A collection of snapshots organized in a stack.
USTRUCT(BlueprintType)
struct FMaterialSnapshotSet
{
	GENERATED_BODY()

	///////////////////////////////////////////////////////////////////////////////////////////////

	// The tag of the component we should snapshot materials from. If NAME_None, looks for a component
	// of class `TargetComponentClass` instead.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Properties")
	FName TargetComponentTag {NAME_None};

	// The class of the component we should snapshot materials from.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Properties")
	TSubclassOf<UPrimitiveComponent> TargetComponentClass;

	///////////////////////////////////////////////////////////////////////////////////////////////

	UPROPERTY(NonTransactional)
	TMap<UPrimitiveComponent*, FMaterialSnapshotArray> SnapshotMap;

	///////////////////////////////////////////////////////////////////////////////////////////////

	void Push(const FString& Key, UPrimitiveComponent* Comp, int32 DesiredSize = 0);
	void Pop(const FString& Key, UPrimitiveComponent* Comp);

	TArray<UPrimitiveComponent*> FindPrimitiveComponents(AActor* OwningActor) const;
	TArray<FMaterialSnapshot>& GetSnapshots(UPrimitiveComponent* Comp);

	void StashMaterialsFromPrimitive(const FString& Key, UPrimitiveComponent* Comp, int32 StashIdx = -1);
	void StashMaterialsFromList(const FString& Key, UPrimitiveComponent* Comp, const TArray<UMaterialInterface*>& Materials, bool bCustomRenderDepth, int32 StashIdx = -1);
	void StashMaterialsFromInstanceList(const FString& Key, UPrimitiveComponent* Comp, const TArray<UMaterialInstance*>& Materials, bool bCustomRenderDepth, int32 StashIdx = -1);
	void ApplyMaterialsToPrimitive(UPrimitiveComponent* Comp, const TArray<UMaterialInterface*>& Materials, bool bCustomRenderDepth);
	void PopKeyAndApplyMaterialsToPrimitive(const FString& Key, UPrimitiveComponent* Comp);
	void EnsureRootSnapshot(const FString& Key, UPrimitiveComponent* Comp, bool bForce = false);
};

// Component that manages applying materials to various primitives on the owning actor in a stack. 
// The idea is you can temporarily assign materials and then pop them off, restoring old materials.
UCLASS(BlueprintType, Blueprintable, ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class UMaterialStackComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMaterialStackComponent();

	///////////////////////////////////////////////////////////////////////////////////////////////

	// The different managed snapshot sets. Add members in the Editor and specify what primitives
	// they will manage. When a Push/Pop action arrives it will be a snapshot of the data.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TArray<FMaterialSnapshotSet> SnapshotSets;

	// The key used at the root of the stacks.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Materials")
	FString StackRootKey {"__Root"};

	///////////////////////////////////////////////////////////////////////////////////////////////
	
	// Applies (or re-ups) a new set of materials to all the primitives' material stacks by Key.
	UFUNCTION(BlueprintCallable, Category = "Materials")
	void PushNewMaterials(const FString& Key, const TArray<UMaterialInterface*>& Materials, bool bCustomRenderDepth = false);

	// Applies (or re-ups) a new material (expanded to the number of materials of the primitive) to all the primitives' material stacks by Key.
	UFUNCTION(BlueprintCallable, Category = "Materials")
	void PushNewMaterial(const FString& Key, UMaterialInterface* Material, bool bCustomRenderDepth = false);

	// Removes a set of materials from all the primitives' material stacks by Key.
	UFUNCTION(BlueprintCallable, Category = "Materials")
	void PopMaterials(const FString& Key);

	UFUNCTION(BlueprintCallable, Category = "Materials")
	void RefreshRootMaterials(bool bResetStacks = false);

	///////////////////////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& ObjectReplacementMap);
#endif
};



