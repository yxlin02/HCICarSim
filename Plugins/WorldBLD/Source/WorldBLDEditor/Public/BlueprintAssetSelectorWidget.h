// Copyright WorldBLD. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldBLDItemSelectorWidget.h"
#include "BlueprintAssetSelectorWidget.generated.h"

class UBlueprintAssetItemButtonWidget;

/**
 * UBlueprintAssetSelectorWidget - Selector widget for Blueprint assets of a given base class
 * Automatically loads and displays all Blueprint assets that inherit from a user-specified base class.
 * Uses Asset Registry to find assets and displays them with their asset thumbnails and names.
 */
UCLASS()
class WORLDBLDEDITOR_API UBlueprintAssetSelectorWidget : public UWorldBLDItemSelectorWidget
{
	GENERATED_BODY()

public:
	
	/**
	 * Base class to filter Blueprint assets by.
	 * Set this in Blueprint to specify which type of Blueprint assets to display.
	 * For example, set to "Actor" to display all Blueprint Actors.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLD Blueprint")
	TSubclassOf<UObject> BaseClassFilter;

	/**
	 * Loads all Blueprint assets that inherit from BaseClassFilter and displays them in the selector.
	 * Call this after the widget is constructed and after setting BaseClassFilter.
	 * 
	 * @param SearchPath - Optional custom path to search (e.g., "/Game/Blueprints"). If empty, searches entire project.
	 * @param bRecursive - If true, searches subdirectories recursively (default: true)
	 */
	UFUNCTION(BlueprintCallable, Category="WorldBLD Blueprint")
	void LoadAndDisplayBlueprintAssets(const FString& SearchPath = TEXT(""), bool bRecursive = true);

	/**
	 * Helper function to scan the Asset Registry for all content.
	 * Call this if assets aren't being found - it forces a rescan.
	 */
	UFUNCTION(BlueprintCallable, Category="WorldBLD Blueprint")
	static void ForceRescanAssetRegistry();

	/**
	 * Blueprint implementable event called when a Blueprint asset is clicked.
	 * @param ClickedAssetClass - The Blueprint class that was clicked
	 */
	UFUNCTION(BlueprintNativeEvent, Category="WorldBLD Blueprint")
	void OnBlueprintAssetClicked(UClass* ClickedAssetClass);
	virtual void OnBlueprintAssetClicked_Implementation(UClass* ClickedAssetClass);

protected:
	
	/**
	 * Override the base OnItemClicked to forward to OnBlueprintAssetClicked for type-specific handling.
	 */
	virtual void OnItemClicked_Implementation(UClass* ClickedItemClass) override;

private:
	
	/** Stores the loaded Blueprint asset classes for reference */
	UPROPERTY(Transient)
	TArray<UClass*> LoadedAssetClasses;
};

