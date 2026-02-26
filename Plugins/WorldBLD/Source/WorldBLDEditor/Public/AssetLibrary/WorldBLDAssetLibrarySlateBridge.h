// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetLibrary/WorldBLDAssetLibraryTypes.h"

#include "WorldBLDAssetLibrarySlateBridge.generated.h"

class SWorldBLDAssetLibraryWindow;

UCLASS()
class WORLDBLDEDITOR_API UWorldBLDAssetLibrarySlateBridge : public UObject
{
	GENERATED_BODY()

public:
	void Init(TSharedRef<SWorldBLDAssetLibraryWindow> InWidget);

	UFUNCTION()
	void HandleAssetsFetched(const FWorldBLDAssetListResponse& Response);

	UFUNCTION()
	void HandleKitsFetched(const FWorldBLDKitListResponse& Response);

	UFUNCTION()
	void HandleAPIError(const FString& ErrorMessage);

	UFUNCTION()
	void HandleAssetStatusChanged(const FString& AssetId, EWorldBLDAssetStatus NewStatus);

	UFUNCTION()
	void HandleAssetDetailsFetched(const FWorldBLDAssetInfo& AssetInfo);

	UFUNCTION()
	void HandleKitDetailsFetched(const FWorldBLDKitInfo& KitInfo);

	UFUNCTION()
	void HandlePurchaseComplete(bool bSuccess, int32 RemainingCredits);

	UFUNCTION()
	void HandleAssetCollectionNamesFetched(const FWorldBLDAssetCollectionNamesResponse& Response);

	UFUNCTION()
	void HandleImportComplete(const FString& AssetId, bool bSuccess);

	UFUNCTION()
	void HandleSubscriptionStatusChanged(bool bIsKnown, bool bHasActiveSubscription);

	UFUNCTION()
	void HandleLicensesUpdated(const TArray<FString>& AuthorizedTools);

private:
	TWeakPtr<SWorldBLDAssetLibraryWindow> Widget;
};
