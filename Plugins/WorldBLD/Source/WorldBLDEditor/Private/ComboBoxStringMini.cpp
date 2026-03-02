// Copyright WorldBLD LLC. All rights reserved.


#include "ComboBoxStringMini.h"

TSharedRef<SWidget> UComboBoxStringMini::RebuildWidget()
{	

	if (!OnOpening.IsAlreadyBound(this, &UComboBoxStringMini::HideContent))
	{
		OnOpening.AddDynamic(this, &UComboBoxStringMini::HideContent);
	}

	if (!OnSelectionChanged.IsAlreadyBound(this, &UComboBoxStringMini::HideContentOnSelectionChanged))
	{
		OnSelectionChanged.AddDynamic(this, &UComboBoxStringMini::HideContentOnSelectionChanged);
	}

	return Super::RebuildWidget();
}

void UComboBoxStringMini::HideContentOnSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	HideContent();
}

void UComboBoxStringMini::HideContent()
{
	if (ComboBoxContent.IsValid() && bHideComboBoxContent)
	{
		ComboBoxContent->SetContent(SNullWidget::NullWidget);		
	}
}