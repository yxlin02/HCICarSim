// Copyright WorldBLD LLC. All Rights Reserved.

#include "WorldBLDBootstrapModule.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/PrettyJsonPrintPolicy.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldBLDBootstrap, Log, All);

// Journal phase identifiers.
namespace JournalPhase
{
	const TCHAR* BackupOld = TEXT("BackupOld");
	const TCHAR* CopyNew   = TEXT("CopyNew");
	const TCHAR* Cleanup   = TEXT("Cleanup");
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON helpers (local)
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
	/** Serialize a JSON object to a pretty-printed string and save to disk. */
	bool SaveJsonToFile(const TSharedRef<FJsonObject>& JsonObj, const FString& FilePath)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
		FString OutJson;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		if (!FJsonSerializer::Serialize(JsonObj, Writer))
		{
			return false;
		}
		return FFileHelper::SaveStringToFile(OutJson, *FilePath);
	}

	/** Load a JSON object from a file. */
	TSharedPtr<FJsonObject> LoadJsonFromFile(const FString& FilePath)
	{
		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
		{
			return nullptr;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return nullptr;
		}
		return Root;
	}

	/** Convert a TArray<FString> to a TArray<TSharedPtr<FJsonValue>> of string values. */
	TArray<TSharedPtr<FJsonValue>> StringArrayToJsonArray(const TArray<FString>& Strings)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		Result.Reserve(Strings.Num());
		for (const FString& S : Strings)
		{
			Result.Add(MakeShared<FJsonValueString>(S));
		}
		return Result;
	}

	/** Read a string array from a JSON object field. */
	TArray<FString> JsonArrayToStringArray(const TSharedPtr<FJsonObject>& Obj, const FString& FieldName)
	{
		TArray<FString> Result;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Obj.IsValid() && Obj->TryGetArrayField(FieldName, Arr) && Arr)
		{
			for (const TSharedPtr<FJsonValue>& Val : *Arr)
			{
				FString S;
				if (Val.IsValid() && Val->TryGetString(S))
				{
					Result.Add(S);
				}
			}
		}
		return Result;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Module lifecycle
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_MODULE(FWorldBLDBootstrapModule, WorldBLDBootstrap)

void FWorldBLDBootstrapModule::StartupModule()
{
	if (IsProtectedDevProject())
	{
		UE_LOG(LogWorldBLDBootstrap, Log, TEXT("Protected dev project detected -- skipping staged update processing."));
		return;
	}

	CleanUpLeftoverOldDirectories();
	RecoverFromAnyJournals();
	ProcessStagedUpdates();
}

void FWorldBLDBootstrapModule::ShutdownModule()
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Path helpers
// ─────────────────────────────────────────────────────────────────────────────

FString FWorldBLDBootstrapModule::GetStagedUpdatesManifestPath()
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("WorldBLD/StagedPluginUpdates.json"));
}

bool FWorldBLDBootstrapModule::IsProtectedDevProject()
{
	const FString ProjectDir = FPaths::ProjectDir();
	return FPaths::FileExists(ProjectDir / TEXT("WORLDBLD_API_DOCUMENTATION.md"))
		|| FPaths::FileExists(ProjectDir / TEXT("GenerateAndBuildProject.bat"));
}

FString FWorldBLDBootstrapModule::GetJournalPath(const FString& PluginName)
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("WorldBLD") / (TEXT("UpdateJournal_") + PluginName + TEXT(".json")));
}

FString FWorldBLDBootstrapModule::GetBackupsBaseDir()
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("WorldBLD/Backups"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Manifest processing
// ─────────────────────────────────────────────────────────────────────────────

void FWorldBLDBootstrapModule::ProcessStagedUpdates()
{
	const FString ManifestPath = GetStagedUpdatesManifestPath();
	if (!FPaths::FileExists(ManifestPath))
	{
		return;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ManifestPath))
	{
		UE_LOG(LogWorldBLDBootstrap, Warning, TEXT("Failed to read staged-updates manifest: %s"), *ManifestPath);
		return;
	}

	TSharedPtr<FJsonObject> Root;
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogWorldBLDBootstrap, Warning, TEXT("Failed to parse staged-updates manifest JSON."));
			return;
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* StagedArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("StagedUpdates"), StagedArray) || (StagedArray == nullptr))
	{
		return;
	}

	const FDateTime Now = FDateTime::UtcNow();
	constexpr int32 MaxStaleDays = 7;
	bool bManifestDirty = false;

	// Collect latest entry per plugin name (in case of duplicates).
	TMap<FString, int32> LatestEntryIndex; // PluginName -> index in StagedArray
	for (int32 Idx = 0; Idx < StagedArray->Num(); ++Idx)
	{
		const TSharedPtr<FJsonObject> Entry = (*StagedArray)[Idx].IsValid()
			? (*StagedArray)[Idx]->AsObject()
			: nullptr;
		if (!Entry.IsValid())
		{
			continue;
		}

		// Skip already-applied entries (will be finalized by CurlDownloaderLibrary later).
		bool bApplied = false;
		Entry->TryGetBoolField(TEXT("Applied"), bApplied);
		if (bApplied)
		{
			continue;
		}

		FString PluginName;
		if (!Entry->TryGetStringField(TEXT("PluginName"), PluginName) || PluginName.IsEmpty())
		{
			continue;
		}

		// Stale check.
		FString TimestampStr;
		if (Entry->TryGetStringField(TEXT("Timestamp"), TimestampStr))
		{
			FDateTime EntryTime;
			if (FDateTime::ParseIso8601(*TimestampStr, EntryTime))
			{
				const FTimespan Age = Now - EntryTime;
				if (Age.GetTotalDays() > MaxStaleDays)
				{
					UE_LOG(LogWorldBLDBootstrap, Warning,
						TEXT("Staged update for '%s' is stale (%.1f days old) -- skipping and cleaning up."),
						*PluginName, Age.GetTotalDays());

					FString StagingDir;
					if (Entry->TryGetStringField(TEXT("StagingDir"), StagingDir) && FPaths::DirectoryExists(StagingDir))
					{
						IFileManager::Get().DeleteDirectory(*StagingDir, false, true);
					}

					Entry->SetBoolField(TEXT("Applied"), true);
					bManifestDirty = true;
					continue;
				}
			}
		}

		// Keep only the latest entry per plugin (by index, which correlates with append order).
		if (const int32* Existing = LatestEntryIndex.Find(PluginName))
		{
			// Mark the older duplicate as applied so it gets cleaned up.
			if (const TSharedPtr<FJsonObject> OlderEntry = (*StagedArray)[*Existing]->AsObject())
			{
				FString OldStagingDir;
				if (OlderEntry->TryGetStringField(TEXT("StagingDir"), OldStagingDir) && FPaths::DirectoryExists(OldStagingDir))
				{
					IFileManager::Get().DeleteDirectory(*OldStagingDir, false, true);
				}
				OlderEntry->SetBoolField(TEXT("Applied"), true);
				bManifestDirty = true;
			}
		}
		LatestEntryIndex.Add(PluginName, Idx);
	}

	// Process the latest entry for each plugin.
	for (const TPair<FString, int32>& Pair : LatestEntryIndex)
	{
		const TSharedPtr<FJsonObject> Entry = (*StagedArray)[Pair.Value]->AsObject();
		if (!Entry.IsValid())
		{
			continue;
		}

		const FString& PluginName = Pair.Key;
		FString StagingDir;
		FString TargetPluginDir;
		if (!Entry->TryGetStringField(TEXT("StagingDir"), StagingDir) || StagingDir.IsEmpty()
			|| !Entry->TryGetStringField(TEXT("TargetPluginDir"), TargetPluginDir) || TargetPluginDir.IsEmpty())
		{
			UE_LOG(LogWorldBLDBootstrap, Warning,
				TEXT("Staged update for '%s' has missing StagingDir or TargetPluginDir -- skipping."), *PluginName);
			continue;
		}

		// Validate that the staging dir contains a .uplugin file.
		const FString ExpectedUpluginFile = StagingDir / (PluginName + TEXT(".uplugin"));
		if (!FPaths::FileExists(ExpectedUpluginFile))
		{
			UE_LOG(LogWorldBLDBootstrap, Error,
				TEXT("Staged update for '%s' is missing .uplugin file at '%s' -- skipping."),
				*PluginName, *ExpectedUpluginFile);
			continue;
		}

		// Before swapping, read module names from both old and new .uplugin for mismatch detection.
		TArray<FString> OldModuleNames;
		TArray<FString> NewModuleNames;
		{
			auto ExtractModuleNames = [](const FString& UpluginPath, TArray<FString>& OutNames)
			{
				FString DescJson;
				if (!FFileHelper::LoadFileToString(DescJson, *UpluginPath))
				{
					return;
				}
				TSharedPtr<FJsonObject> DescRoot;
				const TSharedRef<TJsonReader<>> DescReader = TJsonReaderFactory<>::Create(DescJson);
				if (!FJsonSerializer::Deserialize(DescReader, DescRoot) || !DescRoot.IsValid())
				{
					return;
				}
				const TArray<TSharedPtr<FJsonValue>>* ModulesArray = nullptr;
				if (DescRoot->TryGetArrayField(TEXT("Modules"), ModulesArray) && ModulesArray)
				{
					for (const TSharedPtr<FJsonValue>& ModVal : *ModulesArray)
					{
						const TSharedPtr<FJsonObject> ModObj = ModVal.IsValid() ? ModVal->AsObject() : nullptr;
						if (ModObj.IsValid())
						{
							FString ModName;
							if (ModObj->TryGetStringField(TEXT("Name"), ModName) && !ModName.IsEmpty())
							{
								OutNames.Add(ModName);
							}
						}
					}
				}
			};

			const FString OldUpluginPath = TargetPluginDir / (PluginName + TEXT(".uplugin"));
			if (FPaths::FileExists(OldUpluginPath))
			{
				ExtractModuleNames(OldUpluginPath, OldModuleNames);
			}
			ExtractModuleNames(ExpectedUpluginFile, NewModuleNames);
		}

		UE_LOG(LogWorldBLDBootstrap, Log,
			TEXT("Applying staged update for '%s': '%s' -> '%s'"), *PluginName, *StagingDir, *TargetPluginDir);

		if (SwapPluginDirectoryJournaled(PluginName, StagingDir, TargetPluginDir))
		{
			Entry->SetBoolField(TEXT("Applied"), true);
			bManifestDirty = true;

			UE_LOG(LogWorldBLDBootstrap, Log, TEXT("Staged update for '%s' applied successfully."), *PluginName);

			// Module name mismatch detection: if the update adds or removes modules, the engine's
			// plugin descriptor cache (populated during discovery, before this module runs) references
			// the OLD module list.  New modules won't be loaded until a second restart.
			OldModuleNames.Sort();
			NewModuleNames.Sort();
			if (OldModuleNames != NewModuleNames)
			{
				UE_LOG(LogWorldBLDBootstrap, Warning,
					TEXT("Module list changed for '%s' (old: %d modules, new: %d modules). "
						 "A second restart may be needed to fully load the new module set."),
					*PluginName, OldModuleNames.Num(), NewModuleNames.Num());
			}
		}
		else
		{
			UE_LOG(LogWorldBLDBootstrap, Error, TEXT("Staged update for '%s' FAILED -- old version preserved."), *PluginName);
		}
	}

	// Rewrite manifest so CurlDownloaderLibrary can finalize applied entries.
	if (bManifestDirty)
	{
		SaveJsonToFile(Root.ToSharedRef(), ManifestPath);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// File utilities
// ─────────────────────────────────────────────────────────────────────────────

bool FWorldBLDBootstrapModule::CopyFileWithRetry(const FString& SrcPath, const FString& DstPath, int32 MaxRetries)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Ensure parent directory exists.
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(DstPath), true);

	static const double RetryDelaysSeconds[] = { 0.2, 0.5, 1.0, 2.0 };

	for (int32 Attempt = 0; Attempt <= MaxRetries; ++Attempt)
	{
		if (PlatformFile.CopyFile(*DstPath, *SrcPath))
		{
			return true;
		}

		if (Attempt < MaxRetries)
		{
			const double Delay = (Attempt < UE_ARRAY_COUNT(RetryDelaysSeconds))
				? RetryDelaysSeconds[Attempt]
				: RetryDelaysSeconds[UE_ARRAY_COUNT(RetryDelaysSeconds) - 1];

			UE_LOG(LogWorldBLDBootstrap, Warning,
				TEXT("CopyFile failed (attempt %d/%d): '%s' -> '%s'. Retrying in %.1fs..."),
				Attempt + 1, MaxRetries + 1, *SrcPath, *DstPath, Delay);

			FPlatformProcess::Sleep(static_cast<float>(Delay));

			// On retry, try deleting the destination first in case it's read-only or partially written.
			if (FPaths::FileExists(DstPath))
			{
				PlatformFile.SetReadOnly(*DstPath, false);
				PlatformFile.DeleteFile(*DstPath);
			}
		}
	}

	UE_LOG(LogWorldBLDBootstrap, Error,
		TEXT("CopyFile failed after %d attempts: '%s' -> '%s'"), MaxRetries + 1, *SrcPath, *DstPath);
	return false;
}

void FWorldBLDBootstrapModule::CollectRelativeFilePaths(const FString& RootDir, TArray<FString>& OutRelativePaths)
{
	const FString NormalizedRoot = FPaths::ConvertRelativePathToFull(RootDir);

	IFileManager::Get().IterateDirectoryRecursively(*NormalizedRoot,
		[&NormalizedRoot, &OutRelativePaths](const TCHAR* FilenameOrDir, bool bIsDirectory) -> bool
	{
		if (!bIsDirectory)
		{
			FString FullPath(FilenameOrDir);
			FPaths::MakePathRelativeTo(FullPath, *(NormalizedRoot / TEXT("")));
			OutRelativePaths.Add(MoveTemp(FullPath));
		}
		return true; // continue iteration
	});
}

// ─────────────────────────────────────────────────────────────────────────────
// Journal helpers
// ─────────────────────────────────────────────────────────────────────────────

bool FWorldBLDBootstrapModule::WriteJournal(const TSharedRef<FJsonObject>& Journal, const FString& JournalPath)
{
	return SaveJsonToFile(Journal, JournalPath);
}

void FWorldBLDBootstrapModule::RecoverFromAnyJournals()
{
	const FString WorldBLDDir = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("WorldBLD"));

	if (!FPaths::DirectoryExists(WorldBLDDir))
	{
		return;
	}

	TArray<FString> JournalFiles;
	IFileManager::Get().IterateDirectory(*WorldBLDDir,
		[&JournalFiles](const TCHAR* FilenameOrDir, bool bIsDirectory) -> bool
	{
		if (!bIsDirectory)
		{
			const FString FileName = FPaths::GetCleanFilename(FilenameOrDir);
			if (FileName.StartsWith(TEXT("UpdateJournal_")) && FileName.EndsWith(TEXT(".json")))
			{
				JournalFiles.Add(FilenameOrDir);
			}
		}
		return true;
	});

	for (const FString& JournalPath : JournalFiles)
	{
		UE_LOG(LogWorldBLDBootstrap, Log, TEXT("Found incomplete update journal: %s"), *JournalPath);
		RecoverFromJournal(JournalPath);
	}
}

bool FWorldBLDBootstrapModule::RecoverFromJournal(const FString& JournalPath)
{
	const TSharedPtr<FJsonObject> Journal = LoadJsonFromFile(JournalPath);
	if (!Journal.IsValid())
	{
		UE_LOG(LogWorldBLDBootstrap, Warning, TEXT("Could not parse journal file '%s' -- deleting it."), *JournalPath);
		IFileManager::Get().Delete(*JournalPath);
		return true;
	}

	FString Phase;
	FString PluginName;
	FString BackupDir;
	FString TargetPluginDir;
	Journal->TryGetStringField(TEXT("Phase"), Phase);
	Journal->TryGetStringField(TEXT("PluginName"), PluginName);
	Journal->TryGetStringField(TEXT("BackupDir"), BackupDir);
	Journal->TryGetStringField(TEXT("TargetPluginDir"), TargetPluginDir);

	UE_LOG(LogWorldBLDBootstrap, Log,
		TEXT("Recovering journal for '%s' at phase '%s'"), *PluginName, *Phase);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (Phase == JournalPhase::BackupOld)
	{
		// Backup was incomplete.  Target plugin dir is still intact (we only copy FROM it, never
		// modify it during BackupOld).  Just delete the partial backup and the journal.
		if (!BackupDir.IsEmpty() && FPaths::DirectoryExists(BackupDir))
		{
			UE_LOG(LogWorldBLDBootstrap, Log, TEXT("Deleting partial backup: %s"), *BackupDir);
			IFileManager::Get().DeleteDirectory(*BackupDir, false, true);
		}
		IFileManager::Get().Delete(*JournalPath);
		UE_LOG(LogWorldBLDBootstrap, Log, TEXT("Recovery complete (BackupOld) for '%s'. Update will retry next restart."), *PluginName);
		return true;
	}

	if (Phase == JournalPhase::CopyNew)
	{
		// Partial copy over target.  Restore from backup.
		if (BackupDir.IsEmpty() || !FPaths::DirectoryExists(BackupDir))
		{
			UE_LOG(LogWorldBLDBootstrap, Error,
				TEXT("Cannot rollback '%s': backup directory '%s' is missing. Manual intervention required."),
				*PluginName, *BackupDir);
			IFileManager::Get().Delete(*JournalPath);
			return false;
		}

		// Restore backed-up files.
		const TArray<FString> BackedUpFiles = JsonArrayToStringArray(Journal, TEXT("FilesBackedUp"));
		int32 RestoredCount = 0;
		for (const FString& RelPath : BackedUpFiles)
		{
			const FString SrcFile = BackupDir / RelPath;
			const FString DstFile = TargetPluginDir / RelPath;
			if (FPaths::FileExists(SrcFile))
			{
				if (CopyFileWithRetry(SrcFile, DstFile))
				{
					++RestoredCount;
				}
				else
				{
					UE_LOG(LogWorldBLDBootstrap, Error, TEXT("Failed to restore file: %s"), *DstFile);
				}
			}
		}

		// Remove any files that were copied from the NEW version but don't exist in the backup
		// (i.e., files that the new version added which shouldn't be in the rolled-back state).
		const TArray<FString> CopiedNewFiles = JsonArrayToStringArray(Journal, TEXT("FilesCopied"));
		TSet<FString> BackedUpFileSet(BackedUpFiles);
		for (const FString& RelPath : CopiedNewFiles)
		{
			if (!BackedUpFileSet.Contains(RelPath))
			{
				const FString OrphanFile = TargetPluginDir / RelPath;
				if (FPaths::FileExists(OrphanFile))
				{
					PlatformFile.DeleteFile(*OrphanFile);
				}
			}
		}

		UE_LOG(LogWorldBLDBootstrap, Log,
			TEXT("Rollback complete for '%s': restored %d files from backup. Update will retry next restart."),
			*PluginName, RestoredCount);

		// Clean up backup and journal.
		IFileManager::Get().DeleteDirectory(*BackupDir, false, true);
		IFileManager::Get().Delete(*JournalPath);
		return true;
	}

	if (Phase == JournalPhase::Cleanup)
	{
		// Copy succeeded but cleanup didn't finish.  Just finish the cleanup.
		if (!BackupDir.IsEmpty() && FPaths::DirectoryExists(BackupDir))
		{
			IFileManager::Get().DeleteDirectory(*BackupDir, false, true);
		}

		FString StagingDir;
		if (Journal->TryGetStringField(TEXT("StagingDir"), StagingDir)
			&& !StagingDir.IsEmpty() && FPaths::DirectoryExists(StagingDir))
		{
			IFileManager::Get().DeleteDirectory(*StagingDir, false, true);
		}

		IFileManager::Get().Delete(*JournalPath);
		UE_LOG(LogWorldBLDBootstrap, Log, TEXT("Recovery complete (Cleanup) for '%s'."), *PluginName);
		return true;
	}

	// Unknown phase -- delete the journal.
	UE_LOG(LogWorldBLDBootstrap, Warning, TEXT("Unknown journal phase '%s' for '%s' -- deleting journal."), *Phase, *PluginName);
	IFileManager::Get().Delete(*JournalPath);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Journaled directory swap
// ─────────────────────────────────────────────────────────────────────────────

bool FWorldBLDBootstrapModule::SwapPluginDirectoryJournaled(
	const FString& PluginName,
	const FString& StagingDir,
	const FString& TargetPluginDir)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString JournalPath = GetJournalPath(PluginName);

	// Build the backup directory path.
	const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d%H%M%S"));
	const FString BackupDir = GetBackupsBaseDir() / (PluginName + TEXT("_") + Timestamp);

	// Collect file lists.
	TArray<FString> StagedFiles;
	CollectRelativeFilePaths(StagingDir, StagedFiles);

	TArray<FString> OldFiles;
	const bool bTargetExists = FPaths::DirectoryExists(TargetPluginDir);
	if (bTargetExists)
	{
		CollectRelativeFilePaths(TargetPluginDir, OldFiles);
	}

	UE_LOG(LogWorldBLDBootstrap, Log,
		TEXT("SwapJournaled '%s': %d staged files, %d existing files."),
		*PluginName, StagedFiles.Num(), OldFiles.Num());

	// ── Phase 1: BackupOld ──────────────────────────────────────────────────

	TSharedRef<FJsonObject> Journal = MakeShared<FJsonObject>();
	Journal->SetStringField(TEXT("PluginName"), PluginName);
	Journal->SetStringField(TEXT("Phase"), JournalPhase::BackupOld);
	Journal->SetStringField(TEXT("StagingDir"), StagingDir);
	Journal->SetStringField(TEXT("TargetPluginDir"), TargetPluginDir);
	Journal->SetStringField(TEXT("BackupDir"), BackupDir);
	Journal->SetStringField(TEXT("Timestamp"), FDateTime::UtcNow().ToIso8601());
	Journal->SetArrayField(TEXT("FilesBackedUp"), TArray<TSharedPtr<FJsonValue>>());
	Journal->SetArrayField(TEXT("FilesCopied"), TArray<TSharedPtr<FJsonValue>>());
	Journal->SetArrayField(TEXT("FilesRemoved"), TArray<TSharedPtr<FJsonValue>>());

	if (!WriteJournal(Journal, JournalPath))
	{
		UE_LOG(LogWorldBLDBootstrap, Error, TEXT("Failed to create update journal for '%s'."), *PluginName);
		return false;
	}

	if (bTargetExists)
	{
		UE_LOG(LogWorldBLDBootstrap, Log, TEXT("Phase 1/3: Backing up %d files from '%s'..."), OldFiles.Num(), *TargetPluginDir);

		TArray<FString> BackedUpFiles;
		BackedUpFiles.Reserve(OldFiles.Num());

		for (const FString& RelPath : OldFiles)
		{
			const FString SrcFile = TargetPluginDir / RelPath;
			const FString DstFile = BackupDir / RelPath;

			if (!CopyFileWithRetry(SrcFile, DstFile))
			{
				UE_LOG(LogWorldBLDBootstrap, Error,
					TEXT("Failed to backup file '%s'. Aborting swap for '%s'."), *SrcFile, *PluginName);

				// Abort: clean up partial backup and journal.
				IFileManager::Get().DeleteDirectory(*BackupDir, false, true);
				IFileManager::Get().Delete(*JournalPath);
				return false;
			}

			BackedUpFiles.Add(RelPath);
		}

		// Update journal with completed backup list.
		Journal->SetArrayField(TEXT("FilesBackedUp"), StringArrayToJsonArray(BackedUpFiles));
		WriteJournal(Journal, JournalPath);
	}

	// ── Phase 2: CopyNew ────────────────────────────────────────────────────

	Journal->SetStringField(TEXT("Phase"), JournalPhase::CopyNew);
	WriteJournal(Journal, JournalPath);

	UE_LOG(LogWorldBLDBootstrap, Log, TEXT("Phase 2/3: Copying %d staged files to '%s'..."), StagedFiles.Num(), *TargetPluginDir);

	TArray<FString> CopiedFiles;
	CopiedFiles.Reserve(StagedFiles.Num());
	bool bCopyFailed = false;

	for (const FString& RelPath : StagedFiles)
	{
		const FString SrcFile = StagingDir / RelPath;
		const FString DstFile = TargetPluginDir / RelPath;

		// Clear read-only on the destination before overwriting.
		if (FPaths::FileExists(DstFile))
		{
			PlatformFile.SetReadOnly(*DstFile, false);
		}

		if (!CopyFileWithRetry(SrcFile, DstFile))
		{
			UE_LOG(LogWorldBLDBootstrap, Error,
				TEXT("Failed to copy staged file '%s'. Rolling back '%s'."), *SrcFile, *PluginName);
			bCopyFailed = true;
			break;
		}

		CopiedFiles.Add(RelPath);

		// Periodically update journal with progress (every 50 files).
		if ((CopiedFiles.Num() % 50) == 0)
		{
			Journal->SetArrayField(TEXT("FilesCopied"), StringArrayToJsonArray(CopiedFiles));
			WriteJournal(Journal, JournalPath);
		}
	}

	// Final journal update for copied files.
	Journal->SetArrayField(TEXT("FilesCopied"), StringArrayToJsonArray(CopiedFiles));

	if (bCopyFailed)
	{
		// Rollback: restore from backup.
		WriteJournal(Journal, JournalPath);
		RecoverFromJournal(JournalPath);
		return false;
	}

	// Remove orphan files: files that exist in the old version but not in the new version.
	TSet<FString> StagedFileSet(StagedFiles);
	TArray<FString> RemovedFiles;
	for (const FString& RelPath : OldFiles)
	{
		if (!StagedFileSet.Contains(RelPath))
		{
			const FString OrphanFile = TargetPluginDir / RelPath;
			if (FPaths::FileExists(OrphanFile))
			{
				PlatformFile.SetReadOnly(*OrphanFile, false);
				if (PlatformFile.DeleteFile(*OrphanFile))
				{
					RemovedFiles.Add(RelPath);
				}
				else
				{
					UE_LOG(LogWorldBLDBootstrap, Warning,
						TEXT("Could not remove orphan file '%s' (non-fatal)."), *OrphanFile);
				}
			}
		}
	}

	if (RemovedFiles.Num() > 0)
	{
		UE_LOG(LogWorldBLDBootstrap, Log, TEXT("Removed %d orphan files from old version."), RemovedFiles.Num());
		Journal->SetArrayField(TEXT("FilesRemoved"), StringArrayToJsonArray(RemovedFiles));
	}

	// ── Phase 3: Cleanup ────────────────────────────────────────────────────

	Journal->SetStringField(TEXT("Phase"), JournalPhase::Cleanup);
	WriteJournal(Journal, JournalPath);

	UE_LOG(LogWorldBLDBootstrap, Log, TEXT("Phase 3/3: Cleaning up backup and staging directories..."));

	// Delete backup directory.
	if (FPaths::DirectoryExists(BackupDir))
	{
		if (!IFileManager::Get().DeleteDirectory(*BackupDir, false, true))
		{
			UE_LOG(LogWorldBLDBootstrap, Warning,
				TEXT("Could not delete backup directory '%s' -- deferring."), *BackupDir);
			DeferDirectoryDelete(BackupDir);
		}
	}

	// Delete staging directory.
	if (FPaths::DirectoryExists(StagingDir))
	{
		if (!IFileManager::Get().DeleteDirectory(*StagingDir, false, true))
		{
			UE_LOG(LogWorldBLDBootstrap, Warning,
				TEXT("Could not delete staging directory '%s' -- deferring."), *StagingDir);
			DeferDirectoryDelete(StagingDir);
		}
	}

	// Delete the journal.
	IFileManager::Get().Delete(*JournalPath);

	UE_LOG(LogWorldBLDBootstrap, Log, TEXT("Journaled swap complete for '%s'."), *PluginName);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Deferred deletion (mirrors the CurlDownloaderLibrary pattern)
// ─────────────────────────────────────────────────────────────────────────────

bool FWorldBLDBootstrapModule::DeferDirectoryDelete(const FString& DirToDelete)
{
	const FString PendingPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("WorldBLD/PendingPluginDeletes.json"));

	// Read existing pending list.
	TArray<FString> PendingDirs;
	if (FPaths::FileExists(PendingPath))
	{
		FString JsonText;
		if (FFileHelper::LoadFileToString(JsonText, *PendingPath))
		{
			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
			if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
				if (Root->TryGetArrayField(TEXT("PendingDeletes"), Arr) && Arr)
				{
					for (const TSharedPtr<FJsonValue>& Val : *Arr)
					{
						FString Dir;
						if (Val.IsValid() && Val->TryGetString(Dir) && !Dir.IsEmpty())
						{
							PendingDirs.AddUnique(Dir);
						}
					}
				}
			}
		}
	}

	PendingDirs.AddUnique(DirToDelete);

	// Write back.
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("PendingDeletes"), StringArrayToJsonArray(PendingDirs));
	return SaveJsonToFile(Root, PendingPath);
}

// ─────────────────────────────────────────────────────────────────────────────
// Leftover _old directory cleanup (legacy, kept for backwards compatibility)
// ─────────────────────────────────────────────────────────────────────────────

void FWorldBLDBootstrapModule::CleanUpLeftoverOldDirectories()
{
	const FString PluginsDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir());

	// Pattern: <PluginsDir>/<Name>_old_<timestamp>
	// We look for directories matching *_old_* at the top level of the Plugins folder.
	TArray<FString> FoundDirs;
	IFileManager::Get().IterateDirectory(*PluginsDir, [&FoundDirs](const TCHAR* FilenameOrDir, bool bIsDirectory) -> bool
	{
		if (bIsDirectory)
		{
			const FString DirName = FPaths::GetCleanFilename(FilenameOrDir);
			if (DirName.Contains(TEXT("_old_")))
			{
				FoundDirs.Add(FilenameOrDir);
			}
		}
		return true; // continue iteration
	});

	for (const FString& OldDir : FoundDirs)
	{
		// Only delete if the corresponding "real" plugin directory also exists
		// (meaning the swap completed but cleanup didn't).
		const FString DirName = FPaths::GetCleanFilename(OldDir);
		int32 OldMarkerIdx = INDEX_NONE;
		if (DirName.FindChar(TEXT('_'), OldMarkerIdx))
		{
			// Find the _old_ marker position.
			const int32 OldPos = DirName.Find(TEXT("_old_"), ESearchCase::CaseSensitive);
			if (OldPos != INDEX_NONE)
			{
				const FString BaseName = DirName.Left(OldPos);
				const FString RealDir = PluginsDir / BaseName;
				if (FPaths::DirectoryExists(RealDir))
				{
					UE_LOG(LogWorldBLDBootstrap, Log, TEXT("Cleaning up leftover old directory: %s"), *OldDir);
					if (!IFileManager::Get().DeleteDirectory(*OldDir, false, true))
					{
						DeferDirectoryDelete(OldDir);
					}
				}
				// If the real dir doesn't exist, the swap may not have completed -- leave the _old dir alone.
			}
		}
	}
}
