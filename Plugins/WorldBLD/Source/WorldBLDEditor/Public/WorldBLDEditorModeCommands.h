// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

/**
 * This class contains info about the full set of commands used in this editor mode.
 */
class FWorldBLDEditorModeCommands : public TCommands<FWorldBLDEditorModeCommands>
{
public:
	FWorldBLDEditorModeCommands();

	virtual void RegisterCommands() override;
	static TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetCommands();

	TSharedPtr<FUICommandInfo> SimpleTool;
	TSharedPtr<FUICommandInfo> InteractiveTool;

protected:
	TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Commands;
};
