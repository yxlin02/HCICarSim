#include "LevelTemplate/SWorldBLDLightingPresetDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "WorldBLDLightingPresetDialog"

void SWorldBLDLightingPresetDialog::Construct(const FArguments& InArgs)
{
	PresetItems.Reset();
	for (const FString& PresetName : InArgs._PresetNames)
	{
		PresetItems.Add(MakeShared<FString>(PresetName));
	}

	const FString DefaultPresetName = InArgs._DefaultPresetName;
	if (!DefaultPresetName.IsEmpty())
	{
		for (const TSharedPtr<FString>& Item : PresetItems)
		{
			if (Item.IsValid() && *Item == DefaultPresetName)
			{
				SelectedPreset = Item;
				break;
			}
		}
	}

	if (!SelectedPreset.IsValid() && PresetItems.Num() > 0)
	{
		SelectedPreset = PresetItems[0];
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(12.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Header", "Select Lighting Preset"))
				.Font(FAppStyle::GetFontStyle("NormalFontBold"))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(FMargin(0.0f, 10.0f, 0.0f, 10.0f))
			[
				SAssignNew(PresetListView, SListView<TSharedPtr<FString>>)
				.ListItemsSource(&PresetItems)
				.OnGenerateRow(this, &SWorldBLDLightingPresetDialog::HandleGenerateRow)
				.OnSelectionChanged(this, &SWorldBLDLightingPresetDialog::HandleSelectionChanged)
				.SelectionMode(ESelectionMode::Single)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(4.0f, 0.0f))
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("Ok", "OK"))
					.IsEnabled(this, &SWorldBLDLightingPresetDialog::IsOkEnabled)
					.OnClicked(this, &SWorldBLDLightingPresetDialog::HandleOkClicked)
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked(this, &SWorldBLDLightingPresetDialog::HandleCancelClicked)
				]
			]
		]
	];

	if (PresetListView.IsValid() && SelectedPreset.IsValid())
	{
		PresetListView->SetSelection(SelectedPreset);
		PresetListView->RequestScrollIntoView(SelectedPreset);
	}
}

TOptional<FString> SWorldBLDLightingPresetDialog::ShowModal(const TArray<FString>& PresetNames, const FString& DefaultPresetName)
{
	if (PresetNames.IsEmpty())
	{
		return TOptional<FString>();
	}

	TSharedPtr<SWorldBLDLightingPresetDialog> DialogWidget;
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("Title", "Lighting Preset"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	Window->SetContent(
		SAssignNew(DialogWidget, SWorldBLDLightingPresetDialog)
		.PresetNames(PresetNames)
		.DefaultPresetName(DefaultPresetName));

	DialogWidget->SetHostWindow(Window);

	FSlateApplication::Get().AddModalWindow(Window, FSlateApplication::Get().GetActiveTopLevelWindow());

	if (!DialogWidget.IsValid() || !DialogWidget->bConfirmed || !DialogWidget->SelectedPreset.IsValid())
	{
		return TOptional<FString>();
	}

	return *DialogWidget->SelectedPreset;
}

void SWorldBLDLightingPresetDialog::SetHostWindow(TSharedRef<SWindow> InHostWindow)
{
	HostWindow = InHostWindow;
}

TSharedRef<ITableRow> SWorldBLDLightingPresetDialog::HandleGenerateRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty())
	];
}

void SWorldBLDLightingPresetDialog::HandleSelectionChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo)
{
	(void)SelectInfo;
	SelectedPreset = Item;
}

FReply SWorldBLDLightingPresetDialog::HandleOkClicked()
{
	bConfirmed = true;
	if (TSharedPtr<SWindow> Pinned = HostWindow.Pin())
	{
		Pinned->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SWorldBLDLightingPresetDialog::HandleCancelClicked()
{
	bConfirmed = false;
	if (TSharedPtr<SWindow> Pinned = HostWindow.Pin())
	{
		Pinned->RequestDestroyWindow();
	}
	return FReply::Handled();
}

bool SWorldBLDLightingPresetDialog::IsOkEnabled() const
{
	return SelectedPreset.IsValid();
}

#undef LOCTEXT_NAMESPACE
