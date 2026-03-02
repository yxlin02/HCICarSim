// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "AssetRegistry/AssetData.h"
#include "ContentBrowserDelegates.h"
#include "Widgets/SCompoundWidget.h"

class SInputAssetComboPanel;
class STextBlock;
class UClass;

/**
 * Reusable asset picker widget designed for context-menu panels.
 * Supports mixed values via TOptional<FAssetData> (unset = "Mixed" overlay).
 */
class WORLDBLDEDITOR_API SWorldBLDAssetPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDAssetPicker)
		: _Label(FText::GetEmpty())
		, _ToolTipText(FText::GetEmpty())
		, _AssetClass(nullptr)
		// Default reduced by ~25% to keep context menus compact.
		, _ThumbnailSize(FVector2D(37.5f, 37.5f))
		, _bCompactSquareRightAligned(false)
		, _bAllowClear(true)
		, _TransactionText(FText::GetEmpty())
	{
	}

		SLATE_ATTRIBUTE(TOptional<FAssetData>, SelectedAsset)
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(FText, ToolTipText)
		SLATE_ARGUMENT(UClass*, AssetClass)
		/** Optional additional filter used by the underlying asset picker. Return true to filter OUT an asset. */
		SLATE_EVENT(FOnShouldFilterAsset, OnShouldFilterAsset)
		SLATE_ARGUMENT(FVector2D, ThumbnailSize)
		/**
		 * If true, forces the thumbnail tile size to be square (based on max(ThumbnailSize.X, ThumbnailSize.Y)),
		 * and right-aligns the picker while the label fills remaining space on the left.
		 *
		 * Note: This does not force the overall picker widget to be square; it will remain wide enough
		 * to accommodate the tile plus chrome (dropdown arrow, padding, etc).
		 */
		SLATE_ARGUMENT(bool, bCompactSquareRightAligned)
		SLATE_EVENT(FOnAssetSelected, OnAssetSelected)
		SLATE_ARGUMENT(bool, bAllowClear)
		/** If non-empty, asset selection changes initiated by this widget are wrapped in an editor undo transaction. */
		SLATE_ARGUMENT(FText, TransactionText)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

private:
	TOptional<FAssetData> GetSelectedAsset() const;

	void HandleAssetSelected(const FAssetData& AssetData);

	EVisibility GetMixedValueVisibility() const;
	EVisibility GetAssetComboVisibility() const;
	void RefreshAssetComboFromAttribute();
	TSharedRef<SWidget> MakeAssetComboPanel(const FAssetData& InitialAssetData);

private:
	TAttribute<TOptional<FAssetData>> SelectedAssetAttribute;
	FText LabelText;
	FText ToolTipText;
	UClass* FilterAssetClass = nullptr;
	FOnShouldFilterAsset OnShouldFilterAssetDelegate;
	FVector2D ThumbnailSize = FVector2D(50.0f, 50.0f);
	bool bCompactSquareRightAligned = false;
	bool bAllowClear = true;
	FText TransactionText;

	FOnAssetSelected OnAssetSelectedDelegate;

	TSharedPtr<SInputAssetComboPanel> AssetComboPanel;
	TSharedPtr<class SBox> AssetComboContainer;
	TSharedPtr<STextBlock> MixedValueLabel;
	TSharedPtr<STextBlock> LabelWidget;

	FSoftObjectPath LastSelectedAssetPath;

	double HoverStartTimeSeconds = 0.0;
	bool bIsHovering = false;
	bool bTooltipArmed = false;
};

