#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules\ModuleManager.h"


DECLARE_LOG_CATEGORY_EXTERN(LogWorldBLDRuntime, Log, All);

class FWorldBLDRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};