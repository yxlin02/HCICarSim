// Copyright WorldBLD LLC. All rights reserved.

#include "Downloader/CurlDownloaderLibrary.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "WorldBLDToolUpdateSettings.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Utilities/UtilitiesLibrary.h"
#include "Editor.h"
#include "Authorization/WorldBLDAuthSubsystem.h"
#include "WorldBLDEditorToolkit.h"
#include "UObject/GarbageCollection.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include "curl/curl.h"
#include "Windows/HideWindowsPlatformTypes.h"

#define LOCTEXT_NAMESPACE "CurlDownloader"

// Minizip for zip extraction
THIRD_PARTY_INCLUDES_START
#include "unzip.h"
THIRD_PARTY_INCLUDES_END

namespace CurlDownloaderInternal
{
	static FString GetSessionTokenForRequests()
	{
		if (GEditor == nullptr)
		{
			return FString();
		}

		if (const UWorldBLDAuthSubsystem* AuthSubsystem = GEditor->GetEditorSubsystem<UWorldBLDAuthSubsystem>())
		{
			return AuthSubsystem->GetSessionToken();
		}

		return FString();
	}

	static bool IsProtectedDevProject()
	{
		const FString ProjectDir = FPaths::ProjectDir();
		const FString MarkerApiDoc = FPaths::Combine(ProjectDir, TEXT("WORLDBLD_API_DOCUMENTATION.md"));
		const FString MarkerGenBuild = FPaths::Combine(ProjectDir, TEXT("GenerateAndBuildProject.bat"));

		return FPaths::FileExists(MarkerApiDoc) || FPaths::FileExists(MarkerGenBuild);
	}

	static void ShowProtectedProjectBlockedDialog(const FText& Action)
	{
		const FText Title = LOCTEXT("WorldBLD_ProtectedProject_Title", "Action blocked");
		const FText Message = FText::Format(
			LOCTEXT(
				"WorldBLD_ProtectedProject_Message",
				"{0} is disabled in this protected development project.\n\n"
				"This safeguard prevents modifying plugins inside the WorldBLD/CityBLD development repo."),
			Action);

		UUtilitiesLibrary::ShowOkModalDialog(Message, Title);
	}

	static FString GetProjectUProjectFilePath()
	{
		// Avoid relying on version-specific helpers; build the path from project dir + project name.
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / FString(FApp::GetProjectName()) + TEXT(".uproject"));
	}

	static FString GetPendingPluginDeletesPath()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("WorldBLD/PendingPluginDeletes.json"));
	}

	static bool LoadPendingPluginDeletes(TArray<FString>& OutDirs)
	{
		OutDirs.Reset();

		const FString PendingPath = GetPendingPluginDeletesPath();
		if (!FPaths::FileExists(PendingPath))
		{
			return true;
		}

		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *PendingPath))
		{
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* PendingArray = nullptr;
		if (Root->TryGetArrayField(TEXT("PendingDeletes"), PendingArray) && PendingArray)
		{
			for (const TSharedPtr<FJsonValue>& Value : *PendingArray)
			{
				FString Dir;
				if (Value.IsValid() && Value->TryGetString(Dir) && !Dir.IsEmpty())
				{
					OutDirs.AddUnique(Dir);
				}
			}
		}

		return true;
	}

	static bool SavePendingPluginDeletes(const TArray<FString>& Dirs)
	{
		const FString PendingPath = GetPendingPluginDeletesPath();
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(PendingPath), true);

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> PendingArray;
		PendingArray.Reserve(Dirs.Num());
		for (const FString& Dir : Dirs)
		{
			if (!Dir.IsEmpty())
			{
				PendingArray.Add(MakeShared<FJsonValueString>(Dir));
			}
		}
		Root->SetArrayField(TEXT("PendingDeletes"), PendingArray);

		FString OutJson;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		if (!FJsonSerializer::Serialize(Root, Writer))
		{
			return false;
		}

		return FFileHelper::SaveStringToFile(OutJson, *PendingPath);
	}

	static bool AddPendingPluginDelete(const FString& PluginDir)
	{
		TArray<FString> PendingDirs;
		if (!LoadPendingPluginDeletes(PendingDirs))
		{
			return false;
		}

		PendingDirs.AddUnique(PluginDir);
		return SavePendingPluginDeletes(PendingDirs);
	}

	// ── Staged update manifest helpers ──────────────────────────────────────

	static FString GetStagedUpdatesManifestPath()
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::ProjectSavedDir() / TEXT("WorldBLD/StagedPluginUpdates.json"));
	}

	static FString GetStagedUpdatesBaseDir()
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::ProjectSavedDir() / TEXT("WorldBLD/StagedUpdates"));
	}

	static FString GetStagedUpdatesDirForTool(const FString& ToolId)
	{
		return GetStagedUpdatesBaseDir() / ToolId;
	}

	static FString ResolveTargetPluginDirForManifest(const FString& PluginName)
	{
		// Prefer the currently registered plugin location (project or engine marketplace)
		// so staged swaps update the active install in-place.
		if (const TSharedPtr<IPlugin> ExistingPlugin = IPluginManager::Get().FindPlugin(PluginName))
		{
			const FString ExistingBaseDir = FPaths::ConvertRelativePathToFull(ExistingPlugin->GetBaseDir());
			if (!ExistingBaseDir.IsEmpty())
			{
				return ExistingBaseDir;
			}
		}

		// Fallback for first-time installs.
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() / PluginName);
	}

	static FString ResolveExtractedStagedPluginDir(const FString& PluginName, const FString& StagedExtractionRoot)
	{
		if (StagedExtractionRoot.IsEmpty())
		{
			return FString();
		}

		// Preferred layout: <Root>/<PluginName>/<PluginName>.uplugin
		const FString PreferredDir = StagedExtractionRoot / PluginName;
		const FString PreferredUPlugin = PreferredDir / (PluginName + TEXT(".uplugin"));
		if (FPaths::FileExists(PreferredUPlugin))
		{
			return PreferredDir;
		}

		// Fallback: locate the descriptor anywhere under the staging root.
		FString FoundPluginDir;
		const FString ExpectedDescriptorName = PluginName + TEXT(".uplugin");
		IFileManager::Get().IterateDirectoryRecursively(*StagedExtractionRoot,
			[&FoundPluginDir, &ExpectedDescriptorName](const TCHAR* FilenameOrDir, bool bIsDirectory) -> bool
		{
			if (bIsDirectory)
			{
				return true;
			}

			const FString FileName = FPaths::GetCleanFilename(FilenameOrDir);
			if (FileName.Equals(ExpectedDescriptorName, ESearchCase::IgnoreCase))
			{
				FoundPluginDir = FPaths::GetPath(FilenameOrDir);
				return false; // stop iteration
			}

			return true;
		});

		if (!FoundPluginDir.IsEmpty())
		{
			return FPaths::ConvertRelativePathToFull(FoundPluginDir);
		}

		// Last-resort fallback (keeps previous behavior).
		return PreferredDir;
	}

	static bool WriteStagedUpdateManifestEntry(const FString& PluginName, const FString& StagingDir, const FString& ExpectedSHA1)
	{
		const FString ManifestPath = GetStagedUpdatesManifestPath();
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ManifestPath), true);

		// Load existing manifest (or create empty).
		TSharedPtr<FJsonObject> Root;
		if (FPaths::FileExists(ManifestPath))
		{
			FString JsonText;
			if (FFileHelper::LoadFileToString(JsonText, *ManifestPath))
			{
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
				FJsonSerializer::Deserialize(Reader, Root);
			}
		}
		if (!Root.IsValid())
		{
			Root = MakeShared<FJsonObject>();
		}

		TArray<TSharedPtr<FJsonValue>> StagedArray;
		const TArray<TSharedPtr<FJsonValue>>* ExistingArray = nullptr;
		if (Root->TryGetArrayField(TEXT("StagedUpdates"), ExistingArray) && ExistingArray)
		{
			StagedArray = *ExistingArray;
		}

		// Compute target plugin directory for the bootstrap module.
		// This must match the active install location, including Engine/Plugins/Marketplace.
		const FString TargetPluginDir = ResolveTargetPluginDirForManifest(PluginName);

		// Build new entry.
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("PluginName"), PluginName);
		Entry->SetStringField(TEXT("StagingDir"), StagingDir);
		Entry->SetStringField(TEXT("TargetPluginDir"), TargetPluginDir);
		Entry->SetStringField(TEXT("ExpectedSHA1"), ExpectedSHA1);
		Entry->SetStringField(TEXT("Timestamp"), FDateTime::UtcNow().ToIso8601());
		Entry->SetBoolField(TEXT("Applied"), false);

		StagedArray.Add(MakeShared<FJsonValueObject>(Entry));
		Root->SetArrayField(TEXT("StagedUpdates"), StagedArray);

		FString OutJson;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
		{
			UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] Failed to serialize staged update manifest."));
			return false;
		}

		if (!FFileHelper::SaveStringToFile(OutJson, *ManifestPath))
		{
			UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] Failed to write staged update manifest: %s"), *ManifestPath);
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Staged update manifest entry written for '%s'."), *PluginName);
		return true;
	}

	static bool HasPendingStagedUpdateForTool(const FString& ToolId)
	{
		const FString ManifestPath = GetStagedUpdatesManifestPath();
		if (!FPaths::FileExists(ManifestPath))
		{
			return false;
		}

		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *ManifestPath))
		{
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* StagedArray = nullptr;
		if (!Root->TryGetArrayField(TEXT("StagedUpdates"), StagedArray) || (StagedArray == nullptr))
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Value : *StagedArray)
		{
			const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Entry.IsValid())
			{
				continue;
			}

			bool bApplied = false;
			Entry->TryGetBoolField(TEXT("Applied"), bApplied);
			if (bApplied)
			{
				continue;
			}

			FString Name;
			if (Entry->TryGetStringField(TEXT("PluginName"), Name) && Name.Equals(ToolId, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		return false;
	}

	static bool SetPluginEnabledInProjectFile(const FString& PluginName, const bool bEnabled, FString& OutError)
	{
		static FCriticalSection ProjectFileMutex;
		FScopeLock ProjectFileLock(&ProjectFileMutex);

		OutError.Reset();

		const FString ProjectFilePath = GetProjectUProjectFilePath();
		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *ProjectFilePath))
		{
			OutError = FString::Printf(TEXT("Failed to read project file: %s"), *ProjectFilePath);
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = FString::Printf(TEXT("Failed to parse project file JSON: %s"), *ProjectFilePath);
			return false;
		}

		TArray<TSharedPtr<FJsonValue>> PluginsArray;
		const TArray<TSharedPtr<FJsonValue>>* ExistingPluginsArray = nullptr;
		if (Root->TryGetArrayField(TEXT("Plugins"), ExistingPluginsArray) && ExistingPluginsArray)
		{
			PluginsArray = *ExistingPluginsArray;
		}

		bool bFound = false;
		for (TSharedPtr<FJsonValue>& EntryValue : PluginsArray)
		{
			const TSharedPtr<FJsonObject> EntryObj = EntryValue.IsValid() ? EntryValue->AsObject() : nullptr;
			if (!EntryObj.IsValid())
			{
				continue;
			}

			FString Name;
			if (EntryObj->TryGetStringField(TEXT("Name"), Name) && Name.Equals(PluginName, ESearchCase::IgnoreCase))
			{
				EntryObj->SetBoolField(TEXT("Enabled"), bEnabled);
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			TSharedRef<FJsonObject> EntryObj = MakeShared<FJsonObject>();
			EntryObj->SetStringField(TEXT("Name"), PluginName);
			EntryObj->SetBoolField(TEXT("Enabled"), bEnabled);
			PluginsArray.Add(MakeShared<FJsonValueObject>(EntryObj));
		}

		Root->SetArrayField(TEXT("Plugins"), PluginsArray);

		FString OutJson;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
		{
			OutError = FString::Printf(TEXT("Failed to serialize project file JSON: %s"), *ProjectFilePath);
			return false;
		}

		if (!FFileHelper::SaveStringToFile(OutJson, *ProjectFilePath))
		{
			OutError = FString::Printf(TEXT("Failed to write project file: %s"), *ProjectFilePath);
			return false;
		}

		return true;
	}

	bool IsSSLVerificationDisabled()
	{
		const UWorldBLDToolUpdateSettings* Settings = GetDefault<UWorldBLDToolUpdateSettings>();
		return Settings ? Settings->bDisableSSLVerification : true;
	}

	// Calculate SHA1 hash of a file, returns lowercase hex string
	FString CalculateFileSHA1(const FString& FilePath)
	{
		TArray<uint8> FileData;
		if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
		{
			return FString();
		}

		FSHA1 SHA1;
		SHA1.Update(FileData.GetData(), FileData.Num());
		SHA1.Final();

		uint8 Hash[FSHA1::DigestSize];
		SHA1.GetHash(Hash);

		return BytesToHex(Hash, FSHA1::DigestSize).ToLower();
	}
	struct FChunkDownloadResult
	{
		int32 ChunkIndex = -1;
		bool bSuccess = false;
		long HttpCode = 0;
		CURLcode CurlCode = CURLE_OK;
		TArray<uint8> Data;
		int64 ExpectedSize = 0;
	};

	struct FChunkWriteContext
	{
		TArray<uint8>* Buffer = nullptr;
		TAtomic<int64>* DownloadedBytes = nullptr;
		TAtomic<bool>* bCancelled = nullptr;
	};

	size_t ChunkWriteCallback(void* Data, size_t Size, size_t Nmemb, void* UserPtr)
	{
		FChunkWriteContext* Ctx = static_cast<FChunkWriteContext*>(UserPtr);
		if (!Ctx || !Ctx->Buffer) return 0;
		if (Ctx->bCancelled && Ctx->bCancelled->Load()) return 0;

		const size_t DataSize = Size * Nmemb;
		const int32 OldSize = Ctx->Buffer->Num();
		Ctx->Buffer->AddUninitialized(DataSize);
		FMemory::Memcpy(Ctx->Buffer->GetData() + OldSize, Data, DataSize);

		if (Ctx->DownloadedBytes)
		{
			Ctx->DownloadedBytes->AddExchange(static_cast<int64>(DataSize));
		}

		return DataSize;
	}

	size_t DirectWriteCallback(void* Data, size_t Size, size_t Nmemb, void* UserPtr)
	{
		struct FDirectContext { IFileHandle* File; TAtomic<int64>* Downloaded; TAtomic<bool>* Cancelled; };
		FDirectContext* Ctx = static_cast<FDirectContext*>(UserPtr);
		if (!Ctx || !Ctx->File) return 0;
		if (Ctx->Cancelled && Ctx->Cancelled->Load()) return 0;

		const size_t DataSize = Size * Nmemb;
		if (!Ctx->File->Write(static_cast<const uint8*>(Data), DataSize)) return 0;

		if (Ctx->Downloaded) Ctx->Downloaded->AddExchange(static_cast<int64>(DataSize));
		return DataSize;
	}

	size_t HeaderCallback(char* Buffer, size_t Size, size_t Nitems, void* UserData)
	{
		int64* ContentLength = static_cast<int64*>(UserData);
		const size_t TotalSize = Size * Nitems;
		FString Header(TotalSize, UTF8_TO_TCHAR(Buffer));
		if (Header.StartsWith(TEXT("Content-Length:"), ESearchCase::IgnoreCase))
		{
			*ContentLength = FCString::Atoi64(*Header.Mid(15).TrimStartAndEnd());
		}
		return TotalSize;
	}

	size_t ContentRangeHeaderCallback(char* Buffer, size_t Size, size_t Nitems, void* UserData)
	{
		int64* TotalSizeOut = static_cast<int64*>(UserData);
		const size_t TotalHeaderSize = Size * Nitems;
		FString Header(TotalHeaderSize, UTF8_TO_TCHAR(Buffer));

		// Example: "Content-Range: bytes 0-0/12345\r\n"
		if (Header.StartsWith(TEXT("Content-Range:"), ESearchCase::IgnoreCase))
		{
			const FString Value = Header.Mid(14).TrimStartAndEnd();
			int32 SlashIndex = INDEX_NONE;
			if (Value.FindChar(TEXT('/'), SlashIndex) && SlashIndex != INDEX_NONE)
			{
				const FString TotalPart = Value.Mid(SlashIndex + 1).TrimStartAndEnd();
				if (!TotalPart.IsEmpty() && TotalPart != TEXT("*"))
				{
					*TotalSizeOut = FCString::Atoi64(*TotalPart);
				}
			}
		}

		return TotalHeaderSize;
	}

	void ConfigureCurlHandle(CURL* Curl, const FString& URL, curl_slist*& InOutHeaders)
	{
		curl_easy_setopt(Curl, CURLOPT_URL, TCHAR_TO_UTF8(*URL));
		curl_easy_setopt(Curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(Curl, CURLOPT_MAXREDIRS, 10L);

		const FString Token = GetSessionTokenForRequests();
		if (!Token.IsEmpty())
		{
			const FString HeaderLine = FString::Printf(TEXT("X-Authorization: %s"), *Token);
			InOutHeaders = curl_slist_append(InOutHeaders, TCHAR_TO_UTF8(*HeaderLine));
			curl_easy_setopt(Curl, CURLOPT_HTTPHEADER, InOutHeaders);
		}

		const bool bDisableSSL = IsSSLVerificationDisabled();
		curl_easy_setopt(Curl, CURLOPT_SSL_VERIFYPEER, bDisableSSL ? 0L : 1L);
		curl_easy_setopt(Curl, CURLOPT_SSL_VERIFYHOST, bDisableSSL ? 0L : 2L);
		if (bDisableSSL)
		{
			static TAtomic<bool> bLoggedSSLDisabled{false};
			
		}

		curl_easy_setopt(Curl, CURLOPT_BUFFERSIZE, 256 * 1024L);
		curl_easy_setopt(Curl, CURLOPT_TCP_NODELAY, 1L);
		curl_easy_setopt(Curl, CURLOPT_CONNECTTIMEOUT, 30L);
		curl_easy_setopt(Curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
		curl_easy_setopt(Curl, CURLOPT_LOW_SPEED_TIME, 30L);
	}

	FChunkDownloadResult DownloadChunkOnce(const FString& URL, int64 RangeStart, int64 RangeEnd,
		TAtomic<int64>* DownloadedBytes, TAtomic<bool>* bCancelled)
	{
		FChunkDownloadResult Result;
		Result.ExpectedSize = RangeEnd - RangeStart + 1;

		if (bCancelled && bCancelled->Load()) return Result;

		FChunkWriteContext WriteCtx;
		WriteCtx.Buffer = &Result.Data;
		WriteCtx.DownloadedBytes = DownloadedBytes;
		WriteCtx.bCancelled = bCancelled;

		Result.Data.Reserve(Result.ExpectedSize);

		CURL* Curl = curl_easy_init();
		if (!Curl) return Result;

		curl_slist* Headers = nullptr;
		ConfigureCurlHandle(Curl, URL, Headers);
		curl_easy_setopt(Curl, CURLOPT_WRITEFUNCTION, ChunkWriteCallback);
		curl_easy_setopt(Curl, CURLOPT_WRITEDATA, &WriteCtx);

		FString RangeHeader = FString::Printf(TEXT("%lld-%lld"), RangeStart, RangeEnd);
		curl_easy_setopt(Curl, CURLOPT_RANGE, TCHAR_TO_UTF8(*RangeHeader));

		Result.CurlCode = curl_easy_perform(Curl);
		curl_easy_getinfo(Curl, CURLINFO_RESPONSE_CODE, &Result.HttpCode);
		curl_easy_cleanup(Curl);
		if (Headers)
		{
			curl_slist_free_all(Headers);
		}

		const bool bValidResponse = (Result.HttpCode == 206 || Result.HttpCode == 200);
		const bool bCorrectSize = (Result.Data.Num() == Result.ExpectedSize);
		Result.bSuccess = (Result.CurlCode == CURLE_OK) && bValidResponse && bCorrectSize;

		if (!Result.bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CurlDownloader] Chunk download issue - HTTP: %ld, CurlCode: %d, Got: %lld bytes, Expected: %lld bytes, ValidResponse: %s, CorrectSize: %s"),
				Result.HttpCode, static_cast<int32>(Result.CurlCode), static_cast<int64>(Result.Data.Num()), Result.ExpectedSize,
				bValidResponse ? TEXT("Yes") : TEXT("No"), bCorrectSize ? TEXT("Yes") : TEXT("No"));
		}

		return Result;
	}

	bool IsRetryableError(long HttpCode, CURLcode CurlCode)
	{
		// HTTP 206 (Partial Content) is valid for range requests - always retry if size mismatch
		if (HttpCode == 206) return true;
		// HTTP 200 is also valid - retry if size mismatch
		if (HttpCode == 200) return true;
		// Server errors (5xx) are retryable
		if (HttpCode >= 500 && HttpCode < 600) return true;
		// Too many requests
		if (HttpCode == 429) return true;
		// Timeout errors
		if (CurlCode == CURLE_OPERATION_TIMEDOUT) return true;
		// Connection errors
		if (CurlCode == CURLE_COULDNT_CONNECT) return true;
		if (CurlCode == CURLE_RECV_ERROR) return true;
		if (CurlCode == CURLE_SEND_ERROR) return true;
		return false;
	}

	bool ShouldReduceConnections(long HttpCode)
	{
		// Server overloaded indicators
		return (HttpCode == 429 || HttpCode == 500 || HttpCode == 502 || HttpCode == 503);
	}

	bool ExtractZipToPlugins(const FString& ZipPath, const FString& PluginName, const FString& OverrideExtractionDir = FString())
	{
		// When OverrideExtractionDir is non-empty, extract there (staged update) and skip mounting.
		const bool bStagedExtraction = !OverrideExtractionDir.IsEmpty();
		const FString PluginsDir = bStagedExtraction
			? OverrideExtractionDir
			: FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins"));

		UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Extracting to: %s%s"), *PluginsDir, bStagedExtraction ? TEXT(" (staged)") : TEXT(""));

		// Open zip file using minizip
		unzFile ZipFile = unzOpen64(TCHAR_TO_UTF8(*ZipPath));
		if (!ZipFile)
		{
			UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] Failed to open zip file: %s"), *ZipPath);
			return false;
		}

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		constexpr int32 BufferSize = 65536;
		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(BufferSize);

		int32 FilesExtracted = 0;
		FString ExtractedPluginFile;  // Track .uplugin file path
		int Result = unzGoToFirstFile(ZipFile);

		while (Result == UNZ_OK)
		{
			unz_file_info64 FileInfo;
			char FileName[512];

			if (unzGetCurrentFileInfo64(ZipFile, &FileInfo, FileName, sizeof(FileName), nullptr, 0, nullptr, 0) != UNZ_OK)
			{
				UE_LOG(LogTemp, Warning, TEXT("[CurlDownloader] Failed to get file info"));
				Result = unzGoToNextFile(ZipFile);
				continue;
			}

			FString EntryName = UTF8_TO_TCHAR(FileName);
			FString DestPath = FPaths::Combine(PluginsDir, EntryName);

			// Check if it's a directory (ends with /)
			if (EntryName.EndsWith(TEXT("/")))
			{
				PlatformFile.CreateDirectoryTree(*DestPath);
				Result = unzGoToNextFile(ZipFile);
				continue;
			}

			// Track .uplugin file for mounting
			if (EntryName.EndsWith(TEXT(".uplugin")))
			{
				ExtractedPluginFile = DestPath;
			}

			// Create parent directory
			PlatformFile.CreateDirectoryTree(*FPaths::GetPath(DestPath));

			// Open file in zip
			if (unzOpenCurrentFile(ZipFile) != UNZ_OK)
			{
				UE_LOG(LogTemp, Warning, TEXT("[CurlDownloader] Failed to open: %s"), *EntryName);
				Result = unzGoToNextFile(ZipFile);
				continue;
			}

			// Open destination file
			IFileHandle* DestFile = PlatformFile.OpenWrite(*DestPath);
			if (!DestFile)
			{
				UE_LOG(LogTemp, Warning, TEXT("[CurlDownloader] Failed to create: %s"), *DestPath);
				unzCloseCurrentFile(ZipFile);
				Result = unzGoToNextFile(ZipFile);
				continue;
			}

			// Extract file contents
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

		if (FilesExtracted > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Extraction successful (%d files)"), FilesExtracted);

			// Staged extractions skip mounting entirely -- the bootstrap module will swap
			// the directory into place on the next editor startup.
			if (bStagedExtraction)
			{
				UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Staged extraction complete for '%s' -- mounting deferred to next startup."), *PluginName);
				return true;
			}

			// Mount the plugin on the game thread (plugin manager operations must be on game thread)
			if (!ExtractedPluginFile.IsEmpty())
			{
				// Conservative safety: do not attempt runtime mount for code plugins.
				// First-time installs previously attempted plugin-list insertion/mounting immediately after extraction,
				// which can destabilize editor rendering on some machines/drivers. Code plugins are restart-required anyway.
				bool bDescriptorParsed = false;
				bool bHasCodeModules = false;
				{
					FString DescJson;
					if (FFileHelper::LoadFileToString(DescJson, *ExtractedPluginFile))
					{
						TSharedPtr<FJsonObject> DescRoot;
						const TSharedRef<TJsonReader<>> DescReader = TJsonReaderFactory<>::Create(DescJson);
						if (FJsonSerializer::Deserialize(DescReader, DescRoot) && DescRoot.IsValid())
						{
							bDescriptorParsed = true;
							const TArray<TSharedPtr<FJsonValue>>* ModulesArray = nullptr;
							bHasCodeModules = DescRoot->TryGetArrayField(TEXT("Modules"), ModulesArray) && ModulesArray && (ModulesArray->Num() > 0);
						}
					}
				}

				if (bDescriptorParsed && bHasCodeModules)
				{
					UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] '%s' contains code modules. Skipping runtime mount; restart required."), *PluginName);

					FString PluginNameCopy = PluginName;
					AsyncTask(ENamedThreads::GameThread, [PluginNameCopy]()
					{
						FNotificationInfo Info(FText::Format(
							NSLOCTEXT("CurlDownloader", "RestartRequired_CodePluginInstall", "Plugin '{0}' installed successfully.\nPlease restart the editor to load it."),
							FText::FromString(PluginNameCopy)));
						Info.ExpireDuration = 8.0f;
						Info.bUseSuccessFailIcons = true;
						Info.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.WarningWithColor"));
						FSlateNotificationManager::Get().AddNotification(Info);
					});

					return true;
				}

				FString PluginFileCopy = ExtractedPluginFile;
				FString PluginNameCopy = PluginName;

				AsyncTask(ENamedThreads::GameThread, [PluginFileCopy, PluginNameCopy]()
				{
					IPluginManager& PluginManager = IPluginManager::Get();

					// First check if plugin is already known and mounted
					TSharedPtr<IPlugin> ExistingPlugin = PluginManager.FindPlugin(PluginNameCopy);
					if (ExistingPlugin.IsValid() && ExistingPlugin->IsMounted())
					{
						UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Plugin '%s' is already mounted"), *PluginNameCopy);
						return;
					}

					// Verify the .uplugin file exists
					if (!FPaths::FileExists(PluginFileCopy))
					{
						UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] Plugin file does not exist: %s"), *PluginFileCopy);
						return;
					}

					// Add plugin to the list
					UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Adding plugin to list: %s"), *PluginFileCopy);
					FText FailReason;
					if (!PluginManager.AddToPluginsList(PluginFileCopy, &FailReason))
					{
						UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] Failed to add plugin to list: %s"), *FailReason.ToString());
						return;
					}

					// Check plugin properties
					TSharedPtr<IPlugin> AddedPlugin = PluginManager.FindPlugin(PluginNameCopy);
					if (!AddedPlugin.IsValid())
					{
						UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] Plugin '%s' not found after adding to list"), *PluginNameCopy);
						return;
					}

					const FPluginDescriptor& Desc = AddedPlugin->GetDescriptor();
					const bool bHasCodeModules = Desc.Modules.Num() > 0;
					const bool bIsExplicitlyLoaded = Desc.bExplicitlyLoaded;

					UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Plugin '%s': Modules=%d, ExplicitlyLoaded=%s"),
						*PluginNameCopy, Desc.Modules.Num(), bIsExplicitlyLoaded ? TEXT("yes") : TEXT("no"));

					// Log module names
					for (const FModuleDescriptor& Module : Desc.Modules)
					{
						UE_LOG(LogTemp, Log, TEXT("[CurlDownloader]   Module: %s (Type: %d)"),
							*Module.Name.ToString(), static_cast<int32>(Module.Type));
					}

					bool bMountSuccess = false;
					if (bHasCodeModules)
					{
						// Runtime mounting for code plugins is intentionally disabled for stability.
						UE_LOG(LogTemp, Warning, TEXT("[CurlDownloader] Plugin '%s' has code modules (ExplicitlyLoaded=%s) - restart required; runtime mount skipped."),
							*PluginNameCopy, bIsExplicitlyLoaded ? TEXT("yes") : TEXT("no"));

						FNotificationInfo Info(FText::Format(
							NSLOCTEXT("CurlDownloader", "RestartRequired", "Plugin '{0}' installed successfully.\nPlease restart the editor to load it."),
							FText::FromString(PluginNameCopy)));
						Info.ExpireDuration = 8.0f;
						Info.bUseSuccessFailIcons = true;
						Info.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.WarningWithColor"));
						FSlateNotificationManager::Get().AddNotification(Info);
						return;
					}
					else
					{
						// Content-only plugin - use MountNewlyCreatedPlugin
						UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Plugin '%s' is content-only, mounting"), *PluginNameCopy);
						PluginManager.MountNewlyCreatedPlugin(PluginNameCopy);

						TSharedPtr<IPlugin> MountedPlugin = PluginManager.FindPlugin(PluginNameCopy);
						bMountSuccess = MountedPlugin.IsValid() && MountedPlugin->IsMounted();
					}

					// Show result notification
					if (bMountSuccess)
					{
						FNotificationInfo Info(FText::Format(
							NSLOCTEXT("CurlDownloader", "MountSuccess", "Plugin '{0}' installed and loaded successfully."),
							FText::FromString(PluginNameCopy)));
						Info.ExpireDuration = 5.0f;
						Info.bUseSuccessFailIcons = true;
						Info.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.SuccessWithColor"));
						FSlateNotificationManager::Get().AddNotification(Info);
					}
					else if (!bHasCodeModules)
					{
						UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] Failed to mount content plugin '%s'"), *PluginNameCopy);
					}
				});
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[CurlDownloader] No .uplugin file found in archive"));
			}

			return true;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] No files extracted from zip"));
			return false;
		}
	}

	bool DownloadSingleConnection(TSharedPtr<FCurlDownloadContext> ContextPtr)
	{
		const FString& URL = ContextPtr->URL;
		const FString& SavePath = ContextPtr->SavePath;

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		IFileHandle* FileHandle = PlatformFile.OpenWrite(*SavePath);
		if (!FileHandle)
		{
			UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] Failed to open file: %s"), *SavePath);
			return false;
		}

		struct FDirectContext { IFileHandle* File; TAtomic<int64>* Downloaded; TAtomic<bool>* Cancelled; };
		FDirectContext DirectCtx = { FileHandle, &ContextPtr->DownloadedBytes, &ContextPtr->bCancelled };

		CURL* Curl = curl_easy_init();
		curl_slist* Headers = nullptr;
		ConfigureCurlHandle(Curl, URL, Headers);
		curl_easy_setopt(Curl, CURLOPT_WRITEFUNCTION, DirectWriteCallback);
		curl_easy_setopt(Curl, CURLOPT_WRITEDATA, &DirectCtx);

		UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Downloading with single connection..."));

		CURLcode Res = curl_easy_perform(Curl);
		long HttpCode = 0;
		curl_easy_getinfo(Curl, CURLINFO_RESPONSE_CODE, &HttpCode);
		curl_easy_cleanup(Curl);
		if (Headers)
		{
			curl_slist_free_all(Headers);
		}
		delete FileHandle;

		bool bSuccess = (Res == CURLE_OK) && (HttpCode == 200) && !ContextPtr->bCancelled.Load();

		UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Single connection: %s (HTTP %ld, %lld bytes)"),
			bSuccess ? TEXT("success") : TEXT("failed"), HttpCode, ContextPtr->DownloadedBytes.Load());

		// Extract if download succeeded
		if (bSuccess && ContextPtr->bExtractZipToPlugins)
		{
			bSuccess = ExtractZipToPlugins(SavePath, ContextPtr->PluginName, ContextPtr->StagedExtractionDir);
		}

		return bSuccess;
	}

	void ExecuteAdaptiveDownload(TSharedPtr<FCurlDownloadContext> ContextPtr)
	{
		if (!ContextPtr.IsValid()) return;

		const FString URL = ContextPtr->URL;
		const FString SavePath = ContextPtr->SavePath;

		UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Starting adaptive download: %s"), *URL);

		// Create directory
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*FPaths::GetPath(SavePath));

		// Get file size
		const int64 FileSize = UCurlDownloaderLibrary::GetRemoteFileSize(URL);
		if (FileSize <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CurlDownloader] Unable to determine remote file size (missing Content-Length/Content-Range). Falling back to single connection download."));
			const bool bSuccess = DownloadSingleConnection(ContextPtr);
			ContextPtr->OnProgress.Broadcast(bSuccess ? 1.0f : 0.0f);
			ContextPtr->OnComplete.Broadcast(bSuccess);
			return;
		}

		ContextPtr->TotalBytes.Store(FileSize);
		UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] File size: %lld bytes (%.2f MB)"), FileSize, FileSize / (1024.0 * 1024.0));

		// Check range support
		const bool bSupportsRanges = UCurlDownloaderLibrary::ServerSupportsRangeRequests(URL);
		if (!bSupportsRanges)
		{
			UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Server doesn't support ranges, using single connection"));
			const bool bSuccess = DownloadSingleConnection(ContextPtr);
			ContextPtr->OnProgress.Broadcast(bSuccess ? 1.0f : ContextPtr->GetProgress());
			ContextPtr->OnComplete.Broadcast(bSuccess);
			return;
		}

		// Progress ticker
		FTSTicker::FDelegateHandle TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([ContextPtr](float) -> bool
			{
				if (!ContextPtr.IsValid() || ContextPtr->bCancelled.Load()) return false;
				ContextPtr->OnProgress.Broadcast(ContextPtr->GetProgress());
				return true;
			}), 0.1f);

		bool bFinalSuccess = false;
		int32 FullRetryCount = 0;

		while (!bFinalSuccess && FullRetryCount < ContextPtr->MaxFullRetries && !ContextPtr->bCancelled.Load())
		{
			if (FullRetryCount > 0)
			{
				// Reduce connections on retry
				int32 CurrentConns = ContextPtr->CurrentConnections.Load();
				int32 NewConns = FMath::Max(FCurlDownloadContext::MinConnections, CurrentConns / 2);
				ContextPtr->CurrentConnections.Store(NewConns);
				ContextPtr->Reset();

				UE_LOG(LogTemp, Warning, TEXT("[CurlDownloader] Full retry %d/%d with %d connections"),
					FullRetryCount + 1, ContextPtr->MaxFullRetries, NewConns);

				FPlatformProcess::Sleep(1.0f * FullRetryCount);  // Backoff
			}

			const int32 NumConnections = ContextPtr->CurrentConnections.Load();
			UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Attempting with %d connection(s)"), NumConnections);

			// Setup chunks
			ContextPtr->ChunkRanges.SetNum(NumConnections);
			int64 BytesAssigned = 0;
			const int64 BaseChunkSize = FileSize / NumConnections;
			const int64 Remainder = FileSize % NumConnections;

			for (int32 i = 0; i < NumConnections; ++i)
			{
				const int64 ThisChunkSize = BaseChunkSize + (i < Remainder ? 1 : 0);
				ContextPtr->ChunkRanges[i] = TPair<int64, int64>(BytesAssigned, BytesAssigned + ThisChunkSize - 1);
				BytesAssigned += ThisChunkSize;
			}

			// Download all chunks with retry logic per chunk
			TAtomic<bool> bAnyFatalError{false};
			TAtomic<int32> ServerErrorCount{0};

			ParallelFor(NumConnections, [&](int32 ChunkIdx)
			{
				if (ContextPtr->bCancelled.Load() || bAnyFatalError.Load()) return;

				const int64 RangeStart = ContextPtr->ChunkRanges[ChunkIdx].Key;
				const int64 RangeEnd = ContextPtr->ChunkRanges[ChunkIdx].Value;

				UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Chunk %d: %lld-%lld"), ChunkIdx, RangeStart, RangeEnd);

				FChunkDownloadResult Result;
				int32 ChunkRetry = 0;

				while (!Result.bSuccess && ChunkRetry < ContextPtr->MaxChunkRetries && !ContextPtr->bCancelled.Load() && !bAnyFatalError.Load())
				{
					if (ChunkRetry > 0)
					{
						// Subtract bytes from failed attempt
						if (Result.Data.Num() > 0)
						{
							ContextPtr->DownloadedBytes.AddExchange(-static_cast<int64>(Result.Data.Num()));
						}

						const float DelaySeconds = 0.5f * (1 << FMath::Min(ChunkRetry - 1, 4));
						UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Chunk %d: retry %d after %.1fs"),
							ChunkIdx, ChunkRetry, DelaySeconds);
						FPlatformProcess::Sleep(DelaySeconds);
					}

					Result = DownloadChunkOnce(URL, RangeStart, RangeEnd, &ContextPtr->DownloadedBytes, &ContextPtr->bCancelled);

					if (!Result.bSuccess)
					{
						if (ShouldReduceConnections(Result.HttpCode))
						{
							ServerErrorCount.IncrementExchange();
							// If too many server errors, signal need for full retry
							if (ServerErrorCount.Load() > NumConnections / 2)
							{
								bAnyFatalError.Store(true);
							}
						}
						else if (!IsRetryableError(Result.HttpCode, Result.CurlCode))
						{
							UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] Chunk %d: non-retryable error (HTTP %ld, CurlCode %d)"),
								ChunkIdx, Result.HttpCode, static_cast<int32>(Result.CurlCode));
							bAnyFatalError.Store(true);
						}
						else
						{
							UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Chunk %d: retryable error (HTTP %ld), will retry (%d/%d)"),
								ChunkIdx, Result.HttpCode, ChunkRetry + 1, ContextPtr->MaxChunkRetries);
						}
					}

					ChunkRetry++;
				}

				if (Result.bSuccess)
				{
					FScopeLock Lock(&ContextPtr->ChunkMutex);
					ContextPtr->CompletedChunkBuffers.Add(ChunkIdx, MoveTemp(Result.Data));
					UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Chunk %d: completed"), ChunkIdx);
				}
				else
				{
					FScopeLock Lock(&ContextPtr->ChunkMutex);
					ContextPtr->FailedChunks.Add(ChunkIdx);
					UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] Chunk %d: failed after %d retries (HTTP %ld)"),
						ChunkIdx, ChunkRetry, Result.HttpCode);
				}
			});

			// Check if all chunks succeeded
			{
				FScopeLock Lock(&ContextPtr->ChunkMutex);
				bFinalSuccess = (ContextPtr->CompletedChunkBuffers.Num() == NumConnections) && ContextPtr->FailedChunks.Num() == 0;
			}

			if (!bFinalSuccess && !ContextPtr->bCancelled.Load())
			{
				UE_LOG(LogTemp, Warning, TEXT("[CurlDownloader] Download incomplete, %d/%d chunks succeeded"),
					ContextPtr->CompletedChunkBuffers.Num(), NumConnections);
			}

			FullRetryCount++;
		}

		// Assemble file if successful
		if (bFinalSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Assembling file..."));

			IFileHandle* FileHandle = PlatformFile.OpenWrite(*SavePath);
			if (FileHandle)
			{
				const int32 NumChunks = ContextPtr->ChunkRanges.Num();
				for (int32 i = 0; i < NumChunks; ++i)
				{
					const TArray<uint8>* ChunkData = ContextPtr->CompletedChunkBuffers.Find(i);
					if (ChunkData && ChunkData->Num() > 0)
					{
						FileHandle->Write(ChunkData->GetData(), ChunkData->Num());
					}
				}
				delete FileHandle;

				// Verify file size
				const int64 WrittenSize = PlatformFile.FileSize(*SavePath);
				if (WrittenSize == FileSize)
				{
					UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Download complete: %s (%lld bytes)"), *SavePath, WrittenSize);

					// Extract zip to Plugins folder (or staging directory for staged updates)
					if (ContextPtr->bExtractZipToPlugins)
					{
						if (!ExtractZipToPlugins(SavePath, ContextPtr->PluginName, ContextPtr->StagedExtractionDir))
						{
							bFinalSuccess = false;
						}
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] Size mismatch! Written: %lld, Expected: %lld"), WrittenSize, FileSize);
					bFinalSuccess = false;
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] Failed to open file for writing"));
				bFinalSuccess = false;
			}
		}

		// Cleanup
		ContextPtr->CompletedChunkBuffers.Empty();
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);

		ContextPtr->OnProgress.Broadcast(bFinalSuccess ? 1.0f : ContextPtr->GetProgress());
		ContextPtr->OnComplete.Broadcast(bFinalSuccess);
	}
}

void UCurlDownloaderLibrary::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	curl_global_init(CURL_GLOBAL_ALL);
	ProcessPendingPluginDeletes();
	ProcessAppliedStagedUpdates();
}

// ── Staged update static helpers ────────────────────────────────────────────

FString UCurlDownloaderLibrary::GetStagedUpdatesManifestPath()
{
	return CurlDownloaderInternal::GetStagedUpdatesManifestPath();
}

FString UCurlDownloaderLibrary::GetStagedUpdatesDirForTool(const FString& ToolId)
{
	return CurlDownloaderInternal::GetStagedUpdatesDirForTool(ToolId);
}

bool UCurlDownloaderLibrary::HasPendingStagedUpdate(const FString& ToolId)
{
	return CurlDownloaderInternal::HasPendingStagedUpdateForTool(ToolId);
}

// ── ProcessAppliedStagedUpdates ─────────────────────────────────────────────

void UCurlDownloaderLibrary::ProcessAppliedStagedUpdates()
{
	const FString ManifestPath = CurlDownloaderInternal::GetStagedUpdatesManifestPath();
	if (!FPaths::FileExists(ManifestPath))
	{
		return;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ManifestPath))
	{
		return;
	}

	TSharedPtr<FJsonObject> Root;
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return;
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* StagedArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("StagedUpdates"), StagedArray) || (StagedArray == nullptr) || StagedArray->IsEmpty())
	{
		return;
	}

	TArray<TSharedPtr<FJsonValue>> RemainingEntries;
	bool bAnyFinalized = false;

	for (const TSharedPtr<FJsonValue>& Value : *StagedArray)
	{
		const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Entry.IsValid())
		{
			continue;
		}

		bool bApplied = false;
		Entry->TryGetBoolField(TEXT("Applied"), bApplied);
		if (!bApplied)
		{
			// Not yet applied by bootstrap -- keep in manifest.
			RemainingEntries.Add(Value);
			continue;
		}

		// Applied by bootstrap -- finalize: update SHA1 and clean up.
		FString PluginName;
		Entry->TryGetStringField(TEXT("PluginName"), PluginName);
		FString ExpectedSHA1;
		Entry->TryGetStringField(TEXT("ExpectedSHA1"), ExpectedSHA1);

		if (!PluginName.IsEmpty() && !ExpectedSHA1.IsEmpty())
		{
			if (UWorldBLDToolUpdateSettings* Settings = GetMutableDefault<UWorldBLDToolUpdateSettings>())
			{
				Settings->SetInstalledSha1AndSave(PluginName, ExpectedSHA1);
				UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Finalized staged update for '%s' -- SHA1 recorded."), *PluginName);
			}
		}

		// Clean up staging directory if it still exists.
		FString StagingDir;
		if (Entry->TryGetStringField(TEXT("StagingDir"), StagingDir) && !StagingDir.IsEmpty() && FPaths::DirectoryExists(StagingDir))
		{
			IFileManager::Get().DeleteDirectory(*StagingDir, false, true);
		}

		// Clean up any leftover backup directories created by the journaled swap.
		// Backups are stored in Saved/WorldBLD/Backups/<PluginName>_<timestamp>.
		if (!PluginName.IsEmpty())
		{
			const FString BackupsBaseDir = FPaths::ConvertRelativePathToFull(
				FPaths::ProjectSavedDir() / TEXT("WorldBLD/Backups"));
			if (FPaths::DirectoryExists(BackupsBaseDir))
			{
				const FString Prefix = PluginName + TEXT("_");
				IFileManager::Get().IterateDirectory(*BackupsBaseDir,
					[&Prefix](const TCHAR* FilenameOrDir, bool bIsDirectory) -> bool
				{
					if (bIsDirectory)
					{
						const FString DirName = FPaths::GetCleanFilename(FilenameOrDir);
						if (DirName.StartsWith(Prefix))
						{
							IFileManager::Get().DeleteDirectory(FilenameOrDir, false, true);
						}
					}
					return true;
				});
			}
		}

		bAnyFinalized = true;
	}

	if (bAnyFinalized)
	{
		if (RemainingEntries.IsEmpty())
		{
			// All entries finalized -- delete the manifest file.
			IFileManager::Get().Delete(*ManifestPath, false, false, true);
		}
		else
		{
			// Rewrite with only the remaining entries.
			Root->SetArrayField(TEXT("StagedUpdates"), RemainingEntries);

			FString OutJson;
			const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
			if (FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
			{
				FFileHelper::SaveStringToFile(OutJson, *ManifestPath);
			}
		}
	}
}

void UCurlDownloaderLibrary::Deinitialize()
{
	{
		FScopeLock Lock(&DownloadsMutex);
		for (auto& Pair : CurrentDownloads)
		{
			Pair.Value->bCancelled.Store(true);
		}
		CurrentDownloads.Empty();
	}
	curl_global_cleanup();
	Super::Deinitialize();
}

bool UCurlDownloaderLibrary::SetPluginEnabledInProjectFileNative(const FString& PluginName, const bool bEnabled, FString& OutError) const
{
	return CurlDownloaderInternal::SetPluginEnabledInProjectFile(PluginName, bEnabled, OutError);
}

bool UCurlDownloaderLibrary::PrepareForToolInstallOrUpdate(const FString& ToolId, FString& OutError, bool& bOutStagedUpdate)
{
	OutError.Reset();
	bOutStagedUpdate = false;

	const FString TrimmedId = ToolId.TrimStartAndEnd();
	if (TrimmedId.IsEmpty())
	{
		OutError = TEXT("ToolId is empty.");
		return false;
	}

	// Never auto-disable the plugin that provides the updater UI.
	if (TrimmedId.Equals(TEXT("WorldBLD"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	// Always clear the cached download directory (Saved/<PersistentDownloadDir>/<ToolId>/...).
	const FString CachedDownloadDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPersistentDownloadDir() / TrimmedId);
	if (FPaths::DirectoryExists(CachedDownloadDir))
	{
		const bool bCacheDeleted = IFileManager::Get().DeleteDirectory(*CachedDownloadDir, /*RequireExists*/ false, /*Tree*/ true);
		if (!bCacheDeleted)
		{
			// Files may be locked; defer deletion and require restart before proceeding to avoid mixed/partial state.
			if (CurlDownloaderInternal::AddPendingPluginDelete(CachedDownloadDir))
			{
				OutError = TEXT("Cached download cleanup was scheduled for next editor start. Please restart the editor and try again.");
			}
			else
			{
				OutError = TEXT("Failed to clear cached download directory. Files may be locked.");
			}
			return false;
		}
	}

	// If the plugin is currently installed, attempt to fully remove it before updating.
	const TSharedPtr<IPlugin> ExistingPlugin = IPluginManager::Get().FindPlugin(TrimmedId);
	if (!ExistingPlugin.IsValid())
	{
		return true;
	}

	const FPluginDescriptor& ExistingDesc = ExistingPlugin->GetDescriptor();
	const bool bHasCodeModules = (ExistingDesc.Modules.Num() > 0);
	if (bHasCodeModules)
	{
		// Only block if the plugin's code modules are actually loaded right now (DLLs will be locked on disk).
		// If the plugin is disabled / not loaded (e.g. ExplicitlyLoaded + not mounted), updating in-session is possible.
		bool bAnyModuleLoaded = false;
		for (const FModuleDescriptor& ModuleDesc : ExistingDesc.Modules)
		{
			if (FModuleManager::Get().IsModuleLoaded(ModuleDesc.Name))
			{
				bAnyModuleLoaded = true;
				break;
			}
		}

		if (bAnyModuleLoaded)
		{
			// Code modules are loaded -- DLLs are locked on disk.  Instead of disabling the
			// plugin (which would break levels that reference it), stage the update: download
			// the new version to a staging directory and let the WorldBLDBootstrap module swap
			// the files on the next editor startup before tool DLLs are loaded.
			bOutStagedUpdate = true;

			// Clear any previous staging dir for this tool, then ensure the
			// base staging directory exists.  The ZIP extraction will re-create
			// the per-tool subdirectory inside it (mirroring the normal flow
			// where Plugins/ is the extraction root and the ZIP contains
			// <ToolId>/... entries).
			const FString StagingToolDir = GetStagedUpdatesDirForTool(TrimmedId);
			if (FPaths::DirectoryExists(StagingToolDir))
			{
				IFileManager::Get().DeleteDirectory(*StagingToolDir, false, true);
			}
			const FString StagingBaseDir = CurlDownloaderInternal::GetStagedUpdatesBaseDir();
			IFileManager::Get().MakeDirectory(*StagingBaseDir, true);

			UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Plugin '%s' has loaded modules -- staging update to: %s"), *TrimmedId, *StagingBaseDir);
			return true;
		}
	}

	// Unregister the mount point (content plugins) so assets are no longer served from this plugin content dir.
	const FString MountedAssetPath = ExistingPlugin->GetMountedAssetPath();
	const FString PluginContentDir = ExistingPlugin->GetContentDir();
	if (!MountedAssetPath.IsEmpty() && !PluginContentDir.IsEmpty())
	{
		FPackageName::UnRegisterMountPoint(MountedAssetPath, PluginContentDir);
	}

	// Delete plugin files from disk (content-only plugins should be removable while editor runs).
	const FString PluginBaseDir = FPaths::ConvertRelativePathToFull(ExistingPlugin->GetBaseDir());
	if (!PluginBaseDir.IsEmpty() && FPaths::DirectoryExists(PluginBaseDir))
	{
		const bool bPluginDeleted = IFileManager::Get().DeleteDirectory(*PluginBaseDir, /*RequireExists*/ false, /*Tree*/ true);
		if (!bPluginDeleted)
		{
			if (CurlDownloaderInternal::AddPendingPluginDelete(PluginBaseDir))
			{
				OutError = TEXT("Existing plugin files could not be removed (likely locked). Deletion was scheduled for next editor start. Please restart and try again.");
			}
			else
			{
				OutError = TEXT("Existing plugin files could not be removed (likely locked).");
			}
			return false;
		}
	}

	return true;
}

void UCurlDownloaderLibrary::ProcessPendingPluginDeletes()
{
	TArray<FString> PendingDirs;
	if (!CurlDownloaderInternal::LoadPendingPluginDeletes(PendingDirs))
	{
		return;
	}

	if (PendingDirs.IsEmpty())
	{
		return;
	}

	TArray<FString> RemainingDirs;
	RemainingDirs.Reserve(PendingDirs.Num());

	for (const FString& Dir : PendingDirs)
	{
		if (Dir.IsEmpty() || !FPaths::DirectoryExists(Dir))
		{
			continue;
		}

		const bool bDeleted = IFileManager::Get().DeleteDirectory(*Dir, false, true);
		if (!bDeleted)
		{
			RemainingDirs.Add(Dir);
		}
	}

	(void)CurlDownloaderInternal::SavePendingPluginDeletes(RemainingDirs);
}

FWorldBLDPluginUninstallResult UCurlDownloaderLibrary::UninstallPlugin(const FString& PluginName, const bool bDeferDeletionUntilRestartIfNeeded)
{
	FWorldBLDPluginUninstallResult Result;

	const FString TrimmedName = PluginName.TrimStartAndEnd();
	if (TrimmedName.IsEmpty())
	{
		Result.Message = TEXT("PluginName is empty.");
		return Result;
	}

	if (CurlDownloaderInternal::IsProtectedDevProject())
	{
		CurlDownloaderInternal::ShowProtectedProjectBlockedDialog(LOCTEXT("Action_Uninstall", "Uninstalling plugins"));
		Result.Message = TEXT("Action blocked in protected development project.");
		return Result;
	}

	// Safety: never allow removing the core plugins from within the editor.
	// WorldBLD is required to provide this tool, so keep it protected.
	if (TrimmedName.Equals(TEXT("WorldBLD"), ESearchCase::IgnoreCase))
	{
		Result.Message = TEXT("Refusing to uninstall WorldBLD (protected).");
		return Result;
	}

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TrimmedName);
	FString PluginBaseDir;
	FString PluginContentDir;
	FString MountedAssetPath;
	bool bHasCodeModules = false;

	if (Plugin.IsValid())
	{
		PluginBaseDir = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir());
		PluginContentDir = Plugin->GetContentDir();
		MountedAssetPath = Plugin->GetMountedAssetPath();
		bHasCodeModules = (Plugin->GetDescriptor().Modules.Num() > 0);
	}
	else
	{
		// Fallback for plugins that are present on disk but not registered with the plugin manager yet.
		const FString CandidateDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() / TrimmedName);
		const FString CandidateUPlugin = CandidateDir / (TrimmedName + TEXT(".uplugin"));
		if (FPaths::FileExists(CandidateUPlugin))
		{
			PluginBaseDir = CandidateDir;
		}
	}

	if (PluginBaseDir.IsEmpty())
	{
		Result.Message = TEXT("Plugin not found (not registered and no matching plugin directory found).");
		return Result;
	}

	// Disable in .uproject so it doesn't come back on next editor start.
	{
		FString DisableError;
		if (!CurlDownloaderInternal::SetPluginEnabledInProjectFile(TrimmedName, false, DisableError))
		{
			Result.Message = DisableError;
			return Result;
		}
	}

	// Un-mount (content mount point only). Code plugins cannot be safely unloaded without restart.
	if (!MountedAssetPath.IsEmpty() && !PluginContentDir.IsEmpty())
	{
		FPackageName::UnRegisterMountPoint(MountedAssetPath, PluginContentDir);
	}

	// Best-effort: clear cached download in Saved/<PersistentDownloadDir>/<ToolId>/...
	// The CurlDownloader "tool id" is typically the plugin name for installable plugins.
	const FString CachedDownloadDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPersistentDownloadDir() / TrimmedName);
	bool bCacheDeletedOrMissing = true;
	if (FPaths::DirectoryExists(CachedDownloadDir))
	{
		bCacheDeletedOrMissing = IFileManager::Get().DeleteDirectory(*CachedDownloadDir, false, true);
		if (!bCacheDeletedOrMissing && bDeferDeletionUntilRestartIfNeeded)
		{
			bCacheDeletedOrMissing = CurlDownloaderInternal::AddPendingPluginDelete(CachedDownloadDir);
			if (bCacheDeletedOrMissing)
			{
				Result.bDeletionDeferred = true;
				Result.bRestartRequired = true;
			}
		}
	}

	// Attempt delete now.
	const bool bDeleted = IFileManager::Get().DeleteDirectory(*PluginBaseDir, false, true);
	if (bDeleted)
	{
		Result.bSuccess = true;
		Result.bRestartRequired = bHasCodeModules;
		if (!bCacheDeletedOrMissing)
		{
			Result.Message = TEXT("Plugin disabled and deleted, but cached download cleanup failed.");
		}
		else
		{
			Result.Message = bHasCodeModules
				? TEXT("Plugin disabled and deleted. Editor restart is required for code plugins.")
				: TEXT("Plugin disabled and deleted.");
		}
		return Result;
	}

	// Could not delete (likely locked). Optionally defer until next start.
	if (bDeferDeletionUntilRestartIfNeeded)
	{
		Result.bSuccess = true;
		Result.bRestartRequired = true;
		Result.bDeletionDeferred = true;

		if (!CurlDownloaderInternal::AddPendingPluginDelete(PluginBaseDir))
		{
			Result.bSuccess = false;
			Result.bDeletionDeferred = false;
			Result.Message = TEXT("Plugin was disabled, but deletion failed and could not be scheduled for restart.");
			return Result;
		}

		Result.Message = TEXT("Plugin disabled. Files are locked; deletion scheduled for next editor start.");
		return Result;
	}

	Result.Message = TEXT("Plugin disabled, but deletion failed (files may be locked).");
	return Result;
}

int64 UCurlDownloaderLibrary::GetRemoteFileSize(const FString& URL)
{
	CURL* Curl = curl_easy_init();
	if (!Curl) return -1;

	int64 ContentLength = -1;
	curl_slist* Headers = nullptr;
	CurlDownloaderInternal::ConfigureCurlHandle(Curl, URL, Headers);
	curl_easy_setopt(Curl, CURLOPT_NOBODY, 1L);
	curl_easy_setopt(Curl, CURLOPT_HEADERFUNCTION, CurlDownloaderInternal::HeaderCallback);
	curl_easy_setopt(Curl, CURLOPT_HEADERDATA, &ContentLength);

	const CURLcode Res = curl_easy_perform(Curl);
	long HttpCode = 0;
	curl_easy_getinfo(Curl, CURLINFO_RESPONSE_CODE, &HttpCode);
	if (Res != CURLE_OK)
	{
		UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] HEAD request failed for size probe (curl=%d: %s)"), static_cast<int32>(Res), UTF8_TO_TCHAR(curl_easy_strerror(Res)));
	}

	if (ContentLength <= 0)
	{
		curl_off_t CurlSize = 0;
		curl_easy_getinfo(Curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &CurlSize);
		ContentLength = static_cast<int64>(CurlSize);
	}

	// If Content-Length is unavailable (common for some download endpoints), try a HEAD request with a Range header and parse Content-Range.
	if (ContentLength <= 0 && Res == CURLE_OK && HttpCode >= 200 && HttpCode < 500)
	{
		int64 TotalFromContentRange = -1;
		curl_easy_setopt(Curl, CURLOPT_NOBODY, 1L);
		curl_easy_setopt(Curl, CURLOPT_RANGE, "0-0");
		curl_easy_setopt(Curl, CURLOPT_HEADERFUNCTION, CurlDownloaderInternal::ContentRangeHeaderCallback);
		curl_easy_setopt(Curl, CURLOPT_HEADERDATA, &TotalFromContentRange);

		const CURLcode RangeRes = curl_easy_perform(Curl);
		long RangeHttp = 0;
		curl_easy_getinfo(Curl, CURLINFO_RESPONSE_CODE, &RangeHttp);

		if (RangeRes != CURLE_OK)
		{
			UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] Range HEAD request failed for size probe (curl=%d: %s)"), static_cast<int32>(RangeRes), UTF8_TO_TCHAR(curl_easy_strerror(RangeRes)));
		}
		else if (TotalFromContentRange > 0)
		{
			ContentLength = TotalFromContentRange;
		}
	}

	curl_easy_cleanup(Curl);
	if (Headers)
	{
		curl_slist_free_all(Headers);
	}
	UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] File size: %lld bytes"), ContentLength);
	return ContentLength;
}

bool UCurlDownloaderLibrary::ServerSupportsRangeRequests(const FString& URL)
{
	CURL* Curl = curl_easy_init();
	if (!Curl) return false;

	bool bAcceptsRanges = false;
	curl_slist* Headers = nullptr;
	CurlDownloaderInternal::ConfigureCurlHandle(Curl, URL, Headers);
	curl_easy_setopt(Curl, CURLOPT_NOBODY, 1L);

	struct FData { bool* pResult; } Data = { &bAcceptsRanges };
	curl_easy_setopt(Curl, CURLOPT_HEADERFUNCTION, +[](char* Buffer, size_t Size, size_t N, void* U) -> size_t
	{
		FString H(Size * N, UTF8_TO_TCHAR(Buffer));
		if (H.StartsWith(TEXT("Accept-Ranges:"), ESearchCase::IgnoreCase) && H.Contains(TEXT("bytes")))
			*static_cast<FData*>(U)->pResult = true;
		return Size * N;
	});
	curl_easy_setopt(Curl, CURLOPT_HEADERDATA, &Data);

	const CURLcode Res = curl_easy_perform(Curl);
	long HttpCode = 0;
	curl_easy_getinfo(Curl, CURLINFO_RESPONSE_CODE, &HttpCode);
	if (Res != CURLE_OK)
	{
		UE_LOG(LogTemp, Error, TEXT("[CurlDownloader] HEAD request failed for range probe (curl=%d: %s)"), static_cast<int32>(Res), UTF8_TO_TCHAR(curl_easy_strerror(Res)));
	}

	// If Accept-Ranges wasn't advertised, attempt a HEAD+Range probe and treat a 206 (or Content-Range) as support.
	if (!bAcceptsRanges && Res == CURLE_OK && HttpCode >= 200 && HttpCode < 500)
	{
		int64 TotalFromContentRange = -1;
		curl_easy_setopt(Curl, CURLOPT_NOBODY, 1L);
		curl_easy_setopt(Curl, CURLOPT_RANGE, "0-0");
		curl_easy_setopt(Curl, CURLOPT_HEADERFUNCTION, CurlDownloaderInternal::ContentRangeHeaderCallback);
		curl_easy_setopt(Curl, CURLOPT_HEADERDATA, &TotalFromContentRange);

		const CURLcode RangeRes = curl_easy_perform(Curl);
		long RangeHttp = 0;
		curl_easy_getinfo(Curl, CURLINFO_RESPONSE_CODE, &RangeHttp);
		if (RangeRes == CURLE_OK && (RangeHttp == 206 || TotalFromContentRange > 0))
		{
			bAcceptsRanges = true;
		}
	}

	curl_easy_cleanup(Curl);
	if (Headers)
	{
		curl_slist_free_all(Headers);
	}

	UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Range requests: %s"), bAcceptsRanges ? TEXT("yes") : TEXT("no"));
	return bAcceptsRanges;
}

void UCurlDownloaderLibrary::RequestToolDownload(const FString& ToolId, const FString& DownloadUrl,
                                                  const FString& ExpectedSHA1, FDOnDownloadProgress OnProgress, FDOnComplete OnComplete)
{
	if (CurlDownloaderInternal::IsProtectedDevProject())
	{
		CurlDownloaderInternal::ShowProtectedProjectBlockedDialog(LOCTEXT("Action_Install", "Installing tools"));
		AsyncTask(ENamedThreads::GameThread, [OnProgress, OnComplete]()
		{
			OnProgress.ExecuteIfBound(0.0f);
			OnComplete.ExecuteIfBound(false);
		});
		return;
	}

	// If already downloading, bind to existing download instead of starting a new one
	{
		FScopeLock Lock(&DownloadsMutex);
		if (TSharedRef<FCurlDownloadContext>* Found = CurrentDownloads.Find(ToolId))
		{
			UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Already downloading '%s', binding callbacks to existing download"), *ToolId);

			const TSharedRef<FCurlDownloadContext>& Context = *Found;
			const float CurrentProgress = Context->GetProgress();

			// Report current progress immediately
			AsyncTask(ENamedThreads::GameThread, [OnProgress, CurrentProgress]() { OnProgress.ExecuteIfBound(CurrentProgress); });

			// Bind new callbacks
			Context->OnProgress.AddLambda([OnProgress](float P)
			{
				AsyncTask(ENamedThreads::GameThread, [OnProgress, P]() { OnProgress.ExecuteIfBound(P); });
			});
			Context->OnComplete.AddLambda([OnComplete](bool S)
			{
				AsyncTask(ENamedThreads::GameThread, [OnComplete, S]() { OnComplete.ExecuteIfBound(S); });
			});
			return;
		}
	}

	// Ensure we start from a clean state for tool installs/updates (remove cached download + existing plugin files for content plugins).
	bool bStagedUpdate = false;
	{
		FString PrepareError;
		if (!PrepareForToolInstallOrUpdate(ToolId, PrepareError, bStagedUpdate))
		{
			if (!PrepareError.IsEmpty())
			{
				UUtilitiesLibrary::ShowOkModalDialog(FText::FromString(PrepareError), LOCTEXT("ToolUpdateBlocked_Title", "Update blocked"));
			}

			AsyncTask(ENamedThreads::GameThread, [OnProgress, OnComplete]()
			{
				OnProgress.ExecuteIfBound(0.0f);
				OnComplete.ExecuteIfBound(false);
			});
			return;
		}
	}

	// Pass the staging BASE directory as the extraction root (not the per-tool subdir).
	// The ZIP's internal paths (e.g. "CityBLD/...") will create the tool subdirectory
	// inside it, mirroring the normal flow where Plugins/ is the extraction root.
	const FString StagedExtractionRoot = bStagedUpdate ? CurlDownloaderInternal::GetStagedUpdatesBaseDir() : FString();

	// Construct URL from ToolId if DownloadUrl is empty
	FString FinalDownloadUrl = DownloadUrl;
	if (FinalDownloadUrl.IsEmpty() && !ToolId.IsEmpty())
	{
		FinalDownloadUrl = FString::Printf(TEXT("https://worldbld.com/api/tools/%s/download"), *ToolId);
		UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] DownloadUrl empty, constructed from ToolId: %s"), *FinalDownloadUrl);
	}

	const FString SavePath = FPaths::Combine(FPaths::ProjectPersistentDownloadDir(), ToolId, ToolId);
	RequestFileDownloadInternal(ToolId, FinalDownloadUrl, SavePath, ExpectedSHA1, /*bExtractZipToPlugins*/ true, OnProgress, OnComplete, StagedExtractionRoot);
}

void UCurlDownloaderLibrary::RequestToolDownloadNative(const FString& ToolId, const FString& DownloadUrl, const FString& ExpectedSHA1,
	FOnDownloadProgress::FDelegate OnProgress, FOnComplete::FDelegate OnComplete)
{
	if (CurlDownloaderInternal::IsProtectedDevProject())
	{
		CurlDownloaderInternal::ShowProtectedProjectBlockedDialog(LOCTEXT("Action_Install", "Installing tools"));
		AsyncTask(ENamedThreads::GameThread, [OnProgress, OnComplete]()
		{
			OnProgress.ExecuteIfBound(0.0f);
			OnComplete.ExecuteIfBound(false);
		});
		return;
	}

	// Ensure we start from a clean state for tool installs/updates (remove cached download + existing plugin files for content plugins).
	bool bStagedUpdate = false;
	{
		FString PrepareError;
		if (!PrepareForToolInstallOrUpdate(ToolId, PrepareError, bStagedUpdate))
		{
			if (!PrepareError.IsEmpty())
			{
				UUtilitiesLibrary::ShowOkModalDialog(FText::FromString(PrepareError), LOCTEXT("ToolUpdateBlocked_Title2", "Update blocked"));
			}

			AsyncTask(ENamedThreads::GameThread, [OnProgress, OnComplete]()
			{
				OnProgress.ExecuteIfBound(0.0f);
				OnComplete.ExecuteIfBound(false);
			});
			return;
		}
	}

	// Pass the staging BASE directory as the extraction root (see comment in RequestToolDownload above).
	const FString StagedExtractionRoot = bStagedUpdate ? CurlDownloaderInternal::GetStagedUpdatesBaseDir() : FString();

	// Construct URL from ToolId if DownloadUrl is empty
	FString FinalDownloadUrl = DownloadUrl;
	if (FinalDownloadUrl.IsEmpty() && !ToolId.IsEmpty())
	{
		FinalDownloadUrl = FString::Printf(TEXT("https://worldbld.com/api/tools/%s/download"), *ToolId);
		UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] DownloadUrl empty, constructed from ToolId: %s"), *FinalDownloadUrl);
	}

	const FString SavePath = FPaths::Combine(FPaths::ProjectPersistentDownloadDir(), ToolId, ToolId);
	RequestFileDownloadInternalNative(ToolId, FinalDownloadUrl, SavePath, ExpectedSHA1, /*bExtractZipToPlugins*/ true, MoveTemp(OnProgress), MoveTemp(OnComplete), StagedExtractionRoot);
}

void UCurlDownloaderLibrary::RequestFileDownload(const FString& ToolId, const FString& DownloadUrl, const FString& SavePath,
	const FString& ExpectedSHA1, FDOnDownloadProgress OnProgress, FDOnComplete OnComplete)
{
	const bool bExtractZipToPlugins = SavePath.StartsWith(FPaths::ProjectDir());
	RequestFileDownloadInternal(ToolId, DownloadUrl, SavePath, ExpectedSHA1, bExtractZipToPlugins, OnProgress, OnComplete);
}

void UCurlDownloaderLibrary::RequestFileDownloadNative(const FString& ToolId, const FString& DownloadUrl, const FString& SavePath, const FString& ExpectedSHA1,
	FOnDownloadProgress::FDelegate OnProgress, FOnComplete::FDelegate OnComplete)
{
	const bool bExtractZipToPlugins = SavePath.StartsWith(FPaths::ProjectDir());
	RequestFileDownloadInternalNative(ToolId, DownloadUrl, SavePath, ExpectedSHA1, bExtractZipToPlugins, MoveTemp(OnProgress), MoveTemp(OnComplete));
}

void UCurlDownloaderLibrary::RequestFileDownloadInternal(const FString& ToolId, const FString& DownloadUrl, const FString& SavePath,
	const FString& ExpectedSHA1, const bool bExtractZipToPlugins, FDOnDownloadProgress OnProgress, FDOnComplete OnComplete,
	const FString& InStagedExtractionDir)
{
	const bool bIsStagedUpdate = !InStagedExtractionDir.IsEmpty();

	// If already downloading, bind to existing download instead of starting a new one
	{
		FScopeLock Lock(&DownloadsMutex);
		if (TSharedRef<FCurlDownloadContext>* Found = CurrentDownloads.Find(ToolId))
		{
			UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Already downloading '%s', binding callbacks to existing download"), *ToolId);

			const TSharedRef<FCurlDownloadContext>& Context = *Found;
			const float CurrentProgress = Context->GetProgress();

			// Report current progress immediately
			AsyncTask(ENamedThreads::GameThread, [OnProgress, CurrentProgress]() { OnProgress.ExecuteIfBound(CurrentProgress); });

			// Bind new callbacks
			Context->OnProgress.AddLambda([OnProgress](float P)
			{
				AsyncTask(ENamedThreads::GameThread, [OnProgress, P]() { OnProgress.ExecuteIfBound(P); });
			});
			Context->OnComplete.AddLambda([OnComplete](bool S)
			{
				AsyncTask(ENamedThreads::GameThread, [OnComplete, S]() { OnComplete.ExecuteIfBound(S); });
			});
			return;
		}
	}

	// Check if file already exists with matching SHA1
	if (!ExpectedSHA1.IsEmpty() && FPaths::FileExists(SavePath))
	{
		UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Checking existing file SHA1: %s"), *SavePath);
		FString ExistingSHA1 = CurlDownloaderInternal::CalculateFileSHA1(SavePath);

		if (ExistingSHA1.Equals(ExpectedSHA1, ESearchCase::IgnoreCase))
		{
			UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] SHA1 match! Skipping download"));

			Async(EAsyncExecution::ThreadPool, [SavePath, ToolId, ExpectedSHA1, bExtractZipToPlugins, bIsStagedUpdate, StagedDir = InStagedExtractionDir, OnProgress, OnComplete]()
			{
				AsyncTask(ENamedThreads::GameThread, [OnProgress]() { OnProgress.ExecuteIfBound(1.0f); });

				bool bSuccess = true;
				if (bExtractZipToPlugins)
				{
					bSuccess = CurlDownloaderInternal::ExtractZipToPlugins(SavePath, ToolId, StagedDir);
				}

				AsyncTask(ENamedThreads::GameThread, [ToolId, ExpectedSHA1, bExtractZipToPlugins, bIsStagedUpdate, StagedDir, OnComplete, bSuccess]()
				{
					bool bFinalSuccess = bSuccess;

					if (bFinalSuccess && bIsStagedUpdate)
					{
						const FString StagedToolDir = CurlDownloaderInternal::ResolveExtractedStagedPluginDir(ToolId, StagedDir);
						CurlDownloaderInternal::WriteStagedUpdateManifestEntry(ToolId, StagedToolDir, ExpectedSHA1);
					}
					else
					{
						if (bFinalSuccess && bExtractZipToPlugins)
						{
							FString EnableError;
							if (!CurlDownloaderInternal::SetPluginEnabledInProjectFile(ToolId, true, EnableError))
							{
								bFinalSuccess = false;
								UUtilitiesLibrary::ShowOkModalDialog(FText::FromString(EnableError), LOCTEXT("EnablePluginFailed_Title", "Install failed"));
							}
						}

						if (bFinalSuccess && bExtractZipToPlugins && !ExpectedSHA1.IsEmpty())
						{
							if (UWorldBLDToolUpdateSettings* Settings = GetMutableDefault<UWorldBLDToolUpdateSettings>())
							{
								Settings->SetInstalledSha1AndSave(ToolId, ExpectedSHA1);
							}
						}
					}

					OnComplete.ExecuteIfBound(bFinalSuccess);
				});
			});
			return;
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] SHA1 mismatch (existing: %s, expected: %s), re-downloading"), *ExistingSHA1, *ExpectedSHA1);
		}
	}

	TSharedRef<FCurlDownloadContext> Context = MakeShared<FCurlDownloadContext>();
	Context->URL = DownloadUrl;
	Context->SavePath = SavePath;
	Context->PluginName = ToolId;
	Context->ExpectedSHA1 = ExpectedSHA1;
	Context->bExtractZipToPlugins = bExtractZipToPlugins;
	Context->bStagedUpdate = bIsStagedUpdate;
	Context->StagedExtractionDir = InStagedExtractionDir;

	Context->OnProgress.AddLambda([OnProgress](float P)
	{
		AsyncTask(ENamedThreads::GameThread, [OnProgress, P]() { OnProgress.ExecuteIfBound(P); });
	});

	FString ToolIdCopy = ToolId;
	const FString ExpectedSha1Copy = ExpectedSHA1;
	const FString StagedDirCopy = InStagedExtractionDir;
	Context->OnComplete.AddLambda([this, ToolIdCopy, ExpectedSha1Copy, bExtractZipToPlugins, bIsStagedUpdate, StagedDirCopy, OnComplete](bool bSuccess)
	{
		AsyncTask(ENamedThreads::GameThread, [this, ToolIdCopy, ExpectedSha1Copy, bExtractZipToPlugins, bIsStagedUpdate, StagedDirCopy, OnComplete, bSuccess]()
		{
			{ FScopeLock Lock(&DownloadsMutex); CurrentDownloads.Remove(ToolIdCopy); }

			bool bFinalSuccess = bSuccess;

			if (bFinalSuccess && bIsStagedUpdate)
			{
				const FString StagedToolDir = CurlDownloaderInternal::ResolveExtractedStagedPluginDir(ToolIdCopy, StagedDirCopy);
				CurlDownloaderInternal::WriteStagedUpdateManifestEntry(ToolIdCopy, StagedToolDir, ExpectedSha1Copy);
			}
			else
			{
				if (bFinalSuccess && bExtractZipToPlugins)
				{
					FString EnableError;
					if (!CurlDownloaderInternal::SetPluginEnabledInProjectFile(ToolIdCopy, true, EnableError))
					{
						bFinalSuccess = false;
						UUtilitiesLibrary::ShowOkModalDialog(FText::FromString(EnableError), LOCTEXT("EnablePluginFailed_Title2", "Install failed"));
					}
				}

				if (bFinalSuccess && bExtractZipToPlugins && !ExpectedSha1Copy.IsEmpty())
				{
					if (UWorldBLDToolUpdateSettings* Settings = GetMutableDefault<UWorldBLDToolUpdateSettings>())
					{
						Settings->SetInstalledSha1AndSave(ToolIdCopy, ExpectedSha1Copy);
					}
				}
			}

			OnComplete.ExecuteIfBound(bFinalSuccess);
		});
	});

	{ FScopeLock Lock(&DownloadsMutex); CurrentDownloads.Add(ToolId, Context); }

	UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Starting: %s -> %s%s"), *ToolId, *SavePath, bIsStagedUpdate ? TEXT(" (staged)") : TEXT(""));
	StartAdaptiveDownload(Context);
}

void UCurlDownloaderLibrary::RequestFileDownloadInternalNative(const FString& ToolId, const FString& DownloadUrl, const FString& SavePath,
	const FString& ExpectedSHA1, const bool bExtractZipToPlugins, FOnDownloadProgress::FDelegate OnProgress, FOnComplete::FDelegate OnComplete,
	const FString& InStagedExtractionDir)
{
	const bool bIsStagedUpdate = !InStagedExtractionDir.IsEmpty();

	// If already downloading, bind to existing download instead of starting a new one
	{
		FScopeLock Lock(&DownloadsMutex);
		if (TSharedRef<FCurlDownloadContext>* Found = CurrentDownloads.Find(ToolId))
		{
			UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Already downloading '%s', binding callbacks to existing download"), *ToolId);

			const TSharedRef<FCurlDownloadContext>& Context = *Found;
			const float CurrentProgress = Context->GetProgress();

			// Report current progress immediately
			AsyncTask(ENamedThreads::GameThread, [OnProgress, CurrentProgress]() { OnProgress.ExecuteIfBound(CurrentProgress); });

			// Bind new callbacks
			Context->OnProgress.AddLambda([OnProgress](float P)
			{
				AsyncTask(ENamedThreads::GameThread, [OnProgress, P]() { OnProgress.ExecuteIfBound(P); });
			});
			Context->OnComplete.AddLambda([OnComplete](bool S)
			{
				AsyncTask(ENamedThreads::GameThread, [OnComplete, S]() { OnComplete.ExecuteIfBound(S); });
			});
			return;
		}
	}

	// Check if file already exists with matching SHA1
	if (!ExpectedSHA1.IsEmpty() && FPaths::FileExists(SavePath))
	{
		UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Checking existing file SHA1: %s"), *SavePath);
		FString ExistingSHA1 = CurlDownloaderInternal::CalculateFileSHA1(SavePath);

		if (ExistingSHA1.Equals(ExpectedSHA1, ESearchCase::IgnoreCase))
		{
			UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] SHA1 match! Skipping download"));

			Async(EAsyncExecution::ThreadPool, [SavePath, ToolId, ExpectedSHA1, bExtractZipToPlugins, bIsStagedUpdate, StagedDir = InStagedExtractionDir, OnProgress, OnComplete]()
			{
				AsyncTask(ENamedThreads::GameThread, [OnProgress]() { OnProgress.ExecuteIfBound(1.0f); });

				bool bSuccess = true;
				if (bExtractZipToPlugins)
				{
					bSuccess = CurlDownloaderInternal::ExtractZipToPlugins(SavePath, ToolId, StagedDir);
				}

				AsyncTask(ENamedThreads::GameThread, [ToolId, ExpectedSHA1, bExtractZipToPlugins, bIsStagedUpdate, StagedDir, OnComplete, bSuccess]()
				{
					bool bFinalSuccess = bSuccess;

					if (bFinalSuccess && bIsStagedUpdate)
					{
						const FString StagedToolDir = CurlDownloaderInternal::ResolveExtractedStagedPluginDir(ToolId, StagedDir);
						CurlDownloaderInternal::WriteStagedUpdateManifestEntry(ToolId, StagedToolDir, ExpectedSHA1);
					}
					else
					{
						if (bFinalSuccess && bExtractZipToPlugins)
						{
							FString EnableError;
							if (!CurlDownloaderInternal::SetPluginEnabledInProjectFile(ToolId, true, EnableError))
							{
								bFinalSuccess = false;
								UUtilitiesLibrary::ShowOkModalDialog(FText::FromString(EnableError), LOCTEXT("EnablePluginFailed_Title", "Install failed"));
							}
						}

						if (bFinalSuccess && bExtractZipToPlugins && !ExpectedSHA1.IsEmpty())
						{
							if (UWorldBLDToolUpdateSettings* Settings = GetMutableDefault<UWorldBLDToolUpdateSettings>())
							{
								Settings->SetInstalledSha1AndSave(ToolId, ExpectedSHA1);
							}
						}
					}

					OnComplete.ExecuteIfBound(bFinalSuccess);
				});
			});
			return;
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] SHA1 mismatch (existing: %s, expected: %s), re-downloading"), *ExistingSHA1, *ExpectedSHA1);
		}
	}

	TSharedRef<FCurlDownloadContext> Context = MakeShared<FCurlDownloadContext>();
	Context->URL = DownloadUrl;
	Context->SavePath = SavePath;
	Context->PluginName = ToolId;
	Context->ExpectedSHA1 = ExpectedSHA1;
	Context->bExtractZipToPlugins = bExtractZipToPlugins;
	Context->bStagedUpdate = bIsStagedUpdate;
	Context->StagedExtractionDir = InStagedExtractionDir;

	Context->OnProgress.AddLambda([OnProgress](float P)
	{
		AsyncTask(ENamedThreads::GameThread, [OnProgress, P]() { OnProgress.ExecuteIfBound(P); });
	});

	FString ToolIdCopy = ToolId;
	const FString ExpectedSha1Copy = ExpectedSHA1;
	const FString StagedDirCopy = InStagedExtractionDir;
	Context->OnComplete.AddLambda([this, ToolIdCopy, ExpectedSha1Copy, bExtractZipToPlugins, bIsStagedUpdate, StagedDirCopy, OnComplete](bool bSuccess)
	{
		AsyncTask(ENamedThreads::GameThread, [this, ToolIdCopy, ExpectedSha1Copy, bExtractZipToPlugins, bIsStagedUpdate, StagedDirCopy, OnComplete, bSuccess]()
		{
			{
				FScopeLock Lock(&DownloadsMutex);
				CurrentDownloads.Remove(ToolIdCopy);
			}

			bool bFinalSuccess = bSuccess;
			if (bFinalSuccess && bIsStagedUpdate)
			{
				const FString StagedToolDir = CurlDownloaderInternal::ResolveExtractedStagedPluginDir(ToolIdCopy, StagedDirCopy);
				CurlDownloaderInternal::WriteStagedUpdateManifestEntry(ToolIdCopy, StagedToolDir, ExpectedSha1Copy);
			}
			else
			{
				if (bFinalSuccess && bExtractZipToPlugins)
				{
					FString EnableError;
					if (!CurlDownloaderInternal::SetPluginEnabledInProjectFile(ToolIdCopy, true, EnableError))
					{
						bFinalSuccess = false;
						UUtilitiesLibrary::ShowOkModalDialog(FText::FromString(EnableError), LOCTEXT("EnablePluginFailed_Title_Native", "Install failed"));
					}
				}

				if (bFinalSuccess && bExtractZipToPlugins && !ExpectedSha1Copy.IsEmpty())
				{
					if (UWorldBLDToolUpdateSettings* Settings = GetMutableDefault<UWorldBLDToolUpdateSettings>())
					{
						Settings->SetInstalledSha1AndSave(ToolIdCopy, ExpectedSha1Copy);
					}
				}
			}

			OnComplete.ExecuteIfBound(bFinalSuccess);
		});
	});

	{
		FScopeLock Lock(&DownloadsMutex);
		CurrentDownloads.Add(ToolId, Context);
	}

	StartAdaptiveDownload(Context);
}

void UCurlDownloaderLibrary::StartAdaptiveDownload(TSharedRef<FCurlDownloadContext> Context)
{
	TSharedPtr<FCurlDownloadContext> ContextPtr = Context.ToSharedPtr();
	Async(EAsyncExecution::ThreadPool, [ContextPtr]()
	{
		CurlDownloaderInternal::ExecuteAdaptiveDownload(ContextPtr);
	});
}

void UCurlDownloaderLibrary::CancelDownload(const FString& ToolId)
{
	FScopeLock Lock(&DownloadsMutex);
	if (TSharedRef<FCurlDownloadContext>* Found = CurrentDownloads.Find(ToolId))
	{
		(*Found)->bCancelled.Store(true);
		UE_LOG(LogTemp, Log, TEXT("[CurlDownloader] Cancelled: %s"), *ToolId);
	}
}

bool UCurlDownloaderLibrary::IsDownloadInProgress(const FString& ToolId) const
{
	FScopeLock Lock(&const_cast<UCurlDownloaderLibrary*>(this)->DownloadsMutex);
	return CurrentDownloads.Contains(ToolId);
}

float UCurlDownloaderLibrary::GetDownloadProgress(const FString& ToolId) const
{
	FScopeLock Lock(&const_cast<UCurlDownloaderLibrary*>(this)->DownloadsMutex);
	const TSharedRef<FCurlDownloadContext>* Found = CurrentDownloads.Find(ToolId);
	if (!Found)
	{
		return -1.0f;
	}
	return (*Found)->GetProgress();
}

#undef LOCTEXT_NAMESPACE
