// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "WorldBLDContextMenuRegistry.generated.h"

class AActor;
class UClass;
class UWorldBLDContextMenuRegistry;
class IWorldBLDContextMenuFactory;

UCLASS(Transient)
class WORLDBLDEDITOR_API UWorldBLDContextMenuRegistry : public UObject
{
	GENERATED_BODY()

public:
	static UWorldBLDContextMenuRegistry* Get();

	void RegisterFactory(UClass* ActorClass, TScriptInterface<IWorldBLDContextMenuFactory> Factory);
	void UnregisterFactory(UClass* ActorClass);

	TScriptInterface<IWorldBLDContextMenuFactory> FindFactoryForActor(AActor* Actor) const;
	TScriptInterface<IWorldBLDContextMenuFactory> FindFactoryForClass(UClass* ActorClass) const;

private:
	TMap<UClass*, TScriptInterface<IWorldBLDContextMenuFactory>> FactoryMap;
	static UWorldBLDContextMenuRegistry* Instance;
};
