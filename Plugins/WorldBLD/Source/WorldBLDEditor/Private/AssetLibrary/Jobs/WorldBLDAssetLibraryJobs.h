#pragma once

#include "CoreMinimal.h"
#include "AssetLibrary/Jobs/WorldBLDAssetLibraryJobQueue.h"

class UWorldBLDAssetDownloadManager;

class FWorldBLDImportJob : public IWorldBLDAssetJob
{
public:
	FWorldBLDImportJob(TWeakObjectPtr<UWorldBLDAssetDownloadManager> InManager, FString InAssetId);

	virtual bool TickJob(float DeltaTime) override;

private:
	TWeakObjectPtr<UWorldBLDAssetDownloadManager> Manager;
	FString AssetId;
	bool bRan = false;
};

class FWorldBLDUninstallJob : public IWorldBLDAssetJob
{
public:
	FWorldBLDUninstallJob(
		TWeakObjectPtr<UWorldBLDAssetDownloadManager> InManager,
		FString InAssetId,
		bool bInDeleteCache,
		bool bInDeleteImportedContent);

	virtual bool TickJob(float DeltaTime) override;

private:
	TWeakObjectPtr<UWorldBLDAssetDownloadManager> Manager;
	FString AssetId;
	bool bDeleteCache = true;
	bool bDeleteImportedContent = true;
	bool bRan = false;
};

