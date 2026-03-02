// Copyright WorldBLD. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldBLDItemButtonWidget.h"
#include "BlueprintAssetItemButtonWidget.generated.h"

/**
 * UBlueprintAssetItemButtonWidget - Item button widget specialized for Blueprint assets
 * Displays Blueprint asset thumbnails and names using Asset Registry APIs.
 */
UCLASS()
class WORLDBLDEDITOR_API UBlueprintAssetItemButtonWidget : public UWorldBLDItemButtonWidget
{
	GENERATED_BODY()

public:
	
	/** The asset name for this Blueprint (used for display and tooltip) */
	UPROPERTY(BlueprintReadOnly, Category="WorldBLD Blueprint")
	FString AssetName;

	/** The asset path for this Blueprint (for thumbnail lookup) */
	UPROPERTY(BlueprintReadOnly, Category="WorldBLD Blueprint")
	FString AssetPath;

protected:
	
	/**
	 * Extract the thumbnail texture from a Blueprint asset using the Asset Registry.
	 * @param InClass - The Blueprint class to extract the thumbnail from
	 * @return The asset thumbnail texture, or nullptr if not available
	 */
	virtual UTexture2D* GetThumbnailFromClass_Implementation(UClass* InClass) override;

	/**
	 * Extract the asset name as the tooltip text.
	 * @param InClass - The Blueprint class to extract the name from
	 * @return The asset name as FText
	 */
	virtual FText GetTooltipTextFromClass_Implementation(UClass* InClass) override;
};

