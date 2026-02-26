// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "WorldBLDPrefab.generated.h"

class USceneComponent;
class UWorldBLDKitElementComponent;

// A prefab actor that can be placed in the world with WorldBLD kit element support
UCLASS(BlueprintType, Blueprintable)
class WORLDBLDRUNTIME_API AWorldBLDPrefab : public AActor
{
	GENERATED_BODY()

public:
	AWorldBLDPrefab();

	///////////////////////////////////////////////////////////////////////////////////////////////

	// Root scene component for the prefab
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="WorldBLD")
	USceneComponent* PrefabRoot;

	// The display text shown to the user in interfaces
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD")
	FText UserDisplayText;

	// Whether to align this prefab to the landscape when spawned
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD")
	bool bAlignLandscape;

	// Rotation offset to apply when spawning this prefab
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD")
	FRotator SpawnRotationOffset;

	///////////////////////////////////////////////////////////////////////////////////////////////

	// Component that maintains kit element references and properties
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="WorldBLD")
	UWorldBLDKitElementComponent* KitElementComponent;
};

