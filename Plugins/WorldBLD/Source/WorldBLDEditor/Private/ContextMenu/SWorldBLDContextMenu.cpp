// Copyright WorldBLD LLC. All Rights Reserved.

#include "ContextMenu/SWorldBLDContextMenu.h"

#include "ContextMenu/WorldBLDContextMenuIcons.h"
#include "ContextMenu/WorldBLDContextMenuFont.h"
#include "ContextMenu/WorldBLDContextMenuStyle.h"
#include "GameFramework/Actor.h"
#include "Math/Vector4.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace WorldBLDContextMenuStyle
{
	static const FLinearColor BackgroundColor(0.011f, 0.011f, 0.011f, 0.95f);
	static const FLinearColor ErrorBackgroundColor(0.25f, 0.05f, 0.05f, 0.98f);
	static const FVector2D MinWindowSize(250.0f, 200.0f);
	static const FVector2D DefaultMaxWindowSize(500.0f, 400.0f);
}

void SWorldBLDContextMenu::Construct(const FArguments& InArgs)
{
	TitleText = InArgs._Title;
	ContentWidget = InArgs._Content;
	OnRequestClose = InArgs._OnRequestClose;
	bShowTitleBar = InArgs._ShowTitleBar;

	TargetActor = InArgs._TargetActor;
	TargetActors.Reset();
	for (AActor* Actor : InArgs._TargetActors)
	{
		TargetActors.Add(Actor);
	}

	if (!ContentWidget.IsValid())
	{
		ContentWidget = SNullWidget::NullWidget;
	}

	TSharedRef<SOverlay> RootOverlay = SNew(SOverlay);

	RootOverlay->AddSlot()
	[
		( [&]() -> TSharedRef<SWidget>
		{
			TSharedRef<SVerticalBox> MainBox = SNew(SVerticalBox);

			if (bShowTitleBar)
			{
				MainBox->AddSlot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(WorldBLDContextMenuStyle::GetTitleBarBackgroundBrush())
					.BorderBackgroundColor(FLinearColor::White)
					.Padding(FMargin(8.0f, 8.0f, 8.0f, 6.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(TitleText)
							.Font(WorldBLDContextMenuFont::GetTitleBar())
							.ColorAndOpacity(FLinearColor::White)
							.AutoWrapText(true)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
						[
							SNew(SButton)
							.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("FlatButton"))
							.ContentPadding(FMargin(6.0f, 2.0f))
							.Visibility(this, &SWorldBLDContextMenu::GetCloseButtonVisibility)
							.OnClicked(this, &SWorldBLDContextMenu::OnCloseClicked)
							[
								SNew(SImage)
								.Image(WorldBLDContextMenuIcons::GetActionIconBrush(WorldBLDContextMenuIcons::EActionIcon::Minus))
							]
						]
					]
				];
			}

			MainBox->AddSlot()
			.AutoHeight()
			.Padding(8.0f, 8.0f, 8.0f, 0.0f)
			[
				SAssignNew(ErrorBannerBorder, SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(WorldBLDContextMenuStyle::ErrorBackgroundColor)
				.Padding(FMargin(8.0f, 6.0f))
				.Visibility(this, &SWorldBLDContextMenu::GetErrorBannerVisibility)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SWorldBLDContextMenu::GetErrorBannerText)
						.Font(WorldBLDContextMenuFont::Get())
						.ColorAndOpacity(FLinearColor::White)
						.AutoWrapText(true)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Top)
					.Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(SButton)
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("FlatButton"))
						.ContentPadding(FMargin(6.0f, 2.0f))
						.OnClicked(this, &SWorldBLDContextMenu::OnDismissErrorClicked)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("X")))
							.Font(WorldBLDContextMenuFont::Get())
							.ColorAndOpacity(FLinearColor::White)
						]
					]
				]
			];

			MainBox->AddSlot()
			.FillHeight(1.0f)
			[
				SAssignNew(ContentBorder, SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
				.Padding(FMargin(8.0f, 8.0f))
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						ContentWidget.ToSharedRef()
					]
				]
			];

			return MainBox;
		}() )
	];

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(WorldBLDContextMenuStyle::MinWindowSize.X)
		.MinDesiredHeight(WorldBLDContextMenuStyle::MinWindowSize.Y)
		.MaxDesiredWidth(WorldBLDContextMenuStyle::DefaultMaxWindowSize.X)
		.MaxDesiredHeight(WorldBLDContextMenuStyle::DefaultMaxWindowSize.Y)
		[
			// Rounded menu background to match the title bar rounding.
			// Corner order: TopLeft, TopRight, BottomRight, BottomLeft.
			// Note: This is intentionally a RoundedBox brush (not WhiteBrush + tint) so the silhouette is rounded.
			SNew(SBorder)
			.BorderImage([]() -> const FSlateBrush*
			{
				static const FVector4 CornerRadii(10.0f, 10.0f, 10.0f, 10.0f);
				static const FSlateRoundedBoxBrush Brush(WorldBLDContextMenuStyle::BackgroundColor, CornerRadii);
				return &Brush;
			}())
			.Padding(FMargin(1.0f))
			[
			// Outline + content. Background fill is provided by the rounded border above.
				// Outline border (visual framing only; background comes from the fill border above).
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(FLinearColor::Transparent)
				.Padding(FMargin(0.0f))
				[
					RootOverlay
				]
			]
		]
	];
}

FReply SWorldBLDContextMenu::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// If the user clicks an empty/background area of the context menu (i.e. not a child widget that handles input),
	// we must still consume the click so it doesn't fall through to the viewport and change selection.
	return FReply::Handled();
}

FReply SWorldBLDContextMenu::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

FReply SWorldBLDContextMenu::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

void SWorldBLDContextMenu::SetContent(TSharedRef<SWidget> NewContent)
{
	ContentWidget = NewContent;
	if (ContentBorder.IsValid())
	{
		ContentBorder->SetContent(
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				ContentWidget.ToSharedRef()
			]);
	}
}

void SWorldBLDContextMenu::SetTitle(const FText& NewTitle)
{
	TitleText = NewTitle;
}

void SWorldBLDContextMenu::SetErrorMessage(const FText& InErrorMessage)
{
	ErrorMessage = InErrorMessage;
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

void SWorldBLDContextMenu::ClearErrorMessage()
{
	ErrorMessage = FText::GetEmpty();
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

bool SWorldBLDContextMenu::HasErrorMessage() const
{
	return !ErrorMessage.IsEmpty();
}

EVisibility SWorldBLDContextMenu::GetErrorBannerVisibility() const
{
	return HasErrorMessage() ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SWorldBLDContextMenu::GetErrorBannerText() const
{
	return ErrorMessage;
}

FReply SWorldBLDContextMenu::OnDismissErrorClicked()
{
	ClearErrorMessage();
	return FReply::Handled();
}

EVisibility SWorldBLDContextMenu::GetCloseButtonVisibility() const
{
	return OnRequestClose.IsBound() ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SWorldBLDContextMenu::OnCloseClicked()
{
	OnRequestClose.ExecuteIfBound();
	return FReply::Handled();
}


