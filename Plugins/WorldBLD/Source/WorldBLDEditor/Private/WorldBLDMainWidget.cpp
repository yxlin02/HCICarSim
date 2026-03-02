// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDMainWidget.h"

#include "WorldBLDEditorToolkit.h"
#include "WorldBLDToolPaletteWidget.h"
#include "WorldBLDToolButtonWidget.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

void UWorldBLDMainWidget::SetTools(TArray<UWorldBLDToolPaletteWidget*> Tools)
{
	TabButtonContainer->ClearChildren();
	MainWidgetSwitcher->ClearChildren();

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!IsValid(World))
	{
		return;
	}

	for (UWorldBLDToolPaletteWidget* Tool : Tools)
	{
		if (!IsValid(Tool))
		{
			continue;
		}

		FLocalWidgetSet& WidgetSet = AssignedWidgets.AddDefaulted_GetRef();
		if (IsValid(ButtonWidgetClass))
		{
			UWorldBLDToolButtonWidget* NewButton = CreateWidget<UWorldBLDToolButtonWidget>(World, ButtonWidgetClass);
			if (IsValid(NewButton))
			{
				NewButton->OptionalId = Tool->ToolId;
				NewButton->SetIcon(Tool->ToolTabIcon.LoadSynchronous());
				NewButton->SetCustomTooltip(Tool->ToolDisplayName);
				NewButton->OnClicked.AddUniqueDynamic(this, &UWorldBLDMainWidget::OnButtonClicked);
				int32 ChildrenCount = TabButtonContainer->GetChildrenCount();
				int32 InsertIdx = FMath::Min(0, ChildrenCount - 1);
				UPanelSlot* PanelSlot = TabButtonContainer->InsertChildAt(InsertIdx, NewButton);				
				WidgetSet.ButtonContent = NewButton;
			}
		}

		WidgetSet.UniqueId = Tool->ToolId;
		WidgetSet.BodyContent = Tool;
		MainWidgetSwitcher->AddChild(Tool);
	}

	OnToolsSet();
	SetActiveTool(ActiveTool);
}

void UWorldBLDMainWidget::OnButtonClicked(UWidget* Widget)
{
	if (const UWorldBLDToolButtonWidget* ToolButton = Cast<UWorldBLDToolButtonWidget>(Widget))
	{
		SetActiveTool(ToolButton->OptionalId);
	}
}

void UWorldBLDMainWidget::OnLoadedWorldBLDKitsChanged(UWorldBLDEdMode* InEdMode)
{
	if (!IsValid(EdMode))
	{
		return;
	}

	// Update the enabled state of various buttons and widgets.
	const bool bAtLeastOneKitLoaded = (EdMode->GetLoadedWorldBLDKitCount() > 0);
	for (const FLocalWidgetSet& WidgetSet : AssignedWidgets)
	{
		if (WidgetSet.BodyContent.IsValid() && WidgetSet.ButtonContent.IsValid())
		{
			bool bShouldBeEnabled = !WidgetSet.BodyContent->bRequiresLoadedWorldBLDKits || 
					(WidgetSet.BodyContent->bRequiresLoadedWorldBLDKits && bAtLeastOneKitLoaded);

			// If no Kit is relevant to this tool palette, then disable it.
			int32 SupportedKits = 0;
			for (AWorldBLDKitBase* WorldBLDKit : EdMode->GetLoadedWorldBLDKits())
			{
				if (WidgetSet.BodyContent->IsToolPaletteSupportedByWorldBLDKit(WorldBLDKit))
				{
					SupportedKits += 1;
				}
			}
			bool bSetEnabled = bShouldBeEnabled && (WidgetSet.BodyContent->bIsAlwaysEnabled || (SupportedKits > 0));
			
			WidgetSet.ButtonContent->SetEnabled(bShouldBeEnabled && bSetEnabled);
		}
	}
}

void UWorldBLDMainWidget::MakeFirstEnabledPaletteActive()
{
	for (const FLocalWidgetSet& WidgetSet : AssignedWidgets)
	{
		if (WidgetSet.BodyContent.IsValid() && WidgetSet.ButtonContent.IsValid())
		{
			if (WidgetSet.ButtonContent->bButtonIsEnabled)
			{
				SetActiveTool(WidgetSet.UniqueId);
				break;
			}
		}
	}
}

void UWorldBLDMainWidget::SetActiveTool(const FName& NewActiveTool)
{
	if (IsValid(EdMode) && !EdMode->IsThisEdModeActive())
	{
		// Don't allow tool switching if we aren't presented on screen.
		return;
	}

	for (int32 Idx = 0; Idx < AssignedWidgets.Num(); Idx += 1)
	{
		const FLocalWidgetSet& WidgetSet = AssignedWidgets[Idx];
		UWorldBLDToolPaletteWidget* Tool = WidgetSet.BodyContent.Get();
		if (IsValid(Tool) && (ActiveTool == WidgetSet.UniqueId))
		{
			Tool->ToolPaletteDeactivate();
		}

		if (WidgetSet.UniqueId == NewActiveTool)
		{
			MainWidgetSwitcher->SetActiveWidget(Tool);
			if (WidgetSet.ButtonContent.IsValid())
			{
				WidgetSet.ButtonContent->SetHighlighted(true);
			}
			NewToolTabSelected(Tool);
			if (IsValid(Tool))
			{
				Tool->ToolPaletteActivate();
			}
		}
		else
		{
			if (WidgetSet.ButtonContent.IsValid())
			{
				WidgetSet.ButtonContent->SetHighlighted(false);
			}
		}
	}

	ActiveTool = NewActiveTool;
}

UWorldBLDToolPaletteWidget* UWorldBLDMainWidget::GetActiveTool() const
{
	for (int32 Idx = 0; Idx < AssignedWidgets.Num(); Idx += 1)
	{
		const FLocalWidgetSet& WidgetSet = AssignedWidgets[Idx];
		UWorldBLDToolPaletteWidget* Tool = WidgetSet.BodyContent.Get();
		if (IsValid(Tool) && (ActiveTool == WidgetSet.UniqueId))
		{
			return Tool;
		}
	}

	return nullptr;
}

FName UWorldBLDMainWidget::GetFirstToolName() const
{
	return (AssignedWidgets.Num() > 0) ? AssignedWidgets[0].UniqueId : NAME_None;
}

void UWorldBLDMainWidget::ResetContent()
{
	for (int32 Idx = 0; Idx < AssignedWidgets.Num(); Idx += 1)
	{
		const FLocalWidgetSet& WidgetSet = AssignedWidgets[Idx];
		if (WidgetSet.ButtonContent.IsValid())
		{
			WidgetSet.ButtonContent->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty);
		}
		if (WidgetSet.BodyContent.IsValid())
		{
			WidgetSet.BodyContent->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty);
		}
	}

	AssignedWidgets.Empty();
}

void UWorldBLDMainWidget::ResetToDefaultController()
{
	if (IsValid(EdMode))
	{
		EdMode->ClearToolModeStack();
		EdMode->InstallDefaultSelectionTool();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
