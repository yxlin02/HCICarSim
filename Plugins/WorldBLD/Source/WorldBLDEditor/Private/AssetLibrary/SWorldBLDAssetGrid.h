// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "AssetLibrary/WorldBLDAssetLibraryTypes.h"
#include "AssetLibrary/SWorldBLDAssetTile.h"
#include "AssetLibrary/SWorldBLDCollectionTile.h"

#include "Widgets/Layout/SScrollBox.h"

class SWrapBox;

class SWorldBLDAssetGrid : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDAssetGrid) {}
		SLATE_ARGUMENT(TArray<FWorldBLDAssetInfo>, Assets)
		SLATE_ARGUMENT(TArray<FWorldBLDKitInfo>, Kits)
		SLATE_ARGUMENT(TArray<FWorldBLDCollectionListItem>, Collections)
		SLATE_ARGUMENT(bool, bShowKits)
		SLATE_ARGUMENT(bool, bShowCollections)
		SLATE_ARGUMENT(TArray<FWorldBLDFeaturedCollection>, FeaturedCollections)
		SLATE_EVENT(FOnWorldBLDAssetTileClicked, OnAssetClicked)
		SLATE_EVENT(FOnWorldBLDCollectionTileClicked, OnCollectionClicked)
		SLATE_EVENT(FOnUserScrolled, OnUserScrolled)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void RefreshGrid();
	void ScrollToTop();

	void SetAssets(const TArray<FWorldBLDAssetInfo>& InAssets);
	void SetKits(const TArray<FWorldBLDKitInfo>& InKits);
	void SetCollections(const TArray<FWorldBLDCollectionListItem>& InCollections);
	void SetFeaturedCollections(const TArray<FWorldBLDFeaturedCollection>& InCollections);

	/** Returns true if the user is close to the bottom of the scrollable content. */
	bool IsNearEnd(float CurrentScrollOffset, float RemainingThreshold = 300.0f) const;

private:
	TArray<FWorldBLDAssetInfo> Assets;
	TArray<FWorldBLDKitInfo> Kits;
	TArray<FWorldBLDCollectionListItem> Collections;
	bool bShowKits = false;
	bool bShowCollections = false;

	TArray<FWorldBLDFeaturedCollection> FeaturedCollections;

	FOnWorldBLDAssetTileClicked OnAssetClicked;
	FOnWorldBLDCollectionTileClicked OnCollectionClicked;
	FOnUserScrolled OnUserScrolled;

	TSharedPtr<SScrollBox> ScrollBox;
	TSharedPtr<SWrapBox> WrapBox;
};
