// Copyright WorldBLD LLC. All Rights Reserved.

#include "ContextMenu/SWorldBLDContextMenuViewportOverlay.h"

#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

void SWorldBLDContextMenuViewportOverlay::Construct(const FArguments& InArgs)
{
	Padding = InArgs._Padding;
	PanelWidget = InArgs._Panel;

	if (!PanelWidget.IsValid())
	{
		PanelWidget = SNullWidget::NullWidget;
	}

	// Critical: the overlay host widget must not block hit-testing across the entire viewport.
	// SelfHitTestInvisible allows the panel to receive input, while letting mouse hover/click pass through elsewhere.
	SetVisibility(EVisibility::SelfHitTestInvisible);

	// Root overlay should NOT block viewport interaction outside the panel area.
	// SelfHitTestInvisible means the root ignores hit-testing, while children can still receive input.
	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		// Fill so we can do dynamic top/bottom anchoring inside the padded area without mutating the slot.
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(Padding)
		[
			SAssignNew(AlignmentBox, SBox)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			[
				PanelWidget.ToSharedRef()
			]
		]
	];

	// Initialize the anchor based on the initial geometry (Tick will keep it updated).
	bAnchoredTopRight = false;
}

void SWorldBLDContextMenuViewportOverlay::Tick(
	const FGeometry& AllottedGeometry,
	const double InCurrentTime,
	const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	static constexpr float NarrowViewportWidthThreshold = 1200.0f;

	if (!AlignmentBox.IsValid())
	{
		return;
	}

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const bool bShouldAnchorTopRight = (LocalSize.X > 0.0f) && (LocalSize.X < NarrowViewportWidthThreshold);
	if (bShouldAnchorTopRight == bAnchoredTopRight)
	{
		return;
	}

	bAnchoredTopRight = bShouldAnchorTopRight;
	AlignmentBox->SetVAlign(bAnchoredTopRight ? VAlign_Top : VAlign_Bottom);
	Invalidate(EInvalidateWidgetReason::Layout);
}


