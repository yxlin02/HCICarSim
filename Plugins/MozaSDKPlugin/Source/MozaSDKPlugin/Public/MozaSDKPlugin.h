// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"




class FMozaSDKPluginModule : public IModuleInterface
{


public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:

    bool bMozaInitialized = false;
    
    
};

