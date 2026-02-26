#include "LevelTemplate/WorldBLDLevelTemplateWindow.h"

#include "LevelTemplate/SWorldBLDLevelTemplateWindow.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "WorldBLDLevelTemplateWindow"

TWeakPtr<SWindow> FWorldBLDLevelTemplateWindow::LevelTemplateWindow;
FWorldBLDLevelTemplateWindow::FOnWorldBLDLevelTemplateSelected FWorldBLDLevelTemplateWindow::LevelTemplateSelected;

FWorldBLDLevelTemplateWindow::FOnWorldBLDLevelTemplateSelected& FWorldBLDLevelTemplateWindow::OnLevelTemplateSelected()
{
	return LevelTemplateSelected;
}

void FWorldBLDLevelTemplateWindow::OpenLevelTemplateWindow()
{
	if (LevelTemplateWindow.IsValid())
	{
		if (TSharedPtr<SWindow> Pinned = LevelTemplateWindow.Pin())
		{
			Pinned->BringToFront();
		}
		return;
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WorldBLDLevelTemplates_Title", "WorldBLD Level Templates"))
		.ClientSize(FVector2D(800.0f, 600.0f))
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.HasCloseButton(true);

	Window->SetContent(SNew(SWorldBLDLevelTemplateWindow));
	Window->SetOnWindowClosed(FOnWindowClosed::CreateStatic([](const TSharedRef<SWindow>&)
	{
		FWorldBLDLevelTemplateWindow::LevelTemplateWindow.Reset();
	}));

	FSlateApplication::Get().AddWindow(Window);
	LevelTemplateWindow = Window;
}

void FWorldBLDLevelTemplateWindow::CloseLevelTemplateWindow()
{
	if (TSharedPtr<SWindow> Pinned = LevelTemplateWindow.Pin())
	{
		Pinned->RequestDestroyWindow();
	}
	LevelTemplateWindow.Reset();
}

#undef LOCTEXT_NAMESPACE
