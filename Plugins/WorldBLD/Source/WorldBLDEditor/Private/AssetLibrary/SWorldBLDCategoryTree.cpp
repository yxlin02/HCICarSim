// Copyright WorldBLD LLC. All Rights Reserved.

#include "AssetLibrary/SWorldBLDCategoryTree.h"

#include "Widgets/Views/STreeView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/SlateTypes.h"

#include "ContextMenu/WorldBLDContextMenuStyle.h"

namespace
{
	static FLinearColor GetSidebarBackground()
	{
		return FLinearColor(0.08f, 0.08f, 0.08f, 1.0f);
	}

	static FLinearColor GetSidebarItemHover()
	{
		return FLinearColor(0.12f, 0.12f, 0.12f, 1.0f);
	}

	static FLinearColor GetSidebarItemSelected()
	{
		return WorldBLDContextMenuStyle::GetHighlightRedColor();
	}

	static const FTableRowStyle& GetCategoryTreeRowStyle()
	{
		static const FSlateColorBrush TransparentBrush(FLinearColor::Transparent);
		static const FSlateColorBrush HoveredBrush(GetSidebarItemHover());
		static const FSlateColorBrush SelectedBrush(GetSidebarItemSelected());

		static const FTableRowStyle RowStyle = []()
		{
			FTableRowStyle Style = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");

			// Replace the default editor (blue) selection visuals with our red highlight.
			Style
				.SetActiveBrush(SelectedBrush)
				.SetActiveHoveredBrush(SelectedBrush)
				.SetInactiveBrush(SelectedBrush)
				.SetInactiveHoveredBrush(SelectedBrush)
				.SetSelectorFocusedBrush(TransparentBrush)
				.SetEvenRowBackgroundBrush(TransparentBrush)
				.SetOddRowBackgroundBrush(TransparentBrush)
				.SetEvenRowBackgroundHoveredBrush(HoveredBrush)
				.SetOddRowBackgroundHoveredBrush(HoveredBrush);

			return Style;
		}();

		return RowStyle;
	}
}

void SWorldBLDCategoryTree::Construct(const FArguments& InArgs)
{
	OnCategorySelected = InArgs._OnCategorySelected;

	RootItems = FWorldBLDCategoryHierarchy::BuildCategoryTree();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.BorderBackgroundColor(GetSidebarBackground())
		.Padding(FMargin(8.0f, 8.0f))
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FWorldBLDCategoryNode>>)
			.TreeItemsSource(&RootItems)
			.OnGenerateRow(this, &SWorldBLDCategoryTree::OnGenerateRow)
			.OnGetChildren(this, &SWorldBLDCategoryTree::OnGetChildren)
			.OnSelectionChanged(this, &SWorldBLDCategoryTree::OnSelectionChanged)
			.SelectionMode(ESelectionMode::Single)
		]
	];

	if (TreeView.IsValid() && RootItems.Num() > 0)
	{
		TreeView->SetSelection(RootItems[0]);

		// Expand "Assets" by default so its children are visible without requiring a manual dropdown click.
		for (const TSharedPtr<FWorldBLDCategoryNode>& RootItem : RootItems)
		{
			if (RootItem.IsValid() && RootItem->CategoryPath == TEXT("Assets"))
			{
				TreeView->SetItemExpansion(RootItem, true);
				break;
			}
		}
	}
}

void SWorldBLDCategoryTree::SelectCategoryByPath(const FString& CategoryPath)
{
	if (!TreeView.IsValid())
	{
		return;
	}

	FindAndSelectByPath(RootItems, CategoryPath);
}

bool SWorldBLDCategoryTree::FindAndSelectByPath(const TArray<TSharedPtr<FWorldBLDCategoryNode>>& Nodes, const FString& CategoryPath)
{
	if (!TreeView.IsValid())
	{
		return false;
	}

	for (const TSharedPtr<FWorldBLDCategoryNode>& Node : Nodes)
	{
		if (!Node.IsValid())
		{
			continue;
		}

		if (Node->CategoryPath == CategoryPath)
		{
			TreeView->SetSelection(Node);
			return true;
		}

		if (Node->ChildCategories.Num() > 0)
		{
			if (FindAndSelectByPath(Node->ChildCategories, CategoryPath))
			{
				// Ensure the parent chain is expanded so the selection is visible.
				TreeView->SetItemExpansion(Node, true);
				return true;
			}
		}
	}

	return false;
}

TSharedRef<ITableRow> SWorldBLDCategoryTree::OnGenerateRow(TSharedPtr<FWorldBLDCategoryNode> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<STableRow<TSharedPtr<FWorldBLDCategoryNode>>> Row;
	SAssignNew(Row, STableRow<TSharedPtr<FWorldBLDCategoryNode>>, OwnerTable)
		.Style(&GetCategoryTreeRowStyle())
		.Padding(FMargin(2.0f, 2.0f));

	Row->SetContent(
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
		.Padding(FMargin(6.0f, 4.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item.IsValid() ? Item->CategoryName : FString()))
			.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f))
		]
	);

	return Row.ToSharedRef();
}

void SWorldBLDCategoryTree::OnGetChildren(TSharedPtr<FWorldBLDCategoryNode> InParent, TArray<TSharedPtr<FWorldBLDCategoryNode>>& OutChildren) const
{
	if (InParent.IsValid())
	{
		OutChildren.Append(InParent->ChildCategories);
	}
}

void SWorldBLDCategoryTree::OnSelectionChanged(TSharedPtr<FWorldBLDCategoryNode> InItem, ESelectInfo::Type SelectInfo)
{
	(void)SelectInfo;

	if (OnCategorySelected.IsBound())
	{
		OnCategorySelected.Execute(InItem);
	}
}
