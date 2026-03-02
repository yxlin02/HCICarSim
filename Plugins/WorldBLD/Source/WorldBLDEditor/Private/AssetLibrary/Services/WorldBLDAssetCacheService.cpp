#include "AssetLibrary/Services/WorldBLDAssetCacheService.h"

#include "AssetLibrary/Services/WorldBLDAssetImportArtifact.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

THIRD_PARTY_INCLUDES_START
#include "unzip.h"
THIRD_PARTY_INCLUDES_END

bool FWorldBLDAssetCacheService::ExtractArchive(const FString& InArchivePath, const FString& InDestinationPath) const
{
	unzFile ZipFile = unzOpen64(TCHAR_TO_UTF8(*InArchivePath));
	if (!ZipFile)
	{
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*InDestinationPath);

	constexpr int32 BufferSize = 65536;
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(BufferSize);

	int32 FilesExtracted = 0;
	int Result = unzGoToFirstFile(ZipFile);
	while (Result == UNZ_OK)
	{
		unz_file_info64 FileInfo;
		char FileName[512];

		if (unzGetCurrentFileInfo64(ZipFile, &FileInfo, FileName, sizeof(FileName), nullptr, 0, nullptr, 0) != UNZ_OK)
		{
			Result = unzGoToNextFile(ZipFile);
			continue;
		}

		const FString EntryName = UTF8_TO_TCHAR(FileName);
		const FString DestPath = FPaths::Combine(InDestinationPath, EntryName);

		if (EntryName.EndsWith(TEXT("/")))
		{
			PlatformFile.CreateDirectoryTree(*DestPath);
			Result = unzGoToNextFile(ZipFile);
			continue;
		}

		PlatformFile.CreateDirectoryTree(*FPaths::GetPath(DestPath));

		if (unzOpenCurrentFile(ZipFile) != UNZ_OK)
		{
			Result = unzGoToNextFile(ZipFile);
			continue;
		}

		IFileHandle* DestFile = PlatformFile.OpenWrite(*DestPath);
		if (!DestFile)
		{
			unzCloseCurrentFile(ZipFile);
			Result = unzGoToNextFile(ZipFile);
			continue;
		}

		int BytesRead;
		while ((BytesRead = unzReadCurrentFile(ZipFile, Buffer.GetData(), BufferSize)) > 0)
		{
			DestFile->Write(Buffer.GetData(), BytesRead);
		}

		delete DestFile;
		unzCloseCurrentFile(ZipFile);
		FilesExtracted++;

		Result = unzGoToNextFile(ZipFile);
	}

	unzClose(ZipFile);
	return FilesExtracted > 0;
}

static bool FindManifestFileUnderPreparedRoot(const FString& PreparedRootOnDisk, FString& OutManifestPath)
{
	OutManifestPath.Reset();

	if (PreparedRootOnDisk.IsEmpty() || !IFileManager::Get().DirectoryExists(*PreparedRootOnDisk))
	{
		return false;
	}

	TArray<FString> ManifestPaths;
	IFileManager::Get().FindFilesRecursive(
		ManifestPaths,
		*PreparedRootOnDisk,
		TEXT("*_manifest.json"),
		/*Files*/ true,
		/*Directories*/ false,
		/*bClearFileNames*/ false);

	if (ManifestPaths.Num() == 0)
	{
		IFileManager::Get().FindFilesRecursive(
			ManifestPaths,
			*PreparedRootOnDisk,
			TEXT("*.json"),
			/*Files*/ true,
			/*Directories*/ false,
			/*bClearFileNames*/ false);
	}

	if (ManifestPaths.Num() == 0)
	{
		return false;
	}

	ManifestPaths.Sort();
	OutManifestPath = ManifestPaths[0];
	return true;
}

namespace
{
	static bool IsSafeRelativePath(const FString& InPath)
	{
		if (InPath.IsEmpty())
		{
			return false;
		}

		// Reject absolute paths (Windows drive, UNC, or rooted).
		if (InPath.Contains(TEXT(":")) || InPath.StartsWith(TEXT("\\")) || InPath.StartsWith(TEXT("/")))
		{
			return false;
		}

		FString Normalized = InPath;
		FPaths::NormalizeFilename(Normalized); // uses forward slashes

		// Reject traversal segments.
		TArray<FString> Segments;
		Normalized.ParseIntoArray(Segments, TEXT("/"), /*bCullEmpty=*/true);
		for (const FString& Seg : Segments)
		{
			if (Seg == TEXT(".."))
			{
				return false;
			}
		}

		return true;
	}

	static FString ReadStringFieldAny(const TSharedPtr<FJsonObject>& Obj, std::initializer_list<const TCHAR*> Keys)
	{
		if (!Obj.IsValid())
		{
			return FString();
		}

		for (const TCHAR* Key : Keys)
		{
			FString Value;
			if (Obj->TryGetStringField(FStringView(Key), Value) && !Value.IsEmpty())
			{
				return Value;
			}
		}

		return FString();
	}

	static bool TryGetArrayFieldAny(const TSharedPtr<FJsonObject>& Obj, std::initializer_list<const TCHAR*> Keys, const TArray<TSharedPtr<FJsonValue>>*& OutArray)
	{
		OutArray = nullptr;

		if (!Obj.IsValid())
		{
			return false;
		}

		for (const TCHAR* Key : Keys)
		{
			if (Obj->TryGetArrayField(FStringView(Key), OutArray) && OutArray != nullptr)
			{
				return true;
			}
		}

		return false;
	}

	static bool TryInferAuthoredPackageRootFromDependencies(const TSharedPtr<FJsonObject>& RootObject, FString& OutAuthoredPackageRoot)
	{
		OutAuthoredPackageRoot.Reset();

		const TArray<TSharedPtr<FJsonValue>>* DepsArrayPtr = nullptr;
		if (!TryGetArrayFieldAny(RootObject, { TEXT("Dependencies"), TEXT("dependencies") }, DepsArrayPtr) || DepsArrayPtr == nullptr)
		{
			return false;
		}

		static const TArray<FString> KnownTypeFolders =
		{
			TEXT("Materials"),
			TEXT("Textures"),
			TEXT("Meshes"),
			TEXT("Blueprints"),
			TEXT("Data")
		};

		for (const TSharedPtr<FJsonValue>& V : *DepsArrayPtr)
		{
			if (!V.IsValid() || V->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> DepObj = V->AsObject();
			if (!DepObj.IsValid())
			{
				continue;
			}

			FString PackagePath = ReadStringFieldAny(DepObj, { TEXT("PackagePath"), TEXT("packagePath"), TEXT("package_path") });
			if (PackagePath.IsEmpty())
			{
				continue;
			}

			// PackagePath is typically an object path: "/Game/.../Asset.Asset". Convert to a long package path.
			int32 DotIndex = INDEX_NONE;
			if (PackagePath.FindChar(TEXT('.'), DotIndex) && DotIndex > 0)
			{
				PackagePath = PackagePath.Left(DotIndex);
			}

			// Derive the authored root ("/Game/.../<RootFolder>") by stripping the first type-folder segment.
			FString RootCandidate;
			for (const FString& TypeFolder : KnownTypeFolders)
			{
				const FString Needle = TEXT("/") + TypeFolder + TEXT("/");
				const int32 FoundIndex = PackagePath.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart);
				if (FoundIndex != INDEX_NONE)
				{
					RootCandidate = PackagePath.Left(FoundIndex);
					break;
				}
			}

			if (RootCandidate.IsEmpty())
			{
				RootCandidate = FPackageName::GetLongPackagePath(PackagePath);
			}

			if (FPackageName::IsValidLongPackageName(RootCandidate))
			{
				OutAuthoredPackageRoot = RootCandidate;
				return true;
			}
		}

		return false;
	}

	static void BuildFilesListFromDisk(const FString& PreparedRootOnDisk, TArray<FWorldBLDAssetImportFileEntry>& OutFiles)
	{
		OutFiles.Reset();

		if (PreparedRootOnDisk.IsEmpty() || !IFileManager::Get().DirectoryExists(*PreparedRootOnDisk))
		{
			return;
		}

		static const TSet<FString> AllowedExts =
		{
			TEXT("uasset"),
			TEXT("umap"),
			TEXT("uexp"),
			TEXT("ubulk"),
			TEXT("uptnl")
		};

		TArray<FString> AllFiles;
		IFileManager::Get().FindFilesRecursive(AllFiles, *PreparedRootOnDisk, TEXT("*.*"), /*Files*/ true, /*Directories*/ false);
		for (const FString& AbsPath : AllFiles)
		{
			const FString Ext = FPaths::GetExtension(AbsPath).ToLower();
			if (!AllowedExts.Contains(Ext))
			{
				continue;
			}

			FString Rel = AbsPath;
			if (!FPaths::MakePathRelativeTo(Rel, *PreparedRootOnDisk))
			{
				continue;
			}
			FPaths::MakeStandardFilename(Rel);

			FWorldBLDAssetImportFileEntry Entry;
			Entry.RelativePathFromPackageRoot = MoveTemp(Rel);
			Entry.SizeBytes = IFileManager::Get().FileSize(*AbsPath);
			// SHA1 is optional for import; leave empty in this fallback mode.
			OutFiles.Add(MoveTemp(Entry));
		}

		OutFiles.Sort([](const FWorldBLDAssetImportFileEntry& A, const FWorldBLDAssetImportFileEntry& B)
		{
			return A.RelativePathFromPackageRoot < B.RelativePathFromPackageRoot;
		});
	}
}

bool FWorldBLDAssetCacheService::TryLoadDownloadedArtifact(const FString& InPreparedRootOnDisk, FWorldBLDDownloadedArtifact& OutArtifact) const
{
	OutArtifact = FWorldBLDDownloadedArtifact{};
	OutArtifact.PreparedRootOnDisk = InPreparedRootOnDisk;

	FString ManifestPath;
	if (!FindManifestFileUnderPreparedRoot(InPreparedRootOnDisk, ManifestPath))
	{
		return false;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *ManifestPath) || JsonString.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	OutArtifact.ManifestPath = ManifestPath;

	OutArtifact.RootAssetId = ReadStringFieldAny(RootObject, { TEXT("RootAssetID"), TEXT("rootAssetID"), TEXT("rootAssetId") });

	OutArtifact.AuthoredPackageRoot = ReadStringFieldAny(RootObject, { TEXT("AuthoredPackageRoot"), TEXT("authoredPackageRoot"), TEXT("authoredPackageRoot") });
	if (OutArtifact.AuthoredPackageRoot.IsEmpty() || !FPackageName::IsValidLongPackageName(OutArtifact.AuthoredPackageRoot))
	{
		(void)TryInferAuthoredPackageRootFromDependencies(RootObject, OutArtifact.AuthoredPackageRoot);
	}

	const TArray<TSharedPtr<FJsonValue>>* FilesArrayPtr = nullptr;
	if (TryGetArrayFieldAny(RootObject, { TEXT("Files"), TEXT("files") }, FilesArrayPtr) && FilesArrayPtr)
	{
		bool bFoundUnsafePath = false;

		for (const TSharedPtr<FJsonValue>& V : *FilesArrayPtr)
		{
			if (!V.IsValid() || V->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> Obj = V->AsObject();
			if (!Obj.IsValid())
			{
				continue;
			}

			FWorldBLDAssetImportFileEntry Entry;
			Entry.RelativePathFromPackageRoot = ReadStringFieldAny(Obj, {
				TEXT("RelativePathFromPackageRoot"),
				TEXT("relativePathFromPackageRoot"),
				TEXT("relative_path_from_package_root"),
				TEXT("relativePath")
			});
			Entry.SHA1Hash = ReadStringFieldAny(Obj, { TEXT("SHA1Hash"), TEXT("sha1Hash"), TEXT("sha1_hash"), TEXT("sha1") });

			double SizeDouble = 0;
			if (Obj->TryGetNumberField(TEXT("SizeBytes"), SizeDouble) || Obj->TryGetNumberField(TEXT("sizeBytes"), SizeDouble))
			{
				Entry.SizeBytes = static_cast<int64>(SizeDouble);
			}

			if (!Entry.RelativePathFromPackageRoot.IsEmpty())
			{
				if (!IsSafeRelativePath(Entry.RelativePathFromPackageRoot))
				{
					bFoundUnsafePath = true;
					continue;
				}

				OutArtifact.Files.Add(MoveTemp(Entry));
			}
		}

		// If the manifest contains unsafe paths, treat the file list as unusable and rebuild from disk.
		if (bFoundUnsafePath)
		{
			OutArtifact.Files.Reset();
		}
	}

	// Validate that manifest entries exist under the prepared root; if not, rebuild from disk.
	if (OutArtifact.Files.Num() > 0)
	{
		bool bAnyMissing = false;
		for (const FWorldBLDAssetImportFileEntry& Entry : OutArtifact.Files)
		{
			const FString SourcePath = FPaths::Combine(InPreparedRootOnDisk, Entry.RelativePathFromPackageRoot);
			if (!IFileManager::Get().FileExists(*SourcePath))
			{
				bAnyMissing = true;
				break;
			}
		}

		if (bAnyMissing)
		{
			OutArtifact.Files.Reset();
		}
	}

	// Fallback for bad manifests: if there's no usable file list, build one from disk.
	if (OutArtifact.Files.Num() == 0)
	{
		BuildFilesListFromDisk(InPreparedRootOnDisk, OutArtifact.Files);
	}

	return FPackageName::IsValidLongPackageName(OutArtifact.AuthoredPackageRoot) && OutArtifact.Files.Num() > 0;
}

