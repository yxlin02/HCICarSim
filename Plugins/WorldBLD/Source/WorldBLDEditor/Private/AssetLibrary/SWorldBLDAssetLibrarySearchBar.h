// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SSearchBox;

class SWorldBLDAssetLibrarySearchBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDAssetLibrarySearchBar) {}
		SLATE_EVENT(FOnTextChanged, OnSearchTextChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetSearchText(const FText& InText);

private:
	void HandleSearchTextChanged(const FText& InText);

	FOnTextChanged OnSearchTextChanged;

	TSharedPtr<SSearchBox> SearchBox;
};
