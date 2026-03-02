#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/SoftObjectPtr.h"
#include "WorldBLDLevelTemplateBlueprintLibrary.generated.h"

class UWorld;

UCLASS()
class WORLDBLDEDITOR_API UWorldBLDLevelTemplateBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|LevelTemplate")
	static void OpenLevelTemplateWindow();

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|LevelTemplate")
	static void LoadLightingPreset(UWorld* TargetWorld, UWorld* PresetWorld);

	// Loads and applies a lighting preset from a soft world reference. This avoids holding onto other worlds
	// during map transitions (which can trigger "World Memory Leaks" in the editor).
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|LevelTemplate")
	static void LoadLightingPresetFromSoftWorld(UWorld* TargetWorld, TSoftObjectPtr<UWorld> PresetWorld);
};
