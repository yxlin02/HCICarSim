// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "AssetLibrary/WorldBLDAssetLibraryTypes.h"
#include "WorldBLDAssetLibrarySubsystem.generated.h"

class UWorldBLDAuthSubsystem;

// Log category for asset library operations
DECLARE_LOG_CATEGORY_EXTERN(LogWorldBLDAssetLibrary, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAssetsFetched, const FWorldBLDAssetListResponse&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKitsFetched, const FWorldBLDKitListResponse&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAssetDetailsFetched, const FWorldBLDAssetInfo&, AssetInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKitDetailsFetched, const FWorldBLDKitInfo&, KitInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAssetCollectionNamesFetched, const FWorldBLDAssetCollectionNamesResponse&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPurchaseComplete, bool, bSuccess, int32, RemainingCredits);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAPIError, const FString&, ErrorMessage);

UCLASS()
class WORLDBLDEDITOR_API UWorldBLDAssetLibrarySubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void FetchAssets(int32 Page = 1, int32 PerPage = 20, const FString& Category = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void FetchKits(int32 Page = 1, int32 PerPage = 20);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void FetchAssetDetails(const FString& AssetID);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void FetchKitDetails(const FString& KitID);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void PurchaseAsset(const FString& AssetId);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void FetchFreeAssets();

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void FetchFreeKits();

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void RequestDownloadURL(const FString& AssetId, bool bIsKit);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void FetchAssetCollectionNames();

public:
	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|AssetLibrary")
	FOnAssetsFetched OnAssetsFetched;

	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|AssetLibrary")
	FOnKitsFetched OnKitsFetched;

	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|AssetLibrary")
	FOnAssetDetailsFetched OnAssetDetailsFetched;

	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|AssetLibrary")
	FOnKitDetailsFetched OnKitDetailsFetched;

	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|AssetLibrary")
	FOnPurchaseComplete OnPurchaseComplete;

	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|AssetLibrary")
	FOnAssetCollectionNamesFetched OnAssetCollectionNamesFetched;

	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|AssetLibrary")
	FOnAPIError OnAPIError;

private:
	void OnFetchAssetsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
	void OnFetchKitsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
	void OnFetchAssetDetailsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
	void OnFetchKitDetailsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
	void OnPurchaseResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
	void OnRequestDownloadURLResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
	void OnFetchAssetCollectionNamesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	FString GetAuthToken() const;
	FString ConvertRelativeURLToAbsolute(const FString& RelativeURL) const;

private:
	TObjectPtr<UWorldBLDAuthSubsystem> AuthSubsystem = nullptr;
	static constexpr const TCHAR* BaseURL = TEXT("https://worldbld.com/api");
};
