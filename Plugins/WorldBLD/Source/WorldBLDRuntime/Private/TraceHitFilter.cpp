// Copyright WorldBLD LLC. All rights reserved.

#include "TraceHitFilter.h"
#include "WorldBLDKitElementUtils.h"

void UTraceHitFilterBase::FilterHits_Implementation(TArray<FHitResult>& Hits) 
{
	for (int32 Idx = 0; Idx < Hits.Num(); Idx += 1)
	{
		if (!FilterAcceptHit(Hits[Idx]))
		{
			Hits.RemoveAtSwap(Idx);
			Idx -= 1;
		}
	}
}

bool UTraceHitFilterBase::FilterAcceptHit_Implementation(const FHitResult& Hit) const 
{
	return true;
}

void UTraceSequentialFilter::FilterHits_Implementation(TArray<FHitResult>& Hits)
{
	for (UTraceHitFilterBase* Filter : Filters)
	{
		if (IsValid(Filter))
		{
			Filter->FilterHits(Hits);
		}
	}
}

bool UTraceActorClassFilter::FilterAcceptHit_Implementation(const FHitResult& Hit) const 
{
	if (!IsValid(Hit.GetActor())) 
	{
		return RequiredActorTypes.Num() == 0;
	}
	TSubclassOf<AActor> ActorClass = Hit.GetActor()->GetClass();
	for (TSoftClassPtr<AActor> RequiredClass : RequiredActorTypes)
	{
		if (!ActorClass->IsChildOf(RequiredClass.LoadSynchronous()))
		{
			return false;
		}
	}
	for (TSoftClassPtr<AActor> RejectedClass : RejectedActorTypes)
	{
		if (ActorClass->IsChildOf(RejectedClass.LoadSynchronous()))
		{
			return false;
		}
	}
	return true;
}

bool UTraceCityKitElementFilter::FilterAcceptHit_Implementation(const FHitResult& Hit) const
{
	return !bRejectElements == (UWorldBLDKitElementUtils::FindWorldBLDKitElementImplementor(Hit.GetActor()) != nullptr);
}