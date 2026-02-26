#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UWorldBLDLevelTemplateBundle;
class STextBlock;
class SScrollBox;
class SWrapBox;

class SWorldBLDLevelTemplateWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDLevelTemplateWindow) {}
	SLATE_END_ARGS()

	virtual ~SWorldBLDLevelTemplateWindow() override;

	void Construct(const FArguments& InArgs);

private:
	void HandleAvailableLevelTemplateBundlesChanged();
	void RefreshTemplateList();
	void RebuildGrid();
	FReply HandleGetMoreClicked();
	FReply HandleTemplateTileClicked(UWorldBLDLevelTemplateBundle* Bundle);

	FReply CloseHostWindow();

private:
	TArray<TWeakObjectPtr<UWorldBLDLevelTemplateBundle>> TemplateBundles;
	TWeakObjectPtr<UWorldBLDLevelTemplateBundle> SelectedTemplate;

	TSharedPtr<STextBlock> EmptyStateText;
	TSharedPtr<SScrollBox> ScrollBox;
	TSharedPtr<SWrapBox> WrapBox;

	FDelegateHandle LevelTemplateBundlesChangedHandle;
};
