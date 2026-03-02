#include "AssetLibrary/Services/WorldBLDAssetStateStore.h"

#include "AssetLibrary/WorldBLDAssetLibraryTypes.h"

#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"

FString FWorldBLDAssetStateStore::GetCacheDirectory()
{
	return FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("WorldBLD"), TEXT("AssetLibrary"), TEXT("Cache"));
}

FString FWorldBLDAssetStateStore::GetManifestPath()
{
	return FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("WorldBLD"), TEXT("AssetLibrary"), TEXT("manifest.json"));
}

bool FWorldBLDAssetStateStore::LoadStates(TMap<FString, FWorldBLDAssetDownloadState>& OutStates)
{
	OutStates.Reset();

	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *GetManifestPath()))
	{
		return false;
	}

	FWorldBLDAssetCacheManifest Manifest;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Content, &Manifest, 0, 0))
	{
		return false;
	}

	OutStates = MoveTemp(Manifest.AssetStates);
	return true;
}

bool FWorldBLDAssetStateStore::SaveStates(const TMap<FString, FWorldBLDAssetDownloadState>& States)
{
	FWorldBLDAssetCacheManifest Manifest;
	Manifest.AssetStates = States;

	FString Content;
	if (!FJsonObjectConverter::UStructToJsonObjectString(Manifest, Content, 0, 0))
	{
		return false;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(GetManifestPath()), true);
	return FFileHelper::SaveStringToFile(Content, *GetManifestPath());
}

