#include "AssetLibrary/Services/WorldBLDAssetDownloadService.h"

#include "Downloader/CurlDownloaderLibrary.h"

FWorldBLDAssetDownloadService::FWorldBLDAssetDownloadService(UCurlDownloaderLibrary* InDownloaderLibrary)
	: DownloaderLibrary(InDownloaderLibrary)
{
}

bool FWorldBLDAssetDownloadService::IsDownloadInProgress(const FString& AssetId) const
{
	return DownloaderLibrary.IsValid() ? DownloaderLibrary->IsDownloadInProgress(AssetId) : false;
}

float FWorldBLDAssetDownloadService::GetDownloadProgress(const FString& AssetId) const
{
	return DownloaderLibrary.IsValid() ? DownloaderLibrary->GetDownloadProgress(AssetId) : -1.0f;
}

void FWorldBLDAssetDownloadService::RequestFileDownload(
	const FString& AssetId,
	const FString& DownloadUrl,
	const FString& ArchivePath,
	const FString& OptionalHeaders,
	const FDOnDownloadProgress& OnProgress,
	const FDOnComplete& OnComplete) const
{
	if (!DownloaderLibrary.IsValid())
	{
		return;
	}

	DownloaderLibrary->RequestFileDownload(AssetId, DownloadUrl, ArchivePath, OptionalHeaders, OnProgress, OnComplete);
}

void FWorldBLDAssetDownloadService::CancelDownload(const FString& AssetId) const
{
	if (DownloaderLibrary.IsValid())
	{
		DownloaderLibrary->CancelDownload(AssetId);
	}
}

