// Copyright WorldBLD LLC. All rights reserved.

#include "MaterialStackComponent.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

UMaterialStackComponent::UMaterialStackComponent()
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReplaced.AddUObject(this, &UMaterialStackComponent::OnObjectsReplaced);
#endif
}

void UMaterialStackComponent::PushNewMaterials(
	const FString& Key, 
	const TArray<UMaterialInterface*>& Materials, 
	bool bCustomRenderDepth)
{
	for (FMaterialSnapshotSet& SnapshotSet : SnapshotSets)
	{
		TArray<UPrimitiveComponent*> Primitives = SnapshotSet.FindPrimitiveComponents(GetOwner());
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			SnapshotSet.EnsureRootSnapshot(StackRootKey, Primitive);
			SnapshotSet.StashMaterialsFromList(Key, Primitive, Materials, bCustomRenderDepth);
		}
	}
}

void UMaterialStackComponent::PushNewMaterial(
	const FString& Key, 
	UMaterialInterface* Material, 
	bool bCustomRenderDepth)
{
	for (FMaterialSnapshotSet& SnapshotSet : SnapshotSets)
	{
		TArray<UPrimitiveComponent*> Primitives = SnapshotSet.FindPrimitiveComponents(GetOwner());
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			SnapshotSet.EnsureRootSnapshot(StackRootKey, Primitive);
			TArray<UMaterialInterface*> Materials;
			for (int32 Idx = 0; Idx < Primitive->GetNumMaterials(); Idx += 1)
			{
				Materials.Add(Material);
			}
			SnapshotSet.StashMaterialsFromList(Key, Primitive, Materials, bCustomRenderDepth);
		}
	}
}

void UMaterialStackComponent::PopMaterials(
	const FString& Key)
{
	for (FMaterialSnapshotSet& SnapshotSet : SnapshotSets)
	{
		TArray<UPrimitiveComponent*> Primitives = SnapshotSet.FindPrimitiveComponents(GetOwner());
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			SnapshotSet.EnsureRootSnapshot(StackRootKey, Primitive);
			SnapshotSet.PopKeyAndApplyMaterialsToPrimitive(Key, Primitive);
		}
	}
}

void UMaterialStackComponent::RefreshRootMaterials(bool bResetStacks)
{
	for (FMaterialSnapshotSet& SnapshotSet : SnapshotSets)
	{
		if (bResetStacks)
		{
			SnapshotSet.SnapshotMap.Empty();
		}

		TArray<UPrimitiveComponent*> Primitives = SnapshotSet.FindPrimitiveComponents(GetOwner());
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			SnapshotSet.EnsureRootSnapshot(StackRootKey, Primitive, /* force */ true);
		}
	}
}

#if WITH_EDITOR
void UMaterialStackComponent::OnObjectsReplaced(const TMap<UObject*, UObject*>& ObjectReplacementMap)
{
	// As we move around this component, keep track of existing material applications by transferring
	// the values to the newly recreated version of this component.
	if (auto OldComponentRef = ObjectReplacementMap.FindKey(this))
	{
		UMaterialStackComponent* OldComponent = Cast<UMaterialStackComponent>(*OldComponentRef);
		if (UMaterialStackComponent* NewComponent = Cast<UMaterialStackComponent>(ObjectReplacementMap[*OldComponentRef]))
		{
			for (int32 SIdx = 0; SIdx < OldComponent->SnapshotSets.Num(); SIdx += 1)
			{
				const FMaterialSnapshotSet& OldSet = OldComponent->SnapshotSets[SIdx];
				FMaterialSnapshotSet& NewSet = NewComponent->SnapshotSets[SIdx];
				for (const auto& SnapshotKVP : OldSet.SnapshotMap)
				{
					if (auto NewRef = ObjectReplacementMap.Find(SnapshotKVP.Key))
					{
						NewSet.GetSnapshots(Cast<UPrimitiveComponent>(*NewRef)) = SnapshotKVP.Value.Elements;
					}
				}
			}
		}
	}
}

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

TArray<UPrimitiveComponent*> FMaterialSnapshotSet::FindPrimitiveComponents(AActor* OwningActor) const
{
	TArray<UPrimitiveComponent*> MatchingPrimitives;
	if (IsValid(OwningActor))
	{
		TArray<UActorComponent*> Comps;
		if (!TargetComponentTag.IsNone())
		{
			Comps = OwningActor->GetComponentsByTag(TargetComponentClass, TargetComponentTag);
		}
		else if (IsValid(TargetComponentClass))
		{
			OwningActor->GetComponents(TargetComponentClass, Comps, /* Include from Child Actors? */ true);
		}

		for (UActorComponent* Comp : Comps)
		{
			if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp))
			{
				MatchingPrimitives.Add(Prim);
			}
		}
	}
	return MatchingPrimitives;
}

TArray<FMaterialSnapshot>& FMaterialSnapshotSet::GetSnapshots(UPrimitiveComponent* Comp)
{
	ensure(IsValid(Comp));
	return SnapshotMap.FindOrAdd(Comp).Elements;
}

void FMaterialSnapshotSet::Push(
	const FString& Key,
	UPrimitiveComponent* Comp,
	int32 DesiredSize)
{
	int32 FoundIdx = -1;
	int32 ExistingCount = DesiredSize;
	TArray<FMaterialSnapshot>& Snapshots = GetSnapshots(Comp);

	if (Snapshots.Num() > 0)
	{
		ExistingCount = Snapshots.Last().Materials.Num();
	}
	for (int32 Idx = 0; Idx < Snapshots.Num(); Idx += 1)
	{
		if (Snapshots[Idx].Key == Key)
		{
			FoundIdx = Idx;
			break;
		}
	}

	if (FoundIdx == -1)
	{
		// Add a new entry at the end.
		Snapshots.AddDefaulted();
		Snapshots.Last().Materials.AddDefaulted(ExistingCount);
	}
	else
	{
		// Make the found one the top of the stack.
		FMaterialSnapshot Copy = Snapshots[FoundIdx];
		Snapshots.RemoveAt(FoundIdx);
		Snapshots.Add(Copy);
	}
}

void FMaterialSnapshotSet::Pop(
	const FString& Key,
	UPrimitiveComponent* Comp)
{
	TArray<FMaterialSnapshot>& Snapshots = GetSnapshots(Comp);
	for (int32 Idx = 0; Idx < Snapshots.Num(); Idx += 1)
	{
		if (Snapshots[Idx].Key == Key)
		{
			Snapshots.RemoveAt(Idx);
			break;
		}
	}
}

void FMaterialSnapshotSet::StashMaterialsFromPrimitive(
	const FString& Key, 
	UPrimitiveComponent* Comp, 
	int32 StashIdx)
{
	if (ensure(Comp))
	{
		if (StashIdx == -1)
		{
			Push(Key, Comp, Comp->GetNumMaterials());
		}

		TArray<FMaterialSnapshot>& Snapshots = GetSnapshots(Comp);
		FMaterialSnapshot& Set = (StashIdx == -1) ? Snapshots.Last() : Snapshots[StashIdx];

		// If this is the root, we are allowed to resize.
		if ((StashIdx == 0) && (Comp->GetNumMaterials() != Set.Materials.Num()))
		{
			Set.Materials.Empty();
			Set.Materials.AddDefaulted(Comp->GetNumMaterials());
		}

		Set.Key = Key;
		for (int32 Idx = 0; (Idx < Comp->GetNumMaterials()) && (Idx < Set.Materials.Num()); Idx++)
		{
			Set.Materials[Idx] = Comp->GetMaterial(Idx);
		}
		Set.bCustomRenderDepth = Comp->bRenderCustomDepth;
	}
}

void FMaterialSnapshotSet::StashMaterialsFromList(
	const FString& Key, 
	UPrimitiveComponent* Comp, 
	const TArray<UMaterialInterface*>& Materials, 
	bool bCustomRenderDepth, 
	int32 StashIdx)
{
	if (StashIdx == -1)
	{
		Push(Key, Comp, Materials.Num());
	}

	TArray<FMaterialSnapshot>& Snapshots = GetSnapshots(Comp);
	FMaterialSnapshot& Set = (StashIdx == -1) ? Snapshots.Last() : Snapshots[StashIdx];

	// If this is the root, we are allowed to resize.
	if ((StashIdx == 0) && (Comp->GetNumMaterials() != Set.Materials.Num()))
	{
		Set.Materials.Empty();
		Set.Materials.AddDefaulted(Comp->GetNumMaterials());
	}

	Set.Key = Key;
	for (int32 Idx = 0; (Idx < Materials.Num()) && (Idx < Set.Materials.Num()); Idx += 1)
	{
		Set.Materials[Idx] = Materials[Idx];
	}
	Set.bCustomRenderDepth = bCustomRenderDepth;
	if (Snapshots.Last().Key == Key)
	{
		ApplyMaterialsToPrimitive(Comp, Materials, bCustomRenderDepth);
	}
}

void FMaterialSnapshotSet::StashMaterialsFromInstanceList(
	const FString& Key, 
	UPrimitiveComponent* Comp,
	const TArray<UMaterialInstance*>& Materials, 
	bool bCustomRenderDepth, 
	int32 StashIdx)
{
	TArray<UMaterialInterface*> MaterialCopy;
	for (UMaterialInstance* Copy : Materials)
	{
		MaterialCopy.Add(Copy);
	}
	StashMaterialsFromList(Key, Comp, MaterialCopy, bCustomRenderDepth, StashIdx);
}

void FMaterialSnapshotSet::ApplyMaterialsToPrimitive(
	UPrimitiveComponent* Comp, 
	const TArray<UMaterialInterface*>& Materials, 
	bool bCustomRenderDepth)
{
	for (int32 Idx = 0; (Idx < Comp->GetNumMaterials()) && (Idx < Materials.Num()); Idx++)
	{
		if (Comp->GetMaterial(Idx) != Materials[Idx])
		{
			Comp->SetMaterial(Idx, Materials[Idx]);
			Comp->MarkRenderStateDirty();
		}
	}
	Comp->SetRenderCustomDepth(bCustomRenderDepth);
}

void FMaterialSnapshotSet::PopKeyAndApplyMaterialsToPrimitive(
	const FString& Key, 
	UPrimitiveComponent* Comp)
{
	Pop(Key, Comp);
	if (ensure(Comp) && ensure(GetSnapshots(Comp).Num() > 0))
	{
		const FMaterialSnapshot& Set = GetSnapshots(Comp).Last();
		ApplyMaterialsToPrimitive(Comp, Set.Materials, Set.bCustomRenderDepth);
	}
}

void FMaterialSnapshotSet::EnsureRootSnapshot(const FString& Key, UPrimitiveComponent* Comp, bool bForce)
{
	if (ensure(Comp))
	{
		TArray<FMaterialSnapshot>& Snapshots = GetSnapshots(Comp);
		if ((Snapshots.Num() == 0) || (Snapshots[0].Key != Key) || bForce)
		{
			TArray<UMaterialInterface*> Materials;
			for (int32 Idx = 0; Idx < Comp->GetNumMaterials(); Idx += 1)
			{
				Materials.Add(Comp->GetMaterial(Idx));
			}
			StashMaterialsFromList(Key, Comp, Materials, Comp->bRenderCustomDepth, (Snapshots.Num() > 0) ? 0 : -1);
		}
	}
}



