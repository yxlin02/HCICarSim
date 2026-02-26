// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Simple single-child Slate widget that constrains its child to a specific aspect ratio.
 *
 * This is Slate-only (UMG has UAspectRatioBox). We use this in-editor to ensure preview
 * images occupy a stable 16:9 region (letterboxed as needed) without stretching.
 */
class SWorldBLDAspectRatioBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDAspectRatioBox)
		: _AspectRatio(16.0f / 9.0f)
		, _HAlign(HAlign_Center)
		, _VAlign(VAlign_Center)
	{}
		/** Target aspect ratio (Width / Height). Must be > 0. */
		SLATE_ARGUMENT(float, AspectRatio)

		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)

		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		AspectRatio = (InArgs._AspectRatio > 0.0f) ? InArgs._AspectRatio : (16.0f / 9.0f);
		HAlign = InArgs._HAlign;
		VAlign = InArgs._VAlign;

		ChildSlot
		[
			InArgs._Content.Widget
		];
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		// Desired size is the child's desired size, but normalized to the target ratio.
		// This is only a hint; parent layout may override.
		const TSharedRef<SWidget> ChildWidget = ChildSlot.GetWidget();
		if (ChildWidget->GetVisibility() == EVisibility::Collapsed)
		{
			return FVector2D::ZeroVector;
		}

		const FVector2D ChildDesired = ChildWidget->GetDesiredSize();
		if (AspectRatio <= 0.0f)
		{
			return ChildDesired;
		}

		if (ChildDesired.Y > 0.0f)
		{
			// Fit desired into ratio by expanding the smaller dimension.
			const float CurrentRatio = ChildDesired.X / ChildDesired.Y;
			if (CurrentRatio > AspectRatio)
			{
				// Too wide: increase height.
				return FVector2D(ChildDesired.X, ChildDesired.X / AspectRatio);
			}
			// Too tall: increase width.
			return FVector2D(ChildDesired.Y * AspectRatio, ChildDesired.Y);
		}

		return FVector2D(ChildDesired.X, ChildDesired.X / AspectRatio);
	}

	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override
	{
		const TSharedRef<SWidget> ChildWidget = ChildSlot.GetWidget();
		const EVisibility ChildVisibility = ChildWidget->GetVisibility();
		if (!ArrangedChildren.Accepts(ChildVisibility))
		{
			return;
		}

		const FVector2D AllottedSize = AllottedGeometry.GetLocalSize();
		if (AllottedSize.X <= 0.0f || AllottedSize.Y <= 0.0f || AspectRatio <= 0.0f)
		{
			ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild(ChildWidget, FVector2D::ZeroVector, FVector2D::ZeroVector));
			return;
		}

		// Compute the largest 16:9 (or configured) rect that fits within the allotted size.
		float TargetWidth = AllottedSize.X;
		float TargetHeight = AllottedSize.X / AspectRatio;
		if (TargetHeight > AllottedSize.Y)
		{
			TargetHeight = AllottedSize.Y;
			TargetWidth = AllottedSize.Y * AspectRatio;
		}

		TargetWidth = FMath::Max(0.0f, TargetWidth);
		TargetHeight = FMath::Max(0.0f, TargetHeight);

		float OffsetX = 0.0f;
		switch (HAlign)
		{
		case HAlign_Right:
			OffsetX = AllottedSize.X - TargetWidth;
			break;
		case HAlign_Center:
			OffsetX = (AllottedSize.X - TargetWidth) * 0.5f;
			break;
		default: // Left / Fill
			OffsetX = 0.0f;
			break;
		}

		float OffsetY = 0.0f;
		switch (VAlign)
		{
		case VAlign_Bottom:
			OffsetY = AllottedSize.Y - TargetHeight;
			break;
		case VAlign_Center:
			OffsetY = (AllottedSize.Y - TargetHeight) * 0.5f;
			break;
		default: // Top / Fill
			OffsetY = 0.0f;
			break;
		}

		const FVector2D ChildOffset(OffsetX, OffsetY);
		const FVector2D ChildSize(TargetWidth, TargetHeight);
		ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild(ChildWidget, ChildOffset, ChildSize));
	}

private:
	float AspectRatio = 16.0f / 9.0f;
	EHorizontalAlignment HAlign = HAlign_Center;
	EVerticalAlignment VAlign = VAlign_Center;
};

