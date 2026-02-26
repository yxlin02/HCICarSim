// Copyright WorldBLD LLC. All Rights Reserved.

#include "AssetLibrary/SWorldBLDSHA1MismatchDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

void SWorldBLDSHA1MismatchDialog::Construct(const FArguments& InArgs)
{
	OnResolved = InArgs._OnResolved;

	// Convert to shared pointers for list view
	for (const FWorldBLDAssetDependency& Dep : InArgs._MismatchedDependencies)
	{
		DependencyItems.Add(MakeShared<FWorldBLDAssetDependency>(Dep));
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(16.0f)
		[
			SNew(SBox)
			.MinDesiredWidth(600.0f)
			.MinDesiredHeight(400.0f)
			[
				SNew(SVerticalBox)

				// Title
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("SHA1 Hash Mismatch Detected")))
					.Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))
					.ColorAndOpacity(FLinearColor(0.95f, 0.75f, 0.25f, 1.0f))
				]

				// Warning message
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 16.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(
						TEXT("The following %d dependencies exist locally but have different versions than the backend:\n\n")
						TEXT("Replacing will download the backend versions and overwrite your local files.\n")
						TEXT("Keeping local will use your existing files (may cause compatibility issues)."),
						DependencyItems.Num())))
					.AutoWrapText(true)
					.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f))
				]

				// Dependency list
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(0.0f, 0.0f, 0.0f, 16.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
					.Padding(4.0f)
					[
						SNew(SListView<TSharedPtr<FWorldBLDAssetDependency>>)
						.ListItemsSource(&DependencyItems)
						.OnGenerateRow(this, &SWorldBLDSHA1MismatchDialog::GenerateDependencyRow)
						.SelectionMode(ESelectionMode::None)
					]
				]

				// Buttons
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FMargin(8.0f, 0.0f))

					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
						.Text(FText::FromString(TEXT("Replace All with Backend Versions")))
						.ToolTipText(FText::FromString(TEXT("Download and replace all mismatched dependencies with backend versions (Recommended)")))
						.OnClicked(this, &SWorldBLDSHA1MismatchDialog::HandleReplaceAll)
						.HAlign(HAlign_Center)
					]

					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "Button")
						.Text(FText::FromString(TEXT("Keep Local Versions")))
						.ToolTipText(FText::FromString(TEXT("Use existing local files (may cause compatibility issues)")))
						.OnClicked(this, &SWorldBLDSHA1MismatchDialog::HandleKeepLocal)
						.HAlign(HAlign_Center)
					]

					+ SUniformGridPanel::Slot(2, 0)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "Button")
						.Text(FText::FromString(TEXT("Cancel")))
						.ToolTipText(FText::FromString(TEXT("Cancel download operation")))
						.OnClicked(this, &SWorldBLDSHA1MismatchDialog::HandleCancel)
						.HAlign(HAlign_Center)
					]
				]
			]
		]
	];
}

TSharedRef<ITableRow> SWorldBLDSHA1MismatchDialog::GenerateDependencyRow(
	TSharedPtr<FWorldBLDAssetDependency> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FWorldBLDAssetDependency>>, OwnerTable)
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(SHorizontalBox)

			// Warning icon
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("⚠")))
				.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
				.ColorAndOpacity(FLinearColor(0.95f, 0.75f, 0.25f, 1.0f))
			]

			// Asset info
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%s (%s)"), *Item->AssetName, *Item->AssetType)))
					.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("Local SHA1: %s"), *Item->SHA1Hash.Left(16))))
					.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
					.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
				]
			]
		];
}

FReply SWorldBLDSHA1MismatchDialog::HandleReplaceAll()
{
	OnResolved.ExecuteIfBound(true);

	// Close the dialog window
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (ParentWindow.IsValid())
	{
		ParentWindow->RequestDestroyWindow();
	}

	return FReply::Handled();
}

FReply SWorldBLDSHA1MismatchDialog::HandleKeepLocal()
{
	OnResolved.ExecuteIfBound(false);

	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (ParentWindow.IsValid())
	{
		ParentWindow->RequestDestroyWindow();
	}

	return FReply::Handled();
}

FReply SWorldBLDSHA1MismatchDialog::HandleCancel()
{
	// Don't execute the callback - just close
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (ParentWindow.IsValid())
	{
		ParentWindow->RequestDestroyWindow();
	}

	return FReply::Handled();
}
