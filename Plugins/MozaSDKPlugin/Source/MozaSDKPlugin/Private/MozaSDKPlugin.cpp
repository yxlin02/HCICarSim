#include "MozaSDKPlugin.h" 
#include "Interfaces/IPluginManager.h" 
#include "HAL/PlatformProcess.h" 
#include "Misc/Paths.h" 
#include "mozaAPI.h" 

#define LOCTEXT_NAMESPACE "FMozaSDKPluginModule" 

void FMozaSDKPluginModule::StartupModule()

{
    if (!bMozaInitialized)

    {
        moza::installMozaSDK();
        bMozaInitialized = true;
    }
}

void FMozaSDKPluginModule::ShutdownModule()

{
    if (bMozaInitialized)

    {
        moza::removeMozaSDK();
        bMozaInitialized = false;
    }
}







#undef LOCTEXT_NAMESPACE 



IMPLEMENT_MODULE(FMozaSDKPluginModule, MozaSDKPlugin)