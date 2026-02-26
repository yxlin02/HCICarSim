// Copyright WorldBLD LLC. All Rights Reserved.

#include "AssetLibrary/SWorldBLDAssetGrid.h"

#include "AssetLibrary/SWorldBLDAssetTile.h"
#include "AssetLibrary/SWorldBLDCollectionTile.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"

void SWorldBLDAssetGrid::Construct(const FArguments& InArgs)
{
	Assets = InArgs._Assets;
	Kits = InArgs._Kits;
	Collections = InArgs._Collections;
	bShowKits = InArgs._bShowKits;
	bShowCollections = InArgs._bShowCollections;
	FeaturedCollections = InArgs._FeaturedCollections;
	OnAssetClicked = InArgs._OnAssetClicked;
	OnCollectionClicked = InArgs._OnCollectionClicked;
	OnUserScrolled = InArgs._OnUserScrolled;

	ChildSlot
	[
		SAssignNew(ScrollBox, SScrollBox)
		.OnUserScrolled(OnUserScrolled)
	];

	RefreshGrid();
}

void SWorldBLDAssetGrid::RefreshGrid()
{
	if (!ScrollBox.IsValid())
	{
		return;
	}

	const float PreviousScrollOffset = ScrollBox->GetScrollOffset();

	ScrollBox->ClearChildren();

	if (!bShowCollections && FeaturedCollections.Num() > 0)
	{
		for (const FWorldBLDFeaturedCollection& Collection : FeaturedCollections)
		{
			ScrollBox->AddSlot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Collection.Title))
			];

			if (!Collection.Description.IsEmpty())
			{
				ScrollBox->AddSlot()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Collection.Description))
				];
			}

			TSharedPtr<SWrapBox> FeaturedWrap;
			ScrollBox->AddSlot()
			[
				SAssignNew(FeaturedWrap, SWrapBox)
				.UseAllottedSize(true)
				.InnerSlotPadding(FVector2D(10.0f, 10.0f))
			];

			int32 Added = 0;
			const int32 MaxItems = 6;

			for (const FWorldBLDAssetInfo& Asset : Collection.FeaturedAssets)
			{
				if (Added >= MaxItems)
				{
					break;
				}

				FeaturedWrap->AddSlot()
				[
					SNew(SWorldBLDAssetTile)
					.AssetInfo(Asset)
					.bIsKit(false)
					.OnClicked(OnAssetClicked)
				];
				++Added;
			}

			for (const FWorldBLDKitInfo& Kit : Collection.FeaturedKits)
			{
				if (Added >= MaxItems)
				{
					break;
				}

				FeaturedWrap->AddSlot()
				[
					SNew(SWorldBLDAssetTile)
					.KitInfo(Kit)
					.bIsKit(true)
					.OnClicked(OnAssetClicked)
				];
				++Added;
			}

			ScrollBox->AddSlot()
			[
				SNew(SSeparator)
			];
		}
	}

	ScrollBox->AddSlot()
	[
		SAssignNew(WrapBox, SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(FVector2D(10.0f, 10.0f))
	];

	if (!WrapBox.IsValid())
	{
		return;
	}

	WrapBox->ClearChildren();

	if (bShowCollections)
	{
		for (const FWorldBLDCollectionListItem& Collection : Collections)
		{
			WrapBox->AddSlot()
			[
				SNew(SWorldBLDCollectionTile)
				.Collection(Collection)
				.OnClicked(OnCollectionClicked)
			];
		}
	}
	else if (bShowKits)
	{
		for (const FWorldBLDKitInfo& Kit : Kits)
		{
			WrapBox->AddSlot()
			[
				SNew(SWorldBLDAssetTile)
				.KitInfo(Kit)
				.bIsKit(true)
				.OnClicked(OnAssetClicked)
			];
		}
	}
	else
	{
		for (const FWorldBLDAssetInfo& Asset : Assets)
		{
			WrapBox->AddSlot()
			[
				SNew(SWorldBLDAssetTile)
				.AssetInfo(Asset)
				.bIsKit(false)
				.OnClicked(OnAssetClicked)
			];
		}
	}

	// Preserve scroll position when the grid is rebuilt (important for infinite-scroll growth).
	ScrollBox->SetScrollOffset(PreviousScrollOffset);
}

void SWorldBLDAssetGrid::ScrollToTop()
{
	if (ScrollBox.IsValid())
	{
		ScrollBox->SetScrollOffset(0.0f);
	}
}

void SWorldBLDAssetGrid::SetAssets(const TArray<FWorldBLDAssetInfo>& InAssets)
{
	Assets = InAssets;
	bShowKits = false;
	bShowCollections = false;
	RefreshGrid();
}

void SWorldBLDAssetGrid::SetKits(const TArray<FWorldBLDKitInfo>& InKits)
{
	Kits = InKits;
	bShowKits = true;
	bShowCollections = false;
	RefreshGrid();
}

void SWorldBLDAssetGrid::SetCollections(const TArray<FWorldBLDCollectionListItem>& InCollections)
{
	Collections = InCollections;
	bShowCollections = true;
	bShowKits = false;
	RefreshGrid();
}

void SWorldBLDAssetGrid::SetFeaturedCollections(const TArray<FWorldBLDFeaturedCollection>& InCollections)
{
	FeaturedCollections = InCollections;
	RefreshGrid();
}

bool SWorldBLDAssetGrid::IsNearEnd(float CurrentScrollOffset, float RemainingThreshold) const
{
	if (!ScrollBox.IsValid())
	{
		return false;
	}

	const float EndOffset = ScrollBox->GetScrollOffsetOfEnd();
	if (EndOffset <= 0.0f)
	{
		// Not scrollable; do not treat this as "near end" to avoid accidental eager loads.
		return false;
	}

	const float Remaining = EndOffset - CurrentScrollOffset;
	return Remaining <= RemainingThreshold;
}