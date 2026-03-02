// Copyright WorldBLD LLC. All Rights Reserved.

#include "AssetLibrary/WorldBLDAssetLibrarySubsystem.h"
#include "Authorization/WorldBLDAuthSubsystem.h"
#include "AssetLibrary/WorldBLDAssetDownloadManager.h"

#include "Editor.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY(LogWorldBLDAssetLibrary);

void UWorldBLDAssetLibrarySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (GEditor)
	{
		AuthSubsystem = GEditor->GetEditorSubsystem<UWorldBLDAuthSubsystem>();
	}

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("AssetLibrary subsystem initialized"));
}

void UWorldBLDAssetLibrarySubsystem::Deinitialize()
{
	AuthSubsystem = nullptr;
	Super::Deinitialize();
}

void UWorldBLDAssetLibrarySubsystem::FetchAssets(int32 Page, int32 PerPage, const FString& Category)
{
	if (GEditor)
	{
		if (UWorldBLDAssetDownloadManager* DownloadManager = GEditor->GetEditorSubsystem<UWorldBLDAssetDownloadManager>())
		{
			DownloadManager->RefreshOwnershipStatus();
		}
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		OnAPIError.Broadcast(TEXT("HTTP Module unavailable"));
		return;
	}

	FString Url = FString::Printf(TEXT("%s/assets?page=%d&per_page=%d"), BaseURL, Page, PerPage);
	if (!Category.IsEmpty())
	{
		Url += FString::Printf(TEXT("&category=%s"), *Category);
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("=== FETCH ASSETS REQUEST ==="));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("URL: %s"), *Request->GetURL());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Method: %s"), *Request->GetVerb());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Header Content-Type: application/json"));

	Request->OnProcessRequestComplete().BindUObject(this, &UWorldBLDAssetLibrarySubsystem::OnFetchAssetsResponse);
	Request->ProcessRequest();
}

void UWorldBLDAssetLibrarySubsystem::FetchKits(int32 Page, int32 PerPage)
{
	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		OnAPIError.Broadcast(TEXT("HTTP Module unavailable"));
		return;
	}

	FString Url = FString::Printf(TEXT("%s/kits?page=%d&per_page=%d"), BaseURL, Page, PerPage);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("=== FETCH KITS REQUEST ==="));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("URL: %s"), *Request->GetURL());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Method: %s"), *Request->GetVerb());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Header Content-Type: application/json"));

	Request->OnProcessRequestComplete().BindUObject(this, &UWorldBLDAssetLibrarySubsystem::OnFetchKitsResponse);
	Request->ProcessRequest();
}

void UWorldBLDAssetLibrarySubsystem::FetchAssetDetails(const FString& AssetID)
{
	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		OnAPIError.Broadcast(TEXT("HTTP Module unavailable"));
		return;
	}

	FString Url = FString::Printf(TEXT("%s/assets/%s"), BaseURL, *AssetID);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("=== FETCH ASSET DETAILS REQUEST ==="));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("URL: %s"), *Request->GetURL());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Method: %s"), *Request->GetVerb());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Header Content-Type: application/json"));

	Request->OnProcessRequestComplete().BindUObject(this, &UWorldBLDAssetLibrarySubsystem::OnFetchAssetDetailsResponse);
	Request->ProcessRequest();
}

void UWorldBLDAssetLibrarySubsystem::FetchKitDetails(const FString& KitID)
{
	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		OnAPIError.Broadcast(TEXT("HTTP Module unavailable"));
		return;
	}

	FString Url = FString::Printf(TEXT("%s/kits/%s"), BaseURL, *KitID);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("=== FETCH KIT DETAILS REQUEST ==="));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("URL: %s"), *Request->GetURL());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Method: %s"), *Request->GetVerb());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Header Content-Type: application/json"));

	Request->OnProcessRequestComplete().BindUObject(this, &UWorldBLDAssetLibrarySubsystem::OnFetchKitDetailsResponse);
	Request->ProcessRequest();
}

void UWorldBLDAssetLibrarySubsystem::PurchaseAsset(const FString& AssetId)
{
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[PurchaseAsset] Starting purchase for asset: %s"), *AssetId);
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[PurchaseAsset] AuthSubsystem pointer: %p"), AuthSubsystem.Get());

	FString Token = GetAuthToken();
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[PurchaseAsset] Token retrieved: %s"), Token.IsEmpty() ? TEXT("EMPTY") : *Token.Left(20));

	if (Token.IsEmpty())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("[PurchaseAsset] Token is empty - not authenticated"));
		OnAPIError.Broadcast(TEXT("Not authenticated"));
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		OnAPIError.Broadcast(TEXT("HTTP Module unavailable"));
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(FString::Printf(TEXT("%s/assets/purchase"), BaseURL));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	// Use standard Authorization header for Sanctum
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Token));

	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	// Backend expects 'asset_id', not 'patchKitAppId'
	JsonObject->SetStringField(TEXT("asset_id"), AssetId);

	FString Content;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	Request->SetContentAsString(Content);

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("=== PURCHASE REQUEST ==="));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("URL: %s"), *Request->GetURL());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Method: %s"), *Request->GetVerb());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Header Content-Type: application/json"));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Header Authorization: Bearer %s..."), *Token.Left(20));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Body: %s"), *Content);

	Request->OnProcessRequestComplete().BindUObject(this, &UWorldBLDAssetLibrarySubsystem::OnPurchaseResponse);
	Request->ProcessRequest();
}

void UWorldBLDAssetLibrarySubsystem::FetchFreeAssets()
{
	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		OnAPIError.Broadcast(TEXT("HTTP Module unavailable"));
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(FString::Printf(TEXT("%s/assets/free"), BaseURL));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("=== FETCH FREE ASSETS REQUEST ==="));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("URL: %s"), *Request->GetURL());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Method: %s"), *Request->GetVerb());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Header Content-Type: application/json"));

	Request->OnProcessRequestComplete().BindUObject(this, &UWorldBLDAssetLibrarySubsystem::OnFetchAssetsResponse);
	Request->ProcessRequest();
}

void UWorldBLDAssetLibrarySubsystem::FetchFreeKits()
{
	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		OnAPIError.Broadcast(TEXT("HTTP Module unavailable"));
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(FString::Printf(TEXT("%s/kits/free"), BaseURL));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("=== FETCH FREE KITS REQUEST ==="));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("URL: %s"), *Request->GetURL());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Method: %s"), *Request->GetVerb());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Header Content-Type: application/json"));

	Request->OnProcessRequestComplete().BindUObject(this, &UWorldBLDAssetLibrarySubsystem::OnFetchKitsResponse);
	Request->ProcessRequest();
}

void UWorldBLDAssetLibrarySubsystem::RequestDownloadURL(const FString& AssetId, bool bIsKit)
{
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[RequestDownloadURL] Requesting download URL for asset: %s (IsKit: %s)"), *AssetId, bIsKit ? TEXT("true") : TEXT("false"));

	FString Token = GetAuthToken();
	if (Token.IsEmpty())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("[RequestDownloadURL] Not authenticated"));
		OnAPIError.Broadcast(TEXT("Not authenticated"));
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		OnAPIError.Broadcast(TEXT("HTTP Module unavailable"));
		return;
	}

	// Call backend endpoint: /api/downloads/asset/{id} or /api/downloads/kit/{id}
	// NOTE: Backend route requires POST request, not GET
	FString Endpoint = bIsKit ? TEXT("kit") : TEXT("asset");
	FString Url = FString::Printf(TEXT("%s/downloads/%s/%s"), BaseURL, *Endpoint, *AssetId);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));  // Backend route requires POST
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Token));

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("=== REQUEST DOWNLOAD URL ==="));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("URL: %s"), *Request->GetURL());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Method: %s"), *Request->GetVerb());
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Header Authorization: Bearer %s..."), *Token.Left(20));

	Request->OnProcessRequestComplete().BindUObject(this, &UWorldBLDAssetLibrarySubsystem::OnRequestDownloadURLResponse);
	Request->ProcessRequest();
}

void UWorldBLDAssetLibrarySubsystem::FetchAssetCollectionNames()
{
	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		OnAPIError.Broadcast(TEXT("HTTP Module unavailable"));
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(FString::Printf(TEXT("%s/assets/collection-names"), BaseURL));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	Request->OnProcessRequestComplete().BindUObject(this, &UWorldBLDAssetLibrarySubsystem::OnFetchAssetCollectionNamesResponse);
	Request->ProcessRequest();
}

void UWorldBLDAssetLibrarySubsystem::OnFetchAssetsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("=== FETCH ASSETS RESPONSE ==="));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Connected: %s"), bConnectedSuccessfully ? TEXT("YES") : TEXT("NO"));

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Connection failed or invalid response"));
		OnAPIError.Broadcast(TEXT("Connection failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Response Code: %d"), ResponseCode);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Failed to parse response"));
		OnAPIError.Broadcast(TEXT("Failed to parse response"));
		return;
	}

	if (ResponseCode != 200)
	{
		FString Message = JsonObject->HasField(TEXT("message")) ? JsonObject->GetStringField(TEXT("message")) : TEXT("Unknown error");
		if (Message.IsEmpty() && JsonObject->HasField(TEXT("error")))
		{
			Message = JsonObject->GetStringField(TEXT("error"));
		}
		OnAPIError.Broadcast(Message.IsEmpty() ? TEXT("Request failed") : Message);
		return;
	}

	if (!JsonObject->HasField(TEXT("data")))
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Response missing 'data' field"));
		OnAPIError.Broadcast(TEXT("Invalid response format"));
		return;
	}

	FWorldBLDAssetListResponse Parsed;

	const TArray<TSharedPtr<FJsonValue>>& DataArray = JsonObject->GetArrayField(TEXT("data"));
	for (const TSharedPtr<FJsonValue>& Value : DataArray)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			continue;
		}

		TSharedPtr<FJsonObject> AssetObject = Value->AsObject();
		if (!AssetObject.IsValid())
		{
			continue;
		}

		FWorldBLDAssetInfo Asset;
		Asset.ID = AssetObject->HasField(TEXT("id")) ? AssetObject->GetStringField(TEXT("id")) : TEXT("");
		Asset.Name = AssetObject->HasField(TEXT("name")) ? AssetObject->GetStringField(TEXT("name")) : TEXT("");
		Asset.Author = AssetObject->HasField(TEXT("author")) ? AssetObject->GetStringField(TEXT("author")) : TEXT("");
		Asset.Description = AssetObject->HasField(TEXT("description")) ? AssetObject->GetStringField(TEXT("description")) : TEXT("");
		Asset.PatchKitAppId = AssetObject->HasField(TEXT("patchKitAppId")) ? AssetObject->GetStringField(TEXT("patchKitAppId")) : TEXT("");
		Asset.CreditsPrice = AssetObject->HasField(TEXT("creditsPrice")) ? AssetObject->GetIntegerField(TEXT("creditsPrice")) : 0;
		Asset.bIsFree = AssetObject->HasField(TEXT("isFree")) ? AssetObject->GetBoolField(TEXT("isFree")) : false;
		Asset.Category = AssetObject->HasField(TEXT("category")) ? AssetObject->GetStringField(TEXT("category")) : TEXT("");

		// Parse thumbnail from productImage object or thumbnail field
		if (AssetObject->HasField(TEXT("productImage")))
		{
			TSharedPtr<FJsonObject> ProductImageObj = AssetObject->GetObjectField(TEXT("productImage"));
			if (ProductImageObj.IsValid() && ProductImageObj->HasField(TEXT("url")))
			{
				FString RelativeURL = ProductImageObj->GetStringField(TEXT("url"));
				Asset.ThumbnailURL = ConvertRelativeURLToAbsolute(RelativeURL);
			}
		}
		else if (AssetObject->HasField(TEXT("thumbnail")))
		{
			FString RelativeURL = AssetObject->GetStringField(TEXT("thumbnail"));
			Asset.ThumbnailURL = ConvertRelativeURLToAbsolute(RelativeURL);
		}
		else
		{
			Asset.ThumbnailURL = TEXT("");
		}

		if (AssetObject->HasField(TEXT("tags")))
		{
			const TArray<TSharedPtr<FJsonValue>>& TagsArray = AssetObject->GetArrayField(TEXT("tags"));
			for (const TSharedPtr<FJsonValue>& TagValue : TagsArray)
			{
				if (TagValue.IsValid() && TagValue->Type == EJson::String)
				{
					Asset.Tags.Add(TagValue->AsString());
				}
			}
		}

		// Parse human-readable features list (API: features[]). Most commonly an array of strings.
		{
			const TArray<TSharedPtr<FJsonValue>>* FeaturesArrayPtr = nullptr;
			if (AssetObject->TryGetArrayField(TEXT("features"), FeaturesArrayPtr) && FeaturesArrayPtr != nullptr)
			{
				for (const TSharedPtr<FJsonValue>& FeatureValue : *FeaturesArrayPtr)
				{
					if (!FeatureValue.IsValid())
					{
						continue;
					}

					FString FeatureText;
					switch (FeatureValue->Type)
					{
					case EJson::String:
						FeatureText = FeatureValue->AsString();
						break;
					case EJson::Number:
						FeatureText = FString::SanitizeFloat(FeatureValue->AsNumber());
						break;
					case EJson::Boolean:
						FeatureText = FeatureValue->AsBool() ? TEXT("true") : TEXT("false");
						break;
					default:
						break;
					}

					FeatureText = FeatureText.TrimStartAndEnd();
					if (!FeatureText.IsEmpty())
					{
						Asset.Features.Add(FeatureText);
					}
				}
			}
		}

		// Parse multi-collection names: collectionNames[] (preferred), or legacy collectionName (fallback).
		{
			auto AddCollectionName = [&Asset](const FString& InName)
			{
				const FString Trimmed = InName.TrimStartAndEnd();
				if (!Trimmed.IsEmpty())
				{
					Asset.CollectionNames.AddUnique(Trimmed);
				}
			};

			const TArray<TSharedPtr<FJsonValue>>* CollectionsArrayPtr = nullptr;
			if (AssetObject->TryGetArrayField(TEXT("collectionNames"), CollectionsArrayPtr) && CollectionsArrayPtr != nullptr)
			{
				for (const TSharedPtr<FJsonValue>& CollectionValue : *CollectionsArrayPtr)
				{
					if (CollectionValue.IsValid() && CollectionValue->Type == EJson::String)
					{
						AddCollectionName(CollectionValue->AsString());
					}
				}
			}
			else if (AssetObject->HasField(TEXT("collectionName")))
			{
				AddCollectionName(AssetObject->GetStringField(TEXT("collectionName")));
			}
		}

		// Backend uses 'preview_images' (underscore) in many responses.
		if (AssetObject->HasField(TEXT("preview_images")))
		{
			const TArray<TSharedPtr<FJsonValue>>& PreviewArray = AssetObject->GetArrayField(TEXT("preview_images"));
			for (const TSharedPtr<FJsonValue>& PreviewValue : PreviewArray)
			{
				if (PreviewValue.IsValid() && PreviewValue->Type == EJson::String)
				{
					FString RelativeURL = PreviewValue->AsString();
					Asset.PreviewImages.Add(ConvertRelativeURLToAbsolute(RelativeURL));
				}
			}
		}
		else if (AssetObject->HasField(TEXT("previewImages")))
		{
			const TArray<TSharedPtr<FJsonValue>>& PreviewArray = AssetObject->GetArrayField(TEXT("previewImages"));
			for (const TSharedPtr<FJsonValue>& PreviewValue : PreviewArray)
			{
				if (PreviewValue.IsValid() && PreviewValue->Type == EJson::String)
				{
					FString RelativeURL = PreviewValue->AsString();
					Asset.PreviewImages.Add(ConvertRelativeURLToAbsolute(RelativeURL));
				}
			}
		}

		// Parse rootAssetID and download URL
		Asset.RootAssetID = AssetObject->HasField(TEXT("rootAssetID")) ? AssetObject->GetStringField(TEXT("rootAssetID")) : TEXT("");
		Asset.DownloadURL = AssetObject->HasField(TEXT("downloadUrl")) ? AssetObject->GetStringField(TEXT("downloadUrl")) : TEXT("");

		auto ReadStringFieldAny = [](const TSharedPtr<FJsonObject>& Obj, std::initializer_list<const TCHAR*> Keys) -> FString
		{
			if (!Obj.IsValid())
			{
				return TEXT("");
			}

			for (const TCHAR* Key : Keys)
			{
				FString Out;
				if (Obj->TryGetStringField(Key, Out))
				{
					return Out;
				}
			}

			return TEXT("");
		};

		auto TryGetObjectFieldShared = [](const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, TSharedPtr<FJsonObject>& OutObject) -> bool
		{
			OutObject.Reset();
			if (!Obj.IsValid())
			{
				return false;
			}

			const TSharedPtr<FJsonObject>* OutPtr = nullptr;
			if (!Obj->TryGetObjectField(FStringView(FieldName), OutPtr) || OutPtr == nullptr || !OutPtr->IsValid())
			{
				return false;
			}

			OutObject = *OutPtr;
			return OutObject.IsValid();
		};

		// Parse manifest if present
		TSharedPtr<FJsonObject> ManifestObject;
		if (TryGetObjectFieldShared(AssetObject, TEXT("manifest"), ManifestObject))
		{
			Asset.Manifest.RootAssetID = ReadStringFieldAny(ManifestObject, { TEXT("rootAssetID"), TEXT("rootAssetId") });
			Asset.Manifest.AssetName = ReadStringFieldAny(ManifestObject, { TEXT("assetName"), TEXT("name") });
			Asset.Manifest.CollectionName = ReadStringFieldAny(ManifestObject, { TEXT("collectionName"), TEXT("collection_name") });

			// Parse dependencies array
			if (ManifestObject->HasField(TEXT("dependencies")))
			{
				const TArray<TSharedPtr<FJsonValue>>& DepsArray = ManifestObject->GetArrayField(TEXT("dependencies"));
				for (const TSharedPtr<FJsonValue>& DepValue : DepsArray)
				{
					if (DepValue.IsValid() && DepValue->Type == EJson::Object)
					{
						TSharedPtr<FJsonObject> DepObject = DepValue->AsObject();
						if (DepObject.IsValid())
						{
							FWorldBLDAssetDependency Dependency;
							Dependency.UniqueID = ReadStringFieldAny(DepObject, { TEXT("uniqueID"), TEXT("uniqueId"), TEXT("id") });
							Dependency.AssetName = ReadStringFieldAny(DepObject, { TEXT("assetName"), TEXT("name") });
							Dependency.AssetType = ReadStringFieldAny(DepObject, { TEXT("assetType"), TEXT("type") });
							Dependency.PackagePath = ReadStringFieldAny(DepObject, { TEXT("packagePath"), TEXT("package_path") });
							Dependency.SHA1Hash = ReadStringFieldAny(DepObject, { TEXT("sha1Hash"), TEXT("sha1_hash"), TEXT("sha1") });
							Dependency.FileSize = DepObject->HasField(TEXT("fileSize")) ? static_cast<int64>(DepObject->GetNumberField(TEXT("fileSize"))) : 0;

							Asset.Manifest.Dependencies.Add(MoveTemp(Dependency));
						}
					}
				}
			}
		}

		// If the list endpoint exposes collection name outside the manifest, accept it as a fallback.
		if (Asset.Manifest.CollectionName.IsEmpty())
		{
			Asset.Manifest.CollectionName = ReadStringFieldAny(AssetObject, { TEXT("collectionName"), TEXT("collection_name") });
		}

		// Final fallback: if multi-collection is empty, accept manifest collectionName.
		if (Asset.CollectionNames.IsEmpty() && !Asset.Manifest.CollectionName.IsEmpty())
		{
			Asset.CollectionNames.AddUnique(Asset.Manifest.CollectionName.TrimStartAndEnd());
		}

		// Parse requiredTools[] (if present). Shape (per logs / docs):
		// requiredTools: [{ "tool": "<toolId>", "minVersion": "1.0.6" }]
		{
			const TArray<TSharedPtr<FJsonValue>>* RequiredToolsArrayPtr = nullptr;
			if (AssetObject->TryGetArrayField(TEXT("requiredTools"), RequiredToolsArrayPtr) && RequiredToolsArrayPtr != nullptr)
			{
				for (const TSharedPtr<FJsonValue>& ToolValue : *RequiredToolsArrayPtr)
				{
					if (!ToolValue.IsValid() || ToolValue->Type != EJson::Object)
					{
						continue;
					}

					const TSharedPtr<FJsonObject> ToolObj = ToolValue->AsObject();
					if (!ToolObj.IsValid())
					{
						continue;
					}

					FWorldBLDRequiredTool Req;

					// Backend field name: "tool" (string id). Accept "toolId" as a defensive fallback.
					if (!ToolObj->TryGetStringField(TEXT("tool"), Req.ToolId))
					{
						ToolObj->TryGetStringField(TEXT("toolId"), Req.ToolId);
					}

					// Backend field name: "minVersion" (string).
					ToolObj->TryGetStringField(TEXT("minVersion"), Req.MinVersion);

					Req.ToolId = Req.ToolId.TrimStartAndEnd();
					Req.MinVersion = Req.MinVersion.TrimStartAndEnd();

					if (!Req.ToolId.IsEmpty())
					{
						Asset.RequiredTools.Add(MoveTemp(Req));
					}
				}
			}
		}

		// Parse primaryAssets[] (if present). These are the "core" assets for this entry.
		// Backend field name is expected to be "primaryAssets" but we accept common variants.
		{
			const TArray<TSharedPtr<FJsonValue>>* PrimaryAssetsArrayPtr = nullptr;
			if (!AssetObject->TryGetArrayField(TEXT("primaryAssets"), PrimaryAssetsArrayPtr) || PrimaryAssetsArrayPtr == nullptr)
			{
				(void)AssetObject->TryGetArrayField(TEXT("primary_assets"), PrimaryAssetsArrayPtr);
			}
			if (PrimaryAssetsArrayPtr != nullptr)
			{
				for (const TSharedPtr<FJsonValue>& V : *PrimaryAssetsArrayPtr)
				{
					if (V.IsValid() && V->Type == EJson::String)
					{
						const FString Trimmed = V->AsString().TrimStartAndEnd();
						if (!Trimmed.IsEmpty())
						{
							Asset.PrimaryAssets.Add(Trimmed);
						}
					}
				}
			}
		}

		Parsed.Data.Add(MoveTemp(Asset));
	}

	if (JsonObject->HasField(TEXT("meta")))
	{
		TSharedPtr<FJsonObject> MetaObject = JsonObject->GetObjectField(TEXT("meta"));
		if (MetaObject.IsValid())
		{
			Parsed.CurrentPage = MetaObject->HasField(TEXT("current_page")) ? MetaObject->GetIntegerField(TEXT("current_page")) : 0;
			Parsed.LastPage = MetaObject->HasField(TEXT("last_page")) ? MetaObject->GetIntegerField(TEXT("last_page")) : 0;
			Parsed.PerPage = MetaObject->HasField(TEXT("per_page")) ? MetaObject->GetIntegerField(TEXT("per_page")) : 0;
			Parsed.Total = MetaObject->HasField(TEXT("total")) ? MetaObject->GetIntegerField(TEXT("total")) : 0;
		}
	}

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Parsed %d assets"), Parsed.Data.Num());
	OnAssetsFetched.Broadcast(Parsed);
}

void UWorldBLDAssetLibrarySubsystem::OnFetchAssetCollectionNamesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	(void)Request;

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		OnAPIError.Broadcast(TEXT("Connection failed"));
		return;
	}

	const int32 ResponseCode = Response->GetResponseCode();
	const FString ResponseContent = Response->GetContentAsString();

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OnAPIError.Broadcast(TEXT("Failed to parse response"));
		return;
	}

	if (ResponseCode != 200)
	{
		FString Message = JsonObject->HasField(TEXT("message")) ? JsonObject->GetStringField(TEXT("message")) : TEXT("Unknown error");
		if (Message.IsEmpty() && JsonObject->HasField(TEXT("error")))
		{
			Message = JsonObject->GetStringField(TEXT("error"));
		}
		OnAPIError.Broadcast(Message.IsEmpty() ? TEXT("Request failed") : Message);
		return;
	}

	FWorldBLDAssetCollectionNamesResponse Parsed;
	Parsed.bSuccess = JsonObject->HasField(TEXT("success")) ? JsonObject->GetBoolField(TEXT("success")) : true;

	const TArray<TSharedPtr<FJsonValue>>* CollectionsArrayPtr = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("collectionNames"), CollectionsArrayPtr) && CollectionsArrayPtr != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& CollectionValue : *CollectionsArrayPtr)
		{
			if (CollectionValue.IsValid() && CollectionValue->Type == EJson::String)
			{
				const FString Trimmed = CollectionValue->AsString().TrimStartAndEnd();
				if (!Trimmed.IsEmpty())
				{
					Parsed.CollectionNames.AddUnique(Trimmed);
				}
			}
		}
	}

	OnAssetCollectionNamesFetched.Broadcast(Parsed);
}

void UWorldBLDAssetLibrarySubsystem::OnFetchKitsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("=== FETCH KITS RESPONSE ==="));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Connected: %s"), bConnectedSuccessfully ? TEXT("YES") : TEXT("NO"));

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Connection failed or invalid response"));
		OnAPIError.Broadcast(TEXT("Connection failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Response Code: %d"), ResponseCode);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Failed to parse response"));
		OnAPIError.Broadcast(TEXT("Failed to parse response"));
		return;
	}

	if (ResponseCode != 200)
	{
		FString Message = JsonObject->HasField(TEXT("message")) ? JsonObject->GetStringField(TEXT("message")) : TEXT("Unknown error");
		if (Message.IsEmpty() && JsonObject->HasField(TEXT("error")))
		{
			Message = JsonObject->GetStringField(TEXT("error"));
		}
		OnAPIError.Broadcast(Message.IsEmpty() ? TEXT("Request failed") : Message);
		return;
	}

	if (!JsonObject->HasField(TEXT("data")))
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Response missing 'data' field"));
		OnAPIError.Broadcast(TEXT("Invalid response format"));
		return;
	}

	FWorldBLDKitListResponse Parsed;

	const TArray<TSharedPtr<FJsonValue>>& DataArray = JsonObject->GetArrayField(TEXT("data"));
	for (const TSharedPtr<FJsonValue>& Value : DataArray)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			continue;
		}

		TSharedPtr<FJsonObject> KitObject = Value->AsObject();
		if (!KitObject.IsValid())
		{
			continue;
		}

		FWorldBLDKitInfo Kit;
		Kit.ID = KitObject->HasField(TEXT("id")) ? KitObject->GetStringField(TEXT("id")) : TEXT("");
		Kit.KitsName = KitObject->HasField(TEXT("kitsName")) ? KitObject->GetStringField(TEXT("kitsName")) : TEXT("");
		Kit.Description = KitObject->HasField(TEXT("description")) ? KitObject->GetStringField(TEXT("description")) : TEXT("");
		Kit.PatchKitAppId = KitObject->HasField(TEXT("patchKitAppId")) ? KitObject->GetStringField(TEXT("patchKitAppId")) : TEXT("");
		Kit.Price = KitObject->HasField(TEXT("price")) ? KitObject->GetIntegerField(TEXT("price")) : 0;
		Kit.bIsFree = KitObject->HasField(TEXT("isFree")) ? KitObject->GetBoolField(TEXT("isFree")) : false;

		if (KitObject->HasField(TEXT("tags")))
		{
			const TArray<TSharedPtr<FJsonValue>>& TagsArray = KitObject->GetArrayField(TEXT("tags"));
			for (const TSharedPtr<FJsonValue>& TagValue : TagsArray)
			{
				if (TagValue.IsValid() && TagValue->Type == EJson::String)
				{
					Kit.Tags.Add(TagValue->AsString());
				}
			}
		}

		if (KitObject->HasField(TEXT("previewImages")))
		{
			const TArray<TSharedPtr<FJsonValue>>& PreviewArray = KitObject->GetArrayField(TEXT("previewImages"));
			for (const TSharedPtr<FJsonValue>& PreviewValue : PreviewArray)
			{
				if (PreviewValue.IsValid() && PreviewValue->Type == EJson::String)
				{
					Kit.PreviewImages.Add(PreviewValue->AsString());
				}
			}
		}

		Parsed.Data.Add(MoveTemp(Kit));
	}

	if (JsonObject->HasField(TEXT("meta")))
	{
		TSharedPtr<FJsonObject> MetaObject = JsonObject->GetObjectField(TEXT("meta"));
		if (MetaObject.IsValid())
		{
			Parsed.CurrentPage = MetaObject->HasField(TEXT("current_page")) ? MetaObject->GetIntegerField(TEXT("current_page")) : 0;
			Parsed.LastPage = MetaObject->HasField(TEXT("last_page")) ? MetaObject->GetIntegerField(TEXT("last_page")) : 0;
			Parsed.PerPage = MetaObject->HasField(TEXT("per_page")) ? MetaObject->GetIntegerField(TEXT("per_page")) : 0;
			Parsed.Total = MetaObject->HasField(TEXT("total")) ? MetaObject->GetIntegerField(TEXT("total")) : 0;
		}
	}

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Parsed %d kits"), Parsed.Data.Num());
	OnKitsFetched.Broadcast(Parsed);
}

void UWorldBLDAssetLibrarySubsystem::OnFetchAssetDetailsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("=== FETCH ASSET DETAILS RESPONSE ==="));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Connected: %s"), bConnectedSuccessfully ? TEXT("YES") : TEXT("NO"));

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Connection failed or invalid response"));
		OnAPIError.Broadcast(TEXT("Connection failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Response Code: %d"), ResponseCode);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Failed to parse response"));
		OnAPIError.Broadcast(TEXT("Failed to parse response"));
		return;
	}

	if (ResponseCode != 200)
	{
		FString Message = JsonObject->HasField(TEXT("message")) ? JsonObject->GetStringField(TEXT("message")) : TEXT("Unknown error");
		if (Message.IsEmpty() && JsonObject->HasField(TEXT("error")))
		{
			Message = JsonObject->GetStringField(TEXT("error"));
		}
		OnAPIError.Broadcast(Message.IsEmpty() ? TEXT("Request failed") : Message);
		return;
	}

	// Backend response has 'asset' field, not 'data'
	TSharedPtr<FJsonObject> AssetObject = JsonObject;
	if (JsonObject->HasField(TEXT("asset")))
	{
		TSharedPtr<FJsonObject> DataObject = JsonObject->GetObjectField(TEXT("asset"));
		if (DataObject.IsValid())
		{
			AssetObject = DataObject;
		}
	}
	else if (JsonObject->HasField(TEXT("data")))
	{
		TSharedPtr<FJsonObject> DataObject = JsonObject->GetObjectField(TEXT("data"));
		if (DataObject.IsValid())
		{
			AssetObject = DataObject;
		}
	}

	FWorldBLDAssetInfo Asset;
	Asset.ID = AssetObject->HasField(TEXT("id")) ? AssetObject->GetStringField(TEXT("id")) : TEXT("");
	Asset.Name = AssetObject->HasField(TEXT("name")) ? AssetObject->GetStringField(TEXT("name")) : TEXT("");
	Asset.Author = AssetObject->HasField(TEXT("author")) ? AssetObject->GetStringField(TEXT("author")) : TEXT("");
	Asset.Description = AssetObject->HasField(TEXT("description")) ? AssetObject->GetStringField(TEXT("description")) : TEXT("");
	Asset.PatchKitAppId = AssetObject->HasField(TEXT("patchKitAppId")) ? AssetObject->GetStringField(TEXT("patchKitAppId")) : TEXT("");
	Asset.CreditsPrice = AssetObject->HasField(TEXT("creditsPrice")) ? AssetObject->GetIntegerField(TEXT("creditsPrice")) : 0;
	Asset.bIsFree = AssetObject->HasField(TEXT("isFree")) ? AssetObject->GetBoolField(TEXT("isFree")) : false;
	Asset.Category = AssetObject->HasField(TEXT("category")) ? AssetObject->GetStringField(TEXT("category")) : TEXT("");

	// Parse thumbnail from productImage object or thumbnail field
	if (AssetObject->HasField(TEXT("productImage")))
	{
		TSharedPtr<FJsonObject> ProductImageObj = AssetObject->GetObjectField(TEXT("productImage"));
		if (ProductImageObj.IsValid() && ProductImageObj->HasField(TEXT("url")))
		{
			FString RelativeURL = ProductImageObj->GetStringField(TEXT("url"));
			Asset.ThumbnailURL = ConvertRelativeURLToAbsolute(RelativeURL);
			UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Product image URL: %s"), *Asset.ThumbnailURL);
		}
	}
	else if (AssetObject->HasField(TEXT("thumbnail")))
	{
		FString RelativeURL = AssetObject->GetStringField(TEXT("thumbnail"));
		Asset.ThumbnailURL = ConvertRelativeURLToAbsolute(RelativeURL);
	}

	if (AssetObject->HasField(TEXT("tags")))
	{
		const TArray<TSharedPtr<FJsonValue>>& TagsArray = AssetObject->GetArrayField(TEXT("tags"));
		for (const TSharedPtr<FJsonValue>& TagValue : TagsArray)
		{
			if (TagValue.IsValid() && TagValue->Type == EJson::String)
			{
				Asset.Tags.Add(TagValue->AsString());
			}
		}
	}

	// Parse human-readable features list (API: features[]). Most commonly an array of strings.
	{
		const TArray<TSharedPtr<FJsonValue>>* FeaturesArrayPtr = nullptr;
		if (AssetObject->TryGetArrayField(TEXT("features"), FeaturesArrayPtr) && FeaturesArrayPtr != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& FeatureValue : *FeaturesArrayPtr)
			{
				if (!FeatureValue.IsValid())
				{
					continue;
				}

				FString FeatureText;
				switch (FeatureValue->Type)
				{
				case EJson::String:
					FeatureText = FeatureValue->AsString();
					break;
				case EJson::Number:
					FeatureText = FString::SanitizeFloat(FeatureValue->AsNumber());
					break;
				case EJson::Boolean:
					FeatureText = FeatureValue->AsBool() ? TEXT("true") : TEXT("false");
					break;
				default:
					break;
				}

				FeatureText = FeatureText.TrimStartAndEnd();
				if (!FeatureText.IsEmpty())
				{
					Asset.Features.Add(FeatureText);
				}
			}
		}
	}

	// Parse multi-collection names: collectionNames[] (preferred), or legacy collectionName (fallback).
	{
		auto AddCollectionName = [&Asset](const FString& InName)
		{
			const FString Trimmed = InName.TrimStartAndEnd();
			if (!Trimmed.IsEmpty())
			{
				Asset.CollectionNames.AddUnique(Trimmed);
			}
		};

		const TArray<TSharedPtr<FJsonValue>>* CollectionsArrayPtr = nullptr;
		if (AssetObject->TryGetArrayField(TEXT("collectionNames"), CollectionsArrayPtr) && CollectionsArrayPtr != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& CollectionValue : *CollectionsArrayPtr)
			{
				if (CollectionValue.IsValid() && CollectionValue->Type == EJson::String)
				{
					AddCollectionName(CollectionValue->AsString());
				}
			}
		}
		else if (AssetObject->HasField(TEXT("collectionName")))
		{
			AddCollectionName(AssetObject->GetStringField(TEXT("collectionName")));
		}
	}

	// Backend uses 'preview_images' (underscore), not 'previewImages' (camelCase)
	if (AssetObject->HasField(TEXT("preview_images")))
	{
		const TArray<TSharedPtr<FJsonValue>>& PreviewArray = AssetObject->GetArrayField(TEXT("preview_images"));
		UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Found %d preview images"), PreviewArray.Num());
		for (const TSharedPtr<FJsonValue>& PreviewValue : PreviewArray)
		{
			if (PreviewValue.IsValid() && PreviewValue->Type == EJson::String)
			{
				FString RelativeURL = PreviewValue->AsString();
				FString AbsoluteURL = ConvertRelativeURLToAbsolute(RelativeURL);
				Asset.PreviewImages.Add(AbsoluteURL);
				UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Preview image: %s"), *AbsoluteURL);
			}
		}
	}
	else if (AssetObject->HasField(TEXT("previewImages")))
	{
		const TArray<TSharedPtr<FJsonValue>>& PreviewArray = AssetObject->GetArrayField(TEXT("previewImages"));
		for (const TSharedPtr<FJsonValue>& PreviewValue : PreviewArray)
		{
			if (PreviewValue.IsValid() && PreviewValue->Type == EJson::String)
			{
				FString RelativeURL = PreviewValue->AsString();
				FString AbsoluteURL = ConvertRelativeURLToAbsolute(RelativeURL);
				Asset.PreviewImages.Add(AbsoluteURL);
			}
		}
	}

	// Parse rootAssetID and download URL
	Asset.RootAssetID = AssetObject->HasField(TEXT("rootAssetID")) ? AssetObject->GetStringField(TEXT("rootAssetID")) : TEXT("");
	Asset.DownloadURL = AssetObject->HasField(TEXT("downloadUrl")) ? AssetObject->GetStringField(TEXT("downloadUrl")) : TEXT("");

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Asset download URL: %s"), *Asset.DownloadURL);

	// Parse manifest if present (for dependency tracking)
	auto ReadStringFieldAny = [](const TSharedPtr<FJsonObject>& Obj, std::initializer_list<const TCHAR*> Keys) -> FString
	{
		if (!Obj.IsValid())
		{
			return TEXT("");
		}

		for (const TCHAR* Key : Keys)
		{
			FString Out;
			if (Obj->TryGetStringField(Key, Out))
			{
				return Out;
			}
		}

		return TEXT("");
	};

	auto TryGetObjectFieldShared = [](const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, TSharedPtr<FJsonObject>& OutObject) -> bool
	{
		OutObject.Reset();
		if (!Obj.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* OutPtr = nullptr;
		if (!Obj->TryGetObjectField(FStringView(FieldName), OutPtr) || OutPtr == nullptr || !OutPtr->IsValid())
		{
			return false;
		}

		OutObject = *OutPtr;
		return OutObject.IsValid();
	};

	TSharedPtr<FJsonObject> ManifestObject;
	if (TryGetObjectFieldShared(AssetObject, TEXT("manifest"), ManifestObject))
	{
		Asset.Manifest.RootAssetID = ReadStringFieldAny(ManifestObject, { TEXT("rootAssetID"), TEXT("rootAssetId") });
		Asset.Manifest.AssetName = ReadStringFieldAny(ManifestObject, { TEXT("assetName"), TEXT("name") });
		Asset.Manifest.CollectionName = ReadStringFieldAny(ManifestObject, { TEXT("collectionName"), TEXT("collection_name") });

		// Parse dependencies array
		if (ManifestObject->HasField(TEXT("dependencies")))
		{
			const TArray<TSharedPtr<FJsonValue>>& DepsArray = ManifestObject->GetArrayField(TEXT("dependencies"));
			UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Parsing %d dependencies from manifest"), DepsArray.Num());
			for (const TSharedPtr<FJsonValue>& DepValue : DepsArray)
			{
				if (DepValue.IsValid() && DepValue->Type == EJson::Object)
				{
					TSharedPtr<FJsonObject> DepObject = DepValue->AsObject();
					if (DepObject.IsValid())
					{
						FWorldBLDAssetDependency Dependency;
						Dependency.UniqueID = ReadStringFieldAny(DepObject, { TEXT("uniqueID"), TEXT("uniqueId"), TEXT("id") });
						Dependency.AssetName = ReadStringFieldAny(DepObject, { TEXT("assetName"), TEXT("name") });
						Dependency.AssetType = ReadStringFieldAny(DepObject, { TEXT("assetType"), TEXT("type") });
						Dependency.PackagePath = ReadStringFieldAny(DepObject, { TEXT("packagePath"), TEXT("package_path") });
						Dependency.SHA1Hash = ReadStringFieldAny(DepObject, { TEXT("sha1Hash"), TEXT("sha1_hash"), TEXT("sha1") });
						Dependency.FileSize = DepObject->HasField(TEXT("fileSize")) ? static_cast<int64>(DepObject->GetNumberField(TEXT("fileSize"))) : 0;

						Asset.Manifest.Dependencies.Add(MoveTemp(Dependency));
					}
				}
			}
			UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Parsed %d dependencies total"), Asset.Manifest.Dependencies.Num());
		}
	}

	// Fallback: accept top-level collection field if present.
	if (Asset.Manifest.CollectionName.IsEmpty())
	{
		Asset.Manifest.CollectionName = ReadStringFieldAny(AssetObject, { TEXT("collectionName"), TEXT("collection_name") });
	}

	// Final fallback: if multi-collection is empty, accept manifest collectionName.
	if (Asset.CollectionNames.IsEmpty() && !Asset.Manifest.CollectionName.IsEmpty())
	{
		Asset.CollectionNames.AddUnique(Asset.Manifest.CollectionName.TrimStartAndEnd());
	}

	// Parse requiredTools[] (if present). Shape (per logs / docs):
	// requiredTools: [{ "tool": "<toolId>", "minVersion": "1.0.6" }]
	{
		const TArray<TSharedPtr<FJsonValue>>* RequiredToolsArrayPtr = nullptr;
		if (AssetObject->TryGetArrayField(TEXT("requiredTools"), RequiredToolsArrayPtr) && RequiredToolsArrayPtr != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& ToolValue : *RequiredToolsArrayPtr)
			{
				if (!ToolValue.IsValid() || ToolValue->Type != EJson::Object)
				{
					continue;
				}

				const TSharedPtr<FJsonObject> ToolObj = ToolValue->AsObject();
				if (!ToolObj.IsValid())
				{
					continue;
				}

				FWorldBLDRequiredTool Req;
				if (!ToolObj->TryGetStringField(TEXT("tool"), Req.ToolId))
				{
					ToolObj->TryGetStringField(TEXT("toolId"), Req.ToolId);
				}
				ToolObj->TryGetStringField(TEXT("minVersion"), Req.MinVersion);

				Req.ToolId = Req.ToolId.TrimStartAndEnd();
				Req.MinVersion = Req.MinVersion.TrimStartAndEnd();

				if (!Req.ToolId.IsEmpty())
				{
					Asset.RequiredTools.Add(MoveTemp(Req));
				}
			}
		}
	}

	// Parse primaryAssets[] (if present). These are the "core" assets for this entry.
	// Backend field name is expected to be "primaryAssets" but we accept common variants.
	{
		const TArray<TSharedPtr<FJsonValue>>* PrimaryAssetsArrayPtr = nullptr;
		if (!AssetObject->TryGetArrayField(TEXT("primaryAssets"), PrimaryAssetsArrayPtr) || PrimaryAssetsArrayPtr == nullptr)
		{
			(void)AssetObject->TryGetArrayField(TEXT("primary_assets"), PrimaryAssetsArrayPtr);
		}
		if (PrimaryAssetsArrayPtr != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& V : *PrimaryAssetsArrayPtr)
			{
				if (V.IsValid() && V->Type == EJson::String)
				{
					const FString Trimmed = V->AsString().TrimStartAndEnd();
					if (!Trimmed.IsEmpty())
					{
						Asset.PrimaryAssets.Add(Trimmed);
					}
				}
			}
		}
	}

	OnAssetDetailsFetched.Broadcast(Asset);
}

void UWorldBLDAssetLibrarySubsystem::OnFetchKitDetailsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("=== FETCH KIT DETAILS RESPONSE ==="));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Connected: %s"), bConnectedSuccessfully ? TEXT("YES") : TEXT("NO"));

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Connection failed or invalid response"));
		OnAPIError.Broadcast(TEXT("Connection failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Response Code: %d"), ResponseCode);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Failed to parse response"));
		OnAPIError.Broadcast(TEXT("Failed to parse response"));
		return;
	}

	if (ResponseCode != 200)
	{
		FString Message = JsonObject->HasField(TEXT("message")) ? JsonObject->GetStringField(TEXT("message")) : TEXT("Unknown error");
		if (Message.IsEmpty() && JsonObject->HasField(TEXT("error")))
		{
			Message = JsonObject->GetStringField(TEXT("error"));
		}
		OnAPIError.Broadcast(Message.IsEmpty() ? TEXT("Request failed") : Message);
		return;
	}

	TSharedPtr<FJsonObject> KitObject = JsonObject;
	if (JsonObject->HasField(TEXT("data")))
	{
		TSharedPtr<FJsonObject> DataObject = JsonObject->GetObjectField(TEXT("data"));
		if (DataObject.IsValid())
		{
			KitObject = DataObject;
		}
	}

	FWorldBLDKitInfo Kit;
	Kit.ID = KitObject->HasField(TEXT("id")) ? KitObject->GetStringField(TEXT("id")) : TEXT("");
	Kit.KitsName = KitObject->HasField(TEXT("kitsName")) ? KitObject->GetStringField(TEXT("kitsName")) : TEXT("");
	Kit.Description = KitObject->HasField(TEXT("description")) ? KitObject->GetStringField(TEXT("description")) : TEXT("");
	Kit.PatchKitAppId = KitObject->HasField(TEXT("patchKitAppId")) ? KitObject->GetStringField(TEXT("patchKitAppId")) : TEXT("");
	Kit.Price = KitObject->HasField(TEXT("price")) ? KitObject->GetIntegerField(TEXT("price")) : 0;
	Kit.bIsFree = KitObject->HasField(TEXT("isFree")) ? KitObject->GetBoolField(TEXT("isFree")) : false;

	if (KitObject->HasField(TEXT("tags")))
	{
		const TArray<TSharedPtr<FJsonValue>>& TagsArray = KitObject->GetArrayField(TEXT("tags"));
		for (const TSharedPtr<FJsonValue>& TagValue : TagsArray)
		{
			if (TagValue.IsValid() && TagValue->Type == EJson::String)
			{
				Kit.Tags.Add(TagValue->AsString());
			}
		}
	}

	if (KitObject->HasField(TEXT("previewImages")))
	{
		const TArray<TSharedPtr<FJsonValue>>& PreviewArray = KitObject->GetArrayField(TEXT("previewImages"));
		for (const TSharedPtr<FJsonValue>& PreviewValue : PreviewArray)
		{
			if (PreviewValue.IsValid() && PreviewValue->Type == EJson::String)
			{
				Kit.PreviewImages.Add(PreviewValue->AsString());
			}
		}
	}

	OnKitDetailsFetched.Broadcast(Kit);
}

void UWorldBLDAssetLibrarySubsystem::OnPurchaseResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("=== PURCHASE RESPONSE ==="));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Connected: %s"), bConnectedSuccessfully ? TEXT("YES") : TEXT("NO"));

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Connection failed or invalid response"));
		OnAPIError.Broadcast(TEXT("Connection failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Response Code: %d"), ResponseCode);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Failed to parse response"));
		OnAPIError.Broadcast(TEXT("Failed to parse response"));
		return;
	}

	if (ResponseCode == 200)
	{
		const bool bSuccess = JsonObject->HasField(TEXT("success")) ? JsonObject->GetBoolField(TEXT("success")) : true;

		// Refresh ownership state immediately so the UI updates without requiring the asset library window to be reopened.
		// (FetchAssets() already does this on open; purchasing previously did not.)
		if (bSuccess && GEditor)
		{
			if (UWorldBLDAssetDownloadManager* DownloadManager = GEditor->GetEditorSubsystem<UWorldBLDAssetDownloadManager>())
			{
				DownloadManager->RefreshOwnershipStatus();
			}
		}

		// IMPORTANT:
		// The details panel updates the displayed credits by calling AuthSubsystem->SetCachedUserCredits(RemainingCredits)
		// when a purchase succeeds. If we incorrectly parse RemainingCredits as 0 (e.g., wrong JSON field),
		// the UI will temporarily show 0 and block further purchases until ValidateSession runs again.
		auto TryParseCreditsFromObject = [](const TSharedPtr<FJsonObject>& Obj, int32& OutCredits) -> bool
		{
			if (!Obj.IsValid())
			{
				return false;
			}

			auto TryNum = [&OutCredits, &Obj](const TCHAR* Key) -> bool
			{
				if (Obj->HasTypedField<EJson::Number>(Key))
				{
					OutCredits = Obj->GetIntegerField(Key);
					return true;
				}
				return false;
			};

			// Common formats across endpoints (session uses user.credits; purchase may vary).
			return TryNum(TEXT("credits"))
				|| TryNum(TEXT("userCredits"))
				|| TryNum(TEXT("remainingCredits"))
				|| TryNum(TEXT("remaining_credits"));
		};

		// IMPORTANT:
		// RemainingCredits is forwarded to Slate and used to update the cached user credits displayed in the UI.
		// Some purchase responses don't include credits at all (or include them in an unexpected format). In those cases,
		// we must NOT forward 0, because the details panel will treat that as a real value and overwrite the cache,
		// causing the "Your Credits" display to reset to 0 until session validation completes.
		int32 Credits = INDEX_NONE; // sentinel = unknown/unprovided
		bool bHasCredits = TryParseCreditsFromObject(JsonObject, Credits);

		// Nested formats: { user: { credits: ... } } or { data: { credits: ... } } or { data: { user: { credits: ... } } }
		if (!bHasCredits && JsonObject->HasTypedField<EJson::Object>(TEXT("user")))
		{
			bHasCredits = TryParseCreditsFromObject(JsonObject->GetObjectField(TEXT("user")), Credits);
		}

		if (!bHasCredits && JsonObject->HasTypedField<EJson::Object>(TEXT("data")))
		{
			const TSharedPtr<FJsonObject> DataObject = JsonObject->GetObjectField(TEXT("data"));
			bHasCredits = TryParseCreditsFromObject(DataObject, Credits);
			if (!bHasCredits && DataObject.IsValid() && DataObject->HasTypedField<EJson::Object>(TEXT("user")))
			{
				bHasCredits = TryParseCreditsFromObject(DataObject->GetObjectField(TEXT("user")), Credits);
			}
		}

		// If the response doesn't include credits at all, don't clobber the cached value.
		// Opportunistically re-validate in the background and forward INDEX_NONE to indicate "unknown".
		if (!bHasCredits)
		{
			if (UWorldBLDAuthSubsystem* AuthSubsystemPtr = AuthSubsystem.Get())
			{
				AuthSubsystemPtr->ValidateSession();
			}
			Credits = INDEX_NONE;
		}
		else
		{
			Credits = FMath::Max(0, Credits);
		}

		OnPurchaseComplete.Broadcast(bSuccess, Credits);
		return;
	}

	if (ResponseCode == 401)
	{
		OnAPIError.Broadcast(TEXT("Unauthorized - please re-login"));
		return;
	}

	if (ResponseCode == 404)
	{
		OnAPIError.Broadcast(TEXT("Item not found"));
		return;
	}

	if (ResponseCode == 500)
	{
		OnAPIError.Broadcast(TEXT("Purchase failed"));
		return;
	}

	if (ResponseCode == 400)
	{
		FString Error = JsonObject->HasField(TEXT("error")) ? JsonObject->GetStringField(TEXT("error")) : TEXT("");
		FString Message = JsonObject->HasField(TEXT("message")) ? JsonObject->GetStringField(TEXT("message")) : TEXT("");

		// Special handling for "already-owned" - refresh ownership status
		if (Error == TEXT("already-owned"))
		{
			UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[OnPurchaseResponse] Asset already owned"));

			if (GEditor)
			{
				if (UWorldBLDAssetDownloadManager* DownloadManager = GEditor->GetEditorSubsystem<UWorldBLDAssetDownloadManager>())
				{
					DownloadManager->RefreshOwnershipStatus();
				}
			}

			// Broadcast error message to user
			OnAPIError.Broadcast(Message.IsEmpty() ? TEXT("You already own this asset") : Message);
			return;
		}

		FString Combined = !Error.IsEmpty() ? Error : Message;
		OnAPIError.Broadcast(Combined.IsEmpty() ? TEXT("Purchase request failed") : Combined);
		return;
	}

	FString Message = JsonObject->HasField(TEXT("message")) ? JsonObject->GetStringField(TEXT("message")) : TEXT("Purchase request failed");
	OnAPIError.Broadcast(Message.IsEmpty() ? TEXT("Purchase request failed") : Message);
}

FString UWorldBLDAssetLibrarySubsystem::GetAuthToken() const
{
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[GetAuthToken] AuthSubsystem pointer: %p"), AuthSubsystem.Get());

	// If AuthSubsystem is null, try to fetch it (this shouldn't happen, but let's be defensive)
	UWorldBLDAuthSubsystem* AuthSubsystemPtr = AuthSubsystem.Get();
	if (!AuthSubsystemPtr && GEditor)
	{
		UE_LOG(LogWorldBLDAssetLibrary, Warning, TEXT("[GetAuthToken] AuthSubsystem is null, attempting to fetch it..."));
		AuthSubsystemPtr = GEditor->GetEditorSubsystem<UWorldBLDAuthSubsystem>();
		if (AuthSubsystemPtr)
		{
			UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[GetAuthToken] Successfully fetched AuthSubsystem on-demand"));
		}
	}

	if (!AuthSubsystemPtr)
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("[GetAuthToken] AuthSubsystem is null and couldn't be fetched!"));
		return TEXT("");
	}

	FString Token = AuthSubsystemPtr->GetSessionToken();
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[GetAuthToken] Token from AuthSubsystem: %s"), Token.IsEmpty() ? TEXT("EMPTY") : *Token.Left(20));
	return Token;
}

FString UWorldBLDAssetLibrarySubsystem::ConvertRelativeURLToAbsolute(const FString& RelativeURL) const
{
	// If URL is already absolute (starts with http:// or https://), return as-is
	if (RelativeURL.StartsWith(TEXT("http://")) || RelativeURL.StartsWith(TEXT("https://")))
	{
		return RelativeURL;
	}

	// Convert relative URL to absolute by prepending base domain
	// BaseURL is "https://worldbld.com/api", we need just "https://worldbld.com"
	FString BaseDomain = TEXT("https://worldbld.com");

	// Remove leading slash if present (e.g., "/storage/..." -> "storage/...")
	FString CleanRelativeURL = RelativeURL;
	if (CleanRelativeURL.StartsWith(TEXT("/")))
	{
		// Keep the leading slash for absolute paths
		return BaseDomain + CleanRelativeURL;
	}

	return BaseDomain + TEXT("/") + CleanRelativeURL;
}

void UWorldBLDAssetLibrarySubsystem::OnRequestDownloadURLResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("=== REQUEST DOWNLOAD URL RESPONSE ==="));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Connected: %s"), bConnectedSuccessfully ? TEXT("YES") : TEXT("NO"));

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Connection failed or invalid response"));
		OnAPIError.Broadcast(TEXT("Connection failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("Response Code: %d"), ResponseCode);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Failed to parse response"));
		OnAPIError.Broadcast(TEXT("Failed to parse response"));
		return;
	}

	if (ResponseCode != 200)
	{
		FString Message = JsonObject->HasField(TEXT("message")) ? JsonObject->GetStringField(TEXT("message")) : TEXT("Unknown error");
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Request failed: %s"), *Message);
		OnAPIError.Broadcast(Message);
		return;
	}

	// Check if response is successful
	bool bSuccess = JsonObject->HasField(TEXT("success")) ? JsonObject->GetBoolField(TEXT("success")) : false;
	if (!bSuccess)
	{
		FString Message = JsonObject->HasField(TEXT("message")) ? JsonObject->GetStringField(TEXT("message")) : TEXT("Download URL request failed");
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Download URL request failed: %s"), *Message);
		OnAPIError.Broadcast(Message);
		return;
	}

	// Extract download URL from response
	FString DownloadURL = JsonObject->HasField(TEXT("download_url")) ? JsonObject->GetStringField(TEXT("download_url")) : TEXT("");
	if (DownloadURL.IsEmpty())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("Download URL is empty in response"));
		OnAPIError.Broadcast(TEXT("Download URL not provided"));
		return;
	}

	// Extract asset name from response
	FString AssetName = JsonObject->HasField(TEXT("asset_name")) ? JsonObject->GetStringField(TEXT("asset_name")) : TEXT("");

	// Extract asset/kit ID from the request URL
	FString RequestURL = Request->GetURL();
	FString AssetId;

	// Parse asset ID from URL: https://worldbld.com/api/downloads/asset/{id}
	int32 LastSlashIndex;
	if (RequestURL.FindLastChar('/', LastSlashIndex))
	{
		AssetId = RequestURL.RightChop(LastSlashIndex + 1);
	}

	bool bIsKit = RequestURL.Contains(TEXT("/kit/"));

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[OnRequestDownloadURLResponse] Received download URL for asset: %s (Name: %s)"), *AssetId, *AssetName);
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[OnRequestDownloadURLResponse] Download URL: %s"), *DownloadURL);

	// Start the download using the download manager
	if (GEditor)
	{
		if (UWorldBLDAssetDownloadManager* DownloadManager = GEditor->GetEditorSubsystem<UWorldBLDAssetDownloadManager>())
		{
			FDOnDownloadProgress Progress;
			FDOnComplete Complete;

			DownloadManager->DownloadAsset(AssetId, AssetName, DownloadURL, bIsKit, Progress, Complete);
			UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[OnRequestDownloadURLResponse] Download started for asset: %s"), *AssetId);
		}
		else
		{
			UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("[OnRequestDownloadURLResponse] Failed to get DownloadManager"));
			OnAPIError.Broadcast(TEXT("Failed to start download"));
		}
	}
}
