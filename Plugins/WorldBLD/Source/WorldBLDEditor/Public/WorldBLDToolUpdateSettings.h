// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "WorldBLDToolUpdateSettings.generated.h"

/**
 * Persists information about tools installed/updated via the WorldBLD tool downloader.
 * Editor-only (WorldBLDEditor module).
 */
UCLASS(Config=EditorPerProjectUserSettings, DefaultConfig)
class WORLDBLDEDITOR_API UWorldBLDToolUpdateSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** ToolId (plugin name) -> SHA1 of the zip that was last installed via the downloader. */
	UPROPERTY(Config, EditAnywhere, Category="ToolUpdates")
	TMap<FString, FString> InstalledToolSha1;

	/**
	 * INSECURE: Disables TLS certificate verification for libcurl downloads.
	 * This should only be used as a temporary workaround when certificate validation fails.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Downloader")
	bool bDisableSSLVerification = true;

	/** Returns the recorded installed SHA1 for ToolId, or empty string if unknown. */
	FString GetInstalledSha1(const FString& ToolId) const;

	/** Records the installed SHA1 for ToolId and saves config. */
	void SetInstalledSha1AndSave(const FString& ToolId, const FString& Sha1);
};


