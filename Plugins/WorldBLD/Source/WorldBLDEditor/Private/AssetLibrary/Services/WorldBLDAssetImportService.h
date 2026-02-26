#pragma once

#include "CoreMinimal.h"

struct FWorldBLDDownloadedArtifact;

class FWorldBLDAssetImportService
{
public:
	bool CopyPreparedTreeToProjectContent(const FString& InPreparedRootOnDisk, const FString& InDestinationOnDisk) const;

	// Deterministic import: copy authored files to the exact authored package root and perform a targeted registry scan.
	bool ImportDownloadedArtifact(const FWorldBLDDownloadedArtifact& Artifact, FString& OutImportedPackageRoot, FString& OutImportedDiskRoot) const;
};

