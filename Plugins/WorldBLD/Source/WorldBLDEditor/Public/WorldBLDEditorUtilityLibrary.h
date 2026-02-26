// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "WorldBLDEditorUtilityLibrary.generated.h"

 class ALandscape;
 class UMaterialInterface;

UCLASS()
class WORLDBLDEDITOR_API UWorldBLDEditorUtilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Spawns a landscape at world origin with specified resolution and applies a heightmap from file
	 * Compatible with UE 5.6 and 5.7 landscape APIs
	 * 
	 * @param HeightmapFilePath - Absolute path to heightmap file (PNG or RAW format)
	 * @param Resolution - Overall resolution of the landscape (e.g., 1009, 2017, 4033, 8129)
	 * @param NumComponents - Number of components per axis (e.g., 1x1, 2x2, 4x4, 8x8)
	 * @param Material - Optional landscape material to apply (can be nullptr)
	 * @return The spawned landscape actor, or nullptr if creation failed
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Landscape")
	static ALandscape* SpawnLandscapeWithHeightmap(
		const FString& HeightmapFilePath,
		int32 Resolution,
		int32 NumComponents,
		UMaterialInterface* Material = nullptr);

	/** Opens Project Settings focused on the WorldBLD settings section (UWorldBLDRuntimeSettings). */
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Settings")
	static void OpenWorldBLDProjectSettings();

};
