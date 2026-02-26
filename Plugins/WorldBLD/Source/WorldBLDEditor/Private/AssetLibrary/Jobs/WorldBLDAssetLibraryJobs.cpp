#include "AssetLibrary/Jobs/WorldBLDAssetLibraryJobs.h"

#include "AssetLibrary/WorldBLDAssetDownloadManager.h"

FWorldBLDImportJob::FWorldBLDImportJob(TWeakObjectPtr<UWorldBLDAssetDownloadManager> InManager, FString InAssetId)
	: Manager(InManager)
	, AssetId(MoveTemp(InAssetId))
{
}

bool FWorldBLDImportJob::TickJob(float DeltaTime)
{
	(void)DeltaTime;

	if (bRan)
	{
		return true;
	}

	bRan = true;

	if (Manager.IsValid())
	{
		Manager->ImportAsset_Immediate(AssetId);
	}

	return true;
}

FWorldBLDUninstallJob::FWorldBLDUninstallJob(
	TWeakObjectPtr<UWorldBLDAssetDownloadManager> InManager,
	FString InAssetId,
	bool bInDeleteCache,
	bool bInDeleteImportedContent)
	: Manager(InManager)
	, AssetId(MoveTemp(InAssetId))
	, bDeleteCache(bInDeleteCache)
	, bDeleteImportedContent(bInDeleteImportedContent)
{
}

bool FWorldBLDUninstallJob::TickJob(float DeltaTime)
{
	(void)DeltaTime;

	if (bRan)
	{
		return true;
	}

	bRan = true;

	if (Manager.IsValid())
	{
		Manager->UninstallAsset_Immediate(AssetId, bDeleteCache, bDeleteImportedContent);
	}

	return true;
}

