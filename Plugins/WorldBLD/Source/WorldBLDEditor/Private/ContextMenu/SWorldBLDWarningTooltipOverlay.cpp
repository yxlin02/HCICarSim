// Copyright WorldBLD LLC. All Rights Reserved.

#include "ContextMenu/SWorldBLDWarningTooltipOverlay.h"

#include "ContextMenu/SWorldBLDWarningTooltip.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SOverlay.h"

void SWorldBLDWarningTooltipOverlay::Construct(const FArguments& InArgs)
{
	WarningTextAttribute = InArgs._WarningText;

	SetVisibility(EVisibility::SelfHitTestInvisible);

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SAssignNew(TooltipWidget, SWorldBLDWarningTooltip)
			.WarningText(WarningTextAttribute)
			.Visibility(EVisibility::Collapsed)
		]
	];
}

void SWorldBLDWarningTooltipOverlay::Tick(
	const FGeometry& AllottedGeometry,
	const double InCurrentTime,
	const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!TooltipWidget.IsValid())
	{
		return;
	}

	const FText CurrentText = WarningTextAttribute.Get(FText::GetEmpty());
	const bool bShouldShow = !CurrentText.IsEmpty();

	const EVisibility DesiredVisibility = bShouldShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
	if (TooltipWidget->GetVisibility() != DesiredVisibility)
	{
		TooltipWidget->SetVisibility(DesiredVisibility);
	}

	if (!bShouldShow)
	{
		return;
	}

	// Position the tooltip near the mouse cursor using a render transform.
	const FVector2D AbsoluteCursorPos = FSlateApplication::Get().GetCursorPos();
	const FVector2D LocalCursorPos = AllottedGeometry.AbsoluteToLocal(AbsoluteCursorPos);

	// Small offset so the tooltip doesn't overlap the cursor itself.
	static constexpr float OffsetX = 16.0f;
	static constexpr float OffsetY = 20.0f;

	TooltipWidget->SetRenderTransform(FSlateRenderTransform(FVector2D(LocalCursorPos.X + OffsetX, LocalCursorPos.Y + OffsetY)));
}
