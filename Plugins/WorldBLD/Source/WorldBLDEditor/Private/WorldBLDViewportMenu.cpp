// Copyright WorldBLD LLC. All rights reserved.


#include "WorldBLDViewportMenu.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

void UWorldBLDViewportMenu::SynchronizeProperties()
{	
	Super::SynchronizeProperties();

	TGuardValue<bool> AutoRestore(GAllowActorScriptExecutionInEditor, true);

	if (IsValid(ItemClass) && IsValid(ItemContainer))
	{
		// Remove bound delegates and clear Children
		int32 ChildrenCount = ItemContainer->GetChildrenCount();
		for (int i = 0; i < ChildrenCount; i++)
		{
			if (UWidget* Child = ItemContainer->GetChildAt(i))
			{
				if (UWorldBLDViewportMenuItem* Item = Cast<UWorldBLDViewportMenuItem>(Child))
				{
					if (Item->OnMenuItemClicked.IsAlreadyBound(this, &UWorldBLDViewportMenu::OnItemClicked))
					{
						Item->OnMenuItemClicked.RemoveDynamic(this, &UWorldBLDViewportMenu::OnItemClicked);
					}
				}
			}
		}
		ItemContainer->ClearChildren();

		// Populate the container
		for (int i = 0; i < NumItems; i++)
		{
			if (ItemContainer->CanAddMoreChildren())
			{
				UWorldBLDViewportMenuItem* Item = NewObject<UWorldBLDViewportMenuItem>(this, ItemClass);
				IWorldBLDViewportMenuItemInterface::Execute_SetIndex(Item, i);
				Item->OnMenuItemClicked.AddDynamic(this, &UWorldBLDViewportMenu::OnItemClicked);
				ItemContainer->AddChild(Item);
				OnMenuItemSet.Broadcast(Item, i);
			}
		}
	}
}

void UWorldBLDViewportMenu::SetItems(int32 Count)
{
	NumItems = Count;
}

TSharedRef<SWidget> UWorldBLDViewportMenuItem::RebuildWidget() 
{
	TSharedRef<SWidget> Widget = Super::RebuildWidget();
	if (Button && !Button->OnClicked.IsAlreadyBound(this, &UWorldBLDViewportMenuItem::OnButtonClicked))
	{
		Button->OnClicked.AddDynamic(this, &UWorldBLDViewportMenuItem::OnButtonClicked);
	}
	return Widget;
}

void UWorldBLDViewportMenuItem::SetData_Implementation(const FWorldBLDViewportMenuItemData& InData)
{
	MenuItemData = InData;

	// Best-effort UI update for common widget names. This keeps the feature functional even
	// if the Blueprint hasn't been updated yet (we fall back to tagging the Title/tooltip).
	auto FindTextBlock = [this](const TCHAR* Name) -> UTextBlock*
	{
		return (WidgetTree != nullptr) ? Cast<UTextBlock>(WidgetTree->FindWidget(FName(Name))) : nullptr;
	};

	// Common name candidates used in UMG designer.
	UTextBlock* TitleText =
		FindTextBlock(TEXT("TitleText")) ? FindTextBlock(TEXT("TitleText")) :
		FindTextBlock(TEXT("Title")) ? FindTextBlock(TEXT("Title")) :
		FindTextBlock(TEXT("Text_Title"));

	UTextBlock* DescriptionText =
		FindTextBlock(TEXT("DescriptionText")) ? FindTextBlock(TEXT("DescriptionText")) :
		FindTextBlock(TEXT("Description")) ? FindTextBlock(TEXT("Description")) :
		FindTextBlock(TEXT("Text_Description"));

	// Try to toggle a dedicated alert widget if it exists (optional in BP).
	if (WidgetTree != nullptr)
	{
		static const FName AlertWidgetNames[] =
		{
			TEXT("NewVersionAlert"),
			TEXT("NewVersionBadge"),
			TEXT("UpdateAvailableAlert"),
			TEXT("UpdateAvailableBadge"),
			TEXT("UpdateBadge"),
			TEXT("UpdateAlert")
		};

		for (const FName& WidgetName : AlertWidgetNames)
		{
			if (UWidget* AlertWidget = WidgetTree->FindWidget(WidgetName))
			{
				AlertWidget->SetVisibility(MenuItemData.bNewVersionAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
				break;
			}
		}
	}

	FText DisplayTitle = MenuItemData.Title;
	if (MenuItemData.bNewVersionAvailable)
	{
		DisplayTitle = FText::Format(NSLOCTEXT("WorldBLD", "ViewportMenuItem_Title_UpdateAvailable", "{0}  (Update Available)"), MenuItemData.Title);
	}

	if (TitleText != nullptr)
	{
		TitleText->SetText(DisplayTitle);
	}

	if (DescriptionText != nullptr)
	{
		DescriptionText->SetText(MenuItemData.Description);
	}

	if (Button != nullptr)
	{
		// Ensure the update notice is discoverable even if the BP UI doesn't have a badge yet.
		if (MenuItemData.bNewVersionAvailable)
		{
			const FText Tooltip = FText::Format(
				NSLOCTEXT("WorldBLD", "ViewportMenuItem_Tooltip_UpdateAvailable", "{0}\n\nNew version available."),
				MenuItemData.Description);
			Button->SetToolTipText(Tooltip);
		}
		else
		{
			Button->SetToolTipText(MenuItemData.Description);
		}
	}
}