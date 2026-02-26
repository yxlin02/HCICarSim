#pragma once

#include "CoreMinimal.h"

struct FWorldBLDDownloadedArtifact;

class FWorldBLDAssetCacheService
{
public:
	bool ExtractArchive(const FString& InArchivePath, const FString& InDestinationPath) const;

	bool TryLoadDownloadedArtifact(const FString& InPreparedRootOnDisk, FWorldBLDDownloadedArtifact& OutArtifact) const;
};

