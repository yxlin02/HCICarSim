// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

namespace WorldBLDContextMenuFont
{
	/**
	 * Shared context-menu font used across WorldBLD context menu widgets.
	 *
	 * Settings:
	 * - FontObject: /Engine/EngineFonts/Roboto.Roboto
	 * - TypefaceFontName: Regular
	 * - Size: 8
	 * - LetterSpacing: 200
	 */
	WORLDBLDEDITOR_API FSlateFontInfo Get();

	/**
	 * Roboto Medium font helper for widgets that need a medium-weight typeface.
	 *
	 * Settings:
	 * - FontObject: /Engine/EngineFonts/Roboto.Roboto
	 * - TypefaceFontName: Medium
	 * - Size: caller-provided
	 * - LetterSpacing: caller-provided
	 */
	WORLDBLDEDITOR_API FSlateFontInfo GetMedium(const float InSize, const int32 InLetterSpacing);

	/**
	 * Roboto Thin font helper for widgets that need a thin-weight typeface.
	 *
	 * Settings:
	 * - FontObject: /Engine/EngineFonts/Roboto.Roboto
	 * - TypefaceFontName: Thin
	 * - Size: caller-provided
	 * - LetterSpacing: caller-provided
	 */
	WORLDBLDEDITOR_API FSlateFontInfo GetThin(const float InSize, const int32 InLetterSpacing);

	/**
	 * Title bar font used by the WorldBLD context menu window title text.
	 *
	 * Settings:
	 * - FontObject: /Engine/EngineFonts/Roboto.Roboto
	 * - TypefaceFontName: Regular
	 * - Size: 10
	 * - LetterSpacing: 250
	 */
	WORLDBLDEDITOR_API FSlateFontInfo GetTitleBar();
}


