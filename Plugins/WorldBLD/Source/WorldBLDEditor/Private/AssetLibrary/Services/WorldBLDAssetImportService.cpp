#include "AssetLibrary/Services/WorldBLDAssetImportService.h"

#include "AssetLibrary/Services/WorldBLDAssetImportArtifact.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

bool FWorldBLDAssetImportService::CopyPreparedTreeToProjectContent(const FString& InPreparedRootOnDisk, const FString& InDestinationOnDisk) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*InDestinationOnDisk);
	if (!PlatformFile.CopyDirectoryTree(*InDestinationOnDisk, *InPreparedRootOnDisk, true))
	{
		return false;
	}

	// Ensure copied files are writable.
	TArray<FString> CopiedFiles;
	IFileManager::Get().FindFilesRecursive(CopiedFiles, *InDestinationOnDisk, TEXT("*.*"), true, false, false);
	for (const FString& FilePath : CopiedFiles)
	{
		PlatformFile.SetReadOnly(*FilePath, false);
	}

	return true;
}

bool FWorldBLDAssetImportService::ImportDownloadedArtifact(const FWorldBLDDownloadedArtifact& Artifact, FString& OutImportedPackageRoot, FString& OutImportedDiskRoot) const
{
	OutImportedPackageRoot.Reset();
	OutImportedDiskRoot.Reset();

	if (Artifact.PreparedRootOnDisk.IsEmpty() || Artifact.AuthoredPackageRoot.IsEmpty() || Artifact.Files.Num() == 0)
	{
		return false;
	}

	if (!FPackageName::IsValidLongPackageName(Artifact.AuthoredPackageRoot))
	{
		return false;
	}

	FString RelativeContentPath = Artifact.AuthoredPackageRoot;
	RelativeContentPath.RemoveFromStart(TEXT("/Game/"));
	if (RelativeContentPath.IsEmpty())
	{
		return false;
	}

	const FString DestRootOnDisk = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), RelativeContentPath));

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*DestRootOnDisk);

	TArray<FString> NewUassetFiles;
	NewUassetFiles.Reserve(Artifact.Files.Num());

	for (const FWorldBLDAssetImportFileEntry& FileEntry : Artifact.Files)
	{
		if (FileEntry.RelativePathFromPackageRoot.IsEmpty())
		{
			continue;
		}

		const FString SourcePath = FPaths::Combine(Artifact.PreparedRootOnDisk, FileEntry.RelativePathFromPackageRoot);
		const FString DestPath = FPaths::Combine(DestRootOnDisk, FileEntry.RelativePathFromPackageRoot);

		PlatformFile.CreateDirectoryTree(*FPaths::GetPath(DestPath));
		if (!PlatformFile.CopyFile(*DestPath, *SourcePath))
		{
			return false;
		}

		PlatformFile.SetReadOnly(*DestPath, false);

		const FString Ext = FPaths::GetExtension(DestPath).ToLower();
		if (Ext == TEXT("uasset") || Ext == TEXT("umap"))
		{
			NewUassetFiles.Add(DestPath);
		}
	}

	if (NewUassetFiles.Num() == 0)
	{
		return false;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().ScanFilesSynchronous(NewUassetFiles, /*bForceRescan=*/true);

	OutImportedPackageRoot = Artifact.AuthoredPackageRoot;
	OutImportedDiskRoot = DestRootOnDisk;
	return true;
}

