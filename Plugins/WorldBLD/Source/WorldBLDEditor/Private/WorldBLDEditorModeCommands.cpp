// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldBLDEditorModeCommands.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "WorldBLDEditorModeCommands"

FWorldBLDEditorModeCommands::FWorldBLDEditorModeCommands()
	: TCommands<FWorldBLDEditorModeCommands>("WorldBLDEditorMode",
		NSLOCTEXT("WorldBLDEditorMode", "WorldBLDEditorModeCommands", "WorldBLD Editor Mode"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FWorldBLDEditorModeCommands::RegisterCommands()
{
	TArray <TSharedPtr<FUICommandInfo>>& ToolCommands = Commands.FindOrAdd(NAME_Default);

	UI_COMMAND(SimpleTool, "Show Actor Info", "Opens message box with info about a clicked actor", EUserInterfaceActionType::Button, FInputChord());
	ToolCommands.Add(SimpleTool);

	UI_COMMAND(InteractiveTool, "Measure Distance", "Measures distance between 2 points (click to set origin, shift-click to set end point)", EUserInterfaceActionType::ToggleButton, FInputChord());
	ToolCommands.Add(InteractiveTool);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> FWorldBLDEditorModeCommands::GetCommands()
{
	return FWorldBLDEditorModeCommands::Get().Commands;
}

#undef LOCTEXT_NAMESPACE
