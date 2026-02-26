#pragma once

#include "CoreMinimal.h"

#include "Downloader/CurlDownloaderLibrary.h"

class UCurlDownloaderLibrary;

class FWorldBLDAssetDownloadService
{
public:
	explicit FWorldBLDAssetDownloadService(UCurlDownloaderLibrary* InDownloaderLibrary);

	bool IsDownloadInProgress(const FString& AssetId) const;
	float GetDownloadProgress(const FString& AssetId) const;

	void RequestFileDownload(
		const FString& AssetId,
		const FString& DownloadUrl,
		const FString& ArchivePath,
		const FString& OptionalHeaders,
		const FDOnDownloadProgress& OnProgress,
		const FDOnComplete& OnComplete) const;

	void CancelDownload(const FString& AssetId) const;

private:
	TWeakObjectPtr<UCurlDownloaderLibrary> DownloaderLibrary;
};

