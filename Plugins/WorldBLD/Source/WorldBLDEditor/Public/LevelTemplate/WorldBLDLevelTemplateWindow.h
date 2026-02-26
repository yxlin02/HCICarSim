#pragma once

#include "CoreMinimal.h"

class UWorldBLDLevelTemplateBundle;

class SWindow;

class WORLDBLDEDITOR_API FWorldBLDLevelTemplateWindow
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldBLDLevelTemplateSelected, UWorldBLDLevelTemplateBundle*);

	static void OpenLevelTemplateWindow();
	static void CloseLevelTemplateWindow();
	static FOnWorldBLDLevelTemplateSelected& OnLevelTemplateSelected();

private:
	static TWeakPtr<SWindow> LevelTemplateWindow;
	static FOnWorldBLDLevelTemplateSelected LevelTemplateSelected;
};
