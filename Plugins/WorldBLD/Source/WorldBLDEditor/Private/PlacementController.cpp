// Copyright WorldBLD. All rights reserved.

#include "PlacementController.h"
#include "WorldBLDKitElementUtils.h"
#include "Editor.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/MeshComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Volume.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"

UPlacementController* UPlacementController::CreatePlacementController(TSubclassOf<AWorldBLDPrefab> InPrefabClass, bool bInExitToolOnPlacement)
{
	UPlacementController* Controller = NewObject<UPlacementController>();
	Controller->PrefabClass = InPrefabClass;
	Controller->bExitToolOnPlacement = bInExitToolOnPlacement;
	return Controller;
}

void UPlacementController::SetPrefabClass(TSubclassOf<AWorldBLDPrefab> InPrefabClass)
{
	if (PrefabClass == InPrefabClass)
	{
		return;
	}

	PrefabClass = InPrefabClass;
	RefreshPreviewActorForPrefabClass();
}

#if WITH_EDITOR
void UPlacementController::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UPlacementController, PrefabClass))
	{
		RefreshPreviewActorForPrefabClass();
	}
}
#endif

void UPlacementController::ToolBegin_Implementation()
{
	Super::ToolBegin_Implementation();
	
	// Ensure that the spawned actors never get selected.
	// This prevents issues where placed actors appear selected after exiting the tool.
	bActiveBrushIsForceSelected = false;
	
	PlacementZOffset = 0.0f;
	LastPlacementRotation = FRotator::ZeroRotator;
	ResetPreviewValidityCache();
	ClearPreviewOriginalMaterials();
	SpawnPreviewActor();
}

void UPlacementController::ToolExit_Implementation(bool bForceEnd)
{
	// If we still have a preview actor, restore original materials (purely defensive; the actor is usually destroyed).
	RestoreOriginalMaterialsForCachedComponents();
	ClearPreviewOriginalMaterials();
	ResetPreviewValidityCache();

	// If we have an active brush actor, destroy it before exiting
	// The base class handles this, but we want to make sure we don't leave a preview actor behind
	// if we are just exiting without placing.
	// However, if we placed it (ActiveBrushActor set to null), it won't be destroyed.
	
	Super::ToolExit_Implementation(bForceEnd);
}

bool UPlacementController::ToolMouseMove_Implementation(int32 X, int32 Y)
{
	if (IsValid(ActiveBrushActor))
	{
		FVector HitLocation = GetMouseHitPoint(X, Y);
		if (HitLocation != FVector::ZeroVector)
		{
			ActiveBrushActor->SetActorLocation(HitLocation + FVector(0.0f, 0.0f, PlacementZOffset));
		}
		return true;
	}
	return false;
}

void UPlacementController::ToolRender_Implementation(FPrimitiveDrawWrapper Renderer)
{
	Super::ToolRender_Implementation(Renderer);

#if WITH_EDITOR
	UpdatePerFramePreviewValidity();
#endif
}

bool UPlacementController::ConsumesMouseClick_Implementation(int32 Which, bool& bIsPassive) const
{
	// EdMode only forwards mouse clicks if the active controller explicitly opts in.
	// For placement, we want to handle LMB presses for placing the active brush.
	bIsPassive = false;
	return (Which == 0); // LMB = 0
}

void UPlacementController::ToolMouseClick_Implementation(int32 WhichButton, bool bPressed)
{
	if (WhichButton == 0 && bPressed) // Left Click
	{
		if (IsValid(ActiveBrushActor))
		{
			// Restore any original materials before "placing" so we don't permanently tint the placed actor.
			RestoreOriginalMaterialsForCachedComponents();
			ClearPreviewOriginalMaterials();
			ResetPreviewValidityCache();

			// Store the rotation before we detach/place the actor
			LastPlacementRotation = ActiveBrushActor->GetActorRotation();

			// "Place" the actor by detaching it from the controller
			ActiveBrushActor = nullptr;

			if (bExitToolOnPlacement)
			{
				TryExitTool(false);
			}
			else
			{
				SpawnPreviewActor();
				
				// Immediately update position of new actor to avoid frame lag
				// We need the current mouse position, but we don't have it here easily without passing it or storing it.
				// However, the next Tick/MouseMove will update it.
				// To be precise, we could cache the last hit point.
				// For now, let's just let the next move update it.
			}
		}
	}
}

void UPlacementController::ToolMouseScroll_Implementation(float ScrollDelta, bool bControl, bool bAlt, bool bShift)
{
	if (IsValid(ActiveBrushActor))
	{
		if (bControl)
		{
			const float ZIncrement = bAlt ? 1.0f : 10.0f;
			PlacementZOffset += ScrollDelta * ZIncrement;
			
			FVector NewLocation = ActiveBrushActor->GetActorLocation();
			NewLocation.Z += ScrollDelta * ZIncrement;
			ActiveBrushActor->SetActorLocation(NewLocation);
		}
		else
		{
			const float RotationIncrement = bAlt ? 1.0f : 10.0f;
			FRotator NewRotation = ActiveBrushActor->GetActorRotation();
			NewRotation.Yaw += ScrollDelta * RotationIncrement;
			ActiveBrushActor->SetActorRotation(NewRotation);
		}
	}
}

void UPlacementController::SpawnPreviewActor()
{
	if (!PrefabClass)
	{
		return;
	}

	// Prefer widget world, fall back to editor world (widgets may not have a world yet)
	UWorld* World = GetWorld();
	if (!World && GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	// Use LastPlacementRotation instead of ZeroRotator
	ActiveBrushActor = World->SpawnActor<AActor>(PrefabClass, FVector::ZeroVector, LastPlacementRotation, SpawnParams);
	
	if (IsValid(ActiveBrushActor))
	{
		ResetPreviewValidityCache();
		ClearPreviewOriginalMaterials();

		// Explicitly set the rotation to ensure it's applied, in case the actor's construction script overrides it
		ActiveBrushActor->SetActorRotation(LastPlacementRotation);

		// Ensure it's selected so we can see it properly if needed, or at least tracked
		ForceEditorSelectionToActiveBrushActor();
	}
}

void UPlacementController::AddIgnoredActorsForMouseTrace(FCollisionQueryParams& TraceParams) const
{
	Super::AddIgnoredActorsForMouseTrace(TraceParams);

	if (!IsValid(ActiveBrushActor))
	{
		return;
	}

	// Also ignore any attached/child actors (e.g. ChildActorComponent spawned actors) so the mouse trace hits the world.
	TArray<AActor*> Stack;
	Stack.Add(ActiveBrushActor);

	while (Stack.Num() > 0)
	{
		AActor* Current = Stack.Pop(EAllowShrinking::No);
		if (!IsValid(Current))
		{
			continue;
		}

		TraceParams.AddIgnoredActor(Current);

		TArray<AActor*> Attached;
		Current->GetAttachedActors(Attached);
		for (AActor* Child : Attached)
		{
			if (IsValid(Child))
			{
				Stack.Add(Child);
			}
		}
	}
}

void UPlacementController::RefreshPreviewActorForPrefabClass()
{
	// Only meaningful while the tool is active/has begun. Otherwise, ToolBegin will spawn correctly.
	if (!bHasBegun || !bIsActive)
	{
		return;
	}

	const bool bHadBrushActor = IsValid(ActiveBrushActor);
	const FVector OldLocation = bHadBrushActor ? ActiveBrushActor->GetActorLocation() : FVector::ZeroVector;
	const FRotator OldRotation = bHadBrushActor ? ActiveBrushActor->GetActorRotation() : LastPlacementRotation;

	// Always destroy existing preview actor, even if PrefabClass becomes null.
	if (bHadBrushActor)
	{
		RestoreOriginalMaterialsForCachedComponents();
		ClearPreviewOriginalMaterials();
		ResetPreviewValidityCache();

		UWorldBLDKitElementUtils::WorldBLDKitElement_DemolishAndDestroy(ActiveBrushActor);
		ActiveBrushActor = nullptr;
	}

	if (!PrefabClass)
	{
		return;
	}

	LastPlacementRotation = OldRotation;
	SpawnPreviewActor();

	if (IsValid(ActiveBrushActor))
	{
		ActiveBrushActor->SetActorLocation(OldLocation);
		ActiveBrushActor->SetActorRotation(OldRotation);
	}
}

#if WITH_EDITOR
void UPlacementController::UpdatePerFramePreviewValidity()
{
	if (!bIsActive || !IsValid(ActiveBrushActor))
	{
		return;
	}

	// If neither material is configured, don't do any work.
	if (!ValidMaterial && !InvalidMaterial)
	{
		return;
	}

	const bool bHasOverlap = PreviewHasBlockingOverlap();

	if (bHasCachedOverlapState && (bCachedHadOverlap == bHasOverlap))
	{
		return;
	}

	bHasCachedOverlapState = true;
	bCachedHadOverlap = bHasOverlap;

	UMaterialInterface* TargetMaterial = bHasOverlap ? InvalidMaterial.Get() : ValidMaterial.Get();
	if (TargetMaterial)
	{
		ApplyOverrideMaterialToActorHierarchy(ActiveBrushActor, TargetMaterial);
	}
}

void UPlacementController::ResetPreviewValidityCache()
{
	bHasCachedOverlapState = false;
	bCachedHadOverlap = false;
}

void UPlacementController::ClearPreviewOriginalMaterials()
{
	PreviewOriginalMaterials.Empty();
}

void UPlacementController::GetActorHierarchy(AActor* RootActor, TArray<AActor*>& OutActors)
{
	OutActors.Reset();
	if (!IsValid(RootActor))
	{
		return;
	}

	TArray<AActor*> Stack;
	Stack.Add(RootActor);

	while (Stack.Num() > 0)
	{
		AActor* Current = Stack.Pop(EAllowShrinking::No);
		if (!IsValid(Current) || OutActors.Contains(Current))
		{
			continue;
		}

		OutActors.Add(Current);

		TArray<AActor*> Attached;
		Current->GetAttachedActors(Attached);
		for (AActor* Child : Attached)
		{
			if (IsValid(Child))
			{
				Stack.Add(Child);
			}
		}
	}
}

void UPlacementController::CacheOriginalMaterialsForActorHierarchy(AActor* RootActor)
{
	TArray<AActor*> Actors;
	GetActorHierarchy(RootActor, Actors);

	for (AActor* Actor : Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		TInlineComponentArray<UMeshComponent*> MeshComponents;
		Actor->GetComponents(MeshComponents);

		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			if (!IsValid(MeshComponent))
			{
				continue;
			}

			TWeakObjectPtr<UMeshComponent> Key(MeshComponent);
			if (PreviewOriginalMaterials.Contains(Key))
			{
				continue;
			}

			const int32 NumMaterials = MeshComponent->GetNumMaterials();
			TArray<TObjectPtr<UMaterialInterface>> Materials;
			Materials.Reserve(NumMaterials);
			for (int32 MaterialIdx = 0; MaterialIdx < NumMaterials; ++MaterialIdx)
			{
				Materials.Add(MeshComponent->GetMaterial(MaterialIdx));
			}

			PreviewOriginalMaterials.Add(Key, MoveTemp(Materials));
		}
	}
}

void UPlacementController::ApplyOverrideMaterialToActorHierarchy(AActor* RootActor, UMaterialInterface* OverrideMaterial)
{
	if (!IsValid(RootActor) || !OverrideMaterial)
	{
		return;
	}

	CacheOriginalMaterialsForActorHierarchy(RootActor);

	TArray<AActor*> Actors;
	GetActorHierarchy(RootActor, Actors);

	for (AActor* Actor : Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		TInlineComponentArray<UMeshComponent*> MeshComponents;
		Actor->GetComponents(MeshComponents);

		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			if (!IsValid(MeshComponent))
			{
				continue;
			}

			const int32 NumMaterials = MeshComponent->GetNumMaterials();
			for (int32 MaterialIdx = 0; MaterialIdx < NumMaterials; ++MaterialIdx)
			{
				MeshComponent->SetMaterial(MaterialIdx, OverrideMaterial);
			}
		}
	}
}

void UPlacementController::RestoreOriginalMaterialsForCachedComponents()
{
	for (auto It = PreviewOriginalMaterials.CreateIterator(); It; ++It)
	{
		UMeshComponent* MeshComponent = It.Key().Get();
		if (!IsValid(MeshComponent))
		{
			continue;
		}

		const TArray<TObjectPtr<UMaterialInterface>>& OriginalMaterials = It.Value();
		for (int32 MaterialIdx = 0; MaterialIdx < OriginalMaterials.Num(); ++MaterialIdx)
		{
			MeshComponent->SetMaterial(MaterialIdx, OriginalMaterials[MaterialIdx]);
		}
	}
}

bool UPlacementController::ShouldIgnoreOverlapActor(const AActor* OtherActor)
{
	if (!IsValid(OtherActor))
	{
		return true;
	}

	// Ignore landscape and any volumes (including e.g. blocking volumes, post process volumes, etc.)
	if (OtherActor->IsA<ALandscapeProxy>() || OtherActor->IsA<AVolume>())
	{
		return true;
	}

	return false;
}

bool UPlacementController::PreviewHasBlockingOverlap() const
{
	if (!IsValid(ActiveBrushActor))
	{
		return false;
	}

	UWorld* World = ActiveBrushActor->GetWorld();
	if (!World)
	{
		return false;
	}

	TArray<AActor*> BrushActors;
	GetActorHierarchy(ActiveBrushActor, BrushActors);

	// Build combined bounds for the whole brush hierarchy.
	FBox Bounds(ForceInit);
	for (AActor* Actor : BrushActors)
	{
		if (IsValid(Actor))
		{
			Bounds += Actor->GetComponentsBoundingBox(/* bNonColliding */ true);
		}
	}

	if (!Bounds.IsValid)
	{
		return false;
	}

	const FVector Center = Bounds.GetCenter();
	const FVector Extent = Bounds.GetExtent();

	if (Extent.IsNearlyZero())
	{
		return false;
	}

	// Lift the bottom face of the overlap bounds slightly so resting on the ground plane doesn't always count as overlap.
	// We do this by raising the center by HalfClearance and reducing the Z extent by HalfClearance (preserves the top face).
	const float BottomClearance = FMath::Max(0.0f, OverlapCheckBottomClearance);
	const float HalfClearance = BottomClearance * 0.5f;
	FVector OverlapCenter = Center;
	FVector OverlapExtent = Extent;
	if (HalfClearance > 0.0f)
	{
		OverlapCenter.Z += HalfClearance;
		OverlapExtent.Z = FMath::Max(0.0f, OverlapExtent.Z - HalfClearance);
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WorldBLD_PlacementPreviewOverlap), /* bTraceComplex */ false);
	QueryParams.bReturnPhysicalMaterial = false;
	for (AActor* Actor : BrushActors)
	{
		if (IsValid(Actor))
		{
			QueryParams.AddIgnoredActor(Actor);
		}
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	const bool bAnyOverlap = World->OverlapMultiByObjectType(
		Overlaps,
		OverlapCenter,
		FQuat::Identity,
		ObjectParams,
		FCollisionShape::MakeBox(OverlapExtent),
		QueryParams
	);

	if (!bAnyOverlap)
	{
		return false;
	}

	for (const FOverlapResult& Result : Overlaps)
	{
		const AActor* OtherActor = Result.GetActor();
		if (!IsValid(OtherActor))
		{
			continue;
		}

		if (ShouldIgnoreOverlapActor(OtherActor))
		{
			continue;
		}

		// Anything else counts as an overlap for visual feedback.
		return true;
	}

	return false;
}
#endif
