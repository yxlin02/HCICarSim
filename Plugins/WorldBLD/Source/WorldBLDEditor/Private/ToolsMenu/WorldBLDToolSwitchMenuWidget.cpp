// Copyright WorldBLD LLC. All rights reserved.

#include "ToolsMenu/WorldBLDToolSwitchMenuWidget.h"

#include "ToolsMenu/SWorldBLDToolSwitchMenu.h"

#include "Downloader/CurlDownloaderLibrary.h"
#include "WorldBLDEditorToolkit.h"

#include "Editor.h"

UWorldBLDToolSwitchMenuWidget::UWorldBLDToolSwitchMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UWorldBLDToolSwitchMenuWidget::RebuildWidget()
{
	UWorldBLDEdMode* EdMode = UWorldBLDEdMode::GetWorldBLDEdMode();
	UCurlDownloaderLibrary* Downloader = (GEditor != nullptr) ? GEditor->GetEditorSubsystem<UCurlDownloaderLibrary>() : nullptr;

	MyMenu =
		SNew(SWorldBLDToolSwitchMenu)
		.EdMode(EdMode)
		.Downloader(Downloader);

	return MyMenu.ToSharedRef();
}

void UWorldBLDToolSwitchMenuWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyMenu.Reset();
}

