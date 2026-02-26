// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDEditorUtilityLibrary.h"

#include "Containers/ArrayView.h"
#include "Editor.h"
#include "Engine/World.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Landscape.h"
#include "LandscapeImportHelper.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"

#include "WorldBLDRuntimeUtilsLibrary.h"
#include "WorldBLDRuntimeSettings.h"

namespace
{
	static TArray<uint16> ReadHeightmapFromFile(const FString& HeightmapFilePath)
	{
		if (HeightmapFilePath.IsEmpty())
		{
			UE_LOG(LogWorldBLD, Error, TEXT("[ReadHeightmapFromFile] HeightmapFilePath is empty"));
			return {};
		}

		if (!FPaths::FileExists(HeightmapFilePath))
		{
			UE_LOG(LogWorldBLD, Error, TEXT("[ReadHeightmapFromFile] File does not exist: %s"), *HeightmapFilePath);
			return {};
		}

		const FString Extension = FPaths::GetExtension(HeightmapFilePath).ToLower();
		if (Extension == TEXT("png"))
		{
			TArray<uint8> CompressedData;
			if (!FFileHelper::LoadFileToArray(CompressedData, *HeightmapFilePath))
			{
				UE_LOG(LogWorldBLD, Error, TEXT("[ReadHeightmapFromFile] Failed to read PNG file: %s"), *HeightmapFilePath);
				return {};
			}

			IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
			TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
			if (!ImageWrapper.IsValid())
			{
				UE_LOG(LogWorldBLD, Error, TEXT("[ReadHeightmapFromFile] Failed to create PNG image wrapper"));
				return {};
			}

			if (!ImageWrapper->SetCompressed(CompressedData.GetData(), CompressedData.Num()))
			{
				UE_LOG(LogWorldBLD, Error, TEXT("[ReadHeightmapFromFile] PNG decode failed (SetCompressed): %s"), *HeightmapFilePath);
				return {};
			}

			if (ImageWrapper->GetBitDepth() != 16)
			{
				UE_LOG(LogWorldBLD, Error, TEXT("[ReadHeightmapFromFile] PNG is not 16-bit. BitDepth=%d. File=%s"), ImageWrapper->GetBitDepth(), *HeightmapFilePath);
				return {};
			}

			TArray64<uint8> RawGray16;
			if (!ImageWrapper->GetRaw(ERGBFormat::Gray, 16, RawGray16) || RawGray16.Num() == 0)
			{
				UE_LOG(LogWorldBLD, Error, TEXT("[ReadHeightmapFromFile] Failed to extract raw grayscale16 data from PNG: %s"), *HeightmapFilePath);
				return {};
			}

			if ((RawGray16.Num() % 2) != 0)
			{
				UE_LOG(LogWorldBLD, Error, TEXT("[ReadHeightmapFromFile] PNG raw data byte count is not even: %d. File=%s"), RawGray16.Num(), *HeightmapFilePath);
				return {};
			}

			TArray<uint16> Out;
			Out.SetNumUninitialized(RawGray16.Num() / 2);
			FMemory::Memcpy(Out.GetData(), RawGray16.GetData(), RawGray16.Num());

			#if !PLATFORM_LITTLE_ENDIAN
			for (uint16& Value : Out)
			{
				Value = BYTESWAP_ORDER16(Value);
			}
			#endif

			return Out;
		}
		else if (Extension == TEXT("raw") || Extension == TEXT("r16"))
		{
			TArray<uint8> Bytes;
			if (!FFileHelper::LoadFileToArray(Bytes, *HeightmapFilePath))
			{
				UE_LOG(LogWorldBLD, Error, TEXT("[ReadHeightmapFromFile] Failed to read RAW/R16 file: %s"), *HeightmapFilePath);
				return {};
			}

			if ((Bytes.Num() % 2) != 0)
			{
				UE_LOG(LogWorldBLD, Error, TEXT("[ReadHeightmapFromFile] RAW/R16 byte count is not even: %d. File=%s"), Bytes.Num(), *HeightmapFilePath);
				return {};
			}

			const int32 NumSamples = Bytes.Num() / 2;
			TArray<uint16> Out;
			Out.SetNumUninitialized(NumSamples);

			const uint8* Data = Bytes.GetData();
			for (int32 i = 0; i < NumSamples; ++i)
			{
				const uint16 Lo = Data[(i * 2) + 0];
				const uint16 Hi = Data[(i * 2) + 1];
				Out[i] = (Hi << 8) | Lo;
			}

			return Out;
		}

		UE_LOG(LogWorldBLD, Error, TEXT("[ReadHeightmapFromFile] Unsupported heightmap format: .%s (File=%s)"), *Extension, *HeightmapFilePath);
		return {};
	}
}

ALandscape* UWorldBLDEditorUtilityLibrary::SpawnLandscapeWithHeightmap(
	const FString& HeightmapFilePath,
	int32 Resolution,
	int32 NumComponents,
	UMaterialInterface* Material)
{
	if (HeightmapFilePath.IsEmpty())
	{
		UE_LOG(LogWorldBLD, Error, TEXT("[SpawnLandscapeWithHeightmap] HeightmapFilePath is empty"));
		return nullptr;
	}

	if (!FPaths::FileExists(HeightmapFilePath))
	{
		UE_LOG(LogWorldBLD, Error, TEXT("[SpawnLandscapeWithHeightmap] Heightmap file not found: %s"), *HeightmapFilePath);
		return nullptr;
	}

	if (Resolution <= 0)
	{
		UE_LOG(LogWorldBLD, Error, TEXT("[SpawnLandscapeWithHeightmap] Resolution must be > 0. Got=%d"), Resolution);
		return nullptr;
	}

	if (NumComponents <= 0)
	{
		UE_LOG(LogWorldBLD, Error, TEXT("[SpawnLandscapeWithHeightmap] NumComponents must be > 0. Got=%d"), NumComponents);
		return nullptr;
	}

	TArray<uint16> HeightData = ReadHeightmapFromFile(HeightmapFilePath);
	if (HeightData.Num() == 0)
	{
		UE_LOG(LogWorldBLD, Error, TEXT("[SpawnLandscapeWithHeightmap] Failed to read heightmap data from: %s"), *HeightmapFilePath);
		return nullptr;
	}

	const int32 NumSubsections = 2;
	const int32 ComponentSizeQuads = (Resolution + 1) / NumComponents - 1;
	if (ComponentSizeQuads <= 0)
	{
		UE_LOG(LogWorldBLD, Error, TEXT("[SpawnLandscapeWithHeightmap] Computed invalid ComponentSizeQuads=%d (Resolution=%d, NumComponents=%d)"), ComponentSizeQuads, Resolution, NumComponents);
		return nullptr;
	}

	const int32 SubsectionSizeQuads = ComponentSizeQuads / NumSubsections;
	if (SubsectionSizeQuads <= 0)
	{
		UE_LOG(LogWorldBLD, Error, TEXT("[SpawnLandscapeWithHeightmap] Computed invalid SubsectionSizeQuads=%d (ComponentSizeQuads=%d, NumSubsections=%d)"), SubsectionSizeQuads, ComponentSizeQuads, NumSubsections);
		return nullptr;
	}

	if (!FMath::IsPowerOfTwo(SubsectionSizeQuads + 1))
	{
		UE_LOG(LogWorldBLD, Error, TEXT("[SpawnLandscapeWithHeightmap] Landscape requirement failed: (SubsectionSizeQuads+1) must be power of two. SubsectionSizeQuads=%d"), SubsectionSizeQuads);
		return nullptr;
	}

	const int32 MinX = 0;
	const int32 MinY = 0;
	const int32 MaxX = NumComponents * ComponentSizeQuads;
	const int32 MaxY = NumComponents * ComponentSizeQuads;
	const int32 ExpectedSamples = (MaxX - MinX + 1) * (MaxY - MinY + 1);

	if (HeightData.Num() != ExpectedSamples)
	{
		UE_LOG(LogWorldBLD, Error, TEXT("[SpawnLandscapeWithHeightmap] Heightmap sample count mismatch. Expected=%d, Actual=%d. ImportBounds=[%d,%d]-[%d,%d]"), ExpectedSamples, HeightData.Num(), MinX, MinY, MaxX, MaxY);
		return nullptr;
	}

	UWorld* World = (GEditor != nullptr) ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		UE_LOG(LogWorldBLD, Error, TEXT("[SpawnLandscapeWithHeightmap] No editor world context available"));
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ALandscape* Landscape = World->SpawnActor<ALandscape>(ALandscape::StaticClass(), FTransform::Identity, SpawnParams);
	if (!IsValid(Landscape))
	{
		UE_LOG(LogWorldBLD, Error, TEXT("[SpawnLandscapeWithHeightmap] Failed to spawn ALandscape actor"));
		return nullptr;
	}

	const FGuid LandscapeGuid = FGuid::NewGuid();
	TMap<FGuid, TArray<uint16>> HeightmapData;
	HeightmapData.Add(LandscapeGuid, MoveTemp(HeightData));

	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerData;
	TArray<FLandscapeLayer> ImportLayers;

	Landscape->Import(
		LandscapeGuid,
		MinX,
		MinY,
		MaxX,
		MaxY,
		NumSubsections,
		SubsectionSizeQuads,
		HeightmapData,
		*HeightmapFilePath,
		MaterialLayerData,
		ELandscapeImportAlphamapType::Additive,
		MakeArrayView(static_cast<const TArray<FLandscapeLayer>&>(ImportLayers)));

	if (Material != nullptr)
	{
		Landscape->LandscapeMaterial = Material;

		FProperty* Property = FindFProperty<FProperty>(ALandscapeProxy::StaticClass(), TEXT("LandscapeMaterial"));
		if (Property != nullptr)
		{
			FPropertyChangedEvent PropertyChangedEvent(Property);
			Landscape->PostEditChangeProperty(PropertyChangedEvent);
		}
		else
		{
			Landscape->PostEditChange();
		}
	}

	Landscape->RegisterAllComponents();
	Landscape->RecreateCollisionComponents();
	Landscape->MarkPackageDirty();

	return Landscape;
}

void UWorldBLDEditorUtilityLibrary::OpenWorldBLDProjectSettings()
{
#if WITH_EDITOR
	const UWorldBLDRuntimeSettings* Settings = GetDefault<UWorldBLDRuntimeSettings>();
	if (Settings == nullptr)
	{
		return;
	}

	ISettingsModule* SettingsModule = FModuleManager::LoadModulePtr<ISettingsModule>(TEXT("Settings"));
	if (SettingsModule == nullptr)
	{
		return;
	}

	SettingsModule->ShowViewer(Settings->GetContainerName(), Settings->GetCategoryName(), Settings->GetSectionName());
#endif
}
