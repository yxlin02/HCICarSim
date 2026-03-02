#include "AssetLibrary/Services/WorldBLDAssetDependencyService.h"

#include "AssetLibrary/WorldBLDAssetUserData.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "Misc/PackageName.h"

void FWorldBLDAssetDependencyService::Invalidate()
{
	FScopeLock Lock(&CacheMutex);
	LocalAssetCache.Reset();
	bLocalAssetCacheBuilt = false;
}

bool FWorldBLDAssetDependencyService::FindAssetByUniqueID(const FString& UniqueID, FString& OutPackagePath, FString& OutSHA1Hash) const
{
	OutPackagePath.Reset();
	OutSHA1Hash.Reset();

	TMap<FString, TPair<FString, FString>> Cache;
	BuildLocalAssetCache(Cache);

	const FString NormalizedUniqueID = UniqueID.ToLower();
	if (const TPair<FString, FString>* Found = Cache.Find(NormalizedUniqueID))
	{
		OutPackagePath = Found->Key;
		OutSHA1Hash = Found->Value;
		return true;
	}

	return false;
}

void FWorldBLDAssetDependencyService::BuildLocalAssetCache(TMap<FString, TPair<FString, FString>>& OutCache) const
{
	FScopeLock Lock(&CacheMutex);
	EnsureCacheBuilt_Locked();
	OutCache = LocalAssetCache;
}

void FWorldBLDAssetDependencyService::EnsureCacheBuilt_Locked() const
{
	if (bLocalAssetCacheBuilt)
	{
		return;
	}

	LocalAssetCache.Reset();

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(TEXT("/Game/WorldBLD")));
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UMaterialInterface::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UTexture::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		if (!AssetData.IsValid())
		{
			continue;
		}

		const FSoftObjectPath AssetObjectPath = AssetData.GetSoftObjectPath();
		if (!AssetObjectPath.IsValid())
		{
			continue;
		}

		UObject* LoadedAsset = AssetObjectPath.ResolveObject();
		if (!IsValid(LoadedAsset))
		{
			LoadedAsset = AssetObjectPath.TryLoad();
		}

		if (!IsValid(LoadedAsset))
		{
			continue;
		}

		const UWorldBLDAssetUserData* UD = nullptr;
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(LoadedAsset))
		{
			UD = Mesh->GetAssetUserData<UWorldBLDAssetUserData>();
		}
		else if (UMaterialInterface* Material = Cast<UMaterialInterface>(LoadedAsset))
		{
			UD = Material->GetAssetUserData<UWorldBLDAssetUserData>();
		}
		else if (UTexture* Texture = Cast<UTexture>(LoadedAsset))
		{
			UD = Texture->GetAssetUserData<UWorldBLDAssetUserData>();
		}

		if (!UD || !UD->UniqueAssetID.IsValid())
		{
			continue;
		}

		const FString PackageName = AssetData.PackageName.ToString();
		if (PackageName.IsEmpty())
		{
			continue;
		}

		const FString NormalizedUniqueID = UD->UniqueAssetID.ToString(EGuidFormats::DigitsWithHyphensLower);
		LocalAssetCache.Add(NormalizedUniqueID, TPair<FString, FString>(PackageName, UD->SHA1Hash));
	}

	bLocalAssetCacheBuilt = true;
}

