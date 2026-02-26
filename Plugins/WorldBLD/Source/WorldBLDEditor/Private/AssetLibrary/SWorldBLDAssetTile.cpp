// Copyright WorldBLD LLC. All Rights Reserved.

#include "AssetLibrary/SWorldBLDAssetTile.h"

#include "AssetLibrary/WorldBLDAssetDownloadManager.h"
#include "AssetLibrary/WorldBLDAssetLibraryImageLoader.h"
#include "ContextMenu/WorldBLDContextMenuStyle.h"

#include "Editor.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SWorldBLDAssetTile::Construct(const FArguments& InArgs)
{
	constexpr float TileSize = 200.0f;
	constexpr float TextWrapAt = TileSize - 20.0f;

	AssetInfo = InArgs._AssetInfo;
	KitInfo = InArgs._KitInfo;
	bIsKit = InArgs._bIsKit;
	OnClicked = InArgs._OnClicked;

	DownloadManager = GEditor ? GEditor->GetEditorSubsystem<UWorldBLDAssetDownloadManager>() : nullptr;

	const FString ThumbnailURL = bIsKit ? (KitInfo.PreviewImages.Num() > 0 ? KitInfo.PreviewImages[0] : FString()) : AssetInfo.ThumbnailURL;
	ThumbnailBrush = FWorldBLDAssetLibraryImageLoader::Get().GetPlaceholderBrush();

	FWorldBLDAssetLibraryImageLoader::Get().LoadImageFromURL(ThumbnailURL, [WeakThis = TWeakPtr<SWorldBLDAssetTile>(SharedThis(this))](TSharedPtr<FSlateDynamicImageBrush> LoadedBrush)
	{
		if (TSharedPtr<SWorldBLDAssetTile> Pinned = WeakThis.Pin())
		{
			Pinned->ThumbnailBrush = LoadedBrush;
			Pinned->Invalidate(EInvalidateWidgetReason::Paint);
		}
	});

	// Prefetch preview images for this tile while the user is browsing the grid, so detail-panel preview
	// images display instantly on selection. This naturally only runs for tiles that are actually constructed
	// (i.e., items currently visible in the grid).
	const TArray<FString>& PreviewUrls = bIsKit ? KitInfo.PreviewImages : AssetInfo.PreviewImages;
	FWorldBLDAssetLibraryImageLoader::Get().PrefetchImagesFromURLs(PreviewUrls);

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
						.Image(this, &SWorldBLDAssetTile::GetThumbnailBrush)
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Top)
				.Padding(FMargin(6.0f))
				[
					SNew(SBorder)
					.Padding(FMargin(6.0f, 2.0f))
					.BorderBackgroundColor(this, &SWorldBLDAssetTile::GetStatusColor)
					[
						SNew(STextBlock)
						.Text(this, &SWorldBLDAssetTile::GetStatusText)
						.ColorAndOpacity(FLinearColor::White)
					]
				]
				+ SOverlay::Slot()
				.VAlign(VAlign_Bottom)
				[
					SNew(SBorder)
					.Padding(FMargin(8.0f))
					.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(this, &SWorldBLDAssetTile::GetDisplayName)
							.ColorAndOpacity(FLinearColor(0.92f, 0.92f, 0.92f, 1.0f))
							.WrapTextAt(TextWrapAt)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
						[
							SNew(STextBlock)
							.Text(this, &SWorldBLDAssetTile::GetPriceText)
							.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f, 1.0f))
						]
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
					.Visibility(this, &SWorldBLDAssetTile::GetHoverOutlineVisibility)
					.Padding(0.0f)
					.BorderImage(this, &SWorldBLDAssetTile::GetHoverOutlineBrush)
					[
						SNew(SSpacer)
					]
				]
			]
		]
	];
}

void SWorldBLDAssetTile::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bIsHovered = true;
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SWorldBLDAssetTile::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	bIsHovered = false;
	SCompoundWidget::OnMouseLeave(MouseEvent);
	Invalidate(EInvalidateWidgetReason::Paint);
}

FReply SWorldBLDAssetTile::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return HandleClicked();
	}

	return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SWorldBLDAssetTile::HandleClicked()
{
	if (OnClicked.IsBound())
	{
		return OnClicked.Execute(bIsKit, AssetInfo, KitInfo);
	}

	return FReply::Handled();
}

EWorldBLDAssetStatus SWorldBLDAssetTile::GetAssetStatus() const
{
	if (!DownloadManager.IsValid())
	{
		return EWorldBLDAssetStatus::NotOwned;
	}

	const FString AssetId = bIsKit ? KitInfo.ID : AssetInfo.ID;
	return DownloadManager->GetAssetStatus(AssetId);
}

FText SWorldBLDAssetTile::GetStatusText() const
{
	switch (GetAssetStatus())
	{
	case EWorldBLDAssetStatus::NotOwned:
		return FText::FromString(TEXT("Not Owned"));
	case EWorldBLDAssetStatus::Owned:
		return FText::FromString(TEXT("Not Downloaded"));
	case EWorldBLDAssetStatus::Downloading:
		return FText::FromString(TEXT("Downloading..."));
	case EWorldBLDAssetStatus::Downloaded:
		return FText::FromString(TEXT("Downloaded"));
	case EWorldBLDAssetStatus::Imported:
		return FText::FromString(TEXT("Imported"));
	default:
		return FText::FromString(TEXT("Unknown"));
	}
}

FSlateColor SWorldBLDAssetTile::GetStatusColor() const
{
	switch (GetAssetStatus())
	{
	case EWorldBLDAssetStatus::NotOwned:
		return FLinearColor(0.8f, 0.15f, 0.15f, 0.9f);
	case EWorldBLDAssetStatus::Owned:
		return FLinearColor(0.75f, 0.65f, 0.1f, 0.9f);
	case EWorldBLDAssetStatus::Downloading:
		return FLinearColor(0.2f, 0.45f, 0.9f, 0.9f);
	case EWorldBLDAssetStatus::Downloaded:
		return FLinearColor(0.2f, 0.75f, 0.25f, 0.9f);
	case EWorldBLDAssetStatus::Imported:
		return FLinearColor(0.0f, 0.8f, 0.8f, 0.9f);
	default:
		return FLinearColor(0.35f, 0.35f, 0.35f, 0.9f);
	}
}

FText SWorldBLDAssetTile::GetPriceText() const
{
	const bool bFree = bIsKit ? KitInfo.bIsFree : AssetInfo.bIsFree;
	if (bFree)
	{
		return FText::FromString(TEXT("Free"));
	}

	const int32 Price = bIsKit ? KitInfo.Price : AssetInfo.CreditsPrice;
	return FText::FromString(FString::Printf(TEXT("%d Credits"), Price));
}

EVisibility SWorldBLDAssetTile::GetHoverOutlineVisibility() const
{
	return bIsHovered ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SWorldBLDAssetTile::GetHoverOutlineBrush() const
{
	// Simple hover outline: use the same red as the highlighted purchase/get-credits buttons.
	// Corner order: TopLeft, TopRight, BottomRight, BottomLeft.
	static const FVector4 CornerRadii(4.0f, 4.0f, 4.0f, 4.0f);

	static const FSlateBrushOutlineSettings OutlineSettings = []()
	{
		FSlateBrushOutlineSettings Settings;
		Settings.CornerRadii = CornerRadii;
		Settings.Color = FSlateColor(WorldBLDContextMenuStyle::GetHighlightRedColor());
		Settings.Width = 2.0f;
		Settings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Settings.bUseBrushTransparency = false;
		return Settings;
	}();

	static const FSlateRoundedBoxBrush Brush = []()
	{
		FSlateRoundedBoxBrush RoundedBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), CornerRadii);
		RoundedBrush.OutlineSettings = OutlineSettings;
		return RoundedBrush;
	}();

	return &Brush;
}

const FSlateBrush* SWorldBLDAssetTile::GetThumbnailBrush() const
{
	if (ThumbnailBrush.IsValid())
	{
		return ThumbnailBrush.Get();
	}

	TSharedPtr<FSlateDynamicImageBrush> Placeholder = FWorldBLDAssetLibraryImageLoader::Get().GetPlaceholderBrush();
	return Placeholder.IsValid() ? Placeholder.Get() : nullptr;
}

FText SWorldBLDAssetTile::GetDisplayName() const
{
	return FText::FromString(bIsKit ? KitInfo.KitsName : AssetInfo.Name);
}
