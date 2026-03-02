// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "AssetLibrary/WorldBLDAssetLibraryTypes.h"

DECLARE_DELEGATE_RetVal_OneParam(FReply, FOnWorldBLDCollectionTileClicked, const FWorldBLDCollectionListItem& /*Collection*/);

struct FSlateDynamicImageBrush;

class SWorldBLDCollectionTile : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDCollectionTile) {}
		SLATE_ARGUMENT(FWorldBLDCollectionListItem, Collection)
		SLATE_EVENT(FOnWorldBLDCollectionTileClicked, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	FReply HandleClicked();
	EVisibility GetHoverOverlayVisibility() const;
	const FSlateBrush* GetThumbnailBrush() const;
	FText GetDisplayName() const;

private:
	FWorldBLDCollectionListItem Collection;
	FOnWorldBLDCollectionTileClicked OnClicked;

	TSharedPtr<FSlateDynamicImageBrush> ThumbnailBrush;
	bool bIsHovered = false;
};

