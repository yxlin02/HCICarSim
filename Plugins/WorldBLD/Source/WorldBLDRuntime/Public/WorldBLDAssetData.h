#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "WorldBLDAssetData.generated.h"

class UTexture2D;

UCLASS(BlueprintType, Blueprintable)
class WORLDBLDRUNTIME_API UWorldBLDAssetData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UWorldBLDAssetData();

	//Randomly generated ID for the asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|Asset")
	FString AssetID;

	//The name of the asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|Asset")
	FString AssetName;

	//A detailed text description of the asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|Asset")
	FText Description;

	//The tags of the asset, used for searching and filtering.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|Asset")
	TArray<FString> Tags;

	//DEPRECATED: We no longer use PatchKit and the AppId is no longer needed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|Asset")
	FString PatchKitAppId;

	//The thumbnail image of the asset. Can be auto generated from the asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|Asset")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	//The preview images of the asset, if any. Can be auto generated from the asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|Asset")
	TArray<TSoftObjectPtr<UTexture2D>> PreviewImages;

	//The core assets that make up this prepared package. These are the assets that MUST be present.
	//Additional assets may exist in the package, but are only included if referenced by AssetsInPackage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|Asset")
	TArray<TObjectPtr<UObject>> AssetsInPackage;

	//The name of the collection this asset belongs to, if applicable. Case-sensitive.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|Asset")
	FString CollectionName;
};
