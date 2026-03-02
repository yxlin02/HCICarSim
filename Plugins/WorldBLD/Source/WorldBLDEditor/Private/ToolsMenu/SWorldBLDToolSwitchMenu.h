// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UCurlDownloaderLibrary;
class UWorldBLDEdMode;

/**
 * Small in-viewport tool switch / installer menu.
 * - Lists available tools from UWorldBLDEdMode::AvailableTools
 * - Allows switching to installed tools
 * - Allows installing missing tools via UCurlDownloaderLibrary (supports concurrent downloads)
 */
class SWorldBLDToolSwitchMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDToolSwitchMenu)
		: _EdMode(nullptr)
		, _Downloader(nullptr)
	{}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorldBLDEdMode>, EdMode)
		SLATE_ARGUMENT(TWeakObjectPtr<UCurlDownloaderLibrary>, Downloader)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	TWeakObjectPtr<UWorldBLDEdMode> EdMode;
	TWeakObjectPtr<UCurlDownloaderLibrary> Downloader;

	TSharedRef<SWidget> BuildMenuContent();
	void RebuildMenuEntries();
	uint32 ComputeAvailableToolsHash() const;

	TSharedPtr<class SVerticalBox> MenuContainer;
	uint32 CachedAvailableToolsHash = 0;

	// Tool helpers
	static FString NormalizeToolId(const FString& ToolName);
	bool IsToolInstalled(const FString& ToolId) const;
	bool IsToolInstalling(const FString& ToolId) const;

	// UI actions
	FReply HandleToolRowClicked(FString ToolId) const;
	FReply HandleInstallClicked(FString ToolId, FString ToolDisplayName, FString ServerVersion, FString DownloadUrl, FString ExpectedSha1);
	FReply HandleSwitchToWorldBLDClicked() const;
};

