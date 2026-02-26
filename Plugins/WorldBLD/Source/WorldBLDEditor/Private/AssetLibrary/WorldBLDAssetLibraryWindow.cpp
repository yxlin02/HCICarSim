// Copyright WorldBLD LLC. All Rights Reserved.

#include "AssetLibrary/WorldBLDAssetLibraryWindow.h"

#include "AssetLibrary/SWorldBLDAssetLibraryWindow.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "WorldBLDAssetLibraryWindow"

TWeakPtr<SWindow> FWorldBLDAssetLibraryWindow::AssetLibraryWindow;

void FWorldBLDAssetLibraryWindow::OpenAssetLibrary()
{
	if (AssetLibraryWindow.IsValid())
	{
		if (TSharedPtr<SWindow> Pinned = AssetLibraryWindow.Pin())
		{
			Pinned->BringToFront();
		}
		return;
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WorldBLDAssetLibrary_Title", "WorldBLD Asset Library"))
		.ClientSize(FVector2D(1400.0f, 900.0f))
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.HasCloseButton(true);

	Window->SetContent(SNew(SWorldBLDAssetLibraryWindow));
	Window->SetOnWindowClosed(FOnWindowClosed::CreateStatic([](const TSharedRef<SWindow>&)
	{
		FWorldBLDAssetLibraryWindow::AssetLibraryWindow.Reset();
	}));

	FSlateApplication::Get().AddWindow(Window);
	AssetLibraryWindow = Window;
}

void FWorldBLDAssetLibraryWindow::CloseAssetLibrary()
{
	if (TSharedPtr<SWindow> Pinned = AssetLibraryWindow.Pin())
	{
		Pinned->RequestDestroyWindow();
	}
	AssetLibraryWindow.Reset();
}

#undef LOCTEXT_NAMESPACE
