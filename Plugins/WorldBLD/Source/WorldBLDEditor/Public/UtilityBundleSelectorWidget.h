// Copyright WorldBLD. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldBLDItemSelectorWidget.h"
#include "UtilityBundleSelectorWidget.generated.h"

class UWorldBLDUtilityBundle;
class UUtilityBundleItemButtonWidget;
class UTextBlock;

/**
 * UUtilityBundleSelectorWidget - Selector widget specialized for UWorldBLDUtilityBundle
 * Automatically loads and displays all UWorldBLDUtilityBundle data asset instances from /WorldBLD/Utilities/Data/.
 */
UCLASS()
class WORLDBLDEDITOR_API UUtilityBundleSelectorWidget : public UWorldBLDItemSelectorWidget
{
	GENERATED_BODY()

public:
	
	/**
	 * Loads all UWorldBLDUtilityBundle assets from the specified folder (or default /WorldBLD/Utilities/Data/)
	 * and populates the selector with them.
	 * Call this after the widget is constructed to populate the gallery.
	 * 
	 * @param SearchPath - Optional custom path to search (e.g., "/Game/WorldBLD/Utilities/Data"). If empty, uses default "/WorldBLD/Utilities/Data"
	 * @param bTryCommonPaths - If true and nothing found, automatically tries common path variations like /Game/WorldBLD/..., /Plugins/WorldBLD/...
	 */
	UFUNCTION(BlueprintCallable, Category="WorldBLD Utility")
	void LoadAndDisplayUtilityBundles(const FString& SearchPath = TEXT(""), bool bTryCommonPaths = true);

	/**
	 * Helper function to scan the Asset Registry for plugin content.
	 * Call this if assets aren't being found - it forces a rescan of plugin directories.
	 */
	UFUNCTION(BlueprintCallable, Category="WorldBLD Utility")
	static void ForceRescanAssetRegistry();

	/**
	 * Blueprint implementable event called when a utility bundle is clicked.
	 * @param ClickedBundle - The utility bundle asset instance that was clicked
	 */
	UFUNCTION(BlueprintNativeEvent, Category="WorldBLD Utility")
	void OnUtilityBundleClicked(UWorldBLDUtilityBundle* ClickedBundle);
	virtual void OnUtilityBundleClicked_Implementation(UWorldBLDUtilityBundle* ClickedBundle);

protected:
	
	/**
	 * Override the base OnItemClicked to forward to OnUtilityBundleClicked for type-specific handling.
	 * Retrieves the utility bundle asset from the clicked item widget.
	 */
	virtual void OnItemClicked_Implementation(UClass* ClickedItemClass) override;

	/** Text to show when no bundles are found (Hidden when bundles exist). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="WorldBLD Utility")
	UTextBlock* NoBundlesFoundText = nullptr;

private:
	
	/** Stores the loaded utility bundle assets for reference */
	UPROPERTY(Transient)
	TArray<UWorldBLDUtilityBundle*> LoadedBundles;
};

