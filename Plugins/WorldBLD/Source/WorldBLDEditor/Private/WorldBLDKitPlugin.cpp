// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDKitPlugin.h"

#include "WorldBLDKitBase.h"
#include "WorldBLDEditorModule.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<IPlugin> FWorldBLDKitPluginReference::GetPlugin()
{
	if (TSharedPtr<IPlugin> Plugin = PluginHandle.Pin())
	{
		return Plugin;
	}
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(*PluginName);
	PluginHandle = Plugin;
	if (Plugin.IsValid())
	{
		ProcessPlugin(Plugin);
	}
	return Plugin;
}

TSharedPtr<IPlugin> FWorldBLDKitPluginReference::GetPlugin() const
{
	return PluginHandle.Pin();
}

FString FWorldBLDKitPluginReference::GetPluginWorldBLDKitPath(TSharedPtr<IPlugin> Plugin)
{
	const FString BasePluginDir = FPaths::GetPath(Plugin->GetContentDir());
	return FConfigCacheIni::NormalizeConfigIniPath(BasePluginDir / "Config" / "WorldBLDKit.ini");
}

bool FWorldBLDKitPluginReference::PluginIsWorldBLDKit(TSharedPtr<IPlugin> Plugin)
{
	return Plugin && FPaths::FileExists(GetPluginWorldBLDKitPath(Plugin));
}

void FWorldBLDKitPluginReference::ProcessPlugin(TSharedPtr<IPlugin> Plugin)
{
	PluginHandle = Plugin;
	PluginName = Plugin->GetName();
	MountedContentRoot = Plugin->GetMountedAssetPath();

	UWorldBLDKitPluginSettings* LocalSettings = NewObject<UWorldBLDKitPluginSettings>();
	LocalSettings->LoadConfig(nullptr, *GetPluginWorldBLDKitPath(Plugin));
	WorldBLDKitBundle = LocalSettings->WorldBLDKitBundle.LoadSynchronous();
	LocalSettings->ConditionalBeginDestroy();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TArray<FWorldBLDKitPluginReference> UWorldBLDKitPluginUtils::GetAllWorldBLDKitPlugins()
{
	TArray<FWorldBLDKitPluginReference> PluginList;

	for (const TSharedPtr<IPlugin> PluginPtr : IPluginManager::Get().GetEnabledPluginsWithContent())
	{
		FString BasePluginDir = FPaths::GetPath(PluginPtr->GetContentDir());
		FString ConfigFilePath = FConfigCacheIni::NormalizeConfigIniPath(BasePluginDir / "Config" / "WorldBLDKit.ini");
		if (FPaths::FileExists(ConfigFilePath))
		{
			FWorldBLDKitPluginReference& PluginData = PluginList.AddDefaulted_GetRef();
			PluginData.ProcessPlugin(PluginPtr);
		}
	}

	return PluginList;
}

FString UWorldBLDKitPluginUtils::GetWorldBLDVersionString()
{
	FString Version = TEXT("0.0.0");
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().GetModuleOwnerPlugin(*FWorldBLDEditorModule::GetModuleName()))
	{
		Version = Plugin->GetDescriptor().VersionName;
	}
	return Version;
}

void UWorldBLDKitPluginUtils::EnsureWorldBLDKitPluginLoaded(FWorldBLDKitPluginReference& PluginRef)
{
	(void)PluginRef.GetPlugin();
}

FString UWorldBLDKitPluginUtils::GetWorldBLDKitPluginVersionString(const FWorldBLDKitPluginReference& PluginRef)
{
	FString Version = TEXT("0.0.0");
	if (const TSharedPtr<IPlugin> Plugin = PluginRef.GetPlugin())
	{
		Version = Plugin->GetDescriptor().VersionName;
	}
	return Version;
}

bool UWorldBLDKitPluginUtils::FindWorldBLDKitPluginForBundle(UWorldBLDKitBundle* Bundle, FWorldBLDKitPluginReference& OutPluginRef)
{
	bool bFound = false;
	OutPluginRef = FWorldBLDKitPluginReference();
	if (IsValid(Bundle))
	{
		for (const FWorldBLDKitPluginReference& Ref : GetAllWorldBLDKitPlugins())
		{
			if (IsValid(Ref.WorldBLDKitBundle) && Ref.WorldBLDKitBundle == Bundle)
			{
				OutPluginRef = Ref;
				bFound = true;
				break;
			}
		}
	}
	return bFound;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
