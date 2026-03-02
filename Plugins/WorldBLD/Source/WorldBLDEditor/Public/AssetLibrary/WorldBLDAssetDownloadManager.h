#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "AssetLibrary/WorldBLDAssetLibraryTypes.h"
#include "Downloader/CurlDownloaderLibrary.h"

#include "WorldBLDAssetDownloadManager.generated.h"

class UCurlDownloaderLibrary;
class UWorldBLDAssetLibrarySubsystem;
class UWorldBLDAuthSubsystem;
class FWorldBLDAssetLibraryJobQueue;
class FWorldBLDAssetStateStore;
class FWorldBLDAssetCacheService;
class FWorldBLDAssetImportService;
class FWorldBLDAssetDownloadService;
class FWorldBLDAssetDependencyService;

// NOTE:
// UHT-generated code compiles this header in translation units that do not include the full service type
// definitions. Using custom deleters avoids `delete` on incomplete types inside TUniquePtr's inline destructor.
struct FWorldBLDAssetLibraryJobQueueDeleter
{
	void operator()(FWorldBLDAssetLibraryJobQueue* Ptr) const;
};

struct FWorldBLDAssetStateStoreDeleter
{
	void operator()(FWorldBLDAssetStateStore* Ptr) const;
};

struct FWorldBLDAssetCacheServiceDeleter
{
	void operator()(FWorldBLDAssetCacheService* Ptr) const;
};

struct FWorldBLDAssetImportServiceDeleter
{
	void operator()(FWorldBLDAssetImportService* Ptr) const;
};

struct FWorldBLDAssetDownloadServiceDeleter
{
	void operator()(FWorldBLDAssetDownloadService* Ptr) const;
};

struct FWorldBLDAssetDependencyServiceDeleter
{
	void operator()(FWorldBLDAssetDependencyService* Ptr) const;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAssetStatusChanged, const FString&, AssetId, EWorldBLDAssetStatus, NewStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnImportComplete, const FString&, AssetId, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSubscriptionStatusChanged, bool, bIsKnown, bool, bHasActiveSubscription);

UCLASS()
class WORLDBLDEDITOR_API UWorldBLDAssetDownloadCallbackProxy : public UObject
{
	GENERATED_BODY()

public:
	FString AssetId;
	TWeakObjectPtr<class UWorldBLDAssetDownloadManager> Manager;
	FDOnDownloadProgress UserOnProgress;
	FDOnComplete UserOnComplete;

	UFUNCTION()
	void HandleProgress(float Progress);

	UFUNCTION()
	void HandleComplete(bool bWasSuccessful);
};

UCLASS()
class WORLDBLDEDITOR_API UWorldBLDAssetDownloadManager : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	EWorldBLDAssetStatus GetAssetStatus(const FString& AssetId);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void DownloadAsset(const FString& AssetId, const FString& AssetName, const FString& DownloadUrl, bool bIsKit, FDOnDownloadProgress OnProgress, FDOnComplete OnComplete);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void CancelDownload(const FString& AssetId);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	bool ImportAsset(const FString& AssetId);

	/**
	 * Uninstall an imported/downloaded asset from this machine.
	 * - Deletes cached download files under the WorldBLD asset library cache directory.
	 * - Deletes imported assets under /Game/WorldBLD/Assets (and removes the corresponding Content folder).
	 *
	 * Note: This does not revoke ownership; it only removes local files.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	bool UninstallAsset(const FString& AssetId, bool bDeleteCache = true, bool bDeleteImportedContent = true);

	/**
	 * Returns the Content Browser folder (long package name) that an asset was imported into.
	 * Example: "/Game/WorldBLD/Assets/MyAsset_12345".
	 */
	bool GetImportedPackageRoot(const FString& AssetId, FString& OutImportedPackageRoot) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WorldBLD|AssetLibrary")
	float GetDownloadProgress(const FString& AssetId);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WorldBLD|AssetLibrary")
	bool IsDownloadInProgress(const FString& AssetId);

	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void RefreshOwnershipStatus();

	/**
	 * Returns true if the account currently has any active subscription (as reported by /auth/desktop/owned-apps).
	 * Notes:
	 * - Trial licenses should NOT be treated as an "active subscription" for Asset Library credit purchase UX.
	 * - When unknown (not yet fetched / request failed), this conservatively returns false.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WorldBLD|AssetLibrary")
	bool HasActiveSubscription() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WorldBLD|AssetLibrary")
	bool ValidateAssetStructure(const FString& AssetPath) const;

	/**
	 * Check which dependencies of an asset already exist locally by scanning for their unique IDs.
	 * @param AssetInfo The asset to check dependencies for
	 * @param OutMissingDeps Dependencies that don't exist locally
	 * @param OutExistingDeps Dependencies that exist locally (may have SHA1 mismatches)
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void CheckLocalDependencies(const FWorldBLDAssetInfo& AssetInfo,
		TArray<FWorldBLDAssetDependency>& OutMissingDeps,
		TArray<FWorldBLDAssetDependency>& OutExistingDeps);

	/**
	 * Download an asset with smart dependency resolution.
	 * Only downloads dependencies that don't exist locally.
	 * @param AssetInfo The asset to download (must contain Manifest)
	 * @param bCheckLocalFirst Whether to check for local dependencies before downloading
	 * @param OnProgress Progress callback for all downloads
	 * @param OnComplete Complete callback when everything is downloaded
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void DownloadAssetWithDependencies(const FWorldBLDAssetInfo& AssetInfo,
		bool bCheckLocalFirst,
		FDOnDownloadProgress OnProgress,
		FDOnComplete OnComplete);

	/**
	 * Cross-reference API-fetched assets against content already present in this project.
	 *
	 * Assets committed to source control (or imported by another team member) exist on
	 * disk but may have no matching entry in the user-local manifest. This method scans
	 * the Content/WorldBLD/Assets/ directory for in-project manifest JSONs, matches them
	 * to the API's asset list via RootAssetID, and creates/upgrades manifest entries to
	 * Imported where appropriate. Should be called after each FetchAssets response.
	 */
	void ReconcileImportedAssets(const TArray<FWorldBLDAssetInfo>& FetchedAssets);

public:
	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|AssetLibrary")
	FOnAssetStatusChanged OnAssetStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|AssetLibrary")
	FOnImportComplete OnImportComplete;

	/** Broadcast whenever subscription status becomes known/unknown and/or changes. */
	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|AssetLibrary")
	FOnSubscriptionStatusChanged OnSubscriptionStatusChanged;

private:
	friend class UWorldBLDAssetDownloadCallbackProxy;
	friend class FWorldBLDImportJob;
	friend class FWorldBLDUninstallJob;

	UFUNCTION()
	void HandleLicensesUpdated(const TArray<FString>& AuthorizedTools);

	UFUNCTION()
	void HandleSessionValid();

	UFUNCTION()
	void HandleSessionInvalid();

	FString GetCacheDirectory() const;
	FString GetManifestPath() const;

	void LoadManifest();
	void SaveManifest();

	void OnDownloadComplete_Internal(const FString& AssetId, bool bSuccess);
	void OnDownloadProgress_Internal(const FString& AssetId, float Progress);

	bool ExtractAssetArchive(const FString& ArchivePath, const FString& DestinationPath);
	bool CopyAssetToProject(const FString& SourcePath, const FString& DestinationPath);

	// Tick-deferred execution helpers (called by jobs)
	bool ImportAsset_Immediate(const FString& AssetId);
	bool UninstallAsset_Immediate(const FString& AssetId, bool bDeleteCache, bool bDeleteImportedContent);

	/**
	 * Validate persisted Imported/Downloaded states against the current project.
	 *
	 * The asset state manifest is stored per-user (not per-project), so an asset
	 * imported into Project A will still appear as "Imported" when the user opens
	 * Project B. This method downgrades stale statuses:
	 *   - Imported  → Downloaded (if cache exists) or Owned
	 *   - Downloaded → Owned     (if cache is missing)
	 */
	void ValidateStatesForCurrentProject();

	void SetAssetStatus_Internal(const FString& AssetId, EWorldBLDAssetStatus NewStatus);
	FWorldBLDAssetDownloadState GetOrAddState_Internal(const FString& AssetId);

	/**
	 * Scan the project content directory for assets with embedded unique IDs.
	 * @param UniqueID The unique ID to search for
	 * @param OutPackagePath The package path of the found asset
	 * @param OutSHA1Hash The SHA1 hash from the asset's user data
	 * @return True if asset with this ID was found
	 */
	bool FindAssetByUniqueID(const FString& UniqueID, FString& OutPackagePath, FString& OutSHA1Hash) const;

	/**
	 * Build a cache of all locally imported assets with their unique IDs.
	 * This is called once and cached for the duration of the dependency check.
	 */
	void BuildLocalAssetCache(TMap<FString, TPair<FString, FString>>& OutCache) const;

	/**
	 * Invalidate the local asset cache, forcing a rebuild on next check.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|AssetLibrary")
	void InvalidateLocalAssetCache();

	/**
	 * Scan Content/WorldBLD/ for imported asset manifest JSONs and build
	 * the RootAssetID → AuthoredPackageRoot lookup used by ReconcileImportedAssets.
	 */
	void ScanContentForLocalImports();

private:
	TMap<FString, FWorldBLDAssetDownloadState> AssetStates;

	/** RootAssetID → AuthoredPackageRoot for assets found on disk under Content/WorldBLD/. */
	TMap<FString, FString> LocalImportsByRootAssetID;
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UWorldBLDAssetDownloadCallbackProxy>> ActiveCallbacks;
	UCurlDownloaderLibrary* DownloaderLibrary = nullptr;
	UWorldBLDAssetLibrarySubsystem* AssetLibrarySubsystem = nullptr;
	UWorldBLDAuthSubsystem* AuthSubsystem = nullptr;
	TUniquePtr<FWorldBLDAssetLibraryJobQueue, FWorldBLDAssetLibraryJobQueueDeleter> JobQueue;
	TUniquePtr<FWorldBLDAssetStateStore, FWorldBLDAssetStateStoreDeleter> StateStore;
	TUniquePtr<FWorldBLDAssetCacheService, FWorldBLDAssetCacheServiceDeleter> CacheService;
	TUniquePtr<FWorldBLDAssetImportService, FWorldBLDAssetImportServiceDeleter> ImportService;
	TUniquePtr<FWorldBLDAssetDownloadService, FWorldBLDAssetDownloadServiceDeleter> DownloadService;
	TUniquePtr<FWorldBLDAssetDependencyService, FWorldBLDAssetDependencyServiceDeleter> DependencyService;

	mutable FCriticalSection StateMutex;

	// Cached subscription state derived from GET /api/auth/desktop/owned-apps.
	// Protected by StateMutex.
	bool bHasActiveSubscription = false;
	bool bHasActiveSubscriptionKnown = false;
};
