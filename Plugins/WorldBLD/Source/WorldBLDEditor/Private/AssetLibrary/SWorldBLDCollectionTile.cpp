// Copyright WorldBLD LLC. All Rights Reserved.

#include "AssetLibrary/SWorldBLDCollectionTile.h"

#include "AssetLibrary/WorldBLDAssetLibraryImageLoader.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SWorldBLDCollectionTile::Construct(const FArguments& InArgs)
{
	constexpr float TileSize = 200.0f;
	constexpr float TextWrapAt = TileSize - 20.0f;

	Collection = InArgs._Collection;
	OnClicked = InArgs._OnClicked;

	ThumbnailBrush = FWorldBLDAssetLibraryImageLoader::Get().GetPlaceholderBrush();

	FWorldBLDAssetLibraryImageLoader::Get().LoadImageFromURL(Collection.ThumbnailURL, [WeakThis = TWeakPtr<SWorldBLDCollectionTile>(SharedThis(this))](TSharedPtr<FSlateDynamicImageBrush> LoadedBrush)
	{
		if (TSharedPtr<SWorldBLDCollectionTile> Pinned = WeakThis.Pin())
		{
			Pinned->ThumbnailBrush = LoadedBrush;
			Pinned->Invalidate(EInvalidateWidgetReason::Paint);
		}
	});

	// Preload the collection thumbnail so it appears instantly if this tile is reconstructed.
	FWorldBLDAssetLibraryImageLoader::Get().PrefetchImagesFromURLs({ Collection.ThumbnailURL });

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(TileSize)
		.HeightOverride(TileSize)
		[
			SNew(SBorder)
			.Padding(0.0f)
			.BorderBackgroundColor(FLinearColor(0.12f, 0.12f, 0.12f, 1.0f))
			.Cursor(EMouseCursor::Hand)
			.Clipping(EWidgetClipping::ClipToBounds)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SScaleBox)
					.Stretch(EStretch::ScaleToFill)
					.StretchDirection(EStretchDirection::Both)
					[
						SNew(SImage)
						.Image(this, &SWorldBLDCollectionTile::GetThumbnailBrush)
					]
				]
				+ SOverlay::Slot()
				.VAlign(VAlign_Bottom)
				[
					SNew(SBorder)
					.Padding(FMargin(8.0f))
					.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f))
					[
						SNew(STextBlock)
						.Text(this, &SWorldBLDCollectionTile::GetDisplayName)
						.ColorAndOpacity(FLinearColor(0.92f, 0.92f, 0.92f, 1.0f))
						.WrapTextAt(TextWrapAt)
					]
				]
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.Visibility(this, &SWorldBLDCollectionTile::GetHoverOverlayVisibility)
					.Padding(FMargin(8.0f))
					.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("View Collection")))
						.ColorAndOpacity(FLinearColor::White)
					]
				]
			]
		]
	];
}

void SWorldBLDCollectionTile::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bIsHovered = true;
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SWorldBLDCollectionTile::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	bIsHovered = false;
	SCompoundWidget::OnMouseLeave(MouseEvent);
	Invalidate(EInvalidateWidgetReason::Paint);
}

FReply SWorldBLDCollectionTile::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return HandleClicked();
	}

	return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SWorldBLDCollectionTile::HandleClicked()
{
	if (OnClicked.IsBound())
	{
		return OnClicked.Execute(Collection);
	}

	return FReply::Handled();
}

EVisibility SWorldBLDCollectionTile::GetHoverOverlayVisibility() const
{
	return bIsHovered ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SWorldBLDCollectionTile::GetThumbnailBrush() const
{
	if (ThumbnailBrush.IsValid())
	{
		return ThumbnailBrush.Get();
	}

	TSharedPtr<FSlateDynamicImageBrush> Placeholder = FWorldBLDAssetLibraryImageLoader::Get().GetPlaceholderBrush();
	return Placeholder.IsValid() ? Placeholder.Get() : nullptr;
}

FText SWorldBLDCollectionTile::GetDisplayName() const
{
	return FText::FromString(Collection.CollectionName);
}

