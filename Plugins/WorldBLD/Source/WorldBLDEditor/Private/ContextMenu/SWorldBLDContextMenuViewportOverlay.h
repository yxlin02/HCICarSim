// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Full-viewport transparent overlay that anchors its content to the right side of the viewport.
 * By default it anchors to the bottom-right; on narrow viewports it anchors to the top-right to avoid
 * overlapping the viewport toolbar.
 * Intended to host context menus and other small panels without spawning independent windows.
 */
// Exported because it is used by other editor modules (e.g. RoadBLDEditorToolkit).
class WORLDBLDEDITOR_API SWorldBLDContextMenuViewportOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDContextMenuViewportOverlay)
		: _Padding(FMargin(24.0f))
	{
	}

		/** Padding from the viewport edges. */
		SLATE_ARGUMENT(FMargin, Padding)

		/** The panel widget to display in the viewport corner. */
		SLATE_ARGUMENT(TSharedPtr<SWidget>, Panel)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FMargin Padding;
	TSharedPtr<SWidget> PanelWidget;
	TSharedPtr<class SBox> AlignmentBox;
	bool bAnchoredTopRight = false;

	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
};


