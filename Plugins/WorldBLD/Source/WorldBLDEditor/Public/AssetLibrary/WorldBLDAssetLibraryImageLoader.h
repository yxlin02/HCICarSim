// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Brushes/SlateDynamicImageBrush.h"
#include "UObject/StrongObjectPtr.h"

class FWorldBLDAssetLibraryImageLoader
{
public:
	static FWorldBLDAssetLibraryImageLoader& Get();

	void LoadImageFromURL(const FString& URL, TFunction<void(TSharedPtr<FSlateDynamicImageBrush>)> OnComplete);
	void PrefetchImagesFromURLs(const TArray<FString>& Urls);

	TSharedPtr<FSlateDynamicImageBrush> GetPlaceholderBrush();

private:
	FWorldBLDAssetLibraryImageLoader();

	TSharedPtr<FSlateDynamicImageBrush> CreatePlaceholderBrush();

private:
	TMap<FString, TSharedPtr<FSlateDynamicImageBrush>> ImageCache;
	TMap<FString, TStrongObjectPtr<UTexture2D>> TextureCache;
	TMap<FString, TArray<TFunction<void(TSharedPtr<FSlateDynamicImageBrush>)>>> PendingRequests;
	TSharedPtr<FSlateDynamicImageBrush> PlaceholderBrush;
	TStrongObjectPtr<UTexture2D> PlaceholderTexture;
};
