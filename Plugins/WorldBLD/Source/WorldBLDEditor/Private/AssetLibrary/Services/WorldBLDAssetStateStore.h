#pragma once

#include "CoreMinimal.h"

struct FWorldBLDAssetCacheManifest;
struct FWorldBLDAssetDownloadState;

class FWorldBLDAssetStateStore
{
public:
	static FString GetCacheDirectory();
	static FString GetManifestPath();

	static bool LoadStates(TMap<FString, FWorldBLDAssetDownloadState>& OutStates);
	static bool SaveStates(const TMap<FString, FWorldBLDAssetDownloadState>& States);
};

