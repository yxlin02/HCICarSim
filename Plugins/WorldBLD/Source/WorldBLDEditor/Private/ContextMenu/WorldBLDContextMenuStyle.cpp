// Copyright WorldBLD LLC. All Rights Reserved.

#include "ContextMenu/WorldBLDContextMenuStyle.h"

#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Math/Vector4.h"

namespace WorldBLDContextMenuStyle
{
	const FLinearColor& GetTitleBarColor()
	{
		static const FLinearColor TitleBarColor(0.035f, 0.035f, 0.035f, 1.0f);
		return TitleBarColor;
	}

	const FSlateBrush* GetTitleBarBackgroundBrush()
	{
		// Rounded corners: TopLeft, TopRight, BottomRight, BottomLeft.
		static const FVector4 CornerRadii(10.0f, 10.0f, 0.0f, 0.0f);

		// Outline settings to match the UMG title bar border:
		// (CornerRadii=(10,10,0,0), Color=(0.068478,0.068478,0.068478,1), Width=1, RoundingType=FixedRadius, bUseBrushTransparency=False)
		static const FSlateBrushOutlineSettings OutlineSettings = []()
		{
			FSlateBrushOutlineSettings Settings;
			Settings.CornerRadii = CornerRadii;
			Settings.Color = FSlateColor(FLinearColor(0.068478f, 0.068478f, 0.068478f, 1.0f));
			Settings.Width = 1.0f;
			Settings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			Settings.bUseBrushTransparency = false;
			return Settings;
		}();

		// Use a rounded box brush so the corners are actually rounded regardless of editor theme.
		static const FSlateRoundedBoxBrush Brush = []()
		{
			FSlateRoundedBoxBrush RoundedBrush(GetTitleBarColor(), CornerRadii);
			RoundedBrush.OutlineSettings = OutlineSettings;
			return RoundedBrush;
		}();

		return &Brush;
	}

	const FLinearColor& GetNeutralOutlineColor()
	{
		static const FLinearColor NeutralOutlineColor(0.695111f, 0.695111f, 0.695111f, 1.0f);
		return NeutralOutlineColor;
	}

	const FButtonStyle& GetNeutralButtonStyle()
	{
		// A simple neutral button intended for "text left + icon right" content layouts.
		// Corner order: TopLeft, TopRight, BottomRight, BottomLeft.
		static const FVector4 CornerRadii(4.0f, 4.0f, 4.0f, 4.0f);

		static const FSlateBrushOutlineSettings OutlineSettings = []()
		{
			FSlateBrushOutlineSettings Settings;
			Settings.CornerRadii = CornerRadii;
			Settings.Color = FSlateColor(GetNeutralOutlineColor());
			Settings.Width = 1.0f;
			Settings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			Settings.bUseBrushTransparency = true;
			return Settings;
		}();

		static const FLinearColor NormalColor(0.14f, 0.14f, 0.14f, 1.0f);
		static const FLinearColor HoveredColor(0.18f, 0.18f, 0.18f, 1.0f);
		static const FLinearColor PressedColor(0.11f, 0.11f, 0.11f, 1.0f);
		static const FLinearColor DisabledColor(0.14f, 0.14f, 0.14f, 0.35f);

		static const FSlateRoundedBoxBrush NormalBrush = []()
		{
			FSlateRoundedBoxBrush Brush(NormalColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush HoveredBrush = []()
		{
			FSlateRoundedBoxBrush Brush(HoveredColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush PressedBrush = []()
		{
			FSlateRoundedBoxBrush Brush(PressedColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush DisabledBrush = []()
		{
			FSlateRoundedBoxBrush Brush(DisabledColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(NormalBrush)
			.SetHovered(HoveredBrush)
			.SetPressed(PressedBrush)
			.SetDisabled(DisabledBrush)
			.SetNormalForeground(FSlateColor(FLinearColor::White))
			.SetHoveredForeground(FSlateColor(FLinearColor::White))
			.SetPressedForeground(FSlateColor(FLinearColor::White))
			.SetDisabledForeground(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.5f)))
			.SetNormalPadding(FMargin(10.0f, 6.0f))
			.SetPressedPadding(FMargin(10.0f, 7.0f, 10.0f, 5.0f));

		return Style;
	}

	const FButtonStyle& GetHighlightedButtonStyle()
	{
		// Based on Neutral but with a red background (provided by design spec).
		// Corner order: TopLeft, TopRight, BottomRight, BottomLeft.
		static const FVector4 CornerRadii(4.0f, 4.0f, 4.0f, 4.0f);

		static const FSlateBrushOutlineSettings OutlineSettings = []()
		{
			FSlateBrushOutlineSettings Settings;
			Settings.CornerRadii = CornerRadii;
			Settings.Color = FSlateColor(GetNeutralOutlineColor());
			Settings.Width = 1.0f;
			Settings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			Settings.bUseBrushTransparency = true;
			return Settings;
		}();

		static const FLinearColor NormalColor(0.913726f, 0.011765f, 0.058824f, 1.0f);
		static const FLinearColor HoveredColor(0.953726f, 0.051765f, 0.098824f, 1.0f);
		static const FLinearColor PressedColor(0.833726f, 0.000000f, 0.040000f, 1.0f);
		static const FLinearColor DisabledColor(0.913726f, 0.011765f, 0.058824f, 0.35f);

		static const FSlateRoundedBoxBrush NormalBrush = []()
		{
			FSlateRoundedBoxBrush Brush(NormalColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush HoveredBrush = []()
		{
			FSlateRoundedBoxBrush Brush(HoveredColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush PressedBrush = []()
		{
			FSlateRoundedBoxBrush Brush(PressedColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush DisabledBrush = []()
		{
			FSlateRoundedBoxBrush Brush(DisabledColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FButtonStyle Style = FButtonStyle(GetNeutralButtonStyle())
			.SetNormal(NormalBrush)
			.SetHovered(HoveredBrush)
			.SetPressed(PressedBrush)
			.SetDisabled(DisabledBrush);

		return Style;
	}

	const FButtonStyle& GetHighlightedButtonStyleNoOutline()
	{
		// Based on Highlighted but with no outline.
		// Corner order: TopLeft, TopRight, BottomRight, BottomLeft.
		static const FVector4 CornerRadii(4.0f, 4.0f, 4.0f, 4.0f);

		static const FSlateBrushOutlineSettings OutlineSettings = []()
		{
			FSlateBrushOutlineSettings Settings;
			Settings.CornerRadii = CornerRadii;
			Settings.Color = FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			Settings.Width = 0.0f;
			Settings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			Settings.bUseBrushTransparency = true;
			return Settings;
		}();

		static const FLinearColor NormalColor(0.913726f, 0.011765f, 0.058824f, 1.0f);
		static const FLinearColor HoveredColor(0.953726f, 0.051765f, 0.098824f, 1.0f);
		static const FLinearColor PressedColor(0.833726f, 0.000000f, 0.040000f, 1.0f);
		static const FLinearColor DisabledColor(0.913726f, 0.011765f, 0.058824f, 0.35f);

		static const FSlateRoundedBoxBrush NormalBrush = []()
		{
			FSlateRoundedBoxBrush Brush(NormalColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush HoveredBrush = []()
		{
			FSlateRoundedBoxBrush Brush(HoveredColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush PressedBrush = []()
		{
			FSlateRoundedBoxBrush Brush(PressedColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush DisabledBrush = []()
		{
			FSlateRoundedBoxBrush Brush(DisabledColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FButtonStyle Style = FButtonStyle(GetNeutralButtonStyle())
			.SetNormal(NormalBrush)
			.SetHovered(HoveredBrush)
			.SetPressed(PressedBrush)
			.SetDisabled(DisabledBrush);

		return Style;
	}

	const FButtonStyle& GetToolSwitchMenuRowButtonStyle()
	{
		// Transparent when idle; shows neutral hover/pressed background.
		// Corner order: TopLeft, TopRight, BottomRight, BottomLeft.
		static const FVector4 CornerRadii(4.0f, 4.0f, 4.0f, 4.0f);

		static const FSlateBrushOutlineSettings OutlineSettings = []()
		{
			FSlateBrushOutlineSettings Settings;
			Settings.CornerRadii = CornerRadii;
			Settings.Color = FSlateColor(GetNeutralOutlineColor());
			Settings.Width = 1.0f;
			Settings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			// Let brush alpha control outline visibility (idle is fully transparent).
			Settings.bUseBrushTransparency = true;
			return Settings;
		}();

		// Normal is fully transparent: no background, no outline.
		static const FLinearColor NormalColor(0.14f, 0.14f, 0.14f, 0.0f);
		static const FLinearColor HoveredColor(0.18f, 0.18f, 0.18f, 1.0f);
		static const FLinearColor PressedColor(0.11f, 0.11f, 0.11f, 1.0f);
		static const FLinearColor DisabledColor(0.14f, 0.14f, 0.14f, 0.0f);

		static const FSlateRoundedBoxBrush NormalBrush = []()
		{
			FSlateRoundedBoxBrush Brush(NormalColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush HoveredBrush = []()
		{
			FSlateRoundedBoxBrush Brush(HoveredColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush PressedBrush = []()
		{
			FSlateRoundedBoxBrush Brush(PressedColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush DisabledBrush = []()
		{
			FSlateRoundedBoxBrush Brush(DisabledColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FButtonStyle Style = FButtonStyle(GetNeutralButtonStyle())
			.SetNormal(NormalBrush)
			.SetHovered(HoveredBrush)
			.SetPressed(PressedBrush)
			.SetDisabled(DisabledBrush);

		return Style;
	}

	const FButtonStyle& GetToolSwitchMenuInstallButtonStyle()
	{
		// Always visible; red highlight even when idle.
		// Corner order: TopLeft, TopRight, BottomRight, BottomLeft.
		static const FVector4 CornerRadii(4.0f, 4.0f, 4.0f, 4.0f);

		static const FSlateBrushOutlineSettings OutlineSettings = []()
		{
			FSlateBrushOutlineSettings Settings;
			Settings.CornerRadii = CornerRadii;
			Settings.Color = FSlateColor(GetNeutralOutlineColor());
			Settings.Width = 1.0f;
			Settings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			// Let brush alpha control outline visibility (idle is fully transparent).
			Settings.bUseBrushTransparency = true;
			return Settings;
		}();

		// NOTE: Previously Normal/Disabled alpha were 0.0, which made the button effectively invisible
		// until hovered. Keep it readable at all times so users can discover Install/Update affordances.
		static const FLinearColor NormalColor(0.913726f, 0.011765f, 0.058824f, 1.0f);
		static const FLinearColor HoveredColor(0.953726f, 0.051765f, 0.098824f, 1.0f);
		static const FLinearColor PressedColor(0.833726f, 0.000000f, 0.040000f, 1.0f);
		static const FLinearColor DisabledColor(0.913726f, 0.011765f, 0.058824f, 0.35f);

		static const FSlateRoundedBoxBrush NormalBrush = []()
		{
			FSlateRoundedBoxBrush Brush(NormalColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush HoveredBrush = []()
		{
			FSlateRoundedBoxBrush Brush(HoveredColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush PressedBrush = []()
		{
			FSlateRoundedBoxBrush Brush(PressedColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FSlateRoundedBoxBrush DisabledBrush = []()
		{
			FSlateRoundedBoxBrush Brush(DisabledColor, CornerRadii);
			Brush.OutlineSettings = OutlineSettings;
			return Brush;
		}();

		static const FButtonStyle Style = FButtonStyle(GetNeutralButtonStyle())
			.SetNormal(NormalBrush)
			.SetHovered(HoveredBrush)
			.SetPressed(PressedBrush)
			.SetDisabled(DisabledBrush);

		return Style;
	}

	const FLinearColor& GetHighlightRedColor()
	{
		// Keep in sync with GetHighlightedButtonStyle() "NormalColor".
		static const FLinearColor HighlightRed(0.913726f, 0.011765f, 0.058824f, 1.0f);
		return HighlightRed;
	}
}


