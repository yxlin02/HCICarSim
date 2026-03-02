// Copyright WorldBLD LLC. All Rights Reserved.

#include "AssetLibrary/WorldBLDAssetLibrarySlateBridge.h"

#include "AssetLibrary/SWorldBLDAssetLibraryWindow.h"

void UWorldBLDAssetLibrarySlateBridge::Init(TSharedRef<SWorldBLDAssetLibraryWindow> InWidget)
{
	Widget = InWidget;
}

void UWorldBLDAssetLibrarySlateBridge::HandleAssetsFetched(const FWorldBLDAssetListResponse& Response)
{
	if (TSharedPtr<SWorldBLDAssetLibraryWindow> Pinned = Widget.Pin())
	{
		Pinned->OnAssetsFetched(Response);
	}
}

void UWorldBLDAssetLibrarySlateBridge::HandleKitsFetched(const FWorldBLDKitListResponse& Response)
{
	(void)Response;
	// The AssetLibrary window no longer consumes kits. We keep this bridge method so any legacy bindings
	// don't break at runtime, but it intentionally does nothing.
}

void UWorldBLDAssetLibrarySlateBridge::HandleAPIError(const FString& ErrorMessage)
{
	if (TSharedPtr<SWorldBLDAssetLibraryWindow> Pinned = Widget.Pin())
	{
		Pinned->OnAPIError(ErrorMessage);
	}
}

void UWorldBLDAssetLibrarySlateBridge::HandleAssetStatusChanged(const FString& AssetId, EWorldBLDAssetStatus NewStatus)
{
	(void)AssetId;
	(void)NewStatus;

	if (TSharedPtr<SWorldBLDAssetLibraryWindow> Pinned = Widget.Pin())
	{
		Pinned->RefreshAssetGrid();
	}
}

void UWorldBLDAssetLibrarySlateBridge::HandleAssetDetailsFetched(const FWorldBLDAssetInfo& AssetInfo)
{
	if (TSharedPtr<SWorldBLDAssetLibraryWindow> Pinned = Widget.Pin())
	{
		Pinned->OnAssetDetailsFetched(AssetInfo);
	}
}

void UWorldBLDAssetLibrarySlateBridge::HandleKitDetailsFetched(const FWorldBLDKitInfo& KitInfo)
{
	(void)KitInfo;
	// The AssetLibrary window no longer consumes kits. We keep this bridge method so any legacy bindings
	// don't break at runtime, but it intentionally does nothing.
}

void UWorldBLDAssetLibrarySlateBridge::HandlePurchaseComplete(bool bSuccess, int32 RemainingCredits)
{
	if (TSharedPtr<SWorldBLDAssetLibraryWindow> Pinned = Widget.Pin())
	{
		Pinned->OnPurchaseComplete(bSuccess, RemainingCredits);
	}
}

void UWorldBLDAssetLibrarySlateBridge::HandleAssetCollectionNamesFetched(const FWorldBLDAssetCollectionNamesResponse& Response)
{
	if (TSharedPtr<SWorldBLDAssetLibraryWindow> Pinned = Widget.Pin())
	{
		Pinned->OnAssetCollectionNamesFetched(Response);
	}
}

void UWorldBLDAssetLibrarySlateBridge::HandleImportComplete(const FString& AssetId, bool bSuccess)
{
	if (TSharedPtr<SWorldBLDAssetLibraryWindow> Pinned = Widget.Pin())
	{
		Pinned->OnImportComplete(AssetId, bSuccess);
	}
}

void UWorldBLDAssetLibrarySlateBridge::HandleSubscriptionStatusChanged(bool bIsKnown, bool bHasActiveSubscription)
{
	if (TSharedPtr<SWorldBLDAssetLibraryWindow> Pinned = Widget.Pin())
	{
		Pinned->OnSubscriptionStatusChanged(bIsKnown, bHasActiveSubscription);
	}
}

void UWorldBLDAssetLibrarySlateBridge::HandleLicensesUpdated(const TArray<FString>& AuthorizedTools)
{
	(void)AuthorizedTools;

	// We only need to invalidate the Slate widget so bound text/visibility lambdas are re-evaluated.
	if (TSharedPtr<SWorldBLDAssetLibraryWindow> Pinned = Widget.Pin())
	{
		Pinned->Invalidate(EInvalidateWidgetReason::Paint);
	}
}