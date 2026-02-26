// Copyright WorldBLD LLC. All Rights Reserved.

#include "AssetLibrary/SWorldBLDAssetLibrarySearchBar.h"

#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/AppStyle.h"

void SWorldBLDAssetLibrarySearchBar::Construct(const FArguments& InArgs)
{
	OnSearchTextChanged = InArgs._OnSearchTextChanged;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(12.0f, 10.0f))
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SBox)
			.HeightOverride(40.0f)
			[
				SAssignNew(SearchBox, SSearchBox)
				.HintText(FText::FromString(TEXT("Search assets...")))
				.OnTextChanged(this, &SWorldBLDAssetLibrarySearchBar::HandleSearchTextChanged)
			]
		]
	];
}

void SWorldBLDAssetLibrarySearchBar::SetSearchText(const FText& InText)
{
	if (SearchBox.IsValid())
	{
		SearchBox->SetText(InText);
	}
}

void SWorldBLDAssetLibrarySearchBar::HandleSearchTextChanged(const FText& InText)
{
	if (OnSearchTextChanged.IsBound())
	{
		OnSearchTextChanged.Execute(InText);
	}
}
