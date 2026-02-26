// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SNumericEntryBox.h"

/**
 * Reusable numeric property widget designed for context-menu panels.
 * Supports mixed values via TOptional<float> (unset = "Mixed").
 */
class WORLDBLDEDITOR_API SWorldBLDPropertySlider : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDPropertySlider)
		: _Label(FText::GetEmpty())
		, _ToolTipText(FText::GetEmpty())
		, _UnitLabel(FText::GetEmpty())
		, _TransactionText(FText::GetEmpty())
		, _MinValue()
		, _MaxValue()
		, _MinSliderValue()
		, _MaxSliderValue()
		, _SliderExponent(1.0f)
		, _DecimalPlaces(1)
	{
	}

		SLATE_ATTRIBUTE(TOptional<float>, Value)
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(FText, ToolTipText)
		SLATE_ARGUMENT(FText, UnitLabel)
		/** If non-empty, value changes initiated by this widget are wrapped in an editor undo transaction. */
		SLATE_ARGUMENT(FText, TransactionText)
		SLATE_ARGUMENT(TOptional<float>, MinValue)
		SLATE_ARGUMENT(TOptional<float>, MaxValue)
		SLATE_ARGUMENT(TOptional<float>, MinSliderValue)
		SLATE_ARGUMENT(TOptional<float>, MaxSliderValue)
		SLATE_ARGUMENT(float, SliderExponent)
		SLATE_ARGUMENT(int32, DecimalPlaces)
		SLATE_EVENT(FOnFloatValueChanged, OnValueChanged)
		SLATE_EVENT(FOnFloatValueCommitted, OnValueCommitted)
		SLATE_EVENT(FSimpleDelegate, OnBeginSliderMovement)
		SLATE_EVENT(FOnFloatValueChanged, OnEndSliderMovement)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

private:
	TOptional<float> GetValue() const;

	void HandleValueChanged(float NewValue);
	void HandleValueCommitted(float NewValue, ETextCommit::Type CommitType);
	void HandleBeginSliderMovement();
	void HandleEndSliderMovement(float NewValue);

private:
	TAttribute<TOptional<float>> ValueAttribute;

	FText LabelText;
	FText ToolTipText;
	FText UnitLabelText;
	FText TransactionText;

	TOptional<float> MinValue;
	TOptional<float> MaxValue;
	TOptional<float> MinSliderValue;
	TOptional<float> MaxSliderValue;

	float SliderExponent = 1.0f;
	int32 DecimalPlaces = 2;

	FOnFloatValueChanged OnValueChanged;
	FOnFloatValueCommitted OnValueCommitted;
	FSimpleDelegate OnBeginSliderMovement;
	FOnFloatValueChanged OnEndSliderMovement;

	TSharedPtr<class STextBlock> LabelWidget;

	/** Active transaction for interactive slider drags (kept alive until EndSliderMovement). */
	TUniquePtr<class FScopedTransaction> ActiveTransaction;

	double HoverStartTimeSeconds = 0.0;
	bool bIsHovering = false;
	bool bTooltipArmed = false;
};

