// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "WorldBLDKitBase.h"
#include "Utilities/UtilsEditorContext_Manager.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
DECLARE_MULTICAST_DELEGATE(FOnWorldBLDKitPathsChanged);
DECLARE_MULTICAST_DELEGATE(FOnWorldBLDLevelTemplateBundlesChanged);

WORLDBLDEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogWorldBuild, Log, All);

class FMenuBarBuilder;
class FMenuBuilder;
class UWorldBLDLevelTemplateBundle;
class UWorldBLDContextMenuRegistry;
class AActor;
class SLevelViewport;
class SWidget;

/**
 * This is the module definition for the editor mode. You can implement custom functionality
 * as your plugin module starts up and shuts down. See IModuleInterface for more extensibility options.
 */
class FWorldBLDEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FString GetModuleName();

	void ScanContentForKitBundles();
	void ScanContentForLevelTemplateBundles();

	const TArray<TWeakObjectPtr<UWorldBLDLevelTemplateBundle>>& GetAvailableLevelTemplateBundles() const { return AvailableLevelTemplateBundles; }

	FOnWorldBLDKitPathsChanged OnAvailableWorldBLDKitBundlesChanged;
	TArray<TWeakObjectPtr<UWorldBLDKitBundle>> AvailableWorldBLDKitBundles;

	FOnWorldBLDLevelTemplateBundlesChanged OnAvailableLevelTemplateBundlesChanged;
	TArray<TWeakObjectPtr<UWorldBLDLevelTemplateBundle>> AvailableLevelTemplateBundles;

	virtual UUtilsEditorContext_Manager& GetContextManager() const { return *ContextManagerPtr;};
	UWorldBLDContextMenuRegistry* GetContextMenuRegistry();

	/** Debug/testing helper: shows a WorldBLD context menu in the active level viewport overlay. */
	void CreateAndShowContextMenu(AActor* Actor);

private:
	void HandleTestContextMenuCommand();

	/** Debug-only: toggled by the WorldBLD.TestContextMenu console command. */
	TWeakPtr<SLevelViewport> TestContextMenuHostViewport;
	TSharedPtr<SWidget> TestContextMenuOverlayWidget;

	TWeakObjectPtr<class UUtilsEditorContext_Manager> ContextManagerPtr;
	TWeakObjectPtr<UWorldBLDContextMenuRegistry> ContextMenuRegistryPtr;
	static TSharedPtr<class FSlateStyleSet> InstalledStyle;
	void OnPluginMounted(class IPlugin& PluginRef);
	void OnPluginUnmounted(class IPlugin& PluginRef);
	void HandleLevelTemplateSelected(UWorldBLDLevelTemplateBundle* Bundle);

	void ExtendLevelEditorMenu();
	void ExtendFileMenu();
	void AddWorldBLDMenuBarExtension(FMenuBarBuilder& MenuBarBuilder);
	void PopulateWorldBLDMenu(FMenuBuilder& MenuBuilder);
	void AddLevelTemplateFileMenuItem(FMenuBuilder& MenuBuilder);

	TSharedPtr<FExtender> AssetLibraryMenuExtender;
	TSharedPtr<FExtender> FileMenuExtender;

	FDelegateHandle AssetRegistryFilesLoadedHandle;
	FDelegateHandle AssetRegistryLevelTemplatesFilesLoadedHandle;
	FDelegateHandle LevelTemplateSelectedHandle;
	FDelegateHandle PluginMountedHandle;
	FDelegateHandle PluginUnmountedHandle;
};
