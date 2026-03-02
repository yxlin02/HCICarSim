// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SCheckBox.h"

#include "AssetLibrary/WorldBLDAssetLibraryTypes.h"
#include "AssetLibrary/WorldBLDAssetLibraryCategoryTypes.h"

#include "AssetLibrary/SWorldBLDAssetGrid.h"
#include "AssetLibrary/WorldBLDAssetLibrarySlateBridge.h"

class UWorldBLDAssetLibrarySubsystem;
class UWorldBLDAssetDownloadManager;
class UWorldBLDAuthSubsystem;
class SWorldBLDAssetDetailPanel;
class SWorldBLDAssetLibrarySearchBar;
class SWorldBLDCategoryTree;

class STextBlock;
class SButton;

class SWorldBLDAssetLibraryWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDAssetLibraryWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SWorldBLDAssetLibraryWindow() override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent) override;

	void RefreshAssetGrid();

	void OnAssetsFetched(const FWorldBLDAssetListResponse& Response);
	void OnAssetCollectionNamesFetched(const FWorldBLDAssetCollectionNamesResponse& Response);
	void OnAPIError(const FString& ErrorMessage);
	void OnAssetDetailsFetched(const FWorldBLDAssetInfo& AssetInfo);
	void OnPurchaseComplete(bool bSuccess, int32 RemainingCredits);
	void OnImportComplete(const FString& AssetId, bool bSuccess);
	void OnSubscriptionStatusChanged(bool bIsKnown, bool bHasActiveSubscription);

private:
	void CollapseDetailPanel();
	bool IsNewFocusInSearchBar(const FWidgetPath& NewWidgetPath) const;

	void HandleCategorySelected(TSharedPtr<FWorldBLDCategoryNode> InCategory);
	void HandleSearchTextChanged(const FText& InText);
	void HandleFreeFilterChanged(ECheckBoxState NewState);
	FReply HandleAssetTileClicked(bool bIsKit, const FWorldBLDAssetInfo& AssetInfo, const FWorldBLDKitInfo& KitInfo);
	FReply HandleCollectionTileClicked(const FWorldBLDCollectionListItem& Collection);
	void HandleGridUserScrolled(float ScrollOffset);

	void RefreshAssetList();
	void RequestAssetsPage(int32 Page);
	void RequestNextAssetsPage();
	void ResetInfiniteScroll();
	bool GrowVisibleItemWindow();
	bool HasActiveFilter() const;

	TArray<FWorldBLDAssetInfo> BuildVisibleAssets() const;
	TArray<FWorldBLDCollectionListItem> BuildVisibleCollections() const;
	void ApplyFilters();
	void RebuildRightPanel();
	EVisibility GetLoadingVisibility() const;

	FText GetLoginButtonText() const;
	FReply HandleLoginLogoutClicked();
	EVisibility GetNoResultsVisibility() const;

	FText GetCreditsOrPlanButtonText() const;
	FReply HandleCreditsOrPlanClicked();

private:
	TWeakObjectPtr<UWorldBLDAssetLibrarySubsystem> AssetLibrarySubsystem;
	TWeakObjectPtr<UWorldBLDAssetDownloadManager> DownloadManager;
	TWeakObjectPtr<UWorldBLDAuthSubsystem> AuthSubsystem;

	TStrongObjectPtr<UWorldBLDAssetLibrarySlateBridge> Bridge;

	TSharedPtr<SWorldBLDAssetLibrarySearchBar> SearchBar;
	TSharedPtr<SWorldBLDCategoryTree> CategoryTree;

	TSharedPtr<SWorldBLDAssetDetailPanel> DetailPanel;
	TSharedPtr<SWorldBLDAssetGrid> AssetGrid;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> ErrorText;
	TSharedPtr<SButton> LoginLogoutButton;
	TSharedPtr<SButton> CreditsOrPlanButton;

	float CachedRightPanelHeight = 0.0f;

	TSharedPtr<FWorldBLDCategoryNode> CurrentCategory;
	FString CurrentSearchQuery;

	TArray<FWorldBLDAssetInfo> AllAssets;
	TArray<FWorldBLDAssetInfo> FilteredAssets;
	TArray<FWorldBLDCollectionListItem> FilteredCollections;
	TArray<FString> AllCollectionNames;

	int32 AssetsCurrentPage = 1;
	int32 AssetsLastPage = 1;
	int32 RequestedAssetsPage = 1;

	int32 VisibleAssetsCount = 0;
	int32 VisibleCollectionsCount = 0;
	int32 AutoFetchBudget = 0;

	bool bShowOnlyFree = false;

	bool bIsLoading = false;

	// One-shot: when we programmatically select a category (ex: collection click selecting Assets),
	// do not clear the search box, even if the target is Assets/Collections.
	bool bPreserveSearchOnNextCategorySelection = false;
};
