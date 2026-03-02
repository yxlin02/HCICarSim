#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "Widgets/Views/STableViewBase.h"

class ITableRow;
class STableViewBase;
class SWindow;

template<typename ItemType>
class SListView;

template<typename ItemType>
class STableRow;

class SWorldBLDLightingPresetDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDLightingPresetDialog)
		: _PresetNames()
		, _DefaultPresetName()
	{
	}

		SLATE_ARGUMENT(TArray<FString>, PresetNames)
		SLATE_ARGUMENT(FString, DefaultPresetName)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	static TOptional<FString> ShowModal(const TArray<FString>& PresetNames, const FString& DefaultPresetName);

private:
	void SetHostWindow(TSharedRef<SWindow> InHostWindow);

	TSharedRef<ITableRow> HandleGenerateRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void HandleSelectionChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo);

	FReply HandleOkClicked();
	FReply HandleCancelClicked();
	bool IsOkEnabled() const;

private:
	TArray<TSharedPtr<FString>> PresetItems;
	TSharedPtr<SListView<TSharedPtr<FString>>> PresetListView;

	TSharedPtr<FString> SelectedPreset;
	bool bConfirmed = false;

	TWeakPtr<SWindow> HostWindow;
};
