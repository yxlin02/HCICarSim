// Copyright WorldBLD LLC. All Rights Reserved.

#include "ContextMenu/WorldBLDContextMenuPositionManager.h"

#include "UObject/Package.h"

UWorldBLDContextMenuPositionManager* UWorldBLDContextMenuPositionManager::Instance = nullptr;

UWorldBLDContextMenuPositionManager* UWorldBLDContextMenuPositionManager::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UWorldBLDContextMenuPositionManager>(GetTransientPackage());
		Instance->AddToRoot();
	}

	return Instance;
}

FVector2D UWorldBLDContextMenuPositionManager::GetPosition(UClass* ActorClass, const FVector2D& DefaultPosition) const
{
	if (!IsValid(ActorClass))
	{
		return DefaultPosition;
	}

	const FString Key = ActorClass->GetPathName();
	if (const FVector2D* Existing = SavedPositions.Find(Key))
	{
		return *Existing;
	}

	return DefaultPosition;
}

void UWorldBLDContextMenuPositionManager::SavePosition(UClass* ActorClass, const FVector2D& Position)
{
	if (!IsValid(ActorClass))
	{
		return;
	}

	const FString Key = ActorClass->GetPathName();
	SavedPositions.Add(Key, Position);
}

void UWorldBLDContextMenuPositionManager::ClearPositions()
{
	SavedPositions.Reset();
}


