// Copyright WorldBLD LLC. All Rights Reserved.

#include "WorldBLDContextMenuRegistry.h"

#include "IWorldBLDContextMenuFactory.h"
#include "WorldBLDEditorModule.h"

#include "GameFramework/Actor.h"

UWorldBLDContextMenuRegistry* UWorldBLDContextMenuRegistry::Instance = nullptr;

UWorldBLDContextMenuRegistry* UWorldBLDContextMenuRegistry::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UWorldBLDContextMenuRegistry>(GetTransientPackage());
		Instance->AddToRoot();
	}

	return Instance;
}

void UWorldBLDContextMenuRegistry::RegisterFactory(UClass* ActorClass, TScriptInterface<IWorldBLDContextMenuFactory> Factory)
{
	if (!ActorClass)
	{
		UE_LOG(LogWorldBuild, Warning, TEXT("RegisterFactory called with null ActorClass"));
		return;
	}

	if (!Factory.GetObject())
	{
		UE_LOG(LogWorldBuild, Warning, TEXT("RegisterFactory called with null Factory for class %s"), *ActorClass->GetName());
		return;
	}

	FactoryMap.Add(ActorClass, Factory);
	UE_LOG(LogWorldBuild, Log, TEXT("Registered context menu factory for %s"), *ActorClass->GetName());
}

void UWorldBLDContextMenuRegistry::UnregisterFactory(UClass* ActorClass)
{
	if (!ActorClass)
	{
		UE_LOG(LogWorldBuild, Warning, TEXT("UnregisterFactory called with null ActorClass"));
		return;
	}

	FactoryMap.Remove(ActorClass);
	UE_LOG(LogWorldBuild, Log, TEXT("Unregistered context menu factory for %s"), *ActorClass->GetName());
}

TScriptInterface<IWorldBLDContextMenuFactory> UWorldBLDContextMenuRegistry::FindFactoryForActor(AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return TScriptInterface<IWorldBLDContextMenuFactory>();
	}

	return FindFactoryForClass(Actor->GetClass());
}

TScriptInterface<IWorldBLDContextMenuFactory> UWorldBLDContextMenuRegistry::FindFactoryForClass(UClass* ActorClass) const
{
	for (UClass* CurrentClass = ActorClass; CurrentClass; CurrentClass = CurrentClass->GetSuperClass())
	{
		if (const TScriptInterface<IWorldBLDContextMenuFactory>* FoundFactory = FactoryMap.Find(CurrentClass))
		{
			return *FoundFactory;
		}
	}

	return TScriptInterface<IWorldBLDContextMenuFactory>();
}
