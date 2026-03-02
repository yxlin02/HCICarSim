#pragma once

#include "CoreMinimal.h"

class FWorldBLDAssetDependencyService
{
public:
	FWorldBLDAssetDependencyService() = default;

	void Invalidate();

	// Finds a local asset by WorldBLD UniqueID (as stored in UWorldBLDAssetUserData).
	bool FindAssetByUniqueID(const FString& UniqueID, FString& OutPackagePath, FString& OutSHA1Hash) const;

	// Builds a snapshot cache (UniqueID -> (PackagePath, SHA1Hash)).
	void BuildLocalAssetCache(TMap<FString, TPair<FString, FString>>& OutCache) const;

private:
	void EnsureCacheBuilt_Locked() const;

	mutable FCriticalSection CacheMutex;
	mutable TMap<FString, TPair<FString, FString>> LocalAssetCache;
	mutable bool bLocalAssetCacheBuilt = false;
};

