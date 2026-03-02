#include "WorldBLDRuntimeModule.h"

#include "WorldBLDRuntimeSettings.h"
#include "Modules/ModuleManager.h"

#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"

DEFINE_LOG_CATEGORY(LogWorldBLDRuntime);

IMPLEMENT_MODULE(FWorldBLDRuntimeModule, WorldBLDRuntime)

void FWorldBLDRuntimeModule::StartupModule()
{
	// Force load the settings to make sure one-time setup is complete.
	(void)UWorldBLDRuntimeSettings::Get();
}

void FWorldBLDRuntimeModule::ShutdownModule()
{
	UE_LOG(LogWorldBLDRuntime, Warning, TEXT("WorldBLDRuntime module has shut down"));
}

UWorldBLDRuntimeSettings* UWorldBLDRuntimeSettings::Get()
{
	static bool bLoadedOnce = false;
	UWorldBLDRuntimeSettings* Settings = GetMutableDefault<UWorldBLDRuntimeSettings>();
	//@TODO: This code is looking for the "CityBLD" plugin, we need to set up the WorldBLD config folder
	/*if (!bLoadedOnce)
	{
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CityBLD"));
		FString BasePluginDir = FPaths::GetPath(Plugin->GetContentDir());
		FString ConfigFilePath = FConfigCacheIni::NormalizeConfigIniPath(BasePluginDir / "Config" / "DefaultCityBLD.ini");
		Settings->LoadConfig(nullptr, *ConfigFilePath);

		UGameplayTagsManager::Get().AddTagIniSearchPath(FConfigCacheIni::NormalizeConfigIniPath(BasePluginDir / "Config" / "Tags"));

		bLoadedOnce = true;
	}*/
	return Settings;
}

