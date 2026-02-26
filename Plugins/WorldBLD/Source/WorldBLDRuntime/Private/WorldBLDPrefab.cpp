// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDPrefab.h"
#include "WorldBLDRuntime/Classes/WorldBLDKitElementComponent.h"
#include "Components/SceneComponent.h"

AWorldBLDPrefab::AWorldBLDPrefab()
{
	// Set this actor to call Tick() every frame if needed
	PrimaryActorTick.bCanEverTick = false;

	// Create and set up the root scene component
	PrefabRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PrefabRoot"));
	RootComponent = PrefabRoot;

	// Create and attach the WorldBLDKitElementComponent
	KitElementComponent = CreateDefaultSubobject<UWorldBLDKitElementComponent>(TEXT("KitElementComponent"));

	// Initialize default values
	UserDisplayText = FText::FromString(TEXT("WorldBLD Prefab"));
	bAlignLandscape = false;
	SpawnRotationOffset = FRotator::ZeroRotator;
}

