// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "WorldBLDAssetUserData.generated.h"

/**
 * Asset user data that stores a unique identifier for WorldBLD assets.
 * This allows us to track assets across different projects and locations.
 * The UniqueAssetID is embedded directly into the asset file and travels with it.
 *
 * Usage:
 * - Automatically added during asset preparation
 * - Used for dependency resolution when downloading assets
 * - Enables re-referencing downloaded assets to local dependencies
 */
UCLASS(BlueprintType)
class WORLDBLDEDITOR_API UWorldBLDAssetUserData : public UAssetUserData
{
	GENERATED_BODY()

public:
	/** Globally unique identifier for this asset in the WorldBLD asset store */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD")
	FGuid UniqueAssetID;

	/** Asset name for debugging purposes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD")
	FString AssetName;

	/** SHA1 hash of the asset file when it was prepared (for verification) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD")
	FString SHA1Hash;
};
