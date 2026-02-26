// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FSlateBrush;

namespace WorldBLDContextMenuIcons
{
	/**
	 * Action icons intended for use on context menu buttons (e.g. text left + icon right).
	 * These icons reference Texture2D assets stored in the WorldBLD plugin content.
	 */
	enum class EActionIcon : uint8
	{
		None,

		Add,
		Remove,
		/** Explicit minus icon (currently uses the same texture as Remove). */
		Minus,
		Move,
		Confirm,
		Close,

		Settings,
		Search,
		More,

		ArrowLeft,
		/** Larger right arrow (uses UI_arrow-right). */
		ArrowRightLarge,
		ArrowRight,
		ArrowDown
	};

	/** Default icon size for action icons on context-menu buttons. */
	WORLDBLDEDITOR_API const FVector2D& GetActionIconSize();

	/**
	 * Get the Slate brush for the given action icon.
	 * Returns nullptr for None or if the underlying Texture2D could not be loaded.
	 */
	WORLDBLDEDITOR_API const FSlateBrush* GetActionIconBrush(EActionIcon Icon);
}


