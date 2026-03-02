// Copyright WorldBLD LLC. All Rights Reserved.

#include "AssetLibrary/WorldBLDAssetLibraryImageLoader.h"

#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "HttpModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"

namespace
{
	static FName MakeResourceNameFromUrl(const FString& URL)
	{
		// UE5.6/5.7: FMD5::HashAnsiString returns a hex string, it does not fill an output byte buffer.
		const FString HexHash = FMD5::HashAnsiString(*URL);
		return FName(*FString::Printf(TEXT("WorldBLDAssetThumb_%s"), *HexHash));
	}

	static EImageFormat DetectImageFormat(const FString& URL, const FString& ContentType)
	{
		const FString LowerUrl = URL.ToLower();
		const FString LowerType = ContentType.ToLower();

		if (LowerType.Contains(TEXT("png")) || LowerUrl.EndsWith(TEXT(".png")))
		{
			return EImageFormat::PNG;
		}

		if (LowerType.Contains(TEXT("jpeg")) || LowerType.Contains(TEXT("jpg")) || LowerUrl.EndsWith(TEXT(".jpg")) || LowerUrl.EndsWith(TEXT(".jpeg")))
		{
			return EImageFormat::JPEG;
		}

		return EImageFormat::Invalid;
	}

	static UTexture2D* CreateTextureFromBGRA(const TArray<uint8>& RawBGRA, int32 Width, int32 Height)
	{
		if (Width <= 0 || Height <= 0)
		{
			return nullptr;
		}

		const int64 ExpectedBytes = (int64)Width * (int64)Height * 4;
		if (ExpectedBytes <= 0)
		{
			return nullptr;
		}

		if (RawBGRA.Num() < ExpectedBytes)
		{
			return nullptr;
		}

		UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		if (!Texture)
		{
			return nullptr;
		}

		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->SRGB = true;

		if (!Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() <= 0)
		{
			return nullptr;
		}

		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		const int64 BulkSize = (int64)Mip.BulkData.GetBulkDataSize();
		if (BulkSize < ExpectedBytes)
		{
			return nullptr;
		}

		void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(TextureData, RawBGRA.GetData(), ExpectedBytes);
		Mip.BulkData.Unlock();

		Texture->UpdateResource();
		return Texture;
	}
}

FWorldBLDAssetLibraryImageLoader& FWorldBLDAssetLibraryImageLoader::Get()
{
	static FWorldBLDAssetLibraryImageLoader Instance;
	return Instance;
}

FWorldBLDAssetLibraryImageLoader::FWorldBLDAssetLibraryImageLoader()
{
}

void FWorldBLDAssetLibraryImageLoader::PrefetchImagesFromURLs(const TArray<FString>& Urls)
{
	// Fire-and-forget: starts requests and relies on internal caches for subsequent consumers.
	// We intentionally do not log here to avoid noise while browsing.
	for (const FString& Url : Urls)
	{
		if (Url.IsEmpty())
		{
			continue;
		}

		LoadImageFromURL(Url, [](TSharedPtr<FSlateDynamicImageBrush>) {});
	}
}

TSharedPtr<FSlateDynamicImageBrush> FWorldBLDAssetLibraryImageLoader::CreatePlaceholderBrush()
{
	UTexture2D* Texture = UTexture2D::CreateTransient(1, 1, PF_B8G8R8A8);
	if (Texture && Texture->GetPlatformData() && Texture->GetPlatformData()->Mips.Num() > 0)
	{
		const uint32 Gray = 0xFF404040;
		void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(TextureData, &Gray, sizeof(uint32));
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();
	}

	PlaceholderTexture = TStrongObjectPtr<UTexture2D>(Texture);

	TSharedPtr<FSlateDynamicImageBrush> Brush = MakeShared<FSlateDynamicImageBrush>(FName(TEXT("WorldBLDAssetThumb_Placeholder")), FVector2D(1.0f, 1.0f));
	Brush->SetResourceObject(PlaceholderTexture.Get());
	return Brush;
}

TSharedPtr<FSlateDynamicImageBrush> FWorldBLDAssetLibraryImageLoader::GetPlaceholderBrush()
{
	if (!PlaceholderBrush.IsValid())
	{
		PlaceholderBrush = CreatePlaceholderBrush();
	}
	return PlaceholderBrush;
}

void FWorldBLDAssetLibraryImageLoader::LoadImageFromURL(const FString& URL, TFunction<void(TSharedPtr<FSlateDynamicImageBrush>)> OnComplete)
{
	if (URL.IsEmpty())
	{
		AsyncTask(ENamedThreads::GameThread, [this, OnComplete = MoveTemp(OnComplete)]()
		{
			OnComplete(GetPlaceholderBrush());
		});
		return;
	}

	if (TSharedPtr<FSlateDynamicImageBrush>* Cached = ImageCache.Find(URL))
	{
		TSharedPtr<FSlateDynamicImageBrush> CachedBrush = *Cached;
		AsyncTask(ENamedThreads::GameThread, [CachedBrush, OnComplete = MoveTemp(OnComplete)]()
		{
			OnComplete(CachedBrush);
		});
		return;
	}

	// Coalesce in-flight requests so prefetch + UI consumption doesn't double-download.
	if (TArray<TFunction<void(TSharedPtr<FSlateDynamicImageBrush>)>>* Pending = PendingRequests.Find(URL))
	{
		Pending->Add(MoveTemp(OnComplete));
		return;
	}

	PendingRequests.Add(URL, { MoveTemp(OnComplete) });

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(URL);
	Request->SetVerb(TEXT("GET"));

	Request->OnProcessRequestComplete().BindLambda(
		[this, URL](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			TSharedPtr<FSlateDynamicImageBrush> ResultBrush = GetPlaceholderBrush();

			if (bConnectedSuccessfully && Response.IsValid() && Response->GetResponseCode() == 200)
			{
				const TArray<uint8>& Bytes = Response->GetContent();
				const FString ContentType = Response->GetContentType();
				const EImageFormat Format = DetectImageFormat(URL, ContentType);

				if (Format != EImageFormat::Invalid && Bytes.Num() > 0)
				{
					IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
					TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);

					if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(Bytes.GetData(), Bytes.Num()))
					{
						TArray<uint8> RawBGRA;
						if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawBGRA))
						{
							UTexture2D* Texture = CreateTextureFromBGRA(RawBGRA, ImageWrapper->GetWidth(), ImageWrapper->GetHeight());
							if (Texture)
							{
								const FName ResourceName = MakeResourceNameFromUrl(URL);
								TSharedPtr<FSlateDynamicImageBrush> Brush = MakeShared<FSlateDynamicImageBrush>(ResourceName, FVector2D((float)ImageWrapper->GetWidth(), (float)ImageWrapper->GetHeight()));
								Brush->SetResourceObject(Texture);
								ResultBrush = Brush;
							}
						}
					}
				}
			}

			if (ResultBrush.IsValid() && ResultBrush != GetPlaceholderBrush())
			{
				ImageCache.Add(URL, ResultBrush);
				if (!TextureCache.Contains(URL))
				{
					if (UTexture2D* Texture = Cast<UTexture2D>(ResultBrush->GetResourceObject()))
					{
						TextureCache.Add(URL, TStrongObjectPtr<UTexture2D>(Texture));
					}
				}
			}

			// Move out pending callbacks (if any) and complete them on the game thread.
			TArray<TFunction<void(TSharedPtr<FSlateDynamicImageBrush>)>> Callbacks;
			if (TArray<TFunction<void(TSharedPtr<FSlateDynamicImageBrush>)>>* Pending = PendingRequests.Find(URL))
			{
				Callbacks = MoveTemp(*Pending);
				PendingRequests.Remove(URL);
			}

			AsyncTask(ENamedThreads::GameThread, [ResultBrush, Callbacks = MoveTemp(Callbacks)]() mutable
			{
				for (TFunction<void(TSharedPtr<FSlateDynamicImageBrush>)>& Callback : Callbacks)
				{
					Callback(ResultBrush);
				}
			});
		});

	Request->ProcessRequest();
}
