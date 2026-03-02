// Copyright WorldBLD LLC. All Rights Reserved.

#include "ContextMenu/SWorldBLDWarningTooltip.h"

#include "ContextMenu/WorldBLDContextMenuFont.h"
#include "Styling/SlateBrush.h"
#include "Math/Vector4.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

void SWorldBLDWarningTooltip::Construct(const FArguments& InArgs)
{
	// Match the existing context menu background color for consistency.
	static const FLinearColor BackgroundColor(0.011f, 0.011f, 0.011f, 0.95f);
	static const FVector4 CornerRadii(6.0f, 6.0f, 6.0f, 6.0f);

	static const FSlateRoundedBoxBrush BackgroundBrush = []()
	{
		return FSlateRoundedBoxBrush(BackgroundColor, CornerRadii);
	}();

	SetVisibility(EVisibility::HitTestInvisible);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(&BackgroundBrush)
		.Padding(FMargin(10.0f, 6.0f))
		[
			SNew(STextBlock)
			.Text(InArgs._WarningText)
			.Font(WorldBLDContextMenuFont::Get())
			.ColorAndOpacity(FSlateColor(FLinearColor::White))
		]
	];
}
