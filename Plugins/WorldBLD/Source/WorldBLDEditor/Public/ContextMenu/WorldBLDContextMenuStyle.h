// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FSlateBrush;
struct FButtonStyle;

namespace WorldBLDContextMenuStyle
{
	/** Faux title bar background color used behind the context menu title + close button. */
	WORLDBLDEDITOR_API const FLinearColor& GetTitleBarColor();

	/**
	 * Brush used for the faux title bar background. Intended to be used as the BorderImage for an SBorder.
	 * Top-left and top-right corners are rounded by 10px; bottom corners are square.
	 */
	WORLDBLDEDITOR_API const FSlateBrush* GetTitleBarBackgroundBrush();

	/**
	 * Reusable context-menu action button style: neutral gray background.
	 * Intended for SButton widgets that contain a row with text (left) + action icon (right).
	 */
	WORLDBLDEDITOR_API const FButtonStyle& GetNeutralButtonStyle();

	/** Outline color used by the neutral button style (exposed for consistent UI dividers/lines). */
	WORLDBLDEDITOR_API const FLinearColor& GetNeutralOutlineColor();

	/**
	 * Reusable context-menu action button style: highlighted (red) background.
	 * This is based on Neutral, but uses a red background tint.
	 */
	WORLDBLDEDITOR_API const FButtonStyle& GetHighlightedButtonStyle();

	/**
	 * Highlighted (red) action button style with no outline.
	 * Intended for primary CTAs where the border/outline is visually distracting.
	 */
	WORLDBLDEDITOR_API const FButtonStyle& GetHighlightedButtonStyleNoOutline();

	/**
	 * Tool-switch menu row button style: transparent when idle (no background/outline),
	 * but shows a neutral hover/pressed background when interacted with.
	 */
	WORLDBLDEDITOR_API const FButtonStyle& GetToolSwitchMenuRowButtonStyle();

	/**
	 * Tool-switch menu install/update button style: transparent when idle,
	 * highlighted (red) on hover/pressed.
	 */
	WORLDBLDEDITOR_API const FButtonStyle& GetToolSwitchMenuInstallButtonStyle();

	/** Highlight (red) tint used by highlighted action buttons (e.g., purchase / get credits). */
	WORLDBLDEDITOR_API const FLinearColor& GetHighlightRedColor();
}


