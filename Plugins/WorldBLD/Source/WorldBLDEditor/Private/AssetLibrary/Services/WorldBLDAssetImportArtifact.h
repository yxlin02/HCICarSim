#pragma once

#include "CoreMinimal.h"

struct FWorldBLDAssetImportFileEntry
{
	FString RelativePathFromPackageRoot;
	FString SHA1Hash;
	int64 SizeBytes = 0;
};

struct FWorldBLDDownloadedArtifact
{
	// On-disk directory containing authored asset files (Materials/, Textures/, ...).
	FString PreparedRootOnDisk;

	// Full path to the manifest json on disk.
	FString ManifestPath;

	// Long package name for the authored destination root (must be imported exactly).
	FString AuthoredPackageRoot;

	// Root asset GUID (string form from manifest).
	FString RootAssetId;

	TArray<FWorldBLDAssetImportFileEntry> Files;
};

