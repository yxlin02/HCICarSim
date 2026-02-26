// Copyright WorldBLD LLC. All rights reserved.

#include "ContextMenu/Widgets/SWorldBLDAssetPicker.h"

#include "ContextMenu/WorldBLDContextMenuFont.h"

#include "InputAssetComboPanel.h"
#include "InputAssetPanelTypes.h"

#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SWorldBLDAssetPicker"

namespace WorldBLDAssetPickerStyle
{
	static const FSlateColor LabelColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
	static const FSlateColor MixedTextColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f));
	static const FLinearColor MixedBackground(0.15f, 0.15f, 0.15f, 0.90f);
	static const FMargin RowPadding(4.0f, 2.0f);
}

void SWorldBLDAssetPicker::Construct(const FArguments& InArgs)
{
	SelectedAssetAttribute = InArgs._SelectedAsset;
	LabelText = InArgs._Label;
	ToolTipText = InArgs._ToolTipText;
	FilterAssetClass = InArgs._AssetClass;
	OnShouldFilterAssetDelegate = InArgs._OnShouldFilterAsset;
	ThumbnailSize = InArgs._ThumbnailSize;
	bCompactSquareRightAligned = InArgs._bCompactSquareRightAligned;
	OnAssetSelectedDelegate = InArgs._OnAssetSelected;
	bAllowClear = InArgs._bAllowClear;
	TransactionText = InArgs._TransactionText;

	const FSlateFontInfo DetailFont = WorldBLDContextMenuFont::Get();

	if (bCompactSquareRightAligned)
	{
		const float SquareSize = FMath::Max(ThumbnailSize.X, ThumbnailSize.Y);
		ThumbnailSize = FVector2D(SquareSize, SquareSize);
	}

	FAssetData InitialAssetData;
	if (const TOptional<FAssetData> Selected = GetSelectedAsset(); Selected.IsSet())
	{
		InitialAssetData = Selected.GetValue();
	}
	LastSelectedAssetPath = InitialAssetData.GetSoftObjectPath();

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		.Padding(WorldBLDAssetPickerStyle::RowPadding)
		[
			SAssignNew(LabelWidget, STextBlock)
				.Text(LabelText)
				.Font(DetailFont)
				.ColorAndOpacity(WorldBLDAssetPickerStyle::LabelColor)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(WorldBLDAssetPickerStyle::RowPadding)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			[
				SAssignNew(AssetComboContainer, SBox)
				[
					MakeAssetComboPanel(InitialAssetData)
				]
			]

			// Mixed indicator overlay (kept hit-test invisible so the picker still works in mixed state).
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(WorldBLDAssetPickerStyle::MixedBackground)
				.Padding(FMargin(6.0f, 4.0f))
				.Visibility(this, &SWorldBLDAssetPicker::GetMixedValueVisibility)
				[
					SAssignNew(MixedValueLabel, STextBlock)
					.Text(LOCTEXT("MixedValue", "Mixed"))
					.Font(DetailFont)
					.ColorAndOpacity(WorldBLDAssetPickerStyle::MixedTextColor)
					.Justification(ETextJustify::Center)
				]
			]
		]
	];
}

void SWorldBLDAssetPicker::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	RefreshAssetComboFromAttribute();

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

void SWorldBLDAssetPicker::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
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

void SWorldBLDAssetPicker::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);

	bIsHovering = false;
	bTooltipArmed = false;

	if (LabelWidget.IsValid())
	{
		LabelWidget->SetToolTip(nullptr);
	}
}

TOptional<FAssetData> SWorldBLDAssetPicker::GetSelectedAsset() const
{
	return SelectedAssetAttribute.Get();
}

void SWorldBLDAssetPicker::HandleAssetSelected(const FAssetData& AssetData)
{
	if (!bAllowClear && !AssetData.IsValid())
	{
		return;
	}

	const TOptional<FAssetData> Current = GetSelectedAsset();
	if (Current.IsSet() && Current.GetValue().GetSoftObjectPath() == AssetData.GetSoftObjectPath())
	{
		return;
	}

	TUniquePtr<FScopedTransaction> Transaction;
	if (!TransactionText.IsEmpty())
	{
		Transaction = MakeUnique<FScopedTransaction>(TransactionText);
	}

	if (OnAssetSelectedDelegate.IsBound())
	{
		OnAssetSelectedDelegate.Execute(AssetData);
	}

	LastSelectedAssetPath = AssetData.GetSoftObjectPath();
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

EVisibility SWorldBLDAssetPicker::GetMixedValueVisibility() const
{
	const bool bIsMixed = !GetSelectedAsset().IsSet();
	return bIsMixed ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

EVisibility SWorldBLDAssetPicker::GetAssetComboVisibility() const
{
	// Keep visible even in mixed mode; the overlay communicates "Mixed" while allowing editing.
	return EVisibility::Visible;
}

TSharedRef<SWidget> SWorldBLDAssetPicker::MakeAssetComboPanel(const FAssetData& InitialAssetData)
{
	return SAssignNew(AssetComboPanel, SInputAssetComboPanel)
		.FilterSetup(FAssetComboFilterSetup(FilterAssetClass))
		.ComboButtonTileSize(ThumbnailSize)
		.InitiallySelectedAsset(InitialAssetData)
		.OnShouldFilterAsset(OnShouldFilterAssetDelegate)
		.Visibility(this, &SWorldBLDAssetPicker::GetAssetComboVisibility)
		.OnSelectionChanged(this, &SWorldBLDAssetPicker::HandleAssetSelected);
}

void SWorldBLDAssetPicker::RefreshAssetComboFromAttribute()
{
	if (!AssetComboContainer.IsValid())
	{
		return;
	}

	// The underlying SInputAssetComboPanel does not currently support programmatic updates of the
	// selected asset after construction. To keep the UI in sync with undo/redo (and other external
	// changes), rebuild the combo panel when the bound attribute changes.
	const TOptional<FAssetData> Selected = GetSelectedAsset();
	if (!Selected.IsSet())
	{
		// Mixed values across selection. Keep the current combo selection and rely on the "Mixed" overlay.
		return;
	}

	const FSoftObjectPath DesiredPath = Selected.GetValue().GetSoftObjectPath();

	if (DesiredPath == LastSelectedAssetPath)
	{
		return;
	}

	LastSelectedAssetPath = DesiredPath;

	AssetComboContainer->SetContent(MakeAssetComboPanel(Selected.GetValue()));
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

#undef LOCTEXT_NAMESPACE


