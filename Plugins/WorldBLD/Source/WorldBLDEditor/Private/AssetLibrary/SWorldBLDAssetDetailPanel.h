// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/CurveSequence.h"
#include "Misc/CoreDelegates.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBox.h"

#include "AssetLibrary/WorldBLDAssetLibraryTypes.h"

class SScrollBox;
class SWorldBLDCoverImage;
struct FSlateDynamicImageBrush;
class UWorldBLDAssetLibrarySubsystem;
class UWorldBLDAssetDownloadManager;
class UWorldBLDAuthSubsystem;

DECLARE_DELEGATE(FOnWorldBLDDetailPanelCloseRequested);

/**
 * Expandable, animated detail panel for the WorldBLD Asset Library.
 *
 * Implementation notes (high level):
 * - The panel is always present in the widget tree, but its HeightOverride is animated from 0 -> TargetHeight.
 * - Selection updates (ShowAsset/ShowKit) populate text/tag widgets immediately from list data, then optionally
 *   trigger subsystem Fetch*Details() to hydrate missing fields (preview images, description, tags, etc.).
 * - Actions (purchase/download/import) are driven off of download-manager status + auth credits.
 */
class SWorldBLDAssetDetailPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDAssetDetailPanel) {}
		SLATE_ARGUMENT(bool, bIsKit)
		SLATE_ARGUMENT(FWorldBLDAssetInfo, AssetInfo)
		SLATE_ARGUMENT(FWorldBLDKitInfo, KitInfo)
		SLATE_EVENT(FOnWorldBLDDetailPanelCloseRequested, OnCloseRequested)
	SLATE_END_ARGS()

	virtual ~SWorldBLDAssetDetailPanel() override;

	void Construct(const FArguments& InArgs);

	/** Called by the owning window so the panel can expand to a predictable fraction of the available space. */
	void SetHostHeight(float InHostHeight);

	void ShowAsset(const FWorldBLDAssetInfo& Asset);
	void ShowKit(const FWorldBLDKitInfo& Kit);
	void ShowFeaturedContent(const FWorldBLDFeaturedCollection& Collection);

	void Expand();
	void Collapse();

	bool IsExpanded() const { return bIsExpanded; }

	/** Called by the grid/window on scroll; collapses once the scroll offset passes a threshold. */
	void CheckScrollPosition(float ScrollOffset);

	/** Forwarded from subsystem. Used to refresh current panel contents after async API calls. */
	void HandleAssetDetailsFetched(const FWorldBLDAssetInfo& Asset);
	void HandleKitDetailsFetched(const FWorldBLDKitInfo& Kit);
	void HandlePurchaseComplete(bool bSuccess, int32 RemainingCredits);
	void HandleImportComplete(const FString& AssetId, bool bSuccess);

private:
	/** Periodically refreshes credits from the server (throttled) while the panel is open. */
	EActiveTimerReturnType HandleCreditsRefreshTick(double CurrentTime, float DeltaTime);

	/** Refresh credits from the same endpoint used in Construct() (ValidateSession). */
	void RequestCreditsRefreshFromServer(bool bForce);

	/** If the user topped up credits in a browser, refresh on returning focus to the editor. */
	void HandleApplicationHasReactivated();

	FReply HandlePurchaseClicked();
	FReply HandleDownloadClicked();
	FReply HandleImportClicked();
	FReply HandleViewAssetClicked();
	FReply HandlePrevImage();
	FReply HandleNextImage();

	void HandleUninstallAssetClicked();

	EVisibility GetPreviewNavigationVisibility() const;
	int32 GetSelectedPreviewImageUrlCount() const;

	EVisibility GetPurchaseButtonVisibility() const;
	EVisibility GetDownloadButtonVisibility() const;
	EVisibility GetImportButtonVisibility() const;
	EVisibility GetUninstallAssetVisibility() const;
	bool CanUninstallAsset() const;
	EVisibility GetDownloadProgressVisibility() const;
	TOptional<float> GetDownloadProgressValue() const;

	FText GetTitleText() const;
	FText GetDescriptionText() const;
	FText GetPriceText() const;
	FText GetPurchaseButtonText() const;
	FText GetUserCreditsText() const;
	FSlateColor GetStatusColor() const;
	FText GetStatusText() const;
	EVisibility GetStatusTextVisibility() const;
	EVisibility GetViewAssetButtonVisibility() const;
	FText GetDependencyInfoText() const;
	EVisibility GetDependencyInfoVisibility() const;

	const FSlateBrush* GetCurrentPreviewBrush() const;
	FOptionalSize GetAnimatedHeight() const;
	bool HasSufficientCredits() const;
	bool HasActiveSubscription() const;
	int32 GetCurrentPriceCredits() const;
	FString GetCurrentAssetId() const;

	void BeginLoadPreviewImages(const TArray<FString>& Urls);
	void SetPreviewIndex(int32 NewIndex);
	void RefreshDependencyCache();

	/** Returns true if the currently selected asset meets tool/plugin requirements (missing tool or outdated tool disables actions). */
	bool AreToolRequirementsSatisfied();

	/** Re-evaluates tool requirements for the current asset and optionally shows a modal dialog when unmet (one-shot per asset/reason). */
	void EvaluateToolRequirements(bool bShowDialogIfUnmet);

private:
	TWeakObjectPtr<UWorldBLDAssetLibrarySubsystem> AssetLibrarySubsystem;
	TWeakObjectPtr<UWorldBLDAssetDownloadManager> DownloadManager;
	TWeakObjectPtr<UWorldBLDAuthSubsystem> AuthSubsystem;

	FOnWorldBLDDetailPanelCloseRequested OnCloseRequested;

	FCurveSequence ExpandCollapseAnimation;
	FCurveHandle ExpandCurve;

	TSharedPtr<SScrollBox> PreviewImagesScrollBox;
	TArray<TSharedPtr<FSlateDynamicImageBrush>> PreviewImageBrushes;
	int32 CurrentPreviewIndex = 0;
	int32 PreviewImagesLoadGeneration = 0;

	TSharedPtr<SWorldBLDCoverImage> PreviewImageWidget;

	bool bIsExpanded = false;
	float TargetHeightFraction = 0.5f;
	float CachedHostHeight = 0.0f;

	bool bShowingFeatured = false;

	bool bIsKit = false;
	FWorldBLDAssetInfo CurrentAsset;
	FWorldBLDKitInfo CurrentKit;
	FWorldBLDFeaturedCollection CurrentFeaturedCollection;

	// Cached dependency check results to avoid checking on every UI tick
	mutable bool bDependencyCacheValid = false;
	mutable int32 CachedMissingDepsCount = 0;
	mutable int32 CachedExistingDepsCount = 0;

	// Tool requirement gating (requiredTools -> installed plugins & versions)
	bool bToolRequirementsSatisfied = true;
	FString LastToolRequirementDialogAssetId;
	FString LastToolRequirementDialogReason; // "missing" / "outdated"

	// Credits refresh
	TSharedPtr<FActiveTimerHandle> CreditsRefreshTimerHandle;
	FDelegateHandle ApplicationReactivatedHandle;
	double LastCreditsRefreshRequestTimeSeconds = -1.0;
	bool bPendingCreditsRefreshOnAppFocus = false;
	static constexpr float CreditsRefreshIntervalSeconds = 15.0f;
	static constexpr float CreditsRefreshMinGapSeconds = 2.0f;
};
