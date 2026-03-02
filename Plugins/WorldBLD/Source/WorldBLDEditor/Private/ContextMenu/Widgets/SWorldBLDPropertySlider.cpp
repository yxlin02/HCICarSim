// Copyright WorldBLD LLC. All rights reserved.

#include "ContextMenu/Widgets/SWorldBLDPropertySlider.h"

#include "ContextMenu/WorldBLDContextMenuFont.h"

#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SWorldBLDPropertySlider"

namespace WorldBLDPropertyWidgetStyle
{
	static const FSlateColor LabelColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
	static const FSlateColor ValueColor(FLinearColor(0.92f, 0.92f, 0.92f, 1.0f));
	static const FSlateColor InputBackground(FLinearColor(0.03f, 0.03f, 0.03f, 0.95f));
	static const FMargin RowPadding(4.0f, 2.0f);
}

void SWorldBLDPropertySlider::Construct(const FArguments& InArgs)
{
	ValueAttribute = InArgs._Value;
	LabelText = InArgs._Label;
	ToolTipText = InArgs._ToolTipText;
	UnitLabelText = InArgs._UnitLabel;
	TransactionText = InArgs._TransactionText;

	MinValue = InArgs._MinValue;
	MaxValue = InArgs._MaxValue;
	MinSliderValue = InArgs._MinSliderValue;
	MaxSliderValue = InArgs._MaxSliderValue;

	SliderExponent = InArgs._SliderExponent;
	// This widget should always *display* a single fractional digit (eg "12.3").
	// Clamping here also keeps the spin delta consistent with what the user sees.
	DecimalPlaces = FMath::Clamp(InArgs._DecimalPlaces, 1, 1);

	OnValueChanged = InArgs._OnValueChanged;
	OnValueCommitted = InArgs._OnValueCommitted;
	OnBeginSliderMovement = InArgs._OnBeginSliderMovement;
	OnEndSliderMovement = InArgs._OnEndSliderMovement;

	const FSlateFontInfo DetailFont = WorldBLDContextMenuFont::Get();

	const int32 SafeDecimalPlaces = FMath::Max(0, DecimalPlaces);
	const float Delta =
		(SafeDecimalPlaces > 0) ? FMath::Pow(10.0f, -static_cast<float>(SafeDecimalPlaces)) : 1.0f;

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		.Padding(WorldBLDPropertyWidgetStyle::RowPadding)
		[
			SAssignNew(LabelWidget, STextBlock)
				.Text(LabelText)
				.Font(DetailFont)
				.ColorAndOpacity(WorldBLDPropertyWidgetStyle::LabelColor)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(WorldBLDPropertyWidgetStyle::RowPadding)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SNumericEntryBox<float>)
				.Value(this, &SWorldBLDPropertySlider::GetValue)
				.AllowSpin(true)
				.MinValue(MinValue)
				.MaxValue(MaxValue)
				.MinSliderValue(MinSliderValue)
				.MaxSliderValue(MaxSliderValue)
				.SliderExponent(SliderExponent)
				.Delta(Delta)
				.MinFractionalDigits(1)
				.MaxFractionalDigits(1)
				// Keep this compact; auto-width slots won't help if the widget itself has a large minimum.
				.MinDesiredValueWidth(55.0f)
				.Justification(ETextJustify::Right)
				.UndeterminedString(LOCTEXT("MixedValue", "Mixed"))
				.BorderBackgroundColor(WorldBLDPropertyWidgetStyle::InputBackground)
				.Font(DetailFont)
				.OnValueChanged(this, &SWorldBLDPropertySlider::HandleValueChanged)
				.OnValueCommitted(this, &SWorldBLDPropertySlider::HandleValueCommitted)
				.OnBeginSliderMovement(this, &SWorldBLDPropertySlider::HandleBeginSliderMovement)
				.OnEndSliderMovement(this, &SWorldBLDPropertySlider::HandleEndSliderMovement)
			]
		]
	];
}

void SWorldBLDPropertySlider::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!bIsHovering || bTooltipArmed || ToolTipText.IsEmpty() || !LabelWidget.IsValid())
	{
		return;
	}

	const double NowSeconds = FPlatformTime::Seconds();
	if (NowSeconds - HoverStartTimeSeconds >= 0.5)
	{
		LabelWidget->SetToolTipText(ToolTipText);
		bTooltipArmed = true;
	}
}

void SWorldBLDPropertySlider::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);

	bIsHovering = true;
	bTooltipArmed = false;
	HoverStartTimeSeconds = FPlatformTime::Seconds();

	if (LabelWidget.IsValid())
	{
		LabelWidget->SetToolTip(nullptr);
	}
}

void SWorldBLDPropertySlider::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);

	bIsHovering = false;
	bTooltipArmed = false;

	if (LabelWidget.IsValid())
	{
		LabelWidget->SetToolTip(nullptr);
	}
}

TOptional<float> SWorldBLDPropertySlider::GetValue() const
{
	return ValueAttribute.Get();
}

void SWorldBLDPropertySlider::HandleValueChanged(float NewValue)
{
	// If the client is updating properties interactively (eg on slider drag),
	// ensure we have a single transaction spanning the drag.
	if (!ActiveTransaction.IsValid() && !TransactionText.IsEmpty())
	{
		ActiveTransaction = MakeUnique<FScopedTransaction>(TransactionText);
	}

	if (OnValueChanged.IsBound())
	{
		OnValueChanged.Execute(NewValue);
	}
}

void SWorldBLDPropertySlider::HandleValueCommitted(float NewValue, ETextCommit::Type CommitType)
{
	const TOptional<float> CurrentValue = GetValue();
	if (CurrentValue.IsSet() && FMath::IsNearlyEqual(CurrentValue.GetValue(), NewValue, KINDA_SMALL_NUMBER))
	{
		return;
	}

	// Non-interactive commit (typing/enter) should create its own transaction.
	TUniquePtr<FScopedTransaction> LocalTransaction;
	if (!ActiveTransaction.IsValid() && !TransactionText.IsEmpty())
	{
		LocalTransaction = MakeUnique<FScopedTransaction>(TransactionText);
	}

	if (OnValueCommitted.IsBound())
	{
		OnValueCommitted.Execute(NewValue, CommitType);
	}
}

void SWorldBLDPropertySlider::HandleBeginSliderMovement()
{
	if (!ActiveTransaction.IsValid() && !TransactionText.IsEmpty())
	{
		ActiveTransaction = MakeUnique<FScopedTransaction>(TransactionText);
	}

	if (OnBeginSliderMovement.IsBound())
	{
		OnBeginSliderMovement.Execute();
	}
}

void SWorldBLDPropertySlider::HandleEndSliderMovement(float NewValue)
{
	if (OnEndSliderMovement.IsBound())
	{
		OnEndSliderMovement.Execute(NewValue);
	}

	ActiveTransaction.Reset();
}

#undef LOCTEXT_NAMESPACE


