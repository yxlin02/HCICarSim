// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"

/**
 * Slate image widget that behaves like CSS "background-size: cover":
 * - Preserves the source aspect ratio.
 * - Scales uniformly to fill the allotted size.
 * - Crops (clips) any overflow.
 *
 * Intended for preview images where vertical fill is more important than showing the entire image.
 */
class SWorldBLDCoverImage : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDCoverImage)
		: _Image(nullptr)
	{
	}
		SLATE_ATTRIBUTE(const FSlateBrush*, Image)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Image = InArgs._Image;
		SetClipping(EWidgetClipping::ClipToBounds);
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		const FSlateBrush* Brush = Image.Get();
		if (!Brush)
		{
			return FVector2D::ZeroVector;
		}
		return Brush->GetImageSize();
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const FSlateBrush* Brush = Image.Get();
		if (!Brush || Brush->DrawAs == ESlateBrushDrawType::NoDrawType)
		{
			return LayerId;
		}

		// Defensive: prevent Slate from trying to render an invalid/GC'd resource object.
		// This can happen with transient textures/materials if something upstream drops references unexpectedly.
		if (UObject* ResourceObject = Brush->GetResourceObject())
		{
			if (!IsValid(ResourceObject))
			{
				return LayerId;
			}
		}

		const FVector2D AllottedSize = AllottedGeometry.GetLocalSize();
		const FVector2D SourceSize = Brush->GetImageSize();
		if (AllottedSize.X <= 0.0f || AllottedSize.Y <= 0.0f || SourceSize.X <= 0.0f || SourceSize.Y <= 0.0f)
		{
			return LayerId;
		}

		// "Cover" scale: fill the available rect while preserving aspect ratio.
		const float ScaleX = AllottedSize.X / SourceSize.X;
		const float ScaleY = AllottedSize.Y / SourceSize.Y;
		const float Scale = FMath::Max(ScaleX, ScaleY);

		const FVector2D DrawSize = SourceSize * Scale;
		const FVector2D Offset = (AllottedSize - DrawSize) * 0.5f;

		const bool bEnabled = ShouldBeEnabled(bParentEnabled);
		const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		const FLinearColor FinalColor = InWidgetStyle.GetColorAndOpacityTint() * Brush->GetTint(InWidgetStyle);

		const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry(
			DrawSize,
			FSlateLayoutTransform(Offset));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			PaintGeometry,
			Brush,
			DrawEffects,
			FinalColor);

		return LayerId;
	}

private:
	TAttribute<const FSlateBrush*> Image;
};

