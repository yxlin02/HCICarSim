// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "IWorldBLDContextMenuFactory.generated.h"

class AActor;
class SWidget;

UINTERFACE(MinimalAPI)
class UWorldBLDContextMenuFactory : public UInterface
{
	GENERATED_BODY()
};

class WORLDBLDEDITOR_API IWorldBLDContextMenuFactory
{
	GENERATED_BODY()

public:
	virtual bool CanCreateContextMenuForActor(AActor* Actor) const = 0;
	virtual TSharedRef<SWidget> CreateContextMenuWidget(AActor* Actor) const = 0;
	virtual TSharedRef<SWidget> CreateContextMenuWidget(const TArray<AActor*>& Actors) const = 0;
	virtual UClass* GetSupportedActorClass() const = 0;
};
