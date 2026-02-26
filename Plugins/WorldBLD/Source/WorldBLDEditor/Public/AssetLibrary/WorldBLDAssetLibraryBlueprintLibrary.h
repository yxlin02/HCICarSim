// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "WorldBLDAssetLibraryBlueprintLibrary.generated.h"

UCLASS()
class WORLDBLDEDITOR_API UWorldBLDAssetLibraryBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	static void OpenAssetLibraryWindow();
};
