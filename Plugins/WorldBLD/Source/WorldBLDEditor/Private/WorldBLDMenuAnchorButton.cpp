// Copyright WorldBLD LLC. All rights reserved.


#include "WorldBLDMenuAnchorButton.h" 
#include "Blueprint/UserWidget.h"
#include "EditorUtilityWidget.h"
#include "Components/OverlaySlot.h"
#include "WorldBLDMenuAnchor.h"

UWorldBLDMenuAnchorButton::UWorldBLDMenuAnchorButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UWorldBLDMenuAnchorButton::RebuildWidget()
{
	TSharedRef<SWidget> ButtonWidget = Super::RebuildWidget();

	if (!IsValid(MenuAnchor))
	{
		MenuAnchor = NewObject<UWorldBLDMenuAnchor>(this, TEXT("MenuAnchor"));		
	}

	if (!OnClicked.IsAlreadyBound(this, &UWorldBLDMenuAnchorButton::HandleOnClicked))
	{
		OnClicked.AddDynamic(this, &UWorldBLDMenuAnchorButton::HandleOnClicked);
	}

	if (IsValid(MenuAnchor))
	{
		if (!MenuAnchor->OnMenuOpenChanged.IsAlreadyBound(this, &UWorldBLDMenuAnchorButton::HandleMenuOpenChanged))
		{
			MenuAnchor->OnMenuOpenChanged.AddDynamic(this, &UWorldBLDMenuAnchorButton::HandleMenuOpenChanged);
		}
		if (!MenuAnchor->OnGetUserMenuContentEvent.IsBound())
		{
			MenuAnchor->OnGetUserMenuContentEvent.BindDynamic(this, &UWorldBLDMenuAnchorButton::HandleGetUserMenuContentEvent);
		}
	}
	return ButtonWidget;
}

 void UWorldBLDMenuAnchorButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (IsValid(MenuAnchor))
	{
		if (!ContentPanel->HasChild(MenuAnchor))
		{
			ContentPanel->AddChild(MenuAnchor);
		}

		MenuAnchor->MenuClass = MenuClass;
		MenuAnchor->Padding = MenuOffset;
		if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(MenuAnchor->Slot))
		{
			OverlaySlot->SetPadding(MenuOffset);
			OverlaySlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
		}
		MenuAnchor->SetPlacement(MenuPlacement);
	}
}

UWidget* UWorldBLDMenuAnchorButton::FindChildByNameRecursive(UWidget* InWidget, FString& Name)
{
	if (InWidget->GetName().Equals(Name))
	{
		return InWidget;
	}
	else if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(InWidget))
	{
		for (UWidget* Child : PanelWidget->GetAllChildren())
		{
			if (UWidget* Widget = FindChildByNameRecursive(Child, Name))
			{
				return Widget;
			}
		}
	}
	return nullptr;
};

void UWorldBLDMenuAnchorButton::HandleOnClicked_Implementation()
{
	if (IsValid(MenuAnchor))
	{
		MenuAnchor->ToggleOpen(true);
	}	
}

UUserWidget* UWorldBLDMenuAnchorButton::HandleGetUserMenuContentEvent_Implementation()
{
	if (IsValid(MenuAnchor) && IsValid(MenuAnchor->MenuClass))
	{
		MenuWidget = CreateWidget<UUserWidget>(this, MenuAnchor->MenuClass);
		
		MenuWidget->SetFlags(RF_Transient);
		TArray<UWidget*> ChildWidgets;
		MenuWidget->WidgetTree->GetAllWidgets(ChildWidgets);
		for (UWidget* Widget : ChildWidgets)
		{
			if (IsValid(Widget) && Widget->IsA(UEditorUtilityWidget::StaticClass()))
			{
				Widget->SetFlags(RF_Transient);
				Widget->Slot->SetFlags(RF_Transient);
			}
		}

		MenuWidget->Modify();

		if (IsValid(MenuContent))
		{
			MenuContent->SetVisibility(ESlateVisibility::Visible); 
			MenuContent->SetIsEnabled(true);
			MenuWidget->SetContentForSlot(ContentSlotName, MenuContent);
		}

		return MenuWidget;
	}
	return nullptr;
}