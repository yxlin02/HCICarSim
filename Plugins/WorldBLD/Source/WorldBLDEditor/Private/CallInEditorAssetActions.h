// Copyright WorldBLD LLC. All rights reserved.

#pragma once

class FCallInEditorAssetActions
{
public:
	// Extends the content browser to expose CallInEditor functions as asset actions.
	static void InstallHooks();
	static void RemoveHooks();
};
