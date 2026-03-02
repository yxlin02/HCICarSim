// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "AssetRegistry/AssetData.h"
#include "Blueprint/UserWidget.h"

// This header is referenced by UCLASS headers that need `FGenerateLabelForAsset`. UHT needs to parse the delegate
// declaration, so we make this file a UHT header as well.
#include "InputAssetPanelTypes.generated.h"

/**
 * Shared types between InputAssetComboPanel / InputAssetPickerPanel widgets.
 *
 * Kept in a dedicated header to avoid including the full `InputAssetComboPanel.h` from other public headers.
 */

DECLARE_DYNAMIC_DELEGATE_RetVal_ThreeParams(FText, FGenerateLabelForAsset, UWidget*, Widget, UObject*, Item, int32, Idx);

/** Dummy type to ensure UHT always processes this header (do not use directly). */
USTRUCT()
struct FWorldBLDInputAssetPanelTypes_UHT
{
	GENERATED_BODY()
};

struct FAssetComboFilterSetup
{
	/** Class filter used when bPresentSpecificAssets is false. */
	UClass* AssetClassType = nullptr;

	/** If true, use the SpecificAssets list instead of the class filter. */
	bool bPresentSpecificAssets = false;
	TArray<FAssetData> SpecificAssets;

	/**
	 * Optional additional restriction: only show assets whose package is included in AllowedPackageNames.
	 * Intended to support "Loaded Kits Only" filtering from UMG wrappers.
	 */
	bool bRestrictToAllowedPackageNames = false;
	TSharedPtr<const TSet<FName>> AllowedPackageNames;

	FAssetComboFilterSetup() = default;
	explicit FAssetComboFilterSetup(UClass* InAssetClassType) : AssetClassType(InAssetClassType) {}
	explicit FAssetComboFilterSetup(const TArray<FAssetData>& InSpecificAssets)
		: bPresentSpecificAssets(true)
		, SpecificAssets(InSpecificAssets)
	{
	}
};


