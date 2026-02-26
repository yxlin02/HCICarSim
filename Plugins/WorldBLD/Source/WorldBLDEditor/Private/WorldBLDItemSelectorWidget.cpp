// Copyright WorldBLD. All rights reserved.

#include "WorldBLDItemSelectorWidget.h"
#include "WorldBLDItemButtonWidget.h"
#include "Components/ScrollBox.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Blueprint/WidgetTree.h"

void UWorldBLDItemSelectorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Configure ScrollBox settings
	if (ItemScrollBox)
	{
		ItemScrollBox->SetAlwaysShowScrollbar(false);
		ItemScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
	}

	// Configure WrapBox settings with hard-coded defaults
	if (ItemContainer)
	{
		ItemContainer->SetInnerSlotPadding(FVector2D(8.0f, 8.0f));
	}
}

void UWorldBLDItemSelectorWidget::SetItems(const TArray<UClass*>& InItemClasses)
{
	if (!ItemContainer)
	{
		return;
	}

	// Clear existing widgets
	ItemContainer->ClearChildren();
	ItemWidgets.Empty();

	// Validate that we have an item widget class to instantiate
	if (!ItemWidgetClass || !IsValid(ItemWidgetClass))
	{
		UE_LOG(LogTemp, Error, TEXT("UWorldBLDItemSelectorWidget::SetItems - ItemWidgetClass is not set! Please set ItemWidgetClass in the widget's Details panel to a Blueprint subclass of UWorldBLDItemButtonWidget."));
		UE_LOG(LogTemp, Error, TEXT("  Widget: %s, ItemWidgetClass: %s"), *GetName(), ItemWidgetClass ? *ItemWidgetClass->GetName() : TEXT("NULL"));
		return;
	}

	// Create a widget for each item class
	for (UClass* ItemClass : InItemClasses)
	{
		if (!ItemClass)
		{
			continue;
		}

		// Create the item widget using the widget tree
		UWorldBLDItemButtonWidget* ItemWidget = CreateWidget<UWorldBLDItemButtonWidget>(this, ItemWidgetClass);
		
		if (ItemWidget)
		{
			// Configure the item with the item class
			ItemWidget->SetItem(ItemClass);

			// Bind the click delegate
			ItemWidget->OnItemClicked.BindUObject(this, &UWorldBLDItemSelectorWidget::HandleItemClicked);

			// Add to the WrapBox
			UWrapBoxSlot* WrapBoxSlot = ItemContainer->AddChildToWrapBox(ItemWidget);
			
			if (WrapBoxSlot)
			{
				// Configure slot padding (hard-coded defaults)
				WrapBoxSlot->SetPadding(FMargin(4.0f));
				WrapBoxSlot->SetHorizontalAlignment(HAlign_Center);
				WrapBoxSlot->SetVerticalAlignment(VAlign_Center);
			}

			// Keep track of created widgets
			ItemWidgets.Add(ItemWidget);
		}
	}
}

void UWorldBLDItemSelectorWidget::HandleItemClicked(UClass* ItemClass)
{
	// Trigger the blueprint implementable event
	OnItemClicked(ItemClass);
}

void UWorldBLDItemSelectorWidget::OnItemClicked_Implementation(UClass* ClickedItemClass)
{
	// Default implementation does nothing
	// Override this in Blueprint or subclasses to handle item selection
}

bool UWorldBLDItemSelectorWidget::IsWidgetConfigured() const
{
	if (!ItemScrollBox)
	{
		UE_LOG(LogTemp, Error, TEXT("UWorldBLDItemSelectorWidget::IsWidgetConfigured - ItemScrollBox is not bound! Make sure your Blueprint has a ScrollBox named 'ItemScrollBox'."));
		return false;
	}

	if (!ItemContainer)
	{
		UE_LOG(LogTemp, Error, TEXT("UWorldBLDItemSelectorWidget::IsWidgetConfigured - ItemContainer is not bound! Make sure your Blueprint has a WrapBox named 'ItemContainer' inside the ScrollBox."));
		return false;
	}

	if (!ItemWidgetClass || !IsValid(ItemWidgetClass))
	{
		UE_LOG(LogTemp, Error, TEXT("UWorldBLDItemSelectorWidget::IsWidgetConfigured - ItemWidgetClass is not set! Set it in the Details panel to a Blueprint subclass of UWorldBLDItemButtonWidget."));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("UWorldBLDItemSelectorWidget::IsWidgetConfigured - Widget is properly configured."));
	return true;
}

