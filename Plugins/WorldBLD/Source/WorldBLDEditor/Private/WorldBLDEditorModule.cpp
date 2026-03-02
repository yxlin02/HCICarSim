// Copyright WorldBLD LLC. All Rights Reserved.

#include "WorldBLDEditorModule.h"
#include "WorldBLDEditorModeCommands.h"
#include "WorldBLDContextMenuRegistry.h"

#include "ContextMenu/SWorldBLDContextMenu.h"
#include "ContextMenu/SWorldBLDContextMenuViewportOverlay.h"
#include "IWorldBLDContextMenuFactory.h"

#include "Interfaces/IPluginManager.h"

#include "WorldBLDEditorToolkit.h"
#include "WorldBLDKitBase.h"
#include "CallInEditorAssetActions.h"
#include "WorldBLDKitPlugin.h"
#include "AssetLibrary/WorldBLDAssetLibraryWindow.h"

#include "LevelTemplate/SWorldBLDLightingPresetDialog.h"
#include "LevelTemplate/WorldBLDLevelTemplateWindow.h"

#include "WorldBLDLevelTemplateBlueprintLibrary.h"
#include "WorldBLDLevelTemplateBundle.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "LevelEditor.h"
#include "LevelEditorSubsystem.h"
#include "SLevelViewport.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "FileHelpers.h"
#include "ScopedTransaction.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY(LogWorldBuild)

#define LOCTEXT_NAMESPACE "WorldBLDEditorModule"

void FWorldBLDEditorModule::StartupModule()
{
	FWorldBLDEditorModeCommands::Register();
	ExtendLevelEditorMenu();
	ExtendFileMenu();

	LevelTemplateSelectedHandle = FWorldBLDLevelTemplateWindow::OnLevelTemplateSelected().AddRaw(this, &FWorldBLDEditorModule::HandleLevelTemplateSelected);

	PluginMountedHandle = IPluginManager::Get().OnNewPluginMounted().AddRaw(this, &FWorldBLDEditorModule::OnPluginMounted);
	PluginUnmountedHandle = IPluginManager::Get().OnPluginUnmounted().AddRaw(this, &FWorldBLDEditorModule::OnPluginUnmounted);

	FCallInEditorAssetActions::InstallHooks();

	ContextManagerPtr = MakeWeakObjectPtr(const_cast<UUtilsEditorContext_Manager*>(GetDefault<UUtilsEditorContext_Manager>()));
	ContextMenuRegistryPtr = UWorldBLDContextMenuRegistry::Get();
	// Context menu window manager is stateless, no initialization needed
	UE_LOG(LogWorldBuild, Log, TEXT("Context menu registry initialized"));
	for (auto PluginPtr : IPluginManager::Get().GetEnabledPluginsWithContent())
	{
		OnPluginMounted(*PluginPtr);
	}

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("WorldBLD.TestContextMenu"),
		TEXT("Spawn a WorldBLD context menu for the currently selected actor."),
		FConsoleCommandDelegate::CreateRaw(this, &FWorldBLDEditorModule::HandleTestContextMenuCommand),
		ECVF_Default);

	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
		if (!AssetRegistry.IsLoadingAssets())
		{
			ScanContentForKitBundles();
			ScanContentForLevelTemplateBundles();
		}
		else
		{
			AssetRegistryFilesLoadedHandle = AssetRegistry.OnFilesLoaded().AddRaw(this, &FWorldBLDEditorModule::ScanContentForKitBundles);
			AssetRegistryLevelTemplatesFilesLoadedHandle = AssetRegistry.OnFilesLoaded().AddRaw(this, &FWorldBLDEditorModule::ScanContentForLevelTemplateBundles);
		}
	}

	//@TODO: This code is looking for the WorldBLD plugin to find the edmode class, we need to set up the WorldBLD plugin's content folder
	if (UWorldBLDEdModeSettings* Settings = GetMutableDefault<UWorldBLDEdModeSettings>())
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("WorldBLD"));
		const FString BasePluginDir = FPaths::GetPath(Plugin->GetContentDir());
		const FString ConfigFilePath = FConfigCacheIni::NormalizeConfigIniPath(BasePluginDir / "Config" / "DefaultWorldBLD.ini");
		Settings->LoadConfig(nullptr, *ConfigFilePath);

		if (!Settings->EditModeClass.IsNull())
		{
			if (const TSubclassOf<UWorldBLDEdMode> EdModeClass = Settings->EditModeClass.LoadSynchronous())
			{
				// Make sure we handle gracefully recompiling this blueprint so we don't crash the editor.
				if (UWorldBLDEdMode* CDO = Cast<UWorldBLDEdMode>(EdModeClass->GetDefaultObject()))
				{
					CDO->InstallDefaultRecompileHandler();
				}
			}
		}
	}
}

void FWorldBLDEditorModule::ShutdownModule()
{
	FCallInEditorAssetActions::RemoveHooks();
	FWorldBLDEditorModeCommands::Unregister();

	IConsoleManager::Get().UnregisterConsoleObject(TEXT("WorldBLD.TestContextMenu"), false);

	if (LevelTemplateSelectedHandle.IsValid())
	{
		FWorldBLDLevelTemplateWindow::OnLevelTemplateSelected().Remove(LevelTemplateSelectedHandle);
		LevelTemplateSelectedHandle.Reset();
	}

	if (PluginMountedHandle.IsValid())
	{
		IPluginManager::Get().OnNewPluginMounted().Remove(PluginMountedHandle);
		PluginMountedHandle.Reset();
	}

	if (PluginUnmountedHandle.IsValid())
	{
		IPluginManager::Get().OnPluginUnmounted().Remove(PluginUnmountedHandle);
		PluginUnmountedHandle.Reset();
	}

	if (AssetLibraryMenuExtender.IsValid() && FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender(AssetLibraryMenuExtender);
		AssetLibraryMenuExtender.Reset();
	}

	if (FileMenuExtender.IsValid() && FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender(FileMenuExtender);
		FileMenuExtender.Reset();
	}

	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnFilesLoaded().Remove(AssetRegistryFilesLoadedHandle);
		AssetRegistryModule.Get().OnFilesLoaded().Remove(AssetRegistryLevelTemplatesFilesLoadedHandle);
	}

	ContextMenuRegistryPtr = nullptr;
	ContextManagerPtr = nullptr;
}

UWorldBLDContextMenuRegistry* FWorldBLDEditorModule::GetContextMenuRegistry()
{
	if (!ContextMenuRegistryPtr.IsValid())
	{
		ContextMenuRegistryPtr = UWorldBLDContextMenuRegistry::Get();
	}

	return ContextMenuRegistryPtr.Get();
}

void FWorldBLDEditorModule::CreateAndShowContextMenu(AActor* Actor)
{
	if (!GEditor)
	{
		return;
	}

	// Clear previous test overlay (if any).
	if (TestContextMenuOverlayWidget.IsValid())
	{
		if (TSharedPtr<SLevelViewport> PinnedHostViewport = TestContextMenuHostViewport.Pin())
		{
			PinnedHostViewport->RemoveOverlayWidget(TestContextMenuOverlayWidget.ToSharedRef());
		}
		TestContextMenuOverlayWidget.Reset();
		TestContextMenuHostViewport.Reset();
	}

	AActor* Target = Actor;
	if (!IsValid(Target))
	{
		USelection* SelectedActors = GEditor->GetSelectedActors();
		if (SelectedActors)
		{
			for (FSelectionIterator It(*SelectedActors); It; ++It)
			{
				Target = Cast<AActor>(*It);
				if (IsValid(Target))
				{
					break;
				}
			}
		}
	}

	if (!IsValid(Target))
	{
		return;
	}

	UWorldBLDContextMenuRegistry* Registry = GetContextMenuRegistry();
	if (!IsValid(Registry))
	{
		return;
	}

	const TScriptInterface<IWorldBLDContextMenuFactory> Factory = Registry->FindFactoryForActor(Target);
	if (!Factory)
	{
		return;
	}

	if (!Factory->CanCreateContextMenuForActor(Target))
	{
		return;
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	const TSharedPtr<ILevelEditor> PinnedEditor = LevelEditorModule.GetLevelEditorInstance().Pin();
	if (!PinnedEditor.IsValid())
	{
		return;
	}

	const TSharedPtr<SLevelViewport> ActiveViewport = PinnedEditor->GetActiveViewportInterface();
	if (!ActiveViewport.IsValid())
	{
		return;
	}

	const TSharedRef<SWidget> MenuContent = Factory->CreateContextMenuWidget(Target);
	const FText Title = FText::FromString(Target->GetActorLabel());

	const TSharedRef<SWorldBLDContextMenu> ContextMenu =
		SNew(SWorldBLDContextMenu)
		.Title(Title)
		.Content(MenuContent)
		.TargetActor(Target);

	const TSharedRef<SWorldBLDContextMenuViewportOverlay> Overlay =
		SNew(SWorldBLDContextMenuViewportOverlay)
		.Padding(FMargin(24.0f))
		.Panel(ContextMenu);

	ActiveViewport->AddOverlayWidget(Overlay);
	TestContextMenuHostViewport = ActiveViewport;
	TestContextMenuOverlayWidget = Overlay;
}

void FWorldBLDEditorModule::HandleTestContextMenuCommand()
{
	if (TestContextMenuOverlayWidget.IsValid())
	{
		if (TSharedPtr<SLevelViewport> PinnedHostViewport = TestContextMenuHostViewport.Pin())
		{
			PinnedHostViewport->RemoveOverlayWidget(TestContextMenuOverlayWidget.ToSharedRef());
		}
		TestContextMenuOverlayWidget.Reset();
		TestContextMenuHostViewport.Reset();
		return;
	}

	CreateAndShowContextMenu(nullptr);
}

void FWorldBLDEditorModule::HandleLevelTemplateSelected(UWorldBLDLevelTemplateBundle* Bundle)
{
	if (!IsValid(Bundle))
	{
		return;
	}

	auto Notify = [](const FText& Message, SNotificationItem::ECompletionState State)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 5.0f;
		Info.bFireAndForget = true;
		TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid())
		{
			Item->SetCompletionState(State);
		}
	};

	TArray<FString> PresetNames;
	Bundle->CompatibleLightingPresets.GetKeys(PresetNames);
	PresetNames.Sort();

	TOptional<FString> SelectedPresetName;
	if (PresetNames.Num() > 0)
	{
		SelectedPresetName = SWorldBLDLightingPresetDialog::ShowModal(PresetNames, Bundle->DefaultLightingTemplate);
		if (!SelectedPresetName.IsSet())
		{
			return;
		}
	}

	if (Bundle->TemplateLevel.IsNull())
	{
		Notify(LOCTEXT("LevelTemplate_MissingTemplate", "Template level is not set."), SNotificationItem::CS_Fail);
		return;
	}

	const FSoftObjectPath TemplateWorldPath = Bundle->TemplateLevel.ToSoftObjectPath();
	if (!TemplateWorldPath.IsValid())
	{
		Notify(LOCTEXT("LevelTemplate_InvalidTemplatePath", "Template level path is invalid."), SNotificationItem::CS_Fail);
		return;
	}

	TSoftObjectPtr<UWorld> PresetWorld;
	if (SelectedPresetName.IsSet())
	{
		const TSoftObjectPtr<UWorld>* PresetWorldPtr = Bundle->CompatibleLightingPresets.Find(*SelectedPresetName);
		if (!PresetWorldPtr)
		{
			Notify(LOCTEXT("LevelTemplate_PresetMissing", "Selected lighting preset is missing."), SNotificationItem::CS_Fail);
			return;
		}

		PresetWorld = *PresetWorldPtr;
		if (PresetWorld.IsNull() || !PresetWorld.ToSoftObjectPath().IsValid())
		{
			Notify(LOCTEXT("LevelTemplate_InvalidPresetPath", "Lighting preset level path is invalid."), SNotificationItem::CS_Fail);
			return;
		}
	}

	if (!GEditor)
	{
		Notify(LOCTEXT("LevelTemplate_NoEditor", "Editor is not available."), SNotificationItem::CS_Fail);
		return;
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("LevelTemplate_SaveNewLevel", "Create Level from Template");
	SaveAssetDialogConfig.DefaultPath = TEXT("/Game");
	SaveAssetDialogConfig.DefaultAssetName = Bundle->TemplateName.IsEmpty() ? TEXT("NewLevel") : Bundle->TemplateName;
	SaveAssetDialogConfig.AssetClassNames.Add(UWorld::StaticClass()->GetClassPathName());

	const FString NewLevelObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
	if (NewLevelObjectPath.IsEmpty())
	{
		return;
	}

	const FString NewLevelPackagePath = FPackageName::ObjectPathToPackageName(NewLevelObjectPath);
	const FString TemplateLevelPackagePath = TemplateWorldPath.GetLongPackageName();
	if (TemplateLevelPackagePath.IsEmpty())
	{
		Notify(LOCTEXT("LevelTemplate_InvalidTemplatePath", "Template level path is invalid."), SNotificationItem::CS_Fail);
		return;
	}

	bool bCreatedNewLevel = false;
	if (ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
	{
		bCreatedNewLevel = LevelEditorSubsystem->NewLevelFromTemplate(NewLevelPackagePath, TemplateLevelPackagePath);
	}

	if (!bCreatedNewLevel)
	{
		Notify(LOCTEXT("LevelTemplate_NewLevelFailed", "Failed to create new level from template."), SNotificationItem::CS_Fail);
		return;
	}

	UWorld* NewWorld = GEditor->GetEditorWorldContext().World();
	if (!IsValid(NewWorld))
	{
		Notify(LOCTEXT("LevelTemplate_NewWorldInvalid", "New level was created but could not be loaded."), SNotificationItem::CS_Fail);
		return;
	}

	if (!PresetWorld.IsNull())
	{
		UWorldBLDLevelTemplateBlueprintLibrary::LoadLightingPresetFromSoftWorld(NewWorld, PresetWorld);
		NewWorld->MarkPackageDirty();
	}

	Notify(LOCTEXT("LevelTemplate_Success", "Created level from template."), SNotificationItem::CS_Success);
	FWorldBLDLevelTemplateWindow::CloseLevelTemplateWindow();
}

void FWorldBLDEditorModule::OnPluginMounted(IPlugin& PluginRef)
{
	const int32 PreviousCount = AvailableWorldBLDKitBundles.Num();

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(*PluginRef.GetName());
	if (FWorldBLDKitPluginReference::PluginIsWorldBLDKit(Plugin))
	{
		FWorldBLDKitPluginReference PluginData;
		PluginData.ProcessPlugin(Plugin);
		if (IsValid(PluginData.WorldBLDKitBundle))
		{
			AvailableWorldBLDKitBundles.AddUnique(PluginData.WorldBLDKitBundle);
		}
	}

	if (AvailableWorldBLDKitBundles.Num() != PreviousCount)
	{
		OnAvailableWorldBLDKitBundlesChanged.Broadcast();
	}

	ScanContentForKitBundles();
	ScanContentForLevelTemplateBundles();
}

void FWorldBLDEditorModule::OnPluginUnmounted(IPlugin& PluginRef)
{
	const int32 PreviousCount = AvailableWorldBLDKitBundles.Num();

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(*PluginRef.GetName());
	if (FWorldBLDKitPluginReference::PluginIsWorldBLDKit(Plugin))
	{
		FWorldBLDKitPluginReference PluginData;
		PluginData.ProcessPlugin(Plugin);
		AvailableWorldBLDKitBundles.Remove(PluginData.WorldBLDKitBundle);
	}

	if (AvailableWorldBLDKitBundles.Num() != PreviousCount)
	{
		OnAvailableWorldBLDKitBundlesChanged.Broadcast();
	}

	ScanContentForKitBundles();
	ScanContentForLevelTemplateBundles();
}

void FWorldBLDEditorModule::ScanContentForKitBundles()
{
	for (int32 Index = AvailableWorldBLDKitBundles.Num() - 1; Index >= 0; --Index)
	{
		if (!AvailableWorldBLDKitBundles[Index].IsValid())
		{
			AvailableWorldBLDKitBundles.RemoveAt(Index);
		}
	}

	const int32 PreviousCount = AvailableWorldBLDKitBundles.Num();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	if (AssetRegistry.IsLoadingAssets())
	{
		return;
	}

	TArray<FString> PathsToScan;
	AssetRegistry.GetAllCachedPaths(PathsToScan);
	UE_LOG(LogWorldBuild, Log, TEXT("ScanContentForKitBundles: CachedPaths=%d"), PathsToScan.Num());
	if (!PathsToScan.IsEmpty())
	{
		AssetRegistry.ScanPathsSynchronous(PathsToScan, true);
	}

	FARFilter Filter;
	Filter.ClassPaths.Add(UWorldBLDKitBundle::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);
	UE_LOG(LogWorldBuild, Log, TEXT("ScanContentForKitBundles: FoundAssets=%d"), AssetDataList.Num());

	for (const FAssetData& AssetData : AssetDataList)
	{
		UWorldBLDKitBundle* KitBundle = Cast<UWorldBLDKitBundle>(AssetData.GetAsset());
		if (IsValid(KitBundle))
		{
			AvailableWorldBLDKitBundles.AddUnique(KitBundle);
		}
	}

	for (int32 Index = 0; Index < FMath::Min(AssetDataList.Num(), 10); ++Index)
	{
		UE_LOG(LogWorldBuild, Log, TEXT("ScanContentForKitBundles: Asset[%d]=%s"), Index, *AssetDataList[Index].GetObjectPathString());
	}

	if (AvailableWorldBLDKitBundles.Num() != PreviousCount)
	{
		OnAvailableWorldBLDKitBundlesChanged.Broadcast();
	}
}

void FWorldBLDEditorModule::ScanContentForLevelTemplateBundles()
{
    for (int32 Index = AvailableLevelTemplateBundles.Num() - 1; Index >= 0; --Index)
    {
        if (!AvailableLevelTemplateBundles[Index].IsValid())
        {
            AvailableLevelTemplateBundles.RemoveAt(Index);
        }
    }

    const int32 PreviousCount = AvailableLevelTemplateBundles.Num();

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    if (AssetRegistry.IsLoadingAssets())
    {
        return;
    }

    TArray<FString> PathsToScan;
    AssetRegistry.GetAllCachedPaths(PathsToScan);
    UE_LOG(LogWorldBuild, Log, TEXT("ScanContentForLevelTemplateBundles: CachedPaths=%d"), PathsToScan.Num());
    if (!PathsToScan.IsEmpty())
    {
        AssetRegistry.ScanPathsSynchronous(PathsToScan, true);
    }

    FARFilter Filter;
    Filter.ClassPaths.Add(UWorldBLDLevelTemplateBundle::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);
    UE_LOG(LogWorldBuild, Log, TEXT("ScanContentForLevelTemplateBundles: FoundAssets=%d"), AssetDataList.Num());

    for (const FAssetData& AssetData : AssetDataList)
    {
        UWorldBLDLevelTemplateBundle* LevelTemplateBundle = Cast<UWorldBLDLevelTemplateBundle>(AssetData.GetAsset());
        if (IsValid(LevelTemplateBundle))
        {
            AvailableLevelTemplateBundles.AddUnique(LevelTemplateBundle);
        }
    }

    for (int32 Index = 0; Index < FMath::Min(AssetDataList.Num(), 10); ++Index)
    {
        UE_LOG(LogWorldBuild, Log, TEXT("ScanContentForLevelTemplateBundles: Asset[%d]=%s"), Index, *AssetDataList[Index].GetObjectPathString());
    }

    if (AvailableLevelTemplateBundles.Num() != PreviousCount)
    {
        OnAvailableLevelTemplateBundlesChanged.Broadcast();
    }
}

FString FWorldBLDEditorModule::GetModuleName()
{
    return TEXT("WorldBLDEditor");
}

void FWorldBLDEditorModule::ExtendLevelEditorMenu()
{
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

    AssetLibraryMenuExtender = MakeShared<FExtender>();
    AssetLibraryMenuExtender->AddMenuBarExtension(
        "Window",
        EExtensionHook::After,
        nullptr,
        FMenuBarExtensionDelegate::CreateRaw(this, &FWorldBLDEditorModule::AddWorldBLDMenuBarExtension));

    LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(AssetLibraryMenuExtender);
}

void FWorldBLDEditorModule::AddWorldBLDMenuBarExtension(FMenuBarBuilder& MenuBarBuilder)
{
    MenuBarBuilder.AddPullDownMenu(
        LOCTEXT("WorldBLDMenu", "WorldBLD"),
        LOCTEXT("WorldBLDMenu_Tooltip", "WorldBLD tools"),
        FNewMenuDelegate::CreateRaw(this, &FWorldBLDEditorModule::PopulateWorldBLDMenu),
        "WorldBLD");
}

void FWorldBLDEditorModule::PopulateWorldBLDMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("OpenAssetLibrary", "Open Asset Library"),
		LOCTEXT("OpenAssetLibrary_Tooltip", "Open the WorldBLD Asset Library window"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FWorldBLDAssetLibraryWindow::OpenAssetLibrary)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("OpenLevelTemplates", "Open Level Templates..."),
		LOCTEXT("OpenLevelTemplates_Tooltip", "Open the Level Template selection window"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FWorldBLDLevelTemplateWindow::OpenLevelTemplateWindow)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CheckForToolUpdates", "Check for Tool Updates"),
		LOCTEXT("CheckForToolUpdates_Tooltip", "Fetch the latest tool list from the server"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			if (UWorldBLDEdMode* EdMode = UWorldBLDEdMode::GetWorldBLDEdMode())
			{
				EdMode->RefreshAvailableToolsFromServer();
			}
		})));
}

void FWorldBLDEditorModule::ExtendFileMenu()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

	FileMenuExtender = MakeShared<FExtender>();
	FileMenuExtender->AddMenuExtension(
		"FileNew",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateRaw(this, &FWorldBLDEditorModule::AddLevelTemplateFileMenuItem));

	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(FileMenuExtender);
}

void FWorldBLDEditorModule::AddLevelTemplateFileMenuItem(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NewLevelFromTemplate", "New Level from Template..."),
		LOCTEXT("NewLevelFromTemplate_Tooltip", "Create a new level using a WorldBLD Level Template"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FWorldBLDLevelTemplateWindow::OpenLevelTemplateWindow)));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWorldBLDEditorModule, WorldBLDEditor)