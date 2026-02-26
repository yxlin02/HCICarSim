// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FJsonObject;

/**
 * Tiny early-loading module (PostConfigInit) whose sole purpose is to apply
 * staged plugin updates BEFORE tool-plugin DLLs are loaded (Default phase).
 *
 * It reads Saved/WorldBLD/StagedPluginUpdates.json, swaps the old plugin
 * directory with the staged one, and marks the manifest entry as applied so
 * CurlDownloaderLibrary::ProcessAppliedStagedUpdates() can finalize later.
 *
 * Uses a journaled, file-by-file copy approach for reliability:
 *   Phase BackupOld -> CopyNew -> Cleanup -> (journal deleted)
 * If the editor crashes mid-swap, the journal enables full recovery on next startup.
 */
class FWorldBLDBootstrapModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	// ── Path helpers ────────────────────────────────────────────────────────

	/** Path to the staged-updates manifest. */
	static FString GetStagedUpdatesManifestPath();

	/** Returns true if this project is a protected development workspace. */
	static bool IsProtectedDevProject();

	/** Path to the journal file for a given plugin swap operation. */
	static FString GetJournalPath(const FString& PluginName);

	/** Base directory for plugin backups (Saved/WorldBLD/Backups/). */
	static FString GetBackupsBaseDir();

	// ── Manifest processing ─────────────────────────────────────────────────

	/** Process all pending staged updates from the manifest. */
	void ProcessStagedUpdates();

	// ── Journaled swap ──────────────────────────────────────────────────────

	/**
	 * Replace the target plugin directory with the staged one using a
	 * journaled, file-by-file copy that is resilient to transient file locks,
	 * cross-volume paths, and crashes.
	 *
	 * Phases:
	 *   1. BackupOld  -- copy target plugin files to Saved/WorldBLD/Backups/
	 *   2. CopyNew    -- copy staged files over the target, remove orphans
	 *   3. Cleanup    -- delete backup + staging dirs, delete journal
	 */
	static bool SwapPluginDirectoryJournaled(
		const FString& PluginName,
		const FString& StagingDir,
		const FString& TargetPluginDir);

	// ── Journal helpers ─────────────────────────────────────────────────────

	/** Scan for leftover journal files and recover from any incomplete swaps. */
	static void RecoverFromAnyJournals();

	/** Recover from a single journal file. Returns true if handled successfully. */
	static bool RecoverFromJournal(const FString& JournalPath);

	/** Write (or overwrite) the journal JSON to disk. */
	static bool WriteJournal(const TSharedRef<FJsonObject>& Journal, const FString& JournalPath);

	// ── File utilities ──────────────────────────────────────────────────────

	/**
	 * Copy a single file with retry + backoff. Handles AV transient locks.
	 * Retries up to MaxRetries times with increasing delays (200ms, 500ms, 1000ms).
	 */
	static bool CopyFileWithRetry(
		const FString& SrcPath,
		const FString& DstPath,
		int32 MaxRetries = 3);

	/**
	 * Recursively collect all file paths under RootDir, returned as paths
	 * relative to RootDir (using forward slashes).
	 */
	static void CollectRelativeFilePaths(
		const FString& RootDir,
		TArray<FString>& OutRelativePaths);

	// ── Legacy cleanup ──────────────────────────────────────────────────────

	/** Schedule a directory for deferred deletion via PendingPluginDeletes.json. */
	static bool DeferDirectoryDelete(const FString& DirToDelete);

	/**
	 * Scan Plugins/ for leftover _old_ directories from a previous (legacy) swap
	 * that crashed before cleanup could finish, and remove them.
	 * Kept for one release cycle for backwards compatibility.
	 */
	static void CleanUpLeftoverOldDirectories();
};
