// Copyright WorldBLD. All rights reserved.

#include "BlueprintAssetItemButtonWidget.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Texture2D.h"

UTexture2D* UBlueprintAssetItemButtonWidget::GetThumbnailFromClass_Implementation(UClass* InClass)
{
	if (!InClass)
	{
		return nullptr;
	}

	// Get the Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Get the asset data for this Blueprint class
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(InClass));
	
	if (!AssetData.IsValid())
	{
		// Try alternative method: get asset data by class package
		FString PackageName = InClass->GetOutermost()->GetName();
		TArray<FAssetData> AssetsInPackage;
		AssetRegistry.GetAssetsByPackageName(FName(*PackageName), AssetsInPackage);
		
		if (AssetsInPackage.Num() > 0)
		{
			AssetData = AssetsInPackage[0];
		}
	}

	if (AssetData.IsValid())
	{
		// Store asset name and path for later use
		AssetName = AssetData.AssetName.ToString();
		AssetPath = AssetData.GetObjectPathString();
	}

	// Note: Asset thumbnail extraction to UTexture2D for UMG is complex and requires
	// additional rendering infrastructure. For now, we return nullptr and the base widget
	// will display the item without a custom thumbnail (using the class icon instead).
	// 
	// To implement thumbnail display:
	// 1. Use FAssetThumbnail with a thumbnail pool (for Slate widgets)
	// 2. Or store a reference to create a custom brush in the widget
	// 3. Or pre-generate thumbnail textures and store them as properties on the Blueprint
	//
	// Users can override this in Blueprint to provide custom thumbnail logic if needed.
	return nullptr;
}

FText UBlueprintAssetItemButtonWidget::GetTooltipTextFromClass_Implementation(UClass* InClass)
{
	if (!InClass)
	{
		return FText::GetEmpty();
	}

	// If we have the asset name cached, use it
	if (!AssetName.IsEmpty())
	{
		return FText::FromString(AssetName);
	}

	// Otherwise, get it from the Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(InClass));
	
	if (!AssetData.IsValid())
	{
		// Try alternative method
		FString PackageName = InClass->GetOutermost()->GetName();
		TArray<FAssetData> AssetsInPackage;
		AssetRegistry.GetAssetsByPackageName(FName(*PackageName), AssetsInPackage);
		
		if (AssetsInPackage.Num() > 0)
		{
			AssetData = AssetsInPackage[0];
		}
	}

	if (AssetData.IsValid())
	{
		AssetName = AssetData.AssetName.ToString();
		return FText::FromString(AssetName);
	}

	// Fallback to class name
	return FText::FromString(InClass->GetName());
}

