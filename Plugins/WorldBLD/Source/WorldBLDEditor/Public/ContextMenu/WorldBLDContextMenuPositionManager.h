// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "WorldBLDContextMenuPositionManager.generated.h"

UCLASS(Transient)
class WORLDBLDEDITOR_API UWorldBLDContextMenuPositionManager : public UObject
{
	GENERATED_BODY()

public:
	static UWorldBLDContextMenuPositionManager* Get();

	FVector2D GetPosition(UClass* ActorClass, const FVector2D& DefaultPosition) const;
	void SavePosition(UClass* ActorClass, const FVector2D& Position);
	void ClearPositions();

private:
	static UWorldBLDContextMenuPositionManager* Instance;

public:
	UPROPERTY(Transient)
	TMap<FString, FVector2D> SavedPositions;
};


