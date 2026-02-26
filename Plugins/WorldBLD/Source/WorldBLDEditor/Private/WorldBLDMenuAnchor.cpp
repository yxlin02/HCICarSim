// Copyright WorldBLD LLC. All rights reserved.


#include "WorldBLDMenuAnchor.h"
#include "Input/PopupMethodReply.h"

TSharedRef<SWidget> UWorldBLDMenuAnchor::RebuildWidget()
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		MyMenuAnchor = SNew(SMenuAnchor)
		.Padding(Padding)
		.ShowMenuBackground(bShowMenuBackground)
		.ApplyWidgetStyleToMenu(bApplyWidgetStyleToMenu)
		.UseApplicationMenuStack(bUseApplicationMenuStack)
		.ShouldDeferPaintingAfterWindowContent(ShouldDeferPaintingAfterWindowContent)
		.Method(EPopupMethod::UseCurrentWindow)
		.Placement(Placement)
		.FitInWindow(bFitInWindow)
		.OnGetMenuContent(BIND_UOBJECT_DELEGATE(FOnGetContent, HandleGetMenuContent))
		.OnMenuOpenChanged(BIND_UOBJECT_DELEGATE(FOnIsOpenChanged, HandleMenuOpenChanged));

PRAGMA_ENABLE_DEPRECATION_WARNINGS
	if (GetChildrenCount() > 0)
	{
		MyMenuAnchor->SetContent(GetContentSlot()->Content ? GetContentSlot()->Content->TakeWidget() : SNullWidget::NullWidget);
	}

	return MyMenuAnchor.ToSharedRef();
}