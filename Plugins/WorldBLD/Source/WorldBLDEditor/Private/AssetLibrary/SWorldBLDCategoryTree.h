// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

#include "AssetLibrary/WorldBLDAssetLibraryCategoryTypes.h"

DECLARE_DELEGATE_OneParam(FOnWorldBLDCategorySelected, TSharedPtr<FWorldBLDCategoryNode>);

class SWorldBLDCategoryTree : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDCategoryTree) {}
		SLATE_EVENT(FOnWorldBLDCategorySelected, OnCategorySelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SelectCategoryByPath(const FString& CategoryPath);

private:
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FWorldBLDCategoryNode> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FWorldBLDCategoryNode> InParent, TArray<TSharedPtr<FWorldBLDCategoryNode>>& OutChildren) const;
	void OnSelectionChanged(TSharedPtr<FWorldBLDCategoryNode> InItem, ESelectInfo::Type SelectInfo);

	bool FindAndSelectByPath(const TArray<TSharedPtr<FWorldBLDCategoryNode>>& Nodes, const FString& CategoryPath);

	FOnWorldBLDCategorySelected OnCategorySelected;

	TArray<TSharedPtr<FWorldBLDCategoryNode>> RootItems;
	TSharedPtr<STreeView<TSharedPtr<FWorldBLDCategoryNode>>> TreeView;
};
