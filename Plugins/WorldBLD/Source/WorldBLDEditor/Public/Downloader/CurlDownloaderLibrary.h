// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "CurlDownloaderLibrary.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FDOnComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnComplete, bool);
DECLARE_DYNAMIC_DELEGATE_OneParam(FDOnDownloadProgress, float, Progress);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDownloadProgress, float);

USTRUCT(BlueprintType)
struct FWorldBLDPluginUninstallResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldBLD|Plugins")
	bool bSuccess = false;

	/** True if the editor must be restarted for the change to fully apply (common for code plugins or locked files). */
	UPROPERTY(BlueprintReadOnly, Category = "WorldBLD|Plugins")
	bool bRestartRequired = false;

	/** True if file deletion could not occur immediately and was scheduled for next editor start. */
	UPROPERTY(BlueprintReadOnly, Category = "WorldBLD|Plugins")
	bool bDeletionDeferred = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldBLD|Plugins")
	FString Message;
};

struct FCurlDownloadContext : TSharedFromThis<FCurlDownloadContext>
{
	FString URL;
	FString SavePath;
	FString PluginName;  // Name of the plugin being downloaded (for mounting after extraction)
	FString ExpectedSHA1;  // Expected SHA1 hash for verification (empty = skip verification)
	bool bExtractZipToPlugins = true;

	// Staged update support: when true the zip is extracted to a staging directory
	// instead of Plugins/ and no mounting is attempted.  The bootstrap module will
	// swap the directories on the next editor startup.
	bool bStagedUpdate = false;
	FString StagedExtractionDir;  // Non-empty when bStagedUpdate is true

	// Thread-safe progress tracking
	TAtomic<int64> TotalBytes{0};
	TAtomic<int64> DownloadedBytes{0};
	TAtomic<bool> bCancelled{false};

	// Adaptive connection settings
	TAtomic<int32> CurrentConnections{4};  // Start with 4, adapt based on server response
	static constexpr int32 MaxConnections = 8;
	static constexpr int32 MinConnections = 1;

	// Retry settings
	int32 MaxChunkRetries = 5;
	int32 MaxFullRetries = 3;  // Full download retries with reduced connections

	// Delegates (must be called on game thread)
	FOnDownloadProgress OnProgress;
	FOnComplete OnComplete;

	// Chunk tracking
	FCriticalSection ChunkMutex;
	TArray<TPair<int64, int64>> ChunkRanges;
	TMap<int32, TArray<uint8>> CompletedChunkBuffers;  // Only store completed chunks
	TSet<int32> FailedChunks;
	TSet<int32> InProgressChunks;

	float GetProgress() const
	{
		const int64 Total = TotalBytes.Load();
		if (Total <= 0) return 0.f;
		return FMath::Clamp(static_cast<float>(DownloadedBytes.Load()) / Total, 0.f, 1.f);
	}

	void Reset()
	{
		DownloadedBytes.Store(0);
		ChunkRanges.Empty();
		CompletedChunkBuffers.Empty();
		FailedChunks.Empty();
		InProgressChunks.Empty();
	}
};

/**
 * High-performance adaptive multi-connection file downloader using libcurl.
 * This subsystem lives as long as the editor is running, allowing downloads to persist
 * even when UI widgets are destroyed and recreated.
 */
UCLASS()
class WORLDBLDEDITOR_API UCurlDownloaderLibrary : public UEditorSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Ensures a plugin is enabled/disabled in the current project's .uproject file.
	 * Note: this does not hot-load code plugins; an editor restart may still be required.
	 */
	bool SetPluginEnabledInProjectFileNative(const FString& PluginName, bool bEnabled, FString& OutError) const;

	/**
	 * C++ convenience overload that supports lambdas.
	 * Note: This is not a UFUNCTION, so it is not callable from Blueprints.
	 */
	void RequestToolDownloadNative(const FString& ToolId, const FString& DownloadUrl, const FString& ExpectedSHA1,
		FOnDownloadProgress::FDelegate OnProgress, FOnComplete::FDelegate OnComplete);

	/**
	 * C++ convenience overload that supports lambdas.
	 * Note: This is not a UFUNCTION, so it is not callable from Blueprints.
	 */
	void RequestFileDownloadNative(const FString& ToolId, const FString& DownloadUrl, const FString& SavePath, const FString& ExpectedSHA1,
		FOnDownloadProgress::FDelegate OnProgress, FOnComplete::FDelegate OnComplete);

	/**
	 * Starts a download or binds callbacks to an existing download in progress.
	 * Multiple widgets can safely call this - if a download is already active, new callbacks
	 * are added to the existing download and the caller receives the current progress immediately.
	 * @param ToolId Unique identifier for this download (used for tracking and file naming)
	 * @param DownloadUrl URL to download from
	 * @param ExpectedSHA1 Expected SHA1 hash for verification (empty to skip verification)
	 * @param OnProgress Called periodically with download progress (0.0 to 1.0)
	 * @param OnComplete Called when download completes or fails
	 */
	UFUNCTION(BlueprintCallable, Category = "Curl|Downloader", meta = (DisplayName = "Request Tool Download"))
	void RequestToolDownload(const FString& ToolId, const FString& DownloadUrl, const FString& ExpectedSHA1, FDOnDownloadProgress OnProgress, FDOnComplete OnComplete);

	UFUNCTION(BlueprintCallable, Category = "Curl|Downloader", meta = (DisplayName = "Request File Download"))
	void RequestFileDownload(const FString& ToolId, const FString& DownloadUrl, const FString& SavePath, const FString& ExpectedSHA1, FDOnDownloadProgress OnProgress, FDOnComplete OnComplete);

	/** Cancels an active download. All bound callbacks will receive OnComplete(false). */
	UFUNCTION(BlueprintCallable, Category = "Curl|Downloader")
	void CancelDownload(const FString& ToolId);

	/** Returns true if a download is currently in progress for the given ToolId. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Curl|Downloader")
	bool IsDownloadInProgress(const FString& ToolId) const;

	/** Gets the current download progress (0.0 to 1.0). Returns -1.0 if no download is active. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Curl|Downloader")
	float GetDownloadProgress(const FString& ToolId) const;

	/**
	 * Disables a plugin in the current project's .uproject, un-registers its mount point (content plugins),
	 * and deletes its files from disk. If deletion fails due to locked files, it can be deferred until the next
	 * editor start (handled automatically by this subsystem).
	 *
	 * Note: Unloading code plugin modules at runtime is not supported/safe; code plugins generally require an editor restart.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Plugins", meta = (DisplayName = "Uninstall Plugin (Disable + Delete)"))
	FWorldBLDPluginUninstallResult UninstallPlugin(const FString& PluginName, bool bDeferDeletionUntilRestartIfNeeded = true);

	static int64 GetRemoteFileSize(const FString& URL);
	static bool ServerSupportsRangeRequests(const FString& URL);

	// ── Staged update helpers (used by SWorldBLDToolSwitchMenu for UI state) ──

	/** Returns the path to the staged-updates manifest JSON file. */
	static FString GetStagedUpdatesManifestPath();

	/** Returns the staging directory for a given tool (e.g. Saved/WorldBLD/StagedUpdates/<ToolId>/). */
	static FString GetStagedUpdatesDirForTool(const FString& ToolId);

	/** Returns true if a staged update exists for ToolId that has not yet been applied. */
	static bool HasPendingStagedUpdate(const FString& ToolId);

private:
	FCriticalSection DownloadsMutex;
	TMap<FString, TSharedRef<FCurlDownloadContext>> CurrentDownloads;

	void StartAdaptiveDownload(TSharedRef<FCurlDownloadContext> Context);
	void ProcessPendingPluginDeletes();

	/**
	 * Reads the staged-updates manifest for entries marked Applied by WorldBLDBootstrap,
	 * updates SHA1 records in UWorldBLDToolUpdateSettings, and cleans up the manifest.
	 */
	void ProcessAppliedStagedUpdates();

	/**
	 * For tool installs/updates: clears cached download dir and removes existing plugin files
	 * (content plugins) before downloading.
	 *
	 * @param bOutStagedUpdate  Set to true when the plugin has loaded code modules and the
	 *                          update must be staged for next-startup swap rather than applied
	 *                          immediately.
	 */
	bool PrepareForToolInstallOrUpdate(const FString& ToolId, FString& OutError, bool& bOutStagedUpdate);

	// Internal entry-point that allows callers to force extraction to ProjectDir/Plugins even when the zip is cached in Saved/.
	// When InStagedExtractionDir is non-empty the zip is extracted to that staging directory and no mounting/enabling occurs.
	void RequestFileDownloadInternal(const FString& ToolId, const FString& DownloadUrl, const FString& SavePath,
		const FString& ExpectedSHA1, const bool bExtractZipToPlugins, FDOnDownloadProgress OnProgress, FDOnComplete OnComplete,
		const FString& InStagedExtractionDir = FString());

	// Native internal entry-point for C++ callers (supports lambdas).
	void RequestFileDownloadInternalNative(const FString& ToolId, const FString& DownloadUrl, const FString& SavePath,
		const FString& ExpectedSHA1, const bool bExtractZipToPlugins, FOnDownloadProgress::FDelegate OnProgress, FOnComplete::FDelegate OnComplete,
		const FString& InStagedExtractionDir = FString());
};
