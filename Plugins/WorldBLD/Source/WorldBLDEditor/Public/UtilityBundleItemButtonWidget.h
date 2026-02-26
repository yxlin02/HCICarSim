// Copyright WorldBLD. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldBLDItemButtonWidget.h"
#include "UtilityBundleItemButtonWidget.generated.h"

class UWorldBLDUtilityBundle;

/**
 * UUtilityBundleItemButtonWidget - Item button widget specialized for UWorldBLDUtilityBundle
 * Stores and displays UWorldBLDUtilityBundle data asset instances.
 * Since DataAssets work with instances rather than classes, this widget stores both.
 */
UCLASS()
class WORLDBLDEDITOR_API UUtilityBundleItemButtonWidget : public UWorldBLDItemButtonWidget
{
	GENERATED_BODY()

public:
	
	/** The utility bundle asset instance associated with this widget */
	UPROPERTY(BlueprintReadOnly, Category="WorldBLD Utility")
	UWorldBLDUtilityBundle* UtilityBundleAsset;

	/**
	 * Configure this widget with a utility bundle asset instance.
	 * @param InBundleAsset - The utility bundle asset to display
	 */
	UFUNCTION(BlueprintCallable, Category="WorldBLD Utility")
	void SetUtilityBundle(UWorldBLDUtilityBundle* InBundleAsset);

protected:
	
	/**
	 * Extract the ToolImage texture from a UWorldBLDUtilityBundle asset.
	 * Since we're working with DataAssets, we extract from the stored asset instance.
	 * @param InClass - The class (used for compatibility with base class)
	 * @return The ToolImage texture, or nullptr if not available
	 */
	virtual UTexture2D* GetThumbnailFromClass_Implementation(UClass* InClass) override;

	/**
	 * Extract the UtilityName text from a UWorldBLDUtilityBundle asset.
	 * Since we're working with DataAssets, we extract from the stored asset instance.
	 * @param InClass - The class (used for compatibility with base class)
	 * @return The UtilityName text
	 */
	virtual FText GetTooltipTextFromClass_Implementation(UClass* InClass) override;
};

