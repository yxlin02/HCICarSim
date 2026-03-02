// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Simple tooltip widget: a dark-gray rounded rectangle displaying warning text.
 * Intended to be hosted inside SWorldBLDWarningTooltipOverlay to follow the mouse cursor.
 */
class WORLDBLDEDITOR_API SWorldBLDWarningTooltip : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDWarningTooltip)
	{
	}

		/** The warning text to display. */
		SLATE_ATTRIBUTE(FText, WarningText)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
