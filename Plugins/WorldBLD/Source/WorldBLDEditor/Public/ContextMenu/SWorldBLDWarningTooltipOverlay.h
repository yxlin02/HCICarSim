// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SWorldBLDWarningTooltip;

/**
 * Full-viewport transparent overlay that displays a warning tooltip near the mouse cursor.
 * The tooltip automatically shows when the bound warning text is non-empty and hides when empty.
 * Intended to be added once as a viewport overlay widget and left alive for the EdMode lifetime.
 */
class WORLDBLDEDITOR_API SWorldBLDWarningTooltipOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDWarningTooltipOverlay)
	{
	}

		/** Delegate that returns the current warning text to display. */
		SLATE_ATTRIBUTE(FText, WarningText)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	TAttribute<FText> WarningTextAttribute;
	TSharedPtr<SWorldBLDWarningTooltip> TooltipWidget;
};
