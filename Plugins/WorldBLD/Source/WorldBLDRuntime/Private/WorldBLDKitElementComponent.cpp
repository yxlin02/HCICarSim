// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDKitElementComponent.h"
#include "WorldBLDKitElementUtils.h"
#include "Kismet/KismetSystemLibrary.h"

void UWorldBLDKitElementComponent::OnRegister()
{
	Super::OnRegister();

	if (bAutoAssignOwnerToFolder)
	{
		UWorldBLDKitElementUtils::AssignActorsToCityFolderPath({GetOwner()});
	}
}

FWorldBLDKitElementProperties UWorldBLDKitElementComponent::WorldBLDKitElement_Properties_Implementation() const
{
	FWorldBLDKitElementProperties Props = ElementProperties;
	if (bUseOwningActorTraits)
	{
		if (const AActor* MyOwner = GetOwner())
		{
			if (UKismetSystemLibrary::DoesImplementInterface(MyOwner, UWorldBLDKitElementInterface::StaticClass()))
			{
				Props = IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_Properties(MyOwner);
			}
		}
	}
	return Props;
}

void UWorldBLDKitElementComponent::WorldBLDKitElement_UpdateRebuild_Implementation(bool bRerunConstructionScripts)
{
	if (bUseOwningActorTraits)
	{
		if (AActor* MyOwner = GetOwner())
		{
			if (UKismetSystemLibrary::DoesImplementInterface(MyOwner, UWorldBLDKitElementInterface::StaticClass()))
			{
				IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_UpdateRebuild(MyOwner, bRerunConstructionScripts);
			}
		}
	}
	else
	{
		OnUpdateRebuild.Broadcast(this);

		if (bRerunConstructionScripts)
		{
			UWorldBLDKitElementUtils::TryForceRunConstructionScipts({GetOwner()});
		}
		
		// Gather all of the children, demolish them, then safely delete them.
		TArray<AActor*> TheChildren = IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_Children(this, false);
		for (AActor* Child : TheChildren)
		{
			if (UObject* Implementor = UWorldBLDKitElementUtils::FindWorldBLDKitElementImplementor(Child))
			{
				IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_UpdateRebuild(Implementor, bRerunConstructionScripts);
			}
		}
	}
}

void UWorldBLDKitElementComponent::WorldBLDKitElement_Demolish_Implementation()
{
	OnDemolished.Broadcast(this);

	if (bUseOwningActorTraits)
	{
		if (AActor* MyOwner = GetOwner())
		{
			if (UKismetSystemLibrary::DoesImplementInterface(MyOwner, UWorldBLDKitElementInterface::StaticClass()))
			{
				IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_Demolish(MyOwner);
			}
		}
	}
	else
	{
		// Gather all of the children, demolish them, then safely delete them.
		TArray<AActor*> TheChildren = IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_Children(this, false);
		for (AActor* Child : TheChildren)
		{
			if (UObject* Implementor = UWorldBLDKitElementUtils::FindWorldBLDKitElementImplementor(Child))
			{
				IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_Demolish(Implementor);
			}

			if (IsValid(Child))
			{
				UWorldBLDKitElementUtils::SafeDeleteActor(Child);
			}
		}
	}
}

void UWorldBLDKitElementComponent::WorldBLDKitElement_DemolishRoads()
{
	TArray<AActor*> Children = UWorldBLDKitElementUtils::TryGetExpandedWorldBLDKitElementChildren(GetOwner());
	Children.Remove(GetOwner());
	for (AActor* Child : Children)
	{
		const FWorldBLDKitElementProperties Properties = UWorldBLDKitElementUtils::TryGetWorldBLDKitElementProperties(Child);
		if (Properties.bValidElement && (Properties.bIsRoad || Properties.bIsRoadProp))
		{
			UWorldBLDKitElementUtils::WorldBLDKitElement_DemolishAndDestroy(Child);
		}
	}
}

void UWorldBLDKitElementComponent::WorldBLDKitElement_DemolishBuildings()
{
	TArray<AActor*> Children = UWorldBLDKitElementUtils::TryGetExpandedWorldBLDKitElementChildren(GetOwner());
	Children.Remove(GetOwner());
	for (AActor* Child : Children)
	{
		const FWorldBLDKitElementProperties Properties = UWorldBLDKitElementUtils::TryGetWorldBLDKitElementProperties(Child);
		if (Properties.bValidElement && (Properties.bIsBuilding))
		{
			UWorldBLDKitElementUtils::WorldBLDKitElement_DemolishAndDestroy(Child);
		}
	}
}

AActor* UWorldBLDKitElementComponent::WorldBLDKitElement_ParentActor_Implementation() const
{
	AActor* Parent = ElementParent;
	if (bUseOwningActorTraits)
	{
		if (const AActor* MyOwner = GetOwner())
		{
			if (UKismetSystemLibrary::DoesImplementInterface(MyOwner, UWorldBLDKitElementInterface::StaticClass()))
			{
				Parent = IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_ParentActor(MyOwner);
			}
		}
	}
	return Parent;
}

TArray<AActor*> UWorldBLDKitElementComponent::WorldBLDKitElement_Children_Implementation(bool bIncludeSubChildren) const
{
	TArray<AActor*> TheChildren;
	if (bUseOwningActorTraits)
	{
		if (const AActor* MyOwner = GetOwner())
		{
			if (UKismetSystemLibrary::DoesImplementInterface(MyOwner, UWorldBLDKitElementInterface::StaticClass()))
			{
				TheChildren = IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_Children(MyOwner, bIncludeSubChildren);
			}
		}
	}
	else
	{
		TheChildren = bIncludeSubChildren? UWorldBLDKitElementUtils::WorldBLDKitElement_GetSubChildrenHelper(ElementChildren) : ElementChildren;
	}
	return TheChildren;
}

void UWorldBLDKitElementComponent::WorldBLDKitElement_RetrieveEdges_Implementation(TArray<FWorldBLDKitElementEdge>& OutEdges) const
{
	if (bUseOwningActorTraits)
	{
		if (const AActor* MyOwner = GetOwner())
		{
			if (UKismetSystemLibrary::DoesImplementInterface(MyOwner, UWorldBLDKitElementInterface::StaticClass()))
			{
				IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_RetrieveEdges(MyOwner, OutEdges);
			}
		}
	}
	else
	{
		OutEdges = ElementEdges;
	}
}

FWorldBLDKitElementEdge& UWorldBLDKitElementComponent::AllocateElementEdge(FName OptionalId)
{
	return ElementEdges[AllocateElementEdgeIndexed(OptionalId)];
}

int32 UWorldBLDKitElementComponent::AllocateElementEdgeIndexed(FName OptionalId)
{
	// NOTE: We can't allocate anything if we rely on the owner to do this for us.
	ensure(!bUseOwningActorTraits);

	// Search for an existing edge of this same name.
	if (!OptionalId.IsNone())
	{
		if (auto* ElementRef = ElementEdges.FindByPredicate([&](const FWorldBLDKitElementEdge& Member) -> bool { return Member.Id == OptionalId; }))
		{
			return ElementRef - ElementEdges.GetData();
			
			//return ((int64)ElementRef - (int64)ElementEdges.GetData()) / sizeof(void*);
		}
	}

	FWorldBLDKitElementEdge& NewEdge = ElementEdges.AddDefaulted_GetRef();
    NewEdge.Owner = GetOwner();
    NewEdge.Id = OptionalId.IsNone() ? FName(FString::Printf(TEXT("Edge%d"), ElementEdges.Num() - 1)) : OptionalId;
    NewEdge.SplineCurveToWorldTransform = GetOwner()->GetActorTransform();

	return ElementEdges.Num() - 1;
}

FWorldBLDKitElementEdge& UWorldBLDKitElementComponent::AllocateElementEdgeAlongExtrudedPolyline(
	FName OptionalId, 
	FGameplayTagContainer Tags,
	const TArray<FTransform>& LocalSweepPath,
	UCurveFloat* SweepShape, 
	float Scale, 
	bool bAlongMax,
	int32& OutIndex)
{
	const int32 EdgeIdx = AllocateElementEdgeIndexed(OptionalId);
	FWorldBLDKitElementEdge& NewEdge = ElementEdges[EdgeIdx];
	NewEdge.Tags = Tags;
	UWorldBLDKitElementUtils::CalculateEdgeAlongPolylinePath(LocalSweepPath, SweepShape, Scale, bAlongMax, NewEdge);

	OutIndex = EdgeIdx;
	return NewEdge;
}

void UWorldBLDKitElementComponent::WorldBLDKitElement_RetrievePoints_Implementation(TArray<FWorldBLDKitElementPoint>& OutPoints) const
{
	if (bUseOwningActorTraits)
	{
		if (const AActor* MyOwner = GetOwner())
		{
			if (UKismetSystemLibrary::DoesImplementInterface(MyOwner, UWorldBLDKitElementInterface::StaticClass()))
			{
				IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_RetrievePoints(MyOwner, OutPoints);
			}
		}
	}
	else
	{
		OutPoints = ElementPoints;
	}
}

FWorldBLDKitElementPoint& UWorldBLDKitElementComponent::AllocateElementPoint(FName OptionalId)
{
	return ElementPoints[AllocateElementPointIndexed(OptionalId)];
}

int32 UWorldBLDKitElementComponent::AllocateElementPointIndexed(FName OptionalId)
{
	// NOTE: We can't allocate anything if we rely on the owner to do this for us.
	ensure(!bUseOwningActorTraits);

	// Search for an existing edge of this same name.
	if (!OptionalId.IsNone())
	{
		if (auto* ElementRef = ElementPoints.FindByPredicate([&](const FWorldBLDKitElementPoint& Member) -> bool { return Member.Id == OptionalId; }))
		{
			return ((int64)ElementRef - (int64)ElementPoints.GetData()) / sizeof(void*);
		}
	}

	FWorldBLDKitElementPoint& NewPoint = ElementPoints.AddDefaulted_GetRef();
    NewPoint.Owner = GetOwner();
    NewPoint.Id = OptionalId.IsNone() ? FName(FString::Printf(TEXT("Point%d"), ElementPoints.Num() - 1)) : OptionalId;

	return ElementPoints.Num() - 1;
}

void UWorldBLDKitElementComponent::ValidateChildAssociations(TArray<AActor*>& OutInvalidChildren)
{
	TArray<AActor*> Children = IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_Children(this, /* Sub-children? */ false);
	const AActor* MyOwner = GetOwner();

	OutInvalidChildren.Empty();
	for (AActor* Child : Children)
	{
		if (const UObject* ChildImpl = UWorldBLDKitElementUtils::FindWorldBLDKitElementImplementor(Child))
		{
			const AActor* ReportedParent = IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_ParentActor(ChildImpl);
			if (MyOwner != ReportedParent)
			{
				OutInvalidChildren.Add(Child);
			}
		}
	}
}

void UWorldBLDKitElementComponent::RerunOwnerConstructionScript()
{
	if (IsValid(GetOwner()))
	{
		UWorldBLDKitElementUtils::TryForceRunConstructionScipts({GetOwner()});
	}
}

bool UWorldBLDKitElementComponent::SetAssignedStyle(UWorldBLDKitStyleBase* Style, bool bForce)
{
	bool bAcceptedStyle = true;
	if (IsValid(Style))
	{
		bAcceptedStyle = Style->StyleType.GetSingleTagContainer().HasAny(AllowedElementStyles);
	}
	if (bAcceptedStyle)
	{
		FGuid OriginalId;
		bool bDestroyCurrent = false;
		UWorldBLDKitStyleBase* OutgoingStyle = AssignedStyle;
		if (IsValid(OutgoingStyle))
		{
			OriginalId = OutgoingStyle->UniqueId;
		}

		// Actually assign the pointer
		if (IsValid(Style) && ((OriginalId != Style->UniqueId) || bForce))
		{
			AssignedStyle = Style->Duplicate(this, /* Shared ID */ true);
			bDestroyCurrent = true;
		}
		else if (!IsValid(Style))
		{
			AssignedStyle = nullptr;
		}

		// Notify if the style actually changed
		if (OutgoingStyle != AssignedStyle)
		{
			OnStyleAssigned.Broadcast(this);
		}

		// Destroy the old one
		if (bDestroyCurrent && OutgoingStyle)
		{
			OutgoingStyle->ConditionalBeginDestroy();
		}
	}
	return bAcceptedStyle;
}

UWorldBLDKitStyleBase* UWorldBLDKitElementComponent::GetAssignedStyle() const
{
	return AssignedStyle;
}
