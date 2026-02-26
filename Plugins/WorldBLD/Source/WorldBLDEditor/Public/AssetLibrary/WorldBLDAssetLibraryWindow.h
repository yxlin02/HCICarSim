// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SWindow;

class WORLDBLDEDITOR_API FWorldBLDAssetLibraryWindow
{
public:
	static void OpenAssetLibrary();
	static void CloseAssetLibrary();

private:
	static TWeakPtr<SWindow> AssetLibraryWindow;
};
