// Copyright WorldBLD. All rights reserved.

#include "WorldBLDItemButtonWidget.h"
#include "Components/Button.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

void UWorldBLDItemButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind the button click event
	if (ItemButton)		
	{
		if (!ItemButton->OnClicked.IsAlreadyBound(this, &UWorldBLDItemButtonWidget::OnButtonClicked))
		{
			ItemButton->OnClicked.AddDynamic(this, &UWorldBLDItemButtonWidget::OnButtonClicked);
		}
	}
}

void UWorldBLDItemButtonWidget::SetItem(UClass* InItemClass)
{
	ItemClass = InItemClass;

	if (ItemClass && ItemButton)
	{
		// Get the thumbnail using the virtual method
		UTexture2D* Thumbnail = GetThumbnailFromClass(ItemClass);
		
		if (Thumbnail)
		{
			// Get the current button style to preserve all existing settings
			FButtonStyle ButtonStyle = ItemButton->GetStyle();
			
			// Only modify the image resource and size for each state brush
			// This preserves outlines, corner radius, tint, and other style properties set in Blueprint
			ButtonStyle.Normal.SetResourceObject(Thumbnail);
			ButtonStyle.Normal.ImageSize = FVector2D(128.0f, 128.0f); // Hard-coded size: 128x128 pixels
			
			ButtonStyle.Hovered.SetResourceObject(Thumbnail);
			ButtonStyle.Hovered.ImageSize = FVector2D(128.0f, 128.0f);
			
			ButtonStyle.Pressed.SetResourceObject(Thumbnail);
			ButtonStyle.Pressed.ImageSize = FVector2D(128.0f, 128.0f);
			
			ButtonStyle.Disabled.SetResourceObject(Thumbnail);
			ButtonStyle.Disabled.ImageSize = FVector2D(128.0f, 128.0f);
			
			// Apply the modified style back to the button
			ItemButton->SetStyle(ButtonStyle);
		}

		// Set the tooltip using the virtual method
		FText TooltipText = GetTooltipTextFromClass(ItemClass);
		if (!TooltipText.IsEmpty())
		{
			ItemButton->SetToolTipText(TooltipText);
		}
	}
}

void UWorldBLDItemButtonWidget::OnButtonClicked()
{
	// Notify the parent selector widget that this item was clicked
	if (OnItemClicked.IsBound() && ItemClass)
	{
		OnItemClicked.Execute(ItemClass);
	}
}

UTexture2D* UWorldBLDItemButtonWidget::GetThumbnailFromClass_Implementation(UClass* InClass)
{
	// Default implementation returns nullptr
	// Override this in subclasses to extract the thumbnail from your specific class type
	return nullptr;
}

FText UWorldBLDItemButtonWidget::GetTooltipTextFromClass_Implementation(UClass* InClass)
{
	// Default implementation returns empty text
	// Override this in subclasses to extract the tooltip from your specific class type
	return FText::GetEmpty();
}

