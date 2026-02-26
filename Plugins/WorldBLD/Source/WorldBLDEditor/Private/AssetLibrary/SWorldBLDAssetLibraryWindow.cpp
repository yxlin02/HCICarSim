// Copyright WorldBLD LLC. All Rights Reserved.

#include "AssetLibrary/SWorldBLDAssetLibraryWindow.h"

#include "AssetLibrary/SWorldBLDAssetLibrarySearchBar.h"
#include "AssetLibrary/SWorldBLDCategoryTree.h"
#include "AssetLibrary/WorldBLDAssetLibrarySlateBridge.h"
#include "AssetLibrary/SWorldBLDAssetDetailPanel.h"

#include "AssetLibrary/WorldBLDAssetLibrarySubsystem.h"
#include "AssetLibrary/WorldBLDAssetDownloadManager.h"
#include "AssetLibrary/WorldBLDAssetLibraryWindow.h"
#include "Authorization/WorldBLDLoginWindow.h"
#include "Authorization/WorldBLDAuthSubsystem.h"
#include "ContextMenu/WorldBLDContextMenuStyle.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/SNullWidget.h"
#include "Styling/AppStyle.h"

namespace WorldBLDAssetLibraryWindow_Constants
{
	static constexpr int32 AssetsPerPage = 50;
	static constexpr int32 InitialVisibleCount = 60;
	static constexpr int32 VisibleGrowStep = 40;
	static constexpr int32 MaxAutoFetchPagesPerFilterChange = 8;
	static constexpr float InfiniteScrollRemainingThreshold = 600.0f;
}

void SWorldBLDAssetLibraryWindow::Construct(const FArguments& InArgs)
{
	(void)InArgs;

	AssetLibrarySubsystem = GEditor ? GEditor->GetEditorSubsystem<UWorldBLDAssetLibrarySubsystem>() : nullptr;
	DownloadManager = GEditor ? GEditor->GetEditorSubsystem<UWorldBLDAssetDownloadManager>() : nullptr;
	AuthSubsystem = GEditor ? GEditor->GetEditorSubsystem<UWorldBLDAuthSubsystem>() : nullptr;

	// Validate session on startup
	if (AuthSubsystem.IsValid() && UWorldBLDAuthSubsystem::IsLoggedIn())
	{
		AuthSubsystem->ValidateSession();
	}

	Bridge = TStrongObjectPtr<UWorldBLDAssetLibrarySlateBridge>(NewObject<UWorldBLDAssetLibrarySlateBridge>(GetTransientPackage()));
	Bridge->Init(SharedThis(this));

	if (AssetLibrarySubsystem.IsValid())
	{
		AssetLibrarySubsystem->OnAssetsFetched.AddDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleAssetsFetched);
		AssetLibrarySubsystem->OnAssetDetailsFetched.AddDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleAssetDetailsFetched);
		AssetLibrarySubsystem->OnPurchaseComplete.AddDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandlePurchaseComplete);
		AssetLibrarySubsystem->OnAssetCollectionNamesFetched.AddDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleAssetCollectionNamesFetched);
		AssetLibrarySubsystem->OnAPIError.AddDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleAPIError);
	}

	if (DownloadManager.IsValid())
	{
		DownloadManager->OnAssetStatusChanged.AddDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleAssetStatusChanged);
		DownloadManager->OnImportComplete.AddDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleImportComplete);
		DownloadManager->OnSubscriptionStatusChanged.AddDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleSubscriptionStatusChanged);
	}

	if (AuthSubsystem.IsValid())
	{
		AuthSubsystem->OnLicensesUpdated.AddDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleLicensesUpdated);
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(SearchBar, SWorldBLDAssetLibrarySearchBar)
					.OnSearchTextChanged(this, &SWorldBLDAssetLibraryWindow::HandleSearchTextChanged)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(10.0f, 0.0f, 12.0f, 0.0f))
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this, &SWorldBLDAssetLibraryWindow::HandleFreeFilterChanged)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Free Only")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(10.0f, 0.0f, 12.0f, 0.0f))
				[
					SAssignNew(LoginLogoutButton, SButton)
					.ButtonStyle(&WorldBLDContextMenuStyle::GetNeutralButtonStyle())
					.Text(this, &SWorldBLDAssetLibraryWindow::GetLoginButtonText)
					.OnClicked(this, &SWorldBLDAssetLibraryWindow::HandleLoginLogoutClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(10.0f, 0.0f, 12.0f, 0.0f))
				[
					SAssignNew(CreditsOrPlanButton, SButton)
					.ButtonStyle(&WorldBLDContextMenuStyle::GetHighlightedButtonStyle())
					.Text(this, &SWorldBLDAssetLibraryWindow::GetCreditsOrPlanButtonText)
					.IsEnabled_Lambda([]()
					{
						return UWorldBLDAuthSubsystem::IsLoggedIn();
					})
					.OnClicked(this, &SWorldBLDAssetLibraryWindow::HandleCreditsOrPlanClicked)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SSplitter)
				+ SSplitter::Slot()
				.Value(0.25f)
				[
					SAssignNew(CategoryTree, SWorldBLDCategoryTree)
					.OnCategorySelected(this, &SWorldBLDAssetLibraryWindow::HandleCategorySelected)
				]
				+ SSplitter::Slot()
				.Value(0.75f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
					.Padding(FMargin(12.0f, 10.0f))
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SAssignNew(StatusText, STextBlock)
								.Text(FText::FromString(TEXT("Loading...")))
								.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f))
								.Visibility_Lambda([this]()
								{
									return (DetailPanel.IsValid() && DetailPanel->IsExpanded()) ? EVisibility::Collapsed : EVisibility::Visible;
								})
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SAssignNew(DetailPanel, SWorldBLDAssetDetailPanel)
								.OnCloseRequested(FOnWorldBLDDetailPanelCloseRequested::CreateLambda([this]()
								{
									// No-op for now; collapse behavior is handled within the panel.
								}))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(FMargin(0.0f, 6.0f, 0.0f, 6.0f))
							[
								SAssignNew(ErrorText, STextBlock)
								.Text(FText::GetEmpty())
								.ColorAndOpacity(FLinearColor(1.0f, 0.25f, 0.25f, 1.0f))
							]
							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									SAssignNew(AssetGrid, SWorldBLDAssetGrid)
									.OnAssetClicked(this, &SWorldBLDAssetLibraryWindow::HandleAssetTileClicked)
									.OnCollectionClicked(this, &SWorldBLDAssetLibraryWindow::HandleCollectionTileClicked)
									.OnUserScrolled(this, &SWorldBLDAssetLibraryWindow::HandleGridUserScrolled)
								]
								+ SOverlay::Slot()
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Visibility(this, &SWorldBLDAssetLibraryWindow::GetNoResultsVisibility)
									.Text(FText::FromString(TEXT("Nothing found, please check your filter.")))
									.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
								]
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNullWidget::NullWidget
							]
						]
						+ SOverlay::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
							.Padding(FMargin(16.0f, 12.0f))
							.Visibility(this, &SWorldBLDAssetLibraryWindow::GetLoadingVisibility)
							[
								SNew(SThrobber)
							]
						]
					]
				]
			]
		]
	];

	ResetInfiniteScroll();
	RefreshAssetList();
}

FText SWorldBLDAssetLibraryWindow::GetCreditsOrPlanButtonText() const
{
	const bool bHasAnyRelevantToolLicense =
		AuthSubsystem.IsValid()
		&& (AuthSubsystem->HasLicenseForTool(TEXT("RoadBLD")) || AuthSubsystem->HasLicenseForTool(TEXT("CityBLD")));

	return bHasAnyRelevantToolLicense
		? FText::FromString(TEXT("Get Credits"))
		: FText::FromString(TEXT("Get Plan"));
}

FReply SWorldBLDAssetLibraryWindow::HandleCreditsOrPlanClicked()
{
	static const TCHAR* PurchaseCreditsUrl = TEXT("https://worldbld.com/purchase-credits");
	static const TCHAR* PlansUrl = TEXT("https://www.worldbld.com/plans");

	const bool bHasAnyRelevantToolLicense =
		AuthSubsystem.IsValid()
		&& (AuthSubsystem->HasLicenseForTool(TEXT("RoadBLD")) || AuthSubsystem->HasLicenseForTool(TEXT("CityBLD")));

	FPlatformProcess::LaunchURL(bHasAnyRelevantToolLicense ? PurchaseCreditsUrl : PlansUrl, nullptr, nullptr);
	return FReply::Handled();
}

SWorldBLDAssetLibraryWindow::~SWorldBLDAssetLibraryWindow()
{
	if (AssetLibrarySubsystem.IsValid() && Bridge.IsValid())
	{
		AssetLibrarySubsystem->OnAssetsFetched.RemoveDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleAssetsFetched);
		AssetLibrarySubsystem->OnAssetDetailsFetched.RemoveDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleAssetDetailsFetched);
		AssetLibrarySubsystem->OnPurchaseComplete.RemoveDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandlePurchaseComplete);
		AssetLibrarySubsystem->OnAssetCollectionNamesFetched.RemoveDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleAssetCollectionNamesFetched);
		AssetLibrarySubsystem->OnAPIError.RemoveDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleAPIError);
	}

	if (DownloadManager.IsValid() && Bridge.IsValid())
	{
		DownloadManager->OnAssetStatusChanged.RemoveDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleAssetStatusChanged);
		DownloadManager->OnImportComplete.RemoveDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleImportComplete);
		DownloadManager->OnSubscriptionStatusChanged.RemoveDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleSubscriptionStatusChanged);
	}

	if (AuthSubsystem.IsValid() && Bridge.IsValid())
	{
		AuthSubsystem->OnLicensesUpdated.RemoveDynamic(Bridge.Get(), &UWorldBLDAssetLibrarySlateBridge::HandleLicensesUpdated);
	}
}

void SWorldBLDAssetLibraryWindow::OnSubscriptionStatusChanged(bool bIsKnown, bool bHasActiveSubscription)
{
	(void)bIsKnown;
	(void)bHasActiveSubscription;

	// Ensure bound Slate attributes (button text, etc.) are re-evaluated after async ownership refresh.
	Invalidate(EInvalidateWidgetReason::Paint);
	if (CreditsOrPlanButton.IsValid())
	{
		CreditsOrPlanButton->Invalidate(EInvalidateWidgetReason::Paint);
	}
	if (DetailPanel.IsValid())
	{
		DetailPanel->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

FReply SWorldBLDAssetLibraryWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CollapseDetailPanel();
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SWorldBLDAssetLibraryWindow::OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	SCompoundWidget::OnFocusChanging(PreviousFocusPath, NewWidgetPath, InFocusEvent);

	// UX: interacting with the search bar should get the details drawer out of the way.
	if (DetailPanel.IsValid() && DetailPanel->IsExpanded() && IsNewFocusInSearchBar(NewWidgetPath))
	{
		CollapseDetailPanel();
	}
}

void SWorldBLDAssetLibraryWindow::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// The right panel height changes as the window/splitter changes.
	// We cache it to drive the detail panel's animated HeightOverride.
	CachedRightPanelHeight = AllottedGeometry.GetLocalSize().Y;
	if (DetailPanel.IsValid())
	{
		DetailPanel->SetHostHeight(CachedRightPanelHeight);
	}
}

void SWorldBLDAssetLibraryWindow::CollapseDetailPanel()
{
	if (DetailPanel.IsValid() && DetailPanel->IsExpanded())
	{
		DetailPanel->Collapse();
	}
}

bool SWorldBLDAssetLibraryWindow::IsNewFocusInSearchBar(const FWidgetPath& NewWidgetPath) const
{
	if (!SearchBar.IsValid())
	{
		return false;
	}

	return NewWidgetPath.ContainsWidget(SearchBar.Get());
}

void SWorldBLDAssetLibraryWindow::HandleCategorySelected(TSharedPtr<FWorldBLDCategoryNode> InCategory)
{
	CurrentCategory = InCategory;
	CollapseDetailPanel();

	const bool bShouldClearSearch =
		!bPreserveSearchOnNextCategorySelection
		&& CurrentCategory.IsValid()
		&& (CurrentCategory->CategoryPath == TEXT("Collections") || CurrentCategory->CategoryPath == TEXT("Assets"));

	// Always reset after one use.
	bPreserveSearchOnNextCategorySelection = false;

	if (bShouldClearSearch)
	{
		CurrentSearchQuery.Reset();
		if (SearchBar.IsValid())
		{
			SearchBar->SetSearchText(FText::GetEmpty());
		}
	}

	ResetInfiniteScroll();
	ApplyFilters();
	RebuildRightPanel();
}

void SWorldBLDAssetLibraryWindow::OnAssetDetailsFetched(const FWorldBLDAssetInfo& AssetInfo)
{
	if (DetailPanel.IsValid())
	{
		DetailPanel->HandleAssetDetailsFetched(AssetInfo);
	}
}

void SWorldBLDAssetLibraryWindow::OnPurchaseComplete(bool bSuccess, int32 RemainingCredits)
{
	if (DetailPanel.IsValid())
	{
		DetailPanel->HandlePurchaseComplete(bSuccess, RemainingCredits);
	}
}

void SWorldBLDAssetLibraryWindow::OnImportComplete(const FString& AssetId, bool bSuccess)
{
	if (DetailPanel.IsValid())
	{
		DetailPanel->HandleImportComplete(AssetId, bSuccess);
	}
}

void SWorldBLDAssetLibraryWindow::HandleSearchTextChanged(const FText& InText)
{
	CollapseDetailPanel();

	CurrentSearchQuery = InText.ToString();
	ResetInfiniteScroll();
	ApplyFilters();
	RebuildRightPanel();
}

void SWorldBLDAssetLibraryWindow::HandleFreeFilterChanged(ECheckBoxState NewState)
{
	bShowOnlyFree = (NewState == ECheckBoxState::Checked);
	ResetInfiniteScroll();
	ApplyFilters();
	RebuildRightPanel();
}

FReply SWorldBLDAssetLibraryWindow::HandleAssetTileClicked(bool bIsKit, const FWorldBLDAssetInfo& AssetInfo, const FWorldBLDKitInfo& KitInfo)
{
	if (DetailPanel.IsValid())
	{
		if (bIsKit)
		{
			DetailPanel->ShowKit(KitInfo);
		}
		else
		{
			DetailPanel->ShowAsset(AssetInfo);
		}

		// Avoid replaying the open animation when switching selection while already expanded.
		if (!DetailPanel->IsExpanded())
		{
			DetailPanel->Expand();
		}
	}
	return FReply::Handled();
}

FReply SWorldBLDAssetLibraryWindow::HandleCollectionTileClicked(const FWorldBLDCollectionListItem& Collection)
{
	const FString CollectionName = Collection.CollectionName.TrimStartAndEnd();
	if (CollectionName.IsEmpty())
	{
		return FReply::Handled();
	}

	CurrentSearchQuery = CollectionName;
	if (SearchBar.IsValid())
	{
		SearchBar->SetSearchText(FText::FromString(CollectionName));
	}

	if (CategoryTree.IsValid())
	{
		bPreserveSearchOnNextCategorySelection = true;
		CategoryTree->SelectCategoryByPath(TEXT("Assets"));
	}
	else
	{
		ApplyFilters();
		RebuildRightPanel();
	}

	return FReply::Handled();
}

void SWorldBLDAssetLibraryWindow::HandleGridUserScrolled(float ScrollOffset)
{
	if (DetailPanel.IsValid())
	{
		DetailPanel->CheckScrollPosition(ScrollOffset);
	}

	if (bIsLoading || !AssetGrid.IsValid())
	{
		return;
	}

	if (!AssetGrid->IsNearEnd(ScrollOffset, WorldBLDAssetLibraryWindow_Constants::InfiniteScrollRemainingThreshold))
	{
		return;
	}

	// Prefer growing the visible window (cheap) before asking the backend for more pages.
	if (GrowVisibleItemWindow())
	{
		RebuildRightPanel();
		return;
	}

	RequestNextAssetsPage();
}

void SWorldBLDAssetLibraryWindow::RefreshAssetGrid()
{
	RebuildRightPanel();
}

void SWorldBLDAssetLibraryWindow::RefreshAssetList()
{
	if (!AssetLibrarySubsystem.IsValid())
	{
		bIsLoading = false;
		if (ErrorText.IsValid())
		{
			ErrorText->SetText(FText::FromString(TEXT("AssetLibrary subsystem unavailable")));
		}
		return;
	}

	// Reset remote pagination + local infinite-scroll window.
	AllAssets.Reset();
	FilteredAssets.Reset();
	FilteredCollections.Reset();
	AllCollectionNames.Reset();

	AssetsCurrentPage = 1;
	AssetsLastPage = 1;
	RequestedAssetsPage = 1;

	ResetInfiniteScroll();
	AssetLibrarySubsystem->FetchAssetCollectionNames();
	RequestAssetsPage(1);
}

void SWorldBLDAssetLibraryWindow::RequestAssetsPage(int32 Page)
{
	if (!AssetLibrarySubsystem.IsValid() || bIsLoading)
	{
		return;
	}

	RequestedAssetsPage = FMath::Max(1, Page);
	bIsLoading = true;

	AssetLibrarySubsystem->FetchAssets(RequestedAssetsPage, WorldBLDAssetLibraryWindow_Constants::AssetsPerPage, TEXT(""));
}

void SWorldBLDAssetLibraryWindow::RequestNextAssetsPage()
{
	if (bIsLoading)
	{
		return;
	}

	if (AssetsCurrentPage >= AssetsLastPage)
	{
		return;
	}

	RequestAssetsPage(AssetsCurrentPage + 1);
}

void SWorldBLDAssetLibraryWindow::OnAssetsFetched(const FWorldBLDAssetListResponse& Response)
{
	AssetsCurrentPage = FMath::Max(1, Response.CurrentPage);
	AssetsLastPage = FMath::Max(1, Response.LastPage);

	if (AssetsCurrentPage <= 1)
	{
		AllAssets = Response.Data;
	}
	else
	{
		// Append new assets, avoiding duplicates by ID.
		TSet<FString> ExistingIds;
		ExistingIds.Reserve(AllAssets.Num());
		for (const FWorldBLDAssetInfo& Asset : AllAssets)
		{
			ExistingIds.Add(Asset.ID);
		}

		for (const FWorldBLDAssetInfo& Asset : Response.Data)
		{
			if (!ExistingIds.Contains(Asset.ID))
			{
				AllAssets.Add(Asset);
			}
		}
	}

	// Reconcile API assets against content already on disk in this project so that
	// assets committed via source control (but never downloaded by this user) show as Imported.
	if (DownloadManager.IsValid())
	{
		DownloadManager->ReconcileImportedAssets(Response.Data);
	}

	bIsLoading = false;
	ApplyFilters();
	RebuildRightPanel();
}

void SWorldBLDAssetLibraryWindow::OnAssetCollectionNamesFetched(const FWorldBLDAssetCollectionNamesResponse& Response)
{
	if (!Response.bSuccess)
	{
		return;
	}

	AllCollectionNames = Response.CollectionNames;
	AllCollectionNames.Sort();

	ApplyFilters();
	RebuildRightPanel();
}

void SWorldBLDAssetLibraryWindow::OnAPIError(const FString& ErrorMessage)
{
	bIsLoading = false;
	if (ErrorText.IsValid())
	{
		ErrorText->SetText(FText::FromString(ErrorMessage));
	}
	RebuildRightPanel();
}

void SWorldBLDAssetLibraryWindow::ApplyFilters()
{
	FilteredAssets.Reset();
	FilteredCollections.Reset();

	const FString Query = CurrentSearchQuery.TrimStartAndEnd();

	const bool bWantsCollections = CurrentCategory.IsValid() && CurrentCategory->CategoryPath == TEXT("Collections");
	if (bWantsCollections)
	{
		TMap<FString, FWorldBLDCollectionListItem> CollectionMap;

		// Seed collections from the dedicated endpoint so we can show every unique collection,
		// even if it isn't yet present in the currently loaded asset pages.
		for (const FString& RawName : AllCollectionNames)
		{
			const FString CollectionName = RawName.TrimStartAndEnd();
			if (CollectionName.IsEmpty())
			{
				continue;
			}

			if (!Query.IsEmpty() && !CollectionName.Contains(Query, ESearchCase::IgnoreCase))
			{
				continue;
			}

			FWorldBLDCollectionListItem Item;
			Item.CollectionName = CollectionName;
			Item.AssetCount = 0;
			CollectionMap.Add(CollectionName, MoveTemp(Item));
		}

		for (const FWorldBLDAssetInfo& Asset : AllAssets)
		{
			if (bShowOnlyFree && !Asset.bIsFree)
			{
				continue;
			}

			// One asset can belong to multiple collections. Create/update a tile for each.
			TSet<FString> AssetCollections;
			for (const FString& Name : Asset.CollectionNames)
			{
				const FString Trimmed = Name.TrimStartAndEnd();
				if (!Trimmed.IsEmpty())
				{
					AssetCollections.Add(Trimmed);
				}
			}

			// Backward compatible fallback: if the asset has no multi-collection list, use the manifest's single field.
			if (AssetCollections.IsEmpty())
			{
				const FString LegacyCollectionName = Asset.Manifest.CollectionName.TrimStartAndEnd();
				if (!LegacyCollectionName.IsEmpty())
				{
					AssetCollections.Add(LegacyCollectionName);
				}
			}

			for (const FString& CollectionName : AssetCollections)
			{
				if (!Query.IsEmpty() && !CollectionName.Contains(Query, ESearchCase::IgnoreCase))
				{
					continue;
				}

				if (FWorldBLDCollectionListItem* Existing = CollectionMap.Find(CollectionName))
				{
					++Existing->AssetCount;
					if (Existing->ThumbnailURL.IsEmpty() && !Asset.ThumbnailURL.IsEmpty())
					{
						Existing->ThumbnailURL = Asset.ThumbnailURL;
					}
				}
				else
				{
					FWorldBLDCollectionListItem Item;
					Item.CollectionName = CollectionName;
					Item.ThumbnailURL = Asset.ThumbnailURL;
					Item.AssetCount = 1;
					CollectionMap.Add(CollectionName, MoveTemp(Item));
				}
			}
		}

		CollectionMap.GenerateValueArray(FilteredCollections);
		FilteredCollections.Sort([](const FWorldBLDCollectionListItem& A, const FWorldBLDCollectionListItem& B)
		{
			return A.CollectionName < B.CollectionName;
		});
	}
	else
	{
		auto AssetMatchesQuery = [](const FWorldBLDAssetInfo& Asset, const FString& InQuery) -> bool
		{
			if (InQuery.IsEmpty())
			{
				return true;
			}

			if (Asset.Name.Contains(InQuery, ESearchCase::IgnoreCase) || Asset.Description.Contains(InQuery, ESearchCase::IgnoreCase))
			{
				return true;
			}

			for (const FString& CollectionName : Asset.CollectionNames)
			{
				if (CollectionName.Contains(InQuery, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}

			// Backward compatible fallback: accept the legacy single collection name.
			if (Asset.Manifest.CollectionName.Contains(InQuery, ESearchCase::IgnoreCase))
			{
				return true;
			}

			for (const FString& Tag : Asset.Tags)
			{
				if (Tag.Contains(InQuery, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}

			return false;
		};

		for (const FWorldBLDAssetInfo& Asset : AllAssets)
		{
			if (CurrentCategory.IsValid() && !CurrentCategory->RequiredTags.IsEmpty() && !CurrentCategory->MatchesCategory(Asset))
			{
				continue;
			}

			if (!AssetMatchesQuery(Asset, Query))
			{
				continue;
			}

			FilteredAssets.Add(Asset);
		}
	}

	if (bShowOnlyFree)
	{
		FilteredAssets.RemoveAll([](const FWorldBLDAssetInfo& Asset) { return !Asset.bIsFree; });
	}
}

void SWorldBLDAssetLibraryWindow::ResetInfiniteScroll()
{
	VisibleAssetsCount = WorldBLDAssetLibraryWindow_Constants::InitialVisibleCount;
	VisibleCollectionsCount = WorldBLDAssetLibraryWindow_Constants::InitialVisibleCount;
	AutoFetchBudget = WorldBLDAssetLibraryWindow_Constants::MaxAutoFetchPagesPerFilterChange;

	if (AssetGrid.IsValid())
	{
		AssetGrid->ScrollToTop();
	}
}

bool SWorldBLDAssetLibraryWindow::HasActiveFilter() const
{
	const FString Query = CurrentSearchQuery.TrimStartAndEnd();
	return bShowOnlyFree || !Query.IsEmpty() || CurrentCategory.IsValid();
}

bool SWorldBLDAssetLibraryWindow::GrowVisibleItemWindow()
{
	const bool bWantsCollections = CurrentCategory.IsValid() && CurrentCategory->CategoryPath == TEXT("Collections");
	if (bWantsCollections)
	{
		const int32 Target = FMath::Min(FilteredCollections.Num(), VisibleCollectionsCount + WorldBLDAssetLibraryWindow_Constants::VisibleGrowStep);
		if (Target != VisibleCollectionsCount)
		{
			VisibleCollectionsCount = Target;
			return true;
		}
	}
	else
	{
		const int32 Target = FMath::Min(FilteredAssets.Num(), VisibleAssetsCount + WorldBLDAssetLibraryWindow_Constants::VisibleGrowStep);
		if (Target != VisibleAssetsCount)
		{
			VisibleAssetsCount = Target;
			return true;
		}
	}

	return false;
}

TArray<FWorldBLDAssetInfo> SWorldBLDAssetLibraryWindow::BuildVisibleAssets() const
{
	const int32 NumToShow = FMath::Clamp(VisibleAssetsCount, 0, FilteredAssets.Num());

	TArray<FWorldBLDAssetInfo> Result;
	Result.Reserve(NumToShow);
	for (int32 Index = 0; Index < NumToShow; ++Index)
	{
		Result.Add(FilteredAssets[Index]);
	}
	return Result;
}

TArray<FWorldBLDCollectionListItem> SWorldBLDAssetLibraryWindow::BuildVisibleCollections() const
{
	const int32 NumToShow = FMath::Clamp(VisibleCollectionsCount, 0, FilteredCollections.Num());

	TArray<FWorldBLDCollectionListItem> Result;
	Result.Reserve(NumToShow);
	for (int32 Index = 0; Index < NumToShow; ++Index)
	{
		Result.Add(FilteredCollections[Index]);
	}
	return Result;
}

void SWorldBLDAssetLibraryWindow::RebuildRightPanel()
{
	if (StatusText.IsValid())
	{
		const FString CategoryName = CurrentCategory.IsValid() ? CurrentCategory->CategoryName : TEXT("Home");
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("%s"), *CategoryName)));
	}

	if (AssetGrid.IsValid())
	{
		const bool bWantsCollections = CurrentCategory.IsValid() && CurrentCategory->CategoryPath == TEXT("Collections");
		if (bWantsCollections)
		{
			AssetGrid->SetCollections(BuildVisibleCollections());
		}
		else
		{
			AssetGrid->SetAssets(BuildVisibleAssets());
		}
	}

	// If the user has an active filter and we currently have no matching items, auto-fetch a few more pages.
	// This keeps search/category filters useful even when the matching items are not yet in the locally loaded pages.
	if (!bIsLoading && HasActiveFilter() && AutoFetchBudget > 0 && AssetsCurrentPage < AssetsLastPage)
	{
		const bool bWantsCollections = CurrentCategory.IsValid() && CurrentCategory->CategoryPath == TEXT("Collections");
		const int32 NumItems = bWantsCollections ? FilteredCollections.Num() : FilteredAssets.Num();
		if (NumItems == 0)
		{
			--AutoFetchBudget;
			RequestNextAssetsPage();
		}
	}
}

EVisibility SWorldBLDAssetLibraryWindow::GetLoadingVisibility() const
{
	return bIsLoading ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SWorldBLDAssetLibraryWindow::GetLoginButtonText() const
{
	if (!AuthSubsystem.IsValid() || !UWorldBLDAuthSubsystem::IsLoggedIn())
	{
		return FText::FromString(TEXT("Login"));
	}

	return FText::FromString(TEXT("Logout"));
}

FReply SWorldBLDAssetLibraryWindow::HandleLoginLogoutClicked()
{
	if (!AuthSubsystem.IsValid())
	{
		return FReply::Handled();
	}

	if (UWorldBLDAuthSubsystem::IsLoggedIn())
	{
		// Logout
		AuthSubsystem->Logout();

		// After logging out, close the Asset Library window automatically.
		FWorldBLDAssetLibraryWindow::CloseAssetLibrary();
		return FReply::Handled();
	}
	else
	{
		if (ErrorText.IsValid())
		{
			ErrorText->SetText(FText::GetEmpty());
		}

		FWorldBLDLoginWindow::OpenLoginDialog();
	}

	return FReply::Handled();
}

EVisibility SWorldBLDAssetLibraryWindow::GetNoResultsVisibility() const
{
	if (bIsLoading)
	{
		return EVisibility::Collapsed;
	}

	const bool bWantsCollections = CurrentCategory.IsValid() && CurrentCategory->CategoryPath == TEXT("Collections");
	const int32 NumItems = bWantsCollections ? FilteredCollections.Num() : FilteredAssets.Num();
	if (NumItems > 0)
	{
		return EVisibility::Collapsed;
	}

	const FString Query = CurrentSearchQuery.TrimStartAndEnd();
	const bool bHasActiveFilter = bShowOnlyFree || !Query.IsEmpty() || CurrentCategory.IsValid();

	return bHasActiveFilter ? EVisibility::Visible : EVisibility::Collapsed;
}
