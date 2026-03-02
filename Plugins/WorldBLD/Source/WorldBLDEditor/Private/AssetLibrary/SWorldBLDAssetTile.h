// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "AssetLibrary/WorldBLDAssetLibraryTypes.h"

class UWorldBLDAssetDownloadManager;
struct FSlateDynamicImageBrush;

DECLARE_DELEGATE_RetVal_ThreeParams(FReply, FOnWorldBLDAssetTileClicked, bool /*bIsKit*/, const FWorldBLDAssetInfo& /*AssetInfo*/, const FWorldBLDKitInfo& /*KitInfo*/);

class SWorldBLDAssetTile : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDAssetTile) {}
		SLATE_ARGUMENT(FWorldBLDAssetInfo, AssetInfo)
		SLATE_ARGUMENT(FWorldBLDKitInfo, KitInfo)
		SLATE_ARGUMENT(bool, bIsKit)
		SLATE_EVENT(FOnWorldBLDAssetTileClicked, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	FReply HandleClicked();

	EWorldBLDAssetStatus GetAssetStatus() const;
	FText GetStatusText() const;
	FSlateColor GetStatusColor() const;
	FText GetPriceText() const;
	EVisibility GetHoverOutlineVisibility() const;
	const FSlateBrush* GetHoverOutlineBrush() const;
	const FSlateBrush* GetThumbnailBrush() const;
	FText GetDisplayName() const;

private:
	FWorldBLDAssetInfo AssetInfo;
	FWorldBLDKitInfo KitInfo;
	bool bIsKit = false;

	FOnWorldBLDAssetTileClicked OnClicked;

	TSharedPtr<FSlateDynamicImageBrush> ThumbnailBrush;
	TWeakObjectPtr<UWorldBLDAssetDownloadManager> DownloadManager;

	bool bIsHovered = false;
};
