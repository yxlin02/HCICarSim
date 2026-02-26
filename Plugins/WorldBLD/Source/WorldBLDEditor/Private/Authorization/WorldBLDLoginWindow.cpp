// Copyright WorldBLD LLC. All Rights Reserved.

#include "Authorization/WorldBLDLoginWindow.h"

#include "Authorization/SWorldBLDLoginDialog.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "WorldBLDLoginWindow"

namespace WorldBLDLoginWindow_Private
{
	static TWeakPtr<SWindow> ActiveLoginWindow;
}

void FWorldBLDLoginWindow::OpenLoginDialog()
{
	if (WorldBLDLoginWindow_Private::ActiveLoginWindow.IsValid())
	{
		if (TSharedPtr<SWindow> Pinned = WorldBLDLoginWindow_Private::ActiveLoginWindow.Pin())
		{
			Pinned->BringToFront();
		}
		return;
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WorldBLDLogin_Title", "WorldBLD Login"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.HasCloseButton(true);

	WorldBLDLoginWindow_Private::ActiveLoginWindow = Window;

	Window->SetOnWindowClosed(FOnWindowClosed::CreateStatic([](const TSharedRef<SWindow>&)
	{
		WorldBLDLoginWindow_Private::ActiveLoginWindow.Reset();
	}));

	const TWeakPtr<SWindow> WeakWindow = Window;
	Window->SetContent(
		SNew(SWorldBLDLoginDialog)
		.OnRequestClose(FSimpleDelegate::CreateLambda([WeakWindow]()
		{
			if (TSharedPtr<SWindow> Pinned = WeakWindow.Pin())
			{
				Pinned->RequestDestroyWindow();
			}
		})));

	TSharedPtr<SWindow> Parent = FSlateApplication::Get().GetActiveTopLevelWindow();

	// Important: Login relies on async HTTP completion callbacks that are dispatched via the editor/engine tick.
	// A true modal Slate window blocks that tick, so the login response won't be processed until the window closes.
	// Use a dialog-like modeless window (native child when possible) so the editor keeps ticking while it's open.
	if (Parent.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(Window, Parent.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window);
	}
}

#undef LOCTEXT_NAMESPACE

