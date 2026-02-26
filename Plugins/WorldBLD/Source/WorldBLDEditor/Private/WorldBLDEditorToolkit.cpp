// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDEditorToolkit.h"
#include "WorldBLDEditController.h"
#include "WorldBLDMainWidget.h"
#include "WorldBLDToolButtonWidget.h"
#include "WorldBLDToolPaletteWidget.h"
#include "WorldBLDDefaultEditController.h"

#include "WorldBLDKitBase.h"
#include "WorldBLDEditorModule.h"
#include "WorldBLDKitElementUtils.h"
#include "WorldBLDLoadedKitsAssetFilter.h"
#include "WorldBLDToolUpdateSettings.h"
#include "WorldBLDContextMenuRegistry.h"

#include "ContextMenu/SWorldBLDContextMenu.h"
#include "ContextMenu/SWorldBLDContextMenuViewportOverlay.h"
#include "ContextMenu/SWorldBLDWarningTooltipOverlay.h"
#include "IWorldBLDContextMenuFactory.h"
#include "IWorldBLDContextMenuProvider.h"

#include "EditorModeManager.h"
#include "EditorUtilityWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "LevelEditor.h"
#include "LevelEditorSubsystem.h"
#include "LevelEditorViewport.h"
#include "Engine/Selection.h"
#include "Editor/Transbuffer.h"

#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "HttpModule.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Dialogs/Dialogs.h"
#include "UObject/SoftObjectPath.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SWindow.h"

#include "Toolkits/AssetEditorModeUILayer.h"
#include "Input/HittestGrid.h"
#include "PrimitiveDrawWrapper.h"

#include "Runtime/Slate/Private/Framework/Docking/SDockingTabStack.h" // Private Slate class used for toggling sidebar state of our tab
#include "Runtime/Slate/Private/Framework/Docking/SDockingArea.h"
#include "Authorization/WorldBLDAuthSubsystem.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IHttpResponse.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Application/SlateApplication.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Utilities/UtilitiesLibrary.h"
#include "WorldBLDKitPlugin.h"
///////////////////////////////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "WorldBLDEditorToolkit"

///////////////////////////////////////////////////////////////////////////////////////////////////

namespace WorldBLDEditorContextMenu
{
	static UClass* ResolveDefaultToolClass(const UWorldBLDEdMode* EdMode)
	{
		if (EdMode == nullptr)
		{
			return UWorldBLDDefaultEditController::StaticClass();
		}

		// Prefer the already-loaded class (fast path).
		if (UClass* Loaded = EdMode->DefaultToolClass.Get())
		{
			return Loaded;
		}

		// If configured but not loaded yet, load synchronously (editor-only).
		if (!EdMode->DefaultToolClass.IsNull())
		{
			const FSoftObjectPath ToolClassPath = EdMode->DefaultToolClass.ToSoftObjectPath();
			const FString ToolClassPackageName = ToolClassPath.GetLongPackageName();
			if (!ToolClassPackageName.IsEmpty() && !FPackageName::DoesPackageExist(ToolClassPackageName))
			{
				return UWorldBLDDefaultEditController::StaticClass();
			}

			if (UClass* Loaded = EdMode->DefaultToolClass.LoadSynchronous())
			{
				return Loaded;
			}
		}

		return UWorldBLDDefaultEditController::StaticClass();
	}

	static bool IsControllerListedInToolPalettes(const UWorldBLDEdMode* EdMode, const UWorldBLDEditController* Controller)
	{
		if ((EdMode == nullptr) || !IsValid(Controller))
		{
			return false;
		}

		const TArray<UWorldBLDToolPaletteWidget*> Palettes = EdMode->GetAvailableToolPalettes();
		for (const UWorldBLDToolPaletteWidget* Palette : Palettes)
		{
			const UWorldBLDToolPaletteWidgetExtended* ExtendedPalette = Cast<UWorldBLDToolPaletteWidgetExtended>(Palette);
			if (ExtendedPalette == nullptr)
			{
				continue;
			}

			const TSubclassOf<UWorldBLDEditController> MainSubtoolClass = ExtendedPalette->MainSubtoolClass;
			if (!IsValid(MainSubtoolClass))
			{
				continue;
			}

			// Use IsA to support BP subclasses of the expected controller class.
			if (Controller->IsA(MainSubtoolClass))
			{
				return true;
			}
		}

		return false;
	}

	static bool IsControllerAllowedToShowContextMenu(const UWorldBLDEdMode* EdMode, const UWorldBLDEditController* Controller)
	{
		// If we don't have an active controller, allow context menus (selection-driven behavior).
		if ((EdMode == nullptr) || !IsValid(Controller))
		{
			return true;
		}

		// Always allow the configured default controller (including BP subclasses).
		if (Controller->IsA(ResolveDefaultToolClass(EdMode)))
		{
			return true;
		}

		// Allow any controller that is the "main subtool" for one of the available tool palettes.
		return IsControllerListedInToolPalettes(EdMode, Controller);
	}
}

class FWorldBLDSelectModeWidgetHelper;
const FEditorModeID UWorldBLDEdMode::EditorModeID(TEXT("WorldBLDEditorMode"));

DEFINE_LOG_CATEGORY(LogWorldBLDEditor);

///////////////////////////////////////////////////////////////////////////////////////////////////

// SEE: DefaultEdMode.cpp
class FWorldBLDSelectModeWidgetHelper : public FLegacyEdModeWidgetHelper
{
	using Super = FLegacyEdModeWidgetHelper;
public:
	virtual bool AllowWidgetMove() override;
	virtual bool CanCycleWidgetMode() const override;
	virtual bool ShowModeWidgets() const override;
	virtual bool ShouldDrawWidget() const override;
	virtual bool UsesTransformWidget() const override;
	virtual bool UsesPropertyWidgets() const override;
    virtual EAxisList::Type GetWidgetAxisToDraw(UE::Widget::EWidgetMode CheckMode) const override;
    virtual FVector GetWidgetLocation() const override;

	virtual void ActorSelectionChangeNotify() override; // testing only

	UWorldBLDEditController* GetActiveController() const;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

UWorldBLDEdMode::UWorldBLDEdMode()
{
	const bool bIsBlueprintTemplate = GetClass()->GetName().Contains("SKEL_");
    Info = FEditorModeInfo(
        bIsBlueprintTemplate ? FEditorModeID(TEXT("WorldBLDEditorMode_Dummy")) : EditorModeID,
		NSLOCTEXT("WorldBLDEditorToolkit", "DisplayName", "WorldBLD"),
		FSlateIcon("WorldBLD", "WorldBLD.ToolIcon"),
		/* Visibility */ !bIsBlueprintTemplate,
		/* PriorityOrder */ 5001 // Picked semi-randomly
    );

	DefaultToolClass = UWorldBLDDefaultEditController::StaticClass();
}

bool UWorldBLDEdMode::IsThisEdModeActive() const
{
	return GLevelEditorModeTools().IsModeActive(FName("WorldBLDEditorMode"));
}

UWorldBLDEdMode* UWorldBLDEdMode::GetWorldBLDEdMode()
{
	return Cast<UWorldBLDEdMode>(GLevelEditorModeTools().GetActiveScriptableMode(UWorldBLDEdMode::EditorModeID));
}

FString UWorldBLDEdMode::GetRecordedInstalledToolSha1(const FString& ToolId) const
{
	return GetDefault<UWorldBLDToolUpdateSettings>()->GetInstalledSha1(ToolId);
}

namespace WorldBLDEdModeToolUpdateInternal
{
	static FString GetInstalledPluginVersionString(const TSharedPtr<IPlugin>& Plugin)
	{
		if (!Plugin.IsValid())
		{
			return FString();
		}

		const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
		if (!Descriptor.VersionName.IsEmpty())
		{
			return Descriptor.VersionName.TrimStartAndEnd();
		}

		// Fall back to numeric version if a version name is not provided.
		return FString::FromInt(Descriptor.Version);
	}

	static bool ParseNumericVersionParts(const FString& InVersion, TArray<int32>& OutParts)
	{
		OutParts.Reset();

		const FString Trimmed = InVersion.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return false;
		}

		// Parse leading numeric dot-separated parts (e.g. "1.2.3" in "1.2.3-beta").
		int32 Current = 0;
		bool bHasDigits = false;

		for (int32 Index = 0; Index < Trimmed.Len(); ++Index)
		{
			const TCHAR C = Trimmed[Index];

			if (FChar::IsDigit(C))
			{
				bHasDigits = true;
				Current = (Current * 10) + (C - TEXT('0'));
				continue;
			}

			if (C == TEXT('.'))
			{
				if (!bHasDigits)
				{
					return false;
				}
				OutParts.Add(Current);
				Current = 0;
				bHasDigits = false;
				continue;
			}

			// Stop at first non digit/dot.
			break;
		}

		if (bHasDigits)
		{
			OutParts.Add(Current);
		}

		return OutParts.Num() > 0;
	}

	static int32 CompareNumericVersionParts(const TArray<int32>& A, const TArray<int32>& B)
	{
		const int32 MaxParts = FMath::Max(A.Num(), B.Num());
		for (int32 Index = 0; Index < MaxParts; ++Index)
		{
			const int32 AV = (Index < A.Num()) ? A[Index] : 0;
			const int32 BV = (Index < B.Num()) ? B[Index] : 0;
			if (AV != BV)
			{
				return (AV < BV) ? -1 : 1;
			}
		}
		return 0;
	}
}

EWorldBLDToolUpdateState UWorldBLDEdMode::GetToolUpdateState(const FString& ToolId, const FString& ServerVersion, const FString& ServerSHA1) const
{
	if (ToolId.IsEmpty())
	{
		return EWorldBLDToolUpdateState::NotInstalled;
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ToolId);
	if (!Plugin.IsValid())
	{
		return EWorldBLDToolUpdateState::NotInstalled;
	}

	const FString InstalledVersion = WorldBLDEdModeToolUpdateInternal::GetInstalledPluginVersionString(Plugin);
	const FString ServerVersionTrimmed = ServerVersion.TrimStartAndEnd();
	if (InstalledVersion.IsEmpty() || ServerVersionTrimmed.IsEmpty())
	{
		return EWorldBLDToolUpdateState::InstalledUnknownSha1;
	}

	// Fast path: exact match (case-insensitive) => up-to-date.
	if (InstalledVersion.Equals(ServerVersionTrimmed, ESearchCase::IgnoreCase))
	{
		return EWorldBLDToolUpdateState::UpToDate;
	}

	// Staged updates are applied very early at startup, but plugin descriptor metadata can remain stale
	// for the current session in some packaging layouts. If we have a recorded installed SHA1 that matches
	// the server SHA1, trust that as up-to-date for UI/update gating.
	const FString ServerSha1Trimmed = ServerSHA1.TrimStartAndEnd();
	if (!ServerSha1Trimmed.IsEmpty())
	{
		const FString RecordedInstalledSha1 = GetRecordedInstalledToolSha1(ToolId).TrimStartAndEnd();
		if (!RecordedInstalledSha1.IsEmpty() && RecordedInstalledSha1.Equals(ServerSha1Trimmed, ESearchCase::IgnoreCase))
		{
			UE_LOG(
				LogWorldBLDEditor,
				Verbose,
				TEXT("[ToolUpdates] '%s' version mismatch (installed='%s', server='%s') but SHA1 matches server; treating as up-to-date."),
				*ToolId,
				*InstalledVersion,
				*ServerVersionTrimmed);
			return EWorldBLDToolUpdateState::UpToDate;
		}
	}

	// Prefer semantic-ish numeric compare if both sides have numeric parts.
	TArray<int32> InstalledParts;
	TArray<int32> ServerParts;
	const bool bInstalledParsed = WorldBLDEdModeToolUpdateInternal::ParseNumericVersionParts(InstalledVersion, InstalledParts);
	const bool bServerParsed = WorldBLDEdModeToolUpdateInternal::ParseNumericVersionParts(ServerVersionTrimmed, ServerParts);
	if (bInstalledParsed && bServerParsed)
	{
		const int32 Cmp = WorldBLDEdModeToolUpdateInternal::CompareNumericVersionParts(InstalledParts, ServerParts);
		return (Cmp < 0) ? EWorldBLDToolUpdateState::UpdateAvailable : EWorldBLDToolUpdateState::UpToDate;
	}

	// Fallback: if strings differ and we can't order reliably, treat as an available update.
	return EWorldBLDToolUpdateState::UpdateAvailable;
}

void UWorldBLDEdMode::RefreshAvailableToolsFromServer()
{
	FetchAvailableTools();
}

bool UWorldBLDEdMode::IsWorldBLDPluginUpToDate() const
{
	const FString ServerVersion = ServerWorldBLDPluginVersion.TrimStartAndEnd();
	if (ServerVersion.IsEmpty())
	{
		// If the server version is unknown, don't block actions based on this check.
		return true;
	}

	const FString LocalVersion = UWorldBLDKitPluginUtils::GetWorldBLDVersionString().TrimStartAndEnd();
	if (LocalVersion.IsEmpty())
	{
		// If the local version is unknown, don't block actions based on this check.
		return true;
	}

	return ServerVersion.Equals(LocalVersion, ESearchCase::IgnoreCase);
}

void UWorldBLDEdMode::EvaluateWorldBLDPluginVersionAndNotify() const
{
	// Keep dialogs editor-only and non-intrusive.
	if (IsRunningCommandlet() || FApp::IsUnattended() || (GEditor == nullptr))
	{
		return;
	}

	// Don't show modals during PIE/SIE.
	if (GEditor->PlayWorld != nullptr)
	{
		return;
	}

	const FString ServerVersion = ServerWorldBLDPluginVersion.TrimStartAndEnd();
	if (ServerVersion.IsEmpty())
	{
		return;
	}

	const FString LocalVersion = UWorldBLDKitPluginUtils::GetWorldBLDVersionString().TrimStartAndEnd();
	if (LocalVersion.IsEmpty())
	{
		return;
	}

	if (ServerVersion.Equals(LocalVersion, ESearchCase::IgnoreCase))
	{
		return;
	}

	// Avoid spamming: only show once per (server, local) pair per editor session.
	static FString LastNotifiedServerVersion;
	static FString LastNotifiedLocalVersion;
	if (ServerVersion.Equals(LastNotifiedServerVersion, ESearchCase::IgnoreCase) &&
		LocalVersion.Equals(LastNotifiedLocalVersion, ESearchCase::IgnoreCase))
	{
		return;
	}
	LastNotifiedServerVersion = ServerVersion;
	LastNotifiedLocalVersion = LocalVersion;

	const FText Message = LOCTEXT(
		"WorldBLD_PluginVersionMismatch_Message",
		"A new version of the WorldBLD plugin is available. Update it in the Epic Games Launcher to ensure stability!");
	const FText Title = LOCTEXT("WorldBLD_PluginVersionMismatch_Title", "WorldBLD Update Available");
	UUtilitiesLibrary::ShowOkModalDialog(Message, Title);
}

UWorldBLDToolPaletteWidget* UWorldBLDEdMode::GetToolPaletteByClass(TSubclassOf<UWorldBLDToolPaletteWidget> ToolClass) const
{
	UWorldBLDToolPaletteWidget* Tool = nullptr;

	for (UUserWidget* ToolWidget : CustomTools)
	{
		if (IsValid(ToolWidget) && ToolWidget->IsA(ToolClass))
		{
			Tool = Cast<UWorldBLDToolPaletteWidget>(ToolWidget);
		}
	}

	return Tool;
}

UWorldBLDToolPaletteWidget* UWorldBLDEdMode::GetActiveToolPalette() const
{
	UWorldBLDToolPaletteWidget* Palette = nullptr;

	if (const UWorldBLDMainWidget* MainWidget = Cast<UWorldBLDMainWidget>(FocusInlineWidget))
	{
		Palette = MainWidget->GetActiveTool();
	}

	return Palette;
}

TArray<UWorldBLDToolPaletteWidget*> UWorldBLDEdMode::GetAvailableToolPalettes() const
{
	TArray<UWorldBLDToolPaletteWidget*> Palettes;

	for (UUserWidget* ToolWidget : CustomTools)
	{
		if (IsValid(ToolWidget))
		{
			Palettes.Add(Cast<UWorldBLDToolPaletteWidget>(ToolWidget));
		}
	}

	return Palettes;
}

void UWorldBLDEdMode::SwitchToToolPaletteByClass(TSubclassOf<UWorldBLDToolPaletteWidget> ToolClass)
{
	if (const UWorldBLDToolPaletteWidget* Palette = GetToolPaletteByClass(ToolClass))
	{
		if (UWorldBLDMainWidget* MainWidget = Cast<UWorldBLDMainWidget>(FocusInlineWidget))
		{
			MainWidget->SetActiveTool(Palette->ToolId);
		}
	}
}

void UWorldBLDEdMode::SwitchToToolPalette(UWorldBLDToolPaletteWidget* Palette, bool bForceFocused) const
{
	if (IsValid(Palette))
	{
		if (UWorldBLDMainWidget* MainWidget = Cast<UWorldBLDMainWidget>(FocusInlineWidget))
		{
			MainWidget->SetActiveTool(Palette->ToolId);
			if (bForceFocused)
			{
				TryOpenPrimaryTabSidebarDrawer();
			}
		}
	}
}

void UWorldBLDEdMode::TryOpenPrimaryTabSidebarDrawer() const
{
	if (Toolkit.IsValid() && IsPrimaryTabInSidebar())
	{
		const TSharedPtr<SDockTab> PrimaryTab = StaticCastSharedPtr<FWorldBLDEdModeToolkit>(Toolkit)->GetPrimaryTab();
		if (PrimaryTab.IsValid())
		{
			(void)PrimaryTab->GetParentDockTabStack()->GetDockArea()->TryOpenSidebarDrawer(PrimaryTab.ToSharedRef());
		}
	}
}

bool UWorldBLDEdMode::TryHideTabWell(bool bHide) const
{
	const TSharedPtr<SDockTab> PrimaryTab = StaticCastSharedPtr<FWorldBLDEdModeToolkit>(Toolkit)->GetPrimaryTab();
	if (PrimaryTab.IsValid())
	{
		TSharedPtr<SDockingTabStack> DockingTabStack = PrimaryTab->GetParentDockTabStack();
		DockingTabStack->SetTabWellHidden(bHide);
		return DockingTabStack->IsTabWellHidden();
	}
	return false;
}

bool UWorldBLDToolPaletteWidget::IsLicensed(UWorldBLDAuthSubsystem* Subsystem)
{
	if (!IsValid(Subsystem))
	{
		return false;
	}

	return Subsystem->HasLicenseForTool(ToolId.ToString());
}

void UWorldBLDToolPaletteWidget::ToolMakeForemostPalette(bool bForceFocused)
{
	if (IsValid(EdMode))
	{
		EdMode->SwitchToToolPalette(this, bForceFocused);
	}
}

bool UWorldBLDEdMode::UsesToolkits() const
{
	return true;
}

bool UWorldBLDEdMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (UWorldBLDEditController* Controller = GetActiveEditController())
	{		
		if (Controller->PressingEscapeTriggersExit() && (Key == EKeys::Escape) && (Event == EInputEvent::IE_Pressed))
		{
			Controller->TryExitTool(/* Force? */ false);
			return true;
		}

		const FModifierKeysState ModifierState = FSlateApplication::Get().GetModifierKeys();
		
		if (((Key == EKeys::MouseScrollDown) || (Key == EKeys::MouseScrollUp)) && 
				(Event == EInputEvent::IE_Pressed) && Controller->ConsumesMouseScroll())
		{
			Controller->ToolMouseScroll((Key == EKeys::MouseScrollDown) ? -1.0f : 1.0f, ModifierState.IsControlDown(), ModifierState.IsAltDown(), ModifierState.IsShiftDown());
			return true;
		}

		if ((Key == EKeys::LeftMouseButton) || (Key == EKeys::MiddleMouseButton) || (Key == EKeys::RightMouseButton))
		{
			const int32 MouseIndex = (Key == EKeys::LeftMouseButton) ? 0 : ((Key == EKeys::MiddleMouseButton) ? 1 : 2);
			bool bPassivelyConsumes = false;
			if (Controller->ConsumesMouseClick(MouseIndex, bPassivelyConsumes) || bPassivelyConsumes)
			{
				Controller->ToolMouseClick(MouseIndex, Event == EInputEvent::IE_Pressed);
				return !bPassivelyConsumes;
			}
		}

		for (const FKeyHandlingSpec& KeySpec : Controller->InterceptedKeyEvents)
		{
			if ((Key == KeySpec.InterceptedKey) && (Event != EInputEvent::IE_Repeat))
			{
				Controller->ToolKeyEvent(Key, Event == EInputEvent::IE_Pressed,
						ModifierState.IsControlDown(), ModifierState.IsAltDown(), ModifierState.IsShiftDown());
				if (KeySpec.bConsumesKey)
				{
					return true;
				}
			}
		}
	}

	return ILegacyEdModeViewportInterface::InputKey(ViewportClient, Viewport, Key, Event);
}

TSharedRef<FLegacyEdModeWidgetHelper> UWorldBLDEdMode::CreateWidgetHelper()
{
	return MakeShared<FWorldBLDSelectModeWidgetHelper>();
}

void UWorldBLDEdMode::CreateToolkit()
{
	// Listen for if key blueprints recompile.
	TArray<UBlueprint*> Blueprints = {
		UBlueprint::GetBlueprintFromClass(GetClass()),
	};

	if (bCreateToolkit)
	{
		const TSharedPtr<FWorldBLDEdModeToolkit> CityToolkit = MakeShareable(new FWorldBLDEdModeToolkit);
		CityToolkit->EditModeRef = this;

		check(!Toolkit.IsValid());
		Toolkit = CityToolkit;		
	}

	TryConstructInlineContent(/* Force? */ true);

	if (IsValid(FocusInlineWidget))
	{
		Blueprints.AddUnique(UBlueprint::GetBlueprintFromClass(FocusInlineWidget->GetClass()));
		if (const UWorldBLDMainWidget* MainWidget = Cast<UWorldBLDMainWidget>(FocusInlineWidget))
		{
			Blueprints.AddUnique(UBlueprint::GetBlueprintFromClass(MainWidget->ButtonWidgetClass));
		}
	}

	for (const UUserWidget* Tool : CustomTools)
	{
		if (IsValid(Tool))
		{
			Blueprints.AddUnique(UBlueprint::GetBlueprintFromClass(Tool->GetClass()));
		}
	}

	for (UBlueprint* Blueprint : Blueprints)
	{
		if (IsValid(Blueprint))
		{
			Blueprint->OnCompiled().AddUObject(this, &UWorldBLDEdMode::RegenerateContents);
		}
	}

	// If we don't have a toolkit, install default tool here,
	// otherwise it will be installed in the FWorldBLDEdModeToolkit::InvokeUI()
	if (!Toolkit.IsValid())
	{
		InstallDefaultSelectionTool();
	}

	FWorldBLDEditorModule& ToolkitModule = FModuleManager::LoadModuleChecked<FWorldBLDEditorModule>("WorldBLDEditor");
	ToolkitModule.OnAvailableWorldBLDKitBundlesChanged.AddUObject(this, &UWorldBLDEdMode::OnKitBundlesChanged);
	
	// Synchronize the kit bundles that are loaded by the module.
	OnKitBundlesChanged();

	if (Toolkit.IsValid() && IsValid(FocusInlineWidget))
	{
		if (UWorldBLDMainWidget* MainWidget = Cast<UWorldBLDMainWidget>(FocusInlineWidget))
		{
			MainWidget->MakeFirstEnabledPaletteActive();
		}
	}
}


void UWorldBLDEdMode::OnKitBundlesChanged()
{
	FWorldBLDEditorModule& ToolkitModule = FModuleManager::LoadModuleChecked<FWorldBLDEditorModule>("WorldBLDEditor");
	TArray<UWorldBLDKitBundle*> NewAvailableBundles;
	for (TWeakObjectPtr<UWorldBLDKitBundle> WeakBundle : ToolkitModule.AvailableWorldBLDKitBundles)
	{
		if (WeakBundle.IsValid())
		{
			NewAvailableBundles.AddUnique(WeakBundle.Get());
		}
	}

	const TSet<UWorldBLDKitBundle*> NewBundlesSet = TSet<UWorldBLDKitBundle*>(NewAvailableBundles);
	const TSet<UWorldBLDKitBundle*> CurrentBundlesSet = TSet<UWorldBLDKitBundle*>(LoadedWorldBLDKitBundles);

	TArray<UWorldBLDKitBundle*> BundlesNoLongerLoaded = CurrentBundlesSet.Difference(NewBundlesSet).Array();
	TArray<UWorldBLDKitBundle*> BundlesThatAreNew     = NewBundlesSet.Difference(CurrentBundlesSet).Array();

	// Remove bundles that no longer are referenced
	for (const auto Bundle : BundlesNoLongerLoaded)
	{
		UnregisterWorldBLDKitBundle(Bundle);
	}

	// Add bundles that we don't know about yet
	for (const auto Bundle : BundlesThatAreNew)
	{
		RegisterWorldBLDKitBundle(Bundle);
	}
}

void UWorldBLDEdMode::OnBlueprintCompiled(UBlueprint* Blueprint)
{
	// If we recompile the editor mode's blueprint
	// it still sits in EditorModeManager::RecycledScriptableModes,
	// and will be re-used on Enter, which leads to the editor crash.
	if (Blueprint == UBlueprint::GetBlueprintFromClass(GetClass()))
	{
		GLevelEditorModeTools().DestroyMode(EditorModeID);
	}
}

void UWorldBLDEdMode::Enter()
{
    Super::Enter();

	EnsureLumenPostProcessVolumeExists();
		
	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditor.OnMapChanged().AddUObject(this, &UWorldBLDEdMode::OnMapChanged);
	
	if (GEditor)
	{
		GEditor->OnBlueprintPreCompile().AddUObject(this, &UWorldBLDEdMode::OnBlueprintCompiled);

		if (UTransBuffer* TransBuffer = Cast<UTransBuffer>(GEditor->Trans))
		{
			TransBuffer->OnUndo().AddUObject(this, &UWorldBLDEdMode::PostEditorUndo);
			TransBuffer->OnRedo().AddUObject(this, &UWorldBLDEdMode::PostEditorRedo);
		}
	}

	if (GEngine)
	{
		GEngine->OnLevelActorAdded().AddUObject(this, &UWorldBLDEdMode::OnActorSpawned);
		GEngine->OnLevelActorDeleted().AddUObject(this, &UWorldBLDEdMode::OnActorDeleted);
	}

	bCheckWorldBLDPluginVersionOnNextToolsFetch = true;
	FetchAvailableTools();

	ShowWarningTooltipOverlay();
}

void UWorldBLDEdMode::EnsureLumenPostProcessVolumeExists()
{
	if (IsRunningCommandlet() || FApp::IsUnattended() || (GEditor == nullptr))
	{
		return;
	}

	// Don't modify maps during PIE/SIE.
	if (GEditor->PlayWorld != nullptr)
	{
		return;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (World == nullptr)
	{
		return;
	}

	APostProcessVolume* CandidateVolume = nullptr;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		APostProcessVolume* Existing = *It;
		if (IsValid(Existing) && !Existing->IsTemplate())
		{
			// Prefer unbound volumes, but fall back to any valid PPV.
			if ((CandidateVolume == nullptr) || Existing->bUnbound)
			{
				CandidateVolume = Existing;
				if (Existing->bUnbound)
				{
					break;
				}
			}
		}
	}

	if (IsValid(CandidateVolume))
	{
		const bool bHasLumenGI =
			CandidateVolume->Settings.bOverride_DynamicGlobalIlluminationMethod &&
			(CandidateVolume->Settings.DynamicGlobalIlluminationMethod == EDynamicGlobalIlluminationMethod::Lumen);

		const bool bHasLumenReflections =
			CandidateVolume->Settings.bOverride_ReflectionMethod &&
			(CandidateVolume->Settings.ReflectionMethod == EReflectionMethod::Lumen);

		if (bHasLumenGI && bHasLumenReflections)
		{
			return;
		}

		const FText Title = LOCTEXT("WorldBLD_LumenRecommended_Title", "Enable Lumen (Recommended)");
		const FText Message = LOCTEXT(
			"WorldBLD_LumenRecommended_Message",
			"We found a PostProcessVolume, but it doesn't have Lumen Global Illumination and Reflections enabled.\n\n"
			"Would you like to enable Lumen on the existing Post Process Volume now? This is recommended for best results with WorldBLD assets."
		);

		static const TCHAR* SuppressSettingName = TEXT("SuppressWorldBLD_LumenPostProcessWarning");
		static const TCHAR* SuppressableDialogsSection = TEXT("SuppressableDialogs");

		// `FSuppressableWarningDialog` only persists its setting on some response paths.
		// This modal is commonly dismissed with "Not now", so we manage persistence ourselves to ensure
		// "Don't show this again" is respected regardless of which button is clicked.
		bool bSuppressed = false;
		if (GConfig != nullptr)
		{
			GConfig->GetBool(SuppressableDialogsSection, SuppressSettingName, bSuppressed, GEditorPerProjectIni);
		}
		if (bSuppressed)
		{
			return;
		}

		if (!FSlateApplication::IsInitialized())
		{
			return;
		}

		class SWorldBLDLumenPostProcessWarningDialog final : public SCompoundWidget
		{
		public:
			SLATE_BEGIN_ARGS(SWorldBLDLumenPostProcessWarningDialog) {}
				SLATE_ARGUMENT(FText, Message)
			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs)
			{
				bEnableClicked = false;
				bDontShowAgain = false;

				ChildSlot
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
					.Padding(12.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(InArgs._Message)
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.0f, 10.0f, 0.0f, 10.0f))
						[
							SNew(SCheckBox)
							.OnCheckStateChanged(this, &SWorldBLDLumenPostProcessWarningDialog::HandleDontShowAgainChanged)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("WorldBLD_LumenRecommended_DontShowAgain", "Don't show this again"))
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Right)
						[
							SNew(SUniformGridPanel)
							.SlotPadding(FMargin(4.0f, 0.0f))
							+ SUniformGridPanel::Slot(0, 0)
							[
								SNew(SButton)
								.Text(LOCTEXT("WorldBLD_LumenRecommended_Enable", "Enable"))
								.OnClicked(this, &SWorldBLDLumenPostProcessWarningDialog::HandleEnableClicked)
							]
							+ SUniformGridPanel::Slot(1, 0)
							[
								SNew(SButton)
								.Text(LOCTEXT("WorldBLD_LumenRecommended_NotNow", "Not now"))
								.OnClicked(this, &SWorldBLDLumenPostProcessWarningDialog::HandleNotNowClicked)
							]
						]
					]
				];
			}

			void SetHostWindow(TSharedRef<SWindow> InHostWindow)
			{
				HostWindow = InHostWindow;
			}

			bool WasEnableClicked() const
			{
				return bEnableClicked;
			}

			bool GetDontShowAgain() const
			{
				return bDontShowAgain;
			}

		private:
			FReply HandleEnableClicked()
			{
				bEnableClicked = true;
				CloseDialog();
				return FReply::Handled();
			}

			FReply HandleNotNowClicked()
			{
				bEnableClicked = false;
				CloseDialog();
				return FReply::Handled();
			}

			void HandleDontShowAgainChanged(ECheckBoxState NewState)
			{
				bDontShowAgain = (NewState == ECheckBoxState::Checked);
			}

			void CloseDialog()
			{
				if (TSharedPtr<SWindow> Pinned = HostWindow.Pin())
				{
					Pinned->RequestDestroyWindow();
				}
			}

			TWeakPtr<SWindow> HostWindow;
			bool bEnableClicked = false;
			bool bDontShowAgain = false;
		};

		bool bDontShowAgain = false;
		bool bEnableLumen = false;
		{
			TSharedPtr<SWorldBLDLumenPostProcessWarningDialog> DialogWidget;
			TSharedRef<SWindow> Window = SNew(SWindow)
				.Title(Title)
				.SizingRule(ESizingRule::Autosized)
				.SupportsMaximize(false)
				.SupportsMinimize(false)
				.HasCloseButton(false);

			Window->SetContent(
				SAssignNew(DialogWidget, SWorldBLDLumenPostProcessWarningDialog)
				.Message(Message)
			);

			DialogWidget->SetHostWindow(Window);

			FSlateApplication::Get().AddModalWindow(Window, FSlateApplication::Get().GetActiveTopLevelWindow());

			if (DialogWidget.IsValid())
			{
				bDontShowAgain = DialogWidget->GetDontShowAgain();
				bEnableLumen = DialogWidget->WasEnableClicked();
			}
		}

		if (bDontShowAgain && (GConfig != nullptr))
		{
			GConfig->SetBool(SuppressableDialogsSection, SuppressSettingName, true, GEditorPerProjectIni);
			GConfig->Flush(false, GEditorPerProjectIni);
		}

		if (bEnableLumen)
		{
			const FScopedTransaction Transaction(LOCTEXT("WorldBLD_EnableLumenOnExistingPPV", "WorldBLD: Enable Lumen on Post Process Volume"));
			if (ULevel* CandidateLevel = CandidateVolume->GetLevel())
			{
				CandidateLevel->Modify();
			}
			CandidateVolume->Modify();

			CandidateVolume->Settings.bOverride_DynamicGlobalIlluminationMethod = true;
			CandidateVolume->Settings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;

			CandidateVolume->Settings.bOverride_ReflectionMethod = true;
			CandidateVolume->Settings.ReflectionMethod = EReflectionMethod::Lumen;
		}
		return;
	}

	ULevel* TargetLevel = nullptr;
	if (ULevelEditorSubsystem* LevelEditor = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
	{
		TargetLevel = LevelEditor->GetCurrentLevel();
	}
	if (TargetLevel == nullptr)
	{
		TargetLevel = World->PersistentLevel;
	}
	if (TargetLevel == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("WorldBLD_AddLumenPostProcessVolume", "WorldBLD: Add Lumen Post Process Volume"));
	TargetLevel->Modify();

	FActorSpawnParameters SpawnParams;
	SpawnParams.OverrideLevel = TargetLevel;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transactional;

	APostProcessVolume* NewVolume = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, SpawnParams);
	if (!IsValid(NewVolume))
	{
		return;
	}

	NewVolume->Modify();

#if WITH_EDITOR
	NewVolume->SetActorLabel(TEXT("WorldBLD PostProcess (Lumen)"));
#endif

	NewVolume->bUnbound = true;
	NewVolume->bEnabled = true;

	NewVolume->Settings.bOverride_DynamicGlobalIlluminationMethod = true;
	NewVolume->Settings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;

	NewVolume->Settings.bOverride_ReflectionMethod = true;
	NewVolume->Settings.ReflectionMethod = EReflectionMethod::Lumen;
}

void UWorldBLDEdMode::FetchAvailableTools()
{
	// Get UE version (e.g., "5.6" from "5.6.0")
	const FString FullVersion = FEngineVersion::Current().ToString();
	FString UEVersion;
	int32 DotCount = 0;
	for (int32 i = 0; i < FullVersion.Len(); i++)
	{
		if (FullVersion[i] == '.')
		{
			DotCount++;
			if (DotCount >= 2) break;
		}
		UEVersion.AppendChar(FullVersion[i]);
	}

	const FString URL = FString::Printf(TEXT("https://worldbld.com/api/tools?ue_version=%s"), *UEVersion);

	// Capture the session token once so we can (safely) retry without auth if needed.
	FString SessionToken;
	if (GEditor != nullptr)
	{
		if (const UWorldBLDAuthSubsystem* AuthSubsystem = GEditor->GetEditorSubsystem<UWorldBLDAuthSubsystem>())
		{
			SessionToken = AuthSubsystem->GetSessionToken();
		}
	}

	UE_LOG(
		LogWorldBLDEditor,
		Log,
		TEXT("[FetchAvailableTools] Requesting server tool list. ue_version='%s' url='%s' useAuth=%s"),
		*UEVersion,
		*URL,
		SessionToken.IsEmpty() ? TEXT("false") : TEXT("true"));

	struct FFetchAvailableToolsRequestContext : public TSharedFromThis<FFetchAvailableToolsRequestContext>
	{
		enum class EAuthHeaderMode : uint8
		{
			None,
			XAuthorization,
			BearerAuthorization
		};

		TWeakObjectPtr<UWorldBLDEdMode> WeakMode;
		FString URL;
		FString SessionToken;
		int32 AttemptCounter = 0;

		void PopulateFallbackFromInstalledPlugins() const
		{
			UWorldBLDEdMode* Mode = WeakMode.Get();
			if (Mode == nullptr)
			{
				return;
			}

			TArray<FPluginToolData> FallbackTools;
			FallbackTools.Reserve(3);

			const auto AddIfInstalled = [&FallbackTools](const FString& ToolId, const FString& Description)
			{
				if (ToolId.IsEmpty())
				{
					return;
				}

				if (!IPluginManager::Get().FindPlugin(ToolId).IsValid())
				{
					return;
				}

				FPluginToolData ToolData;
				ToolData.ToolName = ToolId;
				ToolData.Description = Description;
				FallbackTools.Add(MoveTemp(ToolData));
			};

			AddIfInstalled(TEXT("CityBLD"), TEXT("Installed (server list unavailable)"));
			AddIfInstalled(TEXT("RoadBLD"), TEXT("Installed (server list unavailable)"));
			AddIfInstalled(TEXT("TwinBLD"), TEXT("Installed (server list unavailable)"));

			if (FallbackTools.Num() > 0)
			{
				Mode->AvailableTools = MoveTemp(FallbackTools);
			}
		}

		void StartRequest(EAuthHeaderMode AuthHeaderMode, bool bAllowRetryWithAlternateAuth, bool bAllowRetryWithoutAuth)
		{
			const int32 AttemptNumber = ++AttemptCounter;

			FHttpModule& HttpModule = FHttpModule::Get();
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();

			Request->SetURL(URL);
			Request->SetVerb(TEXT("GET"));
			Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
			Request->SetHeader(TEXT("Accept"), TEXT("application/json"));

			// If the user is logged in, include the session token so the backend can return license-gated tools.
			// If the backend returns an empty tool list when auth is present (eg token expired), we'll retry once without auth.
			if (AuthHeaderMode == EAuthHeaderMode::XAuthorization && !SessionToken.IsEmpty())
			{
				Request->SetHeader(TEXT("X-Authorization"), SessionToken);
			}
			else if (AuthHeaderMode == EAuthHeaderMode::BearerAuthorization && !SessionToken.IsEmpty())
			{
				Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *SessionToken));
			}

			Request->OnProcessRequestComplete().BindLambda(
				[Self = AsShared(), AuthHeaderMode, bAllowRetryWithAlternateAuth, bAllowRetryWithoutAuth, AttemptNumber](FHttpRequestPtr /*InRequest*/, FHttpResponsePtr Response, bool bWasSuccessful)
				{
					UWorldBLDEdMode* Mode = Self->WeakMode.Get();
					if (Mode == nullptr)
					{
						// The editor mode was destroyed/deactivated before the HTTP callback returned.
						return;
					}

					const auto TryRetryAlternateAuth = [&](const TCHAR* Reason) -> bool
					{
						if (bAllowRetryWithAlternateAuth && AuthHeaderMode == EAuthHeaderMode::XAuthorization && !Self->SessionToken.IsEmpty())
						{
							UE_LOG(LogWorldBLDEditor, Warning, TEXT("[FetchAvailableTools] Retrying with Authorization: Bearer. Reason='%s'"), Reason);
							Self->StartRequest(EAuthHeaderMode::BearerAuthorization, false, true);
							return true;
						}
						return false;
					};

					const auto TryRetryWithoutAuth = [&](const TCHAR* Reason) -> bool
					{
						if (bAllowRetryWithoutAuth && AuthHeaderMode != EAuthHeaderMode::None && !Self->SessionToken.IsEmpty())
						{
							UE_LOG(LogWorldBLDEditor, Warning, TEXT("[FetchAvailableTools] Retrying without auth. Reason='%s'"), Reason);
							Self->StartRequest(EAuthHeaderMode::None, false, false);
							return true;
						}
						return false;
					};

					if (!bWasSuccessful || !Response.IsValid())
					{
						UE_LOG(LogWorldBLDEditor, Warning, TEXT("[FetchAvailableTools] HTTP request failed. bWasSuccessful=%s responseValid=%s"), bWasSuccessful ? TEXT("true") : TEXT("false"), Response.IsValid() ? TEXT("true") : TEXT("false"));

						// If we attempted an authenticated request and it failed (eg expired token / proxy / captive portal),
						// retry once without auth so we still show public tools.
						if (TryRetryAlternateAuth(TEXT("RequestFailedOrNoResponse")))
						{
							return;
						}
						if (TryRetryWithoutAuth(TEXT("RequestFailedOrNoResponse")))
						{
							return;
						}

						// Don't clear the current list on request failure; keep last-known tools, and fall back if we have none.
						if (Mode->AvailableTools.Num() == 0)
						{
							Self->PopulateFallbackFromInstalledPlugins();
						}
						return;
					}

					const int32 ResponseCode = Response->GetResponseCode();
					const FString ResponseContentType = Response->GetHeader(TEXT("Content-Type"));
					const FString ResponseRequestId = Response->GetHeader(TEXT("X-Request-Id"));
					const FString ResponseCorrelationId = Response->GetHeader(TEXT("X-Correlation-Id"));
					const FString ResponseCFRay = Response->GetHeader(TEXT("CF-Ray"));
					const FString ResponseServer = Response->GetHeader(TEXT("Server"));

					const FString ContentAsString = Response->GetContentAsString();
					if (ContentAsString.IsEmpty())
					{
						UE_LOG(LogWorldBLDEditor, Warning, TEXT("[FetchAvailableTools] Empty response body. code=%d"), ResponseCode);
						if (TryRetryWithoutAuth(TEXT("EmptyBody")))
						{
							return;
						}

						if (Mode->AvailableTools.Num() == 0)
						{
							Self->PopulateFallbackFromInstalledPlugins();
						}
						return;
					}

					if (ResponseCode < 200 || ResponseCode >= 300)
					{
						UE_LOG(
							LogWorldBLDEditor,
							Warning,
							TEXT("[FetchAvailableTools] Non-2xx HTTP response. code=%d contentType='%s' server='%s' x-request-id='%s' x-correlation-id='%s' cf-ray='%s'"),
							ResponseCode,
							*ResponseContentType,
							*ResponseServer,
							*ResponseRequestId,
							*ResponseCorrelationId,
							*ResponseCFRay);

						if (TryRetryAlternateAuth(TEXT("Non2xxHttpResponse")))
						{
							return;
						}
					}

					const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ContentAsString);

					TSharedPtr<FJsonObject> JsonObject;
					if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
					{
						UE_LOG(LogWorldBLDEditor, Warning, TEXT("[FetchAvailableTools] Failed to deserialize JSON. code=%d contentType='%s'"), ResponseCode, *ResponseContentType);

						if (TryRetryAlternateAuth(TEXT("JsonDeserializeFailed")))
						{
							return;
						}
						if (TryRetryWithoutAuth(TEXT("JsonDeserializeFailed")))
						{
							return;
						}

						if (Mode->AvailableTools.Num() == 0)
						{
							Self->PopulateFallbackFromInstalledPlugins();
						}
						return;
					}

					if (!JsonObject->HasTypedField(TEXT("success"), EJson::Boolean) || !JsonObject->GetBoolField(TEXT("success")))
					{
						FString Message;
						JsonObject->TryGetStringField(TEXT("message"), Message);
						UE_LOG(LogWorldBLDEditor, Warning, TEXT("[FetchAvailableTools] Server returned failure JSON. code=%d message='%s'"), ResponseCode, *Message);

						if (TryRetryAlternateAuth(TEXT("JsonSuccessFalseOrMissing")))
						{
							return;
						}
						if (TryRetryWithoutAuth(TEXT("JsonSuccessFalseOrMissing")))
						{
							return;
						}

						if (Mode->AvailableTools.Num() == 0)
						{
							Self->PopulateFallbackFromInstalledPlugins();
						}
						return;
					}

					if (!JsonObject->HasTypedField(TEXT("tools"), EJson::Array))
					{
						UE_LOG(LogWorldBLDEditor, Warning, TEXT("[FetchAvailableTools] JSON missing expected field 'tools'. code=%d"), ResponseCode);

						if (TryRetryAlternateAuth(TEXT("JsonMissingToolsArray")))
						{
							return;
						}
						if (TryRetryWithoutAuth(TEXT("JsonMissingToolsArray")))
						{
							return;
						}

						if (Mode->AvailableTools.Num() == 0)
						{
							Self->PopulateFallbackFromInstalledPlugins();
						}
						return;
					}

					const TArray<TSharedPtr<FJsonValue>>& ToolsArray = JsonObject->GetArrayField(TEXT("tools"));

					TArray<FPluginToolData> NewTools;
					NewTools.Reserve(ToolsArray.Num());
					FString NewServerWorldBLDPluginVersion;
					FString NewServerWorldBLDToolId;
					bool bTwinBLDPresentInPayload = false;
					bool bTwinBLDAddedToNewTools = false;

					for (const TSharedPtr<FJsonValue>& ToolObj : ToolsArray)
					{
						if (!ToolObj.IsValid())
						{
							continue;
						}

						FPluginToolData ToolData;
						const TSharedPtr<FJsonObject> ToolJsonObject = ToolObj->AsObject();
						if (!ToolJsonObject.IsValid())
						{
							// Some payloads may return tools as simple strings (eg ["CityBLD","RoadBLD"]).
							if (ToolObj->Type == EJson::String)
							{
								ToolData.ToolName = ToolObj->AsString().TrimStartAndEnd();
								if (ToolData.ToolName.Equals(TEXT("TwinBLD"), ESearchCase::IgnoreCase))
								{
									bTwinBLDPresentInPayload = true;
								}
								if (!ToolData.ToolName.IsEmpty() && !ToolData.ToolName.Equals(TEXT("WorldBLD"), ESearchCase::IgnoreCase))
								{
									if (ToolData.ToolName.Equals(TEXT("TwinBLD"), ESearchCase::IgnoreCase))
									{
										bTwinBLDAddedToNewTools = true;
									}
									NewTools.Add(MoveTemp(ToolData));
								}
							}
							continue;
						}

						// Tool identifier can come under different keys depending on backend payload version.
						// Prefer "name" (historical), but fall back to "toolId"/"tool"/"id".
						{
							FString NameOrId;
							if (!ToolJsonObject->TryGetStringField(TEXT("name"), NameOrId) || NameOrId.TrimStartAndEnd().IsEmpty())
							{
								ToolJsonObject->TryGetStringField(TEXT("toolId"), NameOrId);
								if (NameOrId.TrimStartAndEnd().IsEmpty())
								{
									ToolJsonObject->TryGetStringField(TEXT("tool"), NameOrId);
								}
								if (NameOrId.TrimStartAndEnd().IsEmpty())
								{
									ToolJsonObject->TryGetStringField(TEXT("id"), NameOrId);
								}
							}

							ToolData.ToolName = NameOrId.TrimStartAndEnd();
							if (ToolData.ToolName.IsEmpty())
							{
								continue;
							}

							if (ToolData.ToolName.Equals(TEXT("TwinBLD"), ESearchCase::IgnoreCase))
							{
								bTwinBLDPresentInPayload = true;
							}

							if (ToolData.ToolName.Equals(TEXT("WorldBLD"), ESearchCase::IgnoreCase))
							{
								// Keep this out of AvailableTools (UI), but cache the version + tool id for:
								// - mismatch warning (version)
								// - requiredTools gating (tool id -> "WorldBLD" resolution)
								ToolJsonObject->TryGetStringField(TEXT("version"), NewServerWorldBLDPluginVersion);
								if (!ToolJsonObject->TryGetStringField(TEXT("toolId"), NewServerWorldBLDToolId) || NewServerWorldBLDToolId.TrimStartAndEnd().IsEmpty())
								{
									ToolJsonObject->TryGetStringField(TEXT("id"), NewServerWorldBLDToolId);
								}
								NewServerWorldBLDToolId = NewServerWorldBLDToolId.TrimStartAndEnd();
								continue;
							}
						}

						ToolJsonObject->TryGetStringField(TEXT("description"), ToolData.Description);
						ToolJsonObject->TryGetStringField(TEXT("downloadLink"), ToolData.DownloadLink);
						ToolJsonObject->TryGetStringField(TEXT("version"), ToolData.Version);
						ToolJsonObject->TryGetStringField(TEXT("sha1"), ToolData.SHA1);

						// Persist a server-tool-id -> plugin-name mapping so other systems can resolve requiredTools even
						// when license-based lists are incomplete (e.g., trial installs).
						{
							auto TryExtractServerToolIdFromUrl = [](const FString& Url, FString& OutServerToolId) -> bool
							{
								OutServerToolId.Reset();
								static const FString Marker = TEXT("/api/tools/");
								const int32 MarkerIndex = Url.Find(Marker, ESearchCase::IgnoreCase, ESearchDir::FromStart);
								if (MarkerIndex == INDEX_NONE)
								{
									return false;
								}

								const int32 AfterMarker = MarkerIndex + Marker.Len();
								if (AfterMarker >= Url.Len())
								{
									return false;
								}

								const int32 NextSlashIndex = Url.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, AfterMarker);
								if (NextSlashIndex == INDEX_NONE || NextSlashIndex <= AfterMarker)
								{
									return false;
								}

								OutServerToolId = Url.Mid(AfterMarker, NextSlashIndex - AfterMarker).TrimStartAndEnd();
								return !OutServerToolId.IsEmpty();
							};

							FString ServerToolId;
							if (GConfig && !ToolData.ToolName.IsEmpty() && TryExtractServerToolIdFromUrl(ToolData.DownloadLink, ServerToolId))
							{
								GConfig->SetString(TEXT("WorldBLD.ToolIdMap"), *ServerToolId, *ToolData.ToolName, GEditorPerProjectIni);
							}
						}

						if (ToolData.ToolName.Equals(TEXT("TwinBLD"), ESearchCase::IgnoreCase))
						{
							bTwinBLDAddedToNewTools = true;
						}

						NewTools.Add(MoveTemp(ToolData));
					}

					if (GConfig)
					{
						GConfig->Flush(false, GEditorPerProjectIni);
					}

					{
						TArray<FString> ToolNames;
						ToolNames.Reserve(NewTools.Num());
						for (const FPluginToolData& Tool : NewTools)
						{
							ToolNames.Add(Tool.ToolName);
						}

						UE_LOG(
							LogWorldBLDEditor,
							Log,
							TEXT("[FetchAvailableTools] Tool list updated. count=%d names='%s' twinInPayload=%s"),
							NewTools.Num(),
							*FString::Join(ToolNames, TEXT(", ")),
							bTwinBLDPresentInPayload ? TEXT("true") : TEXT("false"));
					}

					// If we requested with auth and got no tools back, retry once without auth so we at least show public tools.
					if (NewTools.Num() == 0)
					{
						if (TryRetryWithoutAuth(TEXT("EmptyToolsArrayAfterParse")))
						{
							return;
						}

						// Keep current list if any; otherwise fall back to installed plugins so the menu is never empty.
						if (Mode->AvailableTools.Num() == 0)
						{
							Self->PopulateFallbackFromInstalledPlugins();
						}
						return;
					}

					Mode->AvailableTools = MoveTemp(NewTools);
					Mode->ServerWorldBLDPluginVersion = MoveTemp(NewServerWorldBLDPluginVersion);
					Mode->ServerWorldBLDToolId = MoveTemp(NewServerWorldBLDToolId);

					if (Mode->bCheckWorldBLDPluginVersionOnNextToolsFetch)
					{
						Mode->bCheckWorldBLDPluginVersionOnNextToolsFetch = false;
						Mode->EvaluateWorldBLDPluginVersionAndNotify();
					}
				});

			if (!Request->ProcessRequest())
			{
				UE_LOG(LogWorldBLDEditor, Warning, TEXT("[FetchAvailableTools] ProcessRequest() failed synchronously."));
				if (UWorldBLDEdMode* Mode = WeakMode.Get())
				{
					if (Mode->AvailableTools.Num() == 0)
					{
						PopulateFallbackFromInstalledPlugins();
					}
				}
			}
		}
	};

	const TSharedRef<FFetchAvailableToolsRequestContext> Context = MakeShared<FFetchAvailableToolsRequestContext>();
	Context->WeakMode = this;
	Context->URL = URL;
	Context->SessionToken = SessionToken;

	// First attempt: use auth if we have a session token.
	Context->StartRequest(SessionToken.IsEmpty() ? FFetchAvailableToolsRequestContext::EAuthHeaderMode::None : FFetchAvailableToolsRequestContext::EAuthHeaderMode::XAuthorization, true, true);
}

void UWorldBLDEdMode::OnActorSpawned(AActor* InActor)
{
	if (AWorldBLDKitBase* WorldBLDKit = Cast<AWorldBLDKitBase>(InActor))
	{
		if (ULevelEditorSubsystem* LevelEditor = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
		{
			if ((WorldBLDKit != WorldBLDKit->GetClass()->GetDefaultObject()) && (WorldBLDKit->GetWorld() == LevelEditor->GetCurrentLevel()->OwningWorld))
			{
				RegisterWorldBLDKit(WorldBLDKit);
			}
		}
	}
}

void UWorldBLDEdMode::OnActorDeleted(AActor* InActor)
{
	if (AWorldBLDKitBase* WorldBLDKit = Cast<AWorldBLDKitBase>(InActor))
	{
		UnregisterWorldBLDKit(WorldBLDKit);
	}
}
    
void UWorldBLDEdMode::Exit()
{
	HideWarningTooltipOverlay();
	HideContextMenu();

	if (GEditor)
	{
		if (UTransBuffer* TransBuffer = Cast<UTransBuffer>(GEditor->Trans))
		{
			TransBuffer->OnUndo().RemoveAll(this);
			TransBuffer->OnRedo().RemoveAll(this);
		}
	}

	if (GEngine)
	{
		GEngine->OnLevelActorAdded().RemoveAll(this);
		GEngine->OnLevelActorDeleted().RemoveAll(this);
	}

	// If we already had a widget, make sure it can get garbage collected.
	if (FocusInlineWidget)
	{
		if (const TSharedPtr<SVerticalBox> Container = StaticCastSharedPtr<SVerticalBox>(TSharedPtr<SWidget>(TabWidget)))
		{
			if (Container->IsValidSlotIndex(0))
			{
				Container->GetSlot(0).AttachWidget(SNullWidget::NullWidget);
			}
		}
	}
	DestroyAllWidgets();

    Super::Exit();
}

void UWorldBLDEdMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	if (HasActiveContextMenu() && !ValidateContextMenuTargets())
	{
		HideContextMenu();
	}
}

void UWorldBLDEdMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	Super::Render(View, Viewport, PDI);

	if (ToolModeStack.Num() > 0)
	{
		ToolModeStack.Last()->RenderInView(View, Viewport, PDI);
	}
}

bool UWorldBLDEdMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	bool bHandled = false;

    // Route hit proxy clicks for WorldBLD runtime selectable elements
    if (const HCityKitSelectableObjectHitProxy* SelectedItemProxy = HitProxyCast<HCityKitSelectableObjectHitProxy>(HitProxy))
    {
        if (ToolModeStack.Num() > 0 && IsValid(ToolModeStack.Last()))
        {
            ToolModeStack.Last()->ToolHitProxyClick(SelectedItemProxy->Context, Click.GetKey());
            bHandled = true;
        }
    }

	if (!bHandled)
	{
		bHandled = Super::HandleClick(InViewportClient, HitProxy, Click);
	}
	return bHandled;
}

bool UWorldBLDEdMode::MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y)
{
	bool bHandled = false;
	if (UWorldBLDEditController* Controller = GetActiveEditController())
	{
		Controller->ToolMouseMove(x, y);
		bHandled = true;
	}

	if (!bHandled)
	{
		Super::MouseMove(ViewportClient, Viewport, x, y);
	}
	return bHandled;
}

void UWorldBLDEdMode::InstallDefaultRecompileHandler()
{
	UBlueprint* MyBlueprint = UBlueprint::GetBlueprintFromClass(GetClass());
	if (IsValid(MyBlueprint))
	{
		MyBlueprint->OnCompiled().AddUObject(this, &UWorldBLDEdMode::RegenerateContents);
	}
}

void UWorldBLDEdMode::MovePrimaryTabToSidebar() const
{
	if (Toolkit.IsValid() && !IsPrimaryTabInSidebar())
	{
		const TSharedPtr<SDockTab> PrimaryTab = StaticCastSharedPtr<FWorldBLDEdModeToolkit>(Toolkit)->GetPrimaryTab();
		if (PrimaryTab.IsValid())
		{
			FSlateRect TabRect = PrimaryTab->GetPaintSpaceGeometry().GetLayoutBoundingRect();
			if (TabRect.GetArea())
			{
				PrimaryTab->GetParentDockTabStack()->MoveTabToSidebar(PrimaryTab.ToSharedRef());
			}
		}
	}
}

void UWorldBLDEdMode::RestorePrimaryTabFromSidebar() const
{
	if (Toolkit.IsValid() && IsPrimaryTabInSidebar())
	{
		const TSharedPtr<SDockTab> PrimaryTab = StaticCastSharedPtr<FWorldBLDEdModeToolkit>(Toolkit)->GetPrimaryTab();
		if (PrimaryTab.IsValid())
		{
			PrimaryTab->GetParentDockTabStack()->GetDockArea()->RestoreTabFromSidebar(PrimaryTab.ToSharedRef());
		}
	}
}

bool UWorldBLDEdMode::IsPrimaryTabInSidebar() const
{
	if (Toolkit.IsValid())
	{
		const TSharedPtr<SDockTab> PrimaryTab = StaticCastSharedPtr<FWorldBLDEdModeToolkit>(Toolkit)->GetPrimaryTab();
		return PrimaryTab.IsValid() && PrimaryTab->GetParentDockTabStack()->GetDockArea()->IsTabInSidebar(PrimaryTab.ToSharedRef());
	}

	return false;
}

void UWorldBLDEdMode::RegenerateContents(UBlueprint* RecompiledBlueprint)
{
	DestroyAllWidgets();

	// Regenerating contents is expensive, so if the ed mode is not presented then ignore this.
	if (!IsThisEdModeActive())
	{
		return;
	}

	// If we are recompiling ourselves, we need to entirely delete this mode so it can be re-registered by the new class.
	if (RecompiledBlueprint == UBlueprint::GetBlueprintFromClass(GetClass()))
	{
		GLevelEditorModeTools().DestroyMode(EditorModeID);
	}
#if 1
	else
	{
		// NOTE: Regenerating contents is just too expensive. Deactivate the EdMode and let it regenerate its contents later.
		GLevelEditorModeTools().DeactivateMode(EditorModeID);
	}
#else
	else
	{	
		TryConstructInlineContent(/* Force? */ true);
	}

	for (UWorldBLDKitBundle* Bundle : LoadedWorldBLDKitBundles)
	{
		if (Bundle && (RecompiledBlueprint == UBlueprint::GetBlueprintFromClass(Bundle->GetClass())))
		{
			OnKitBundlesChanged();
		}
	}

	InstallDefaultSelectionTool();
#endif
}

void UWorldBLDEdMode::OnMapChanged(UWorld* World, EMapChangeType MapChangeType)
{
	if (MapChangeType == EMapChangeType::TearDownWorld)
	{
		// Release our references to the in-map WorldBLDKit actors. Otherwise the engine will complain about a memory leak.
		ResetRegisteredWorldBLDKits();

		// We need to Delete the UMG widget if we are tearing down the World it was built with.
		if (FocusInlineWidget && (World == FocusInlineWidget->GetWorld()))
		{
			if (const TSharedPtr<SVerticalBox> Container = StaticCastSharedPtr<SVerticalBox>(TSharedPtr<SWidget>(TabWidget)))
			{
				if (Container->IsValidSlotIndex(0))
				{
					Container->GetSlot(0).AttachWidget(SNullWidget::NullWidget);
				}
			}
		}

		DestroyAllWidgets();
	}
	else if (MapChangeType != EMapChangeType::SaveMap)
	{
		// Recreate the widget if we are loading a map or opening a new map
		TryConstructInlineContent(/* force? */ true);
	}
}

void UWorldBLDEdMode::PostEditorUndo(const struct FTransactionContext& TransactionContext, bool bSucceeded) const
{
	if (IsThisEdModeActive())
	{
		if (UWorldBLDEditController* Tool = GetActiveEditController())
		{
			Tool->ToolPostUndoRedo(/* is undo? */ true);
		}
	}
}

void UWorldBLDEdMode::PostEditorRedo(const struct FTransactionContext& TransactionContext, bool bSucceeded) const
{
	if (IsThisEdModeActive())
	{
		if (UWorldBLDEditController* Tool = GetActiveEditController())
		{
			Tool->ToolPostUndoRedo(/* is undo? */ false);
		}
	}
}

void UWorldBLDEdMode::ConstructCustomToolWidgets(bool bForce)
{
	// Ensure our other custom tools are all available.
	while (CustomTools.Num() < CustomToolClasses.Num())
	{
		CustomTools.AddDefaulted();
	}
	while (CustomTools.Num() > CustomToolClasses.Num())
	{
		DestructWidget(CustomTools.Last());
		CustomTools.Pop();
	}
	for (int32 Idx = 0; Idx < CustomToolClasses.Num(); Idx += 1)
	{
		UUserWidget*& ToolRef = CustomTools[Idx];
		TryConstructUserWidget(ToolRef, CustomToolClasses[Idx], bForce);
	}
}

UUserWidget* UWorldBLDEdMode::TryConstructInlineContent(bool bForce)
{
	ConstructCustomToolWidgets();	

	// Ensure the main widget
	if ((FocusInlineWidget == nullptr) && !UserInterfaceClass.IsNull())
	{
		TryConstructUserWidget(FocusInlineWidget, UserInterfaceClass, bForce);
		if (UWorldBLDMainWidget* MainWidget = Cast<UWorldBLDMainWidget>(FocusInlineWidget))
		{
			MainWidget->EdMode = this;
			OnWorldBLDKitListChanged.AddUniqueDynamic(MainWidget, &UWorldBLDMainWidget::OnLoadedWorldBLDKitsChanged);

			TArray<UWorldBLDToolPaletteWidget*> PaletteTools;
			for (UUserWidget* Widget : CustomTools)
			{
				if (UWorldBLDToolPaletteWidget* Tool = Cast<UWorldBLDToolPaletteWidget>(Widget))
				{
					Tool->EdMode = this;
					PaletteTools.Add(Tool);
				}
			}

			MainWidget->SetTools(PaletteTools);
			MainWidget->OnLoadedWorldBLDKitsChanged(this);
			MainWidget->MakeFirstEnabledPaletteActive();
		}
	}

	// Now update our actually rendered widget.
	if (bCreateToolkit)
	{
		UpdateFocusWidget(bForce);
	}
	return FocusInlineWidget;
}

void UWorldBLDEdMode::UpdateFocusWidget(bool bForce)
{
	// Make sure our current widget is actually assigned as the visible one.
    if ((bForce || (TabWidget == SNullWidget::NullWidget)) && IsValid(FocusInlineWidget))
    {
		if (TabWidget == SNullWidget::NullWidget)
		{
			TabWidget = SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					FocusInlineWidget->TakeWidget()
				];
		}
		// If we have already created the container, then replace the inner contents with the new contents.
		else if (const TSharedPtr<SVerticalBox> Container = StaticCastSharedPtr<SVerticalBox>(TSharedPtr<SWidget>(TabWidget)))
		{
			if (Container->IsValidSlotIndex(0))
			{
				Container->GetSlot(0).AttachWidget(FocusInlineWidget->TakeWidget());
			}
		}
    }
}

void UWorldBLDEdMode::TryConstructUserWidget(UUserWidget*& WidgetRef, TSoftClassPtr<UUserWidget> WidgetClass, bool bForce)
{
	if ((!IsValid(WidgetRef) || bForce) && !WidgetClass.IsNull() && GEditor)
    {
        /// NOTE: see UEditorUtilityWidgetBlueprint::CreateUtilityWidget() as a reference
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (World)
        {
			DestructWidget(WidgetRef);

			// Avoid noisy LoadPackage warnings and CreateWidget(null class) errors when a configured widget class
			// points at a package that doesn't exist (eg optional companion plugins not installed).
			const FSoftObjectPath WidgetClassPath = WidgetClass.ToSoftObjectPath();
			if (!WidgetClassPath.IsValid())
			{
				return;
			}

			const FString WidgetClassPackageName = WidgetClassPath.GetLongPackageName();
			if (!WidgetClassPackageName.IsEmpty() && !FPackageName::DoesPackageExist(WidgetClassPackageName))
			{
				return;
			}

			// Prefer already-loaded class when possible; otherwise load synchronously (editor-only).
			UClass* LoadedWidgetClass = WidgetClass.Get();
			if (LoadedWidgetClass == nullptr)
			{
				LoadedWidgetClass = WidgetClass.LoadSynchronous();
			}
			if (LoadedWidgetClass == nullptr)
			{
				return;
			}

            WidgetRef = CreateWidget<UUserWidget>(World, LoadedWidgetClass);
		    if (WidgetRef)
		    {
			    // Editor Utility is flagged as transient to prevent from dirty the World it's created in when a property added to the Utility Widget is changed
			    WidgetRef->SetFlags(RF_Transient);

			    // Also mark nested utility widgets as transient to prevent them from dirtying the world (since they'll be created via CreateWidget and not CreateUtilityWidget)
			    TArray<UWidget*> AllWidgets;
			    WidgetRef->WidgetTree->GetAllWidgets(AllWidgets);

			    for (UWidget* Widget : AllWidgets)
			    {
				    if (Widget->IsA(UEditorUtilityWidget::StaticClass()))
				    {
					    Widget->SetFlags(RF_Transient);
					    Widget->Slot->SetFlags(RF_Transient);
				    }
			    }
		    }
        }
    }
}

void UWorldBLDEdMode::DestructWidget(UUserWidget*& WidgetRef)
{
	if (WidgetRef)
	{
		WidgetRef->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty);
		WidgetRef = nullptr;
	}
}

void UWorldBLDEdMode::DestroyAllWidgets()
{
	ClearToolModeStack();

	if (UWorldBLDMainWidget* MainWidget = Cast<UWorldBLDMainWidget>(FocusInlineWidget))
	{
		MainWidget->ResetContent();
	}

	DestructWidget(FocusInlineWidget);
	for (UUserWidget*& Tool : CustomTools)
	{
		DestructWidget(Tool);
	}
	CustomTools.Empty();
}

void UWorldBLDEdMode::RegisterWorldBLDKit(AWorldBLDKitBase* Kit)
{
	if (IsValid(Kit) && !LoadedWorldBLDKits.Contains(Kit))
	{
		LoadedWorldBLDKits.Add(Kit);
		WorldBLD::Editor::LoadedKitsAssetFilter::InvalidateCache();
		OnWorldBLDKitListChanged.Broadcast(this);
		Kit->OnDestroyed.AddUniqueDynamic(this, &UWorldBLDEdMode::OnKitDestroyed);
	}
}

void UWorldBLDEdMode::OnKitDestroyed(AActor* DestroyedActor)
{
	UnregisterWorldBLDKit(Cast<AWorldBLDKitBase>(DestroyedActor));
}

void UWorldBLDEdMode::UnregisterWorldBLDKit(AWorldBLDKitBase* Kit)
{
	if (IsValid(Kit) && LoadedWorldBLDKits.Contains(Kit))
	{
		LoadedWorldBLDKits.Remove(Kit);
		WorldBLD::Editor::LoadedKitsAssetFilter::InvalidateCache();
		OnWorldBLDKitListChanged.Broadcast(this);
	}
	
}


void UWorldBLDEdMode::ResetRegisteredWorldBLDKits()
{
	if (!LoadedWorldBLDKits.IsEmpty())
	{
		LoadedWorldBLDKits.Empty();
		WorldBLD::Editor::LoadedKitsAssetFilter::InvalidateCache();
		OnWorldBLDKitListChanged.Broadcast(this);
	}
}

void UWorldBLDEdMode::RegisterWorldBLDKitBundle(UWorldBLDKitBundle* Bundle)
{
	if (IsValid(Bundle) && !LoadedWorldBLDKitBundles.Contains(Bundle))
	{
		LoadedWorldBLDKitBundles.Add(Bundle);
		WorldBLD::Editor::LoadedKitsAssetFilter::InvalidateCache();
		OnWorldBLDKitBundleListChanged.Broadcast(this);
		
		UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(Bundle->GetClass());
		if (IsValid(Blueprint))
		{
			Blueprint->OnCompiled().AddUObject(this, &UWorldBLDEdMode::RegenerateContents);
		}
	}
}

void UWorldBLDEdMode::UnregisterWorldBLDKitBundle(UWorldBLDKitBundle* Bundle)
{
	if (IsValid(Bundle) && LoadedWorldBLDKitBundles.Contains(Bundle))
	{
		LoadedWorldBLDKitBundles.Remove(Bundle);
		WorldBLD::Editor::LoadedKitsAssetFilter::InvalidateCache();
		OnWorldBLDKitBundleListChanged.Broadcast(this);
	}
}

UWorldBLDEditController* UWorldBLDEdMode::GetActiveEditController() const
{
	return ToolModeStack.IsEmpty() ? nullptr : ToolModeStack.Last();
}

bool UWorldBLDEdMode::AreSameActorSelection(const TArray<AActor*>& Actors, const TArray<TWeakObjectPtr<AActor>>& WeakActors)
{
	if (Actors.Num() != WeakActors.Num())
	{
		return false;
	}

	TSet<const AActor*> ActorSet;
	ActorSet.Reserve(Actors.Num());
	for (AActor* Actor : Actors)
	{
		if (!IsValid(Actor))
		{
			return false;
		}
		ActorSet.Add(Actor);
	}

	// If there were duplicates, sets will collapse them.
	if (ActorSet.Num() != Actors.Num())
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& WeakActor : WeakActors)
	{
		if (!WeakActor.IsValid() || !ActorSet.Contains(WeakActor.Get()))
		{
			return false;
		}
	}

	return true;
}

bool UWorldBLDEdMode::HasActiveContextMenu() const
{
	return ActiveContextMenuOverlayWidget.IsValid();
}

void UWorldBLDEdMode::HideContextMenu()
{
	TSharedPtr<SWidget> OverlayToRemove = ActiveContextMenuOverlayWidget;
	TWeakPtr<SLevelViewport> HostViewport = ActiveContextMenuHostViewport;

	ActiveContextMenuOverlayWidget.Reset();
	ActiveContextMenuHostViewport.Reset();
	ContextMenuTargetActor.Reset();
	ContextMenuTargetActors.Reset();
	bContextMenuHiddenByTool = false;

	if (OverlayToRemove.IsValid())
	{
		if (TSharedPtr<SLevelViewport> PinnedHostViewport = HostViewport.Pin())
		{
			PinnedHostViewport->RemoveOverlayWidget(OverlayToRemove.ToSharedRef());
		}
		else
		{
			// Fallback: attempt removal from the currently active viewport (in case focus changed).
			FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
			if (const TSharedPtr<ILevelEditor> PinnedEditor = LevelEditor.GetLevelEditorInstance().Pin())
			{
				if (const TSharedPtr<SLevelViewport> ActiveViewport = PinnedEditor->GetActiveViewportInterface())
				{
					ActiveViewport->RemoveOverlayWidget(OverlayToRemove.ToSharedRef());
				}
			}
		}
	}
}

void UWorldBLDEdMode::ShowWarningTooltipOverlay()
{
	if (WarningTooltipOverlayWidget.IsValid())
	{
		return;
	}

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	const TSharedPtr<ILevelEditor> LevelEditorInstance = LevelEditor.GetLevelEditorInstance().Pin();
	if (!LevelEditorInstance.IsValid())
	{
		return;
	}

	const TSharedPtr<SLevelViewport> ActiveViewport = LevelEditorInstance->GetActiveViewportInterface();
	if (!ActiveViewport.IsValid())
	{
		return;
	}

	const TWeakObjectPtr<UWorldBLDEdMode> WeakThis(this);

	const TSharedRef<SWorldBLDWarningTooltipOverlay> Overlay =
		SNew(SWorldBLDWarningTooltipOverlay)
		.WarningText_Lambda([WeakThis]() -> FText
		{
			if (WeakThis.IsValid())
			{
				if (const UWorldBLDEditController* Controller = WeakThis->GetActiveEditController())
				{
					return Controller->UserWarning;
				}
			}
			return FText::GetEmpty();
		});

	WarningTooltipOverlayWidget = Overlay;
	WarningTooltipHostViewport = ActiveViewport;

	ActiveViewport->AddOverlayWidget(Overlay);
}

void UWorldBLDEdMode::HideWarningTooltipOverlay()
{
	TSharedPtr<SWidget> OverlayToRemove = WarningTooltipOverlayWidget;
	TWeakPtr<SLevelViewport> HostViewport = WarningTooltipHostViewport;

	WarningTooltipOverlayWidget.Reset();
	WarningTooltipHostViewport.Reset();

	if (OverlayToRemove.IsValid())
	{
		if (TSharedPtr<SLevelViewport> PinnedHostViewport = HostViewport.Pin())
		{
			PinnedHostViewport->RemoveOverlayWidget(OverlayToRemove.ToSharedRef());
			return;
		}

		// Fallback: attempt removal from the currently active viewport (in case focus changed).
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		if (const TSharedPtr<ILevelEditor> PinnedEditor = LevelEditor.GetLevelEditorInstance().Pin())
		{
			if (const TSharedPtr<SLevelViewport> ActiveViewport = PinnedEditor->GetActiveViewportInterface())
			{
				ActiveViewport->RemoveOverlayWidget(OverlayToRemove.ToSharedRef());
			}
		}
	}
}

bool UWorldBLDEdMode::IsContextMenuRelevantForSelection(const TArray<AActor*>& SelectedActors) const
{
	// Only allow context menus when the active controller is an exempt controller (default controller or
	// the MainSubtoolClass of one of our available tool palettes).
	if (!WorldBLDEditorContextMenu::IsControllerAllowedToShowContextMenu(this, GetActiveEditController()))
	{
		return false;
	}

	if (SelectedActors.IsEmpty())
	{
		return false;
	}

	UWorldBLDContextMenuRegistry* Registry = UWorldBLDContextMenuRegistry::Get();
	if (!IsValid(Registry))
	{
		return false;
	}

	auto ResolveTarget = [](AActor* Actor) -> AActor*
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}

		if (Actor->GetClass()->ImplementsInterface(UWorldBLDContextMenuProvider::StaticClass()))
		{
			return IWorldBLDContextMenuProvider::Execute_GetContextMenuTargetActor(Actor);
		}

		return Actor;
	};

	AActor* FirstTarget = ResolveTarget(SelectedActors[0]);
	if (!IsValid(FirstTarget))
	{
		return false;
	}

	// A selection is "context-menu relevant" if every selected actor resolves to a valid target actor that has a factory.
	// We allow mixed selection types here (handled as an error state in the menu UI).
	const TScriptInterface<IWorldBLDContextMenuFactory> FirstFactory = Registry->FindFactoryForActor(FirstTarget);
	const IWorldBLDContextMenuFactory* FirstFactoryInterface = FirstFactory.GetInterface();
	if ((FirstFactoryInterface == nullptr) || !FirstFactoryInterface->CanCreateContextMenuForActor(FirstTarget))
	{
		return false;
	}

	for (AActor* Actor : SelectedActors)
	{
		AActor* TargetActor = ResolveTarget(Actor);
		if (!IsValid(TargetActor))
		{
			return false;
		}

		const TScriptInterface<IWorldBLDContextMenuFactory> Factory = Registry->FindFactoryForActor(TargetActor);
		const IWorldBLDContextMenuFactory* FactoryInterface = Factory.GetInterface();
		if ((FactoryInterface == nullptr) || !FactoryInterface->CanCreateContextMenuForActor(TargetActor))
		{
			return false;
		}
	}

	return true;
}

void UWorldBLDEdMode::ShowContextMenuForActors(const TArray<AActor*>& Actors)
{
	if (Actors.IsEmpty())
	{
		HideContextMenu();
		return;
	}

	// Context menus should only show for exempt controller classes (default controller or
	// main controller of one of our tool palettes).
	if (!WorldBLDEditorContextMenu::IsControllerAllowedToShowContextMenu(this, GetActiveEditController()))
	{
		HideContextMenu();
		return;
	}

	auto ResolveTarget = [](AActor* Actor) -> AActor*
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}

		if (Actor->GetClass()->ImplementsInterface(UWorldBLDContextMenuProvider::StaticClass()))
		{
			return IWorldBLDContextMenuProvider::Execute_GetContextMenuTargetActor(Actor);
		}

		return Actor;
	};

	auto MakeErrorContent = [](const FText& Message) -> TSharedRef<SWidget>
	{
		return SNew(SBorder)
			.Padding(FMargin(8.0f))
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(Message)
			];
	};

	TArray<AActor*> TargetActors;
	TargetActors.Reserve(Actors.Num());

	for (AActor* Actor : Actors)
	{
		if (AActor* Target = ResolveTarget(Actor); IsValid(Target))
		{
			TargetActors.AddUnique(Target);
		}
	}

	if (TargetActors.IsEmpty())
	{
		HideContextMenu();
		return;
	}

	// If we already have an active menu for this exact selection, keep it.
	if (HasActiveContextMenu() && AreSameActorSelection(Actors, ContextMenuTargetActors))
	{
		return;
	}

	// If we have a menu but for a different selection, close it first.
	if (HasActiveContextMenu())
	{
		HideContextMenu();
	}

	UWorldBLDContextMenuRegistry* Registry = UWorldBLDContextMenuRegistry::Get();
	if (!IsValid(Registry))
	{
		return;
	}

	// Determine whether the selection resolves to a single supported target type (factory), or is mixed.
	AActor* FirstTargetActor = TargetActors[0];
	const TScriptInterface<IWorldBLDContextMenuFactory> FirstFactory = Registry->FindFactoryForActor(FirstTargetActor);
	const IWorldBLDContextMenuFactory* FirstFactoryInterface = FirstFactory.GetInterface();

	bool bMixedTargetTypes = false;

	if ((FirstFactoryInterface == nullptr) || !FirstFactoryInterface->CanCreateContextMenuForActor(FirstTargetActor))
	{
		HideContextMenu();
		return;
	}

	UClass* CommonTargetClass = FirstTargetActor->GetClass();

	for (AActor* TargetActor : TargetActors)
	{
		if (!IsValid(TargetActor))
		{
			bMixedTargetTypes = true;
			break;
		}

		const TScriptInterface<IWorldBLDContextMenuFactory> Factory = Registry->FindFactoryForActor(TargetActor);
		const IWorldBLDContextMenuFactory* FactoryInterface = Factory.GetInterface();
		if ((FactoryInterface == nullptr) || !FactoryInterface->CanCreateContextMenuForActor(TargetActor))
		{
			bMixedTargetTypes = true;
			break;
		}

		// Mixed actor type selection is detected at the *target* actor level (after provider indirection).
		if (TargetActor->GetClass() != CommonTargetClass)
		{
			bMixedTargetTypes = true;
			break;
		}
	}

	TSharedRef<SWidget> ContentWidget = SNullWidget::NullWidget;
	FText Title = FText::GetEmpty();
	AActor* MenuPositionActor = FirstTargetActor;

	if (bMixedTargetTypes)
	{
		Title = LOCTEXT("MixedSelectionTitle", "Selection");
		ContentWidget = MakeErrorContent(LOCTEXT("MixedSelectionError", "Mixed actor types selected.\nPlease select actors that resolve to a single supported target type."));
	}
	else
	{
		// Use display name (adds spaces like "CityBlock" -> "City Block" and respects DisplayName metadata).
		Title = CommonTargetClass->GetDisplayNameText();
		ContentWidget = FirstFactoryInterface->CreateContextMenuWidget(TargetActors);
		MenuPositionActor = FirstTargetActor;
	}

	TSharedRef<SWorldBLDContextMenu> ContextMenuWidget =
		SNew(SWorldBLDContextMenu)
		.Title(Title)
		.Content(ContentWidget)
		.TargetActor(MenuPositionActor)
		.TargetActors(TargetActors)
		.OnRequestClose(FSimpleDelegate::CreateUObject(this, &UWorldBLDEdMode::HideContextMenu))
		;

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	const TSharedPtr<ILevelEditor> PinnedEditor = LevelEditor.GetLevelEditorInstance().Pin();
	if (!PinnedEditor.IsValid())
	{
		return;
	}

	const TSharedPtr<SLevelViewport> ActiveViewport = PinnedEditor->GetActiveViewportInterface();
	if (!ActiveViewport.IsValid())
	{
		return;
	}

	const TSharedRef<SWorldBLDContextMenuViewportOverlay> Overlay =
		SNew(SWorldBLDContextMenuViewportOverlay)
		.Padding(FMargin(24.0f))
		.Panel(ContextMenuWidget);

	ActiveViewport->AddOverlayWidget(Overlay);
	ActiveContextMenuHostViewport = ActiveViewport;
	ActiveContextMenuOverlayWidget = Overlay;

	ContextMenuTargetActor = MenuPositionActor;
	ContextMenuTargetActors.Reset();
	for (AActor* Actor : Actors)
	{
		ContextMenuTargetActors.Add(Actor);
	}
}

bool UWorldBLDEdMode::ValidateContextMenuTargets() const
{
	if (!HasActiveContextMenu() || ContextMenuTargetActors.IsEmpty() || (GEditor == nullptr))
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& WeakActor : ContextMenuTargetActors)
	{
		if (!WeakActor.IsValid())
		{
			return false;
		}
	}

	USelection* Selected = GEditor->GetSelectedActors();
	if (Selected == nullptr)
	{
		return false;
	}

	TArray<AActor*> CurrentSelection;
	for (FSelectionIterator It(*Selected); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			CurrentSelection.Add(Actor);
		}
	}

	return !CurrentSelection.IsEmpty() && AreSameActorSelection(CurrentSelection, ContextMenuTargetActors);
}

void UWorldBLDEdMode::SwitchToMainWorldBLDWidget()
{
	InstallDefaultSelectionTool();
}

void UWorldBLDEdMode::PushEditController(UWorldBLDEditController* Controller)
{


	if (IsValid(Controller))
	{
		ToolModeStack.Remove(Controller);
		if (ToolModeStack.Num() > 0)
		{
			ToolModeStack.Last()->ToolResign();
		}
		ToolModeStack.Add(Controller);
		
		UBlueprint* ToolBlueprint = UBlueprint::GetBlueprintFromClass(Controller->GetClass());
		if (IsValid(ToolBlueprint))
		{
			ToolBlueprint->OnCompiled().AddUObject(this, &UWorldBLDEdMode::RegenerateContents);
		}
		
		Controller->AddToEditorLevelViewport();
		Controller->ToolBegin();

		// Hide any active context menu while a non-exempt controller is active (to prevent interference).
		// Note: HideContextMenu() clears bContextMenuHiddenByTool, so preserve our "hidden by tool" intent after hiding.
		if (!WorldBLDEditorContextMenu::IsControllerAllowedToShowContextMenu(this, Controller))
		{
			const bool bHadMenu = HasActiveContextMenu();
			HideContextMenu();
			bContextMenuHiddenByTool = bHadMenu;
		}
	}
}

void UWorldBLDEdMode::PopEditController(UWorldBLDEditController* OptionalController)
{
	if (IsValid(OptionalController))
	{
		OptionalController->ToolResign();
		OptionalController->RemoveFromEditorLevelViewport();
		OptionalController->RemoveFromParent();
		ToolModeStack.Remove(OptionalController);
	}
	else if (ensure(!ToolModeStack.IsEmpty()))
	{
		if (IsValid(ToolModeStack.Last()))
		{
			ToolModeStack.Last()->ToolResign();
			ToolModeStack.Last()->RemoveFromEditorLevelViewport();
			ToolModeStack.Last()->RemoveFromParent();
		}
		ToolModeStack.Pop();
	}

	// New tool on top of stack gets a notification.
	if ((ToolModeStack.Num() > 0) && IsValid(ToolModeStack.Last()))
	{
		ToolModeStack.Last()->ToolActivate();
	}

	// If a tool previously hid the context menu, re-evaluate after popping.
	if (bContextMenuHiddenByTool)
	{
		bContextMenuHiddenByTool = false;

		const UWorldBLDEditController* NewTop = GetActiveEditController();
		if (WorldBLDEditorContextMenu::IsControllerAllowedToShowContextMenu(this, NewTop))
		{
			TArray<AActor*> SelectedActors;
			if (GEditor && GEditor->GetSelectedActors())
			{
				for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
				{
					if (AActor* Actor = Cast<AActor>(*It))
					{
						SelectedActors.Add(Actor);
					}
				}
			}

			if (IsContextMenuRelevantForSelection(SelectedActors))
			{
				ShowContextMenuForActors(SelectedActors);
			}
			else
			{
				HideContextMenu();
			}
		}
	}
}

void UWorldBLDEdMode::ClearToolModeStack()
{
	while (!ToolModeStack.IsEmpty())
	{
		UWorldBLDEditController* Controller = ToolModeStack.Last();
		if (IsValid(Controller))
		{
			Controller->ToolExit(/* Force? */ true);
			if (ToolModeStack.Contains(Controller))
			{
				PopEditController(Controller);
			}
		}
		else
		{
			PopEditController(Controller);
		}

		UUserWidget* WidgetVersion = Cast<UUserWidget>(Controller);
		DestructWidget(WidgetVersion);
	}
}

void UWorldBLDEdMode::InstallDefaultSelectionTool()
{
	ClearToolModeStack();

	UUserWidget* NewTool {nullptr};
	TSoftClassPtr<UUserWidget> LoadedToolClass = UWorldBLDDefaultEditController::StaticClass();

	// Default tool can be overridden in BP config; treat missing packages (eg optional plugins) as "not configured".
	if (!DefaultToolClass.IsNull())
	{
		const FSoftObjectPath DefaultToolPath = DefaultToolClass.ToSoftObjectPath();
		const FString DefaultToolPackageName = DefaultToolPath.GetLongPackageName();
		if (DefaultToolPackageName.IsEmpty() || FPackageName::DoesPackageExist(DefaultToolPackageName))
		{
			if (const TSubclassOf<UWorldBLDEditController> SuppliedTool = DefaultToolClass.LoadSynchronous())
			{
				LoadedToolClass = SuppliedTool;
			}
		}
	}

	TryConstructUserWidget(NewTool, LoadedToolClass, /* Force */ true);

	if (ensure(IsValid(NewTool)))
	{
		PushEditController(Cast<UWorldBLDEditController>(NewTool));
	}
	
}

bool UWorldBLDEdMode::IsLicensed(const FString& Credentials, const FString& ToolName) const
{
	(void)Credentials;

	if (ToolName.IsEmpty() || (GEditor == nullptr))
	{
		return false;
	}

	if (const UWorldBLDAuthSubsystem* AuthSubsystem = GEditor->GetEditorSubsystem<UWorldBLDAuthSubsystem>())
	{
		return AuthSubsystem->HasLicenseForTool(ToolName);
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

FWorldBLDEdModeToolkit::FWorldBLDEdModeToolkit()
{
    // Empty
}

FName FWorldBLDEdModeToolkit::GetToolkitFName() const 
{ 
    return FName("WorldBLDEdMode"); 
}

FText FWorldBLDEdModeToolkit::GetBaseToolkitName() const 
{ 
    return NSLOCTEXT("WorldBLDEditorToolkit", "DisplayName", "City Builder"); 
}

TSharedPtr<class SWidget> FWorldBLDEdModeToolkit::GetInlineContent() const 
{
    return EditModeRef.IsValid() ? TSharedPtr<SWidget>(EditModeRef->TabWidget) : nullptr;
}

TSharedPtr<SDockTab> FWorldBLDEdModeToolkit::GetPrimaryTab() const
{
	return PrimaryTab.Pin();
}

void FWorldBLDEdModeToolkit::InvokeUI()
{
	Super::InvokeUI();
	HACK_RemoveScrollView();
	if (EditModeRef.IsValid())
	{
		EditModeRef->InstallDefaultSelectionTool();
	}

	TSharedPtr<SDockTab> CreatedTab = GetPrimaryTab();
	if (CreatedTab.IsValid())
	{
		// Close existing tabs in the Left Mode Panel
		TSharedPtr<SDockingTabStack> DockingTabStack = CreatedTab->GetParentDockTabStack();
		const TSlotlessChildren<SDockTab>& Tabs = DockingTabStack->GetTabs();
		for (int i = Tabs.Num() - 1; i >= 0; i--)
		{
			if (Tabs[i] != CreatedTab)
			{
				Tabs[i]->RequestCloseTab();
			}
		}

		// We have to setup the tab's size manually by passing it through Paint(),
		// otherwise, while the tabwell is hidden, the tab will be zero size and fail to dock to the sidebar
		FSlateRect AreaRect = CreatedTab->GetDockArea()->GetPaintSpaceGeometry().GetLayoutBoundingRect();
		FSlateRect LeftRect = FSlateRect(AreaRect.Left, AreaRect.Top, AreaRect.Right / 2, AreaRect.Bottom);
		FVector2f LeftSize = LeftRect.GetSize2f();
		UE::Slate::FDeprecateVector2DParameter Vec2dParamZero(0.0, 0.0);
		UE::Slate::FDeprecateVector2DParameter Vec2dParamSize(LeftSize.X, LeftSize.Y);
		FGeometry Geometry(Vec2dParamZero, Vec2dParamZero, Vec2dParamSize, 1.0f);
		
		TSharedPtr<SWidget> ParentWidget = CreatedTab->GetParentWidget();
		if (ParentWidget.IsValid())
		{
			FHittestGrid Grid;
			FPaintArgs PaintArgs(ParentWidget.Get(), Grid, Vec2dParamZero, 0.0, 1.0);
			FSlateWindowElementList SlateWindowElementList(CreatedTab->GetParentWindow());

			CreatedTab->Paint(PaintArgs, Geometry, FSlateRect(), SlateWindowElementList, 0, FWidgetStyle(), false);
			
			FSlateRect TabRect = CreatedTab->GetPaintSpaceGeometry().GetLayoutBoundingRect();
			bool bOverlapsLeft = false;
			FSlateRect LeftOverlap = LeftRect.IntersectionWith(TabRect, bOverlapsLeft);

		}

		CreatedTab->SetParentDockTabStackTabWellHidden(true);
		CreatedTab->SetShouldAutosize(true);
	}

}

void FWorldBLDEdModeToolkit::HACK_RemoveScrollView() const
{
	// NOTE: In BaseToolkit.cpp the InlineContentHolder puts a scroll view around GetInlineContent().
	//       However, this prevents internal scroll views in the content from working. So directly override
	//       this behavior by just directly inserting the widget in there.
	if (InlineContentHolder.IsValid())
	{
		if (const TSharedPtr<SWidget> InlineContent = GetInlineContent())
		{
			InlineContentHolder->SetContent(InlineContent.ToSharedRef());
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool UWorldBLDEdMode::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	if (const UWorldBLDEditController* Controller = GetActiveEditController())
	{
		return Controller->AllowStandardSelectionControls() || (Controller->ActiveBrushActor == InActor);
	}
	return true;
}

bool FWorldBLDSelectModeWidgetHelper::AllowWidgetMove()
{
	if (const UWorldBLDEditController* Controller = GetActiveController())
	{
		return Controller->AllowStandardSelectionControls();
	}
	return Super::AllowWidgetMove();
}

bool FWorldBLDSelectModeWidgetHelper::CanCycleWidgetMode() const
{
	if (const UWorldBLDEditController* Controller = GetActiveController())
	{
		return Controller->AllowStandardSelectionControls();
	}
	return Super::CanCycleWidgetMode();
}

bool FWorldBLDSelectModeWidgetHelper::ShowModeWidgets() const
{
	if (const UWorldBLDEditController* Controller = GetActiveController())
	{
		return Controller->AllowStandardSelectionControls();
	}
	return Super::ShowModeWidgets();
}

bool FWorldBLDSelectModeWidgetHelper::ShouldDrawWidget() const
{
	bool bDoDefaultBehavior = true;
	if (const UWorldBLDEditController* Controller = GetActiveController())
	{
		bDoDefaultBehavior = Controller->AllowStandardSelectionControls();
	}

	if (GCurrentLevelEditingViewportClient && bDoDefaultBehavior)
	{
		const FTypedElementListConstRef ElementsToManipulate = GCurrentLevelEditingViewportClient->GetElementsToManipulate();
		return ElementsToManipulate->Num() > 0;
	}
	return false;
}

bool FWorldBLDSelectModeWidgetHelper::UsesTransformWidget() const
{
	if (const UWorldBLDEditController* Controller = GetActiveController())
	{
        // If controller requests a custom widget, we always use transform widget
        if (Controller->WantsCustomTransformWidget())
        {
            return true;
        }
        return Controller->AllowStandardSelectionControls();
	}
	return Super::UsesTransformWidget();
}

// Note: FLegacyEdModeWidgetHelper does not have a mode-parameter overload in all engine versions,
// so we only override the parameterless UsesTransformWidget above.

bool FWorldBLDSelectModeWidgetHelper::UsesPropertyWidgets() const
{
	if (const UWorldBLDEditController* Controller = GetActiveController())
	{
		return Controller->AllowStandardSelectionControls();
	}
	return true;
}

EAxisList::Type FWorldBLDSelectModeWidgetHelper::GetWidgetAxisToDraw(UE::Widget::EWidgetMode CheckMode) const
{
    if (const UWorldBLDEditController* Controller = GetActiveController())
    {
        if (Controller->WantsCustomTransformWidget())
        {
            return Controller->GetWidgetAxisMask();
        }
    }
    return Super::GetWidgetAxisToDraw(CheckMode);
}

FVector FWorldBLDSelectModeWidgetHelper::GetWidgetLocation() const
{
    if (const UWorldBLDEditController* Controller = GetActiveController())
    {
        if (Controller->WantsCustomTransformWidget())
        {
            return Controller->GetDesiredWidgetLocation();
        }
    }
    return Super::GetWidgetLocation();
}

void FWorldBLDSelectModeWidgetHelper::ActorSelectionChangeNotify()
{
	if (UWorldBLDEdMode* EdMode = UWorldBLDEdMode::GetWorldBLDEdMode())
	{
		if (UWorldBLDEditController* Controller = GetActiveController())
		{
			int Index = EdMode->ToolModeStack.Find(Controller);
			while (Index >= 0)
			{
				Controller = EdMode->ToolModeStack[Index];				
				if (!Controller->ForceEditorSelectionToActiveBrushActor())
				{
					Controller->OnActorSelectionSetChanged();
				}				
				if (Controller->ConsumeSelectionControls())
				{
					break;
				}
				Index -= 1;
			}			
		}

		// Context menu integration: only show for exempt controller classes.
		const UWorldBLDEditController* ActiveController = GetActiveController();
		if (!WorldBLDEditorContextMenu::IsControllerAllowedToShowContextMenu(EdMode, ActiveController))
		{
			EdMode->HideContextMenu();
			return;
		}

		// If our existing targets are stale (deleted, deselected), close the window.
		if (EdMode->HasActiveContextMenu() && !EdMode->ValidateContextMenuTargets())
		{
			EdMode->HideContextMenu();
		}

		TArray<AActor*> SelectedActors;
		if (GEditor && GEditor->GetSelectedActors())
		{
			for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
			{
				if (AActor* Actor = Cast<AActor>(*It))
				{
					SelectedActors.Add(Actor);
				}
			}
		}

		if (SelectedActors.IsEmpty())
		{
			EdMode->HideContextMenu();
			return;
		}

		// Avoid re-creating if selection hasn't changed.
		bool bSelectionMatchesExisting = false;
		if (EdMode->HasActiveContextMenu())
		{
			bSelectionMatchesExisting = UWorldBLDEdMode::AreSameActorSelection(SelectedActors, EdMode->ContextMenuTargetActors);
		}

		if (!bSelectionMatchesExisting)
		{
			if (EdMode->IsContextMenuRelevantForSelection(SelectedActors))
			{
				EdMode->ShowContextMenuForActors(SelectedActors);
			}
			else
			{
				EdMode->HideContextMenu();
			}
		}
	}
}

UWorldBLDEditController* FWorldBLDSelectModeWidgetHelper::GetActiveController() const
{
	UWorldBLDEditController* Controller = nullptr;
	if (const UWorldBLDEdMode* EdMode = UWorldBLDEdMode::GetWorldBLDEdMode())
	{
		Controller = EdMode->GetActiveEditController();
	}
	return Controller;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void UWorldBLDEdModeLicenseSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnLicenseChanged.Broadcast(this, LicenseFile);
}

void UWorldBLDEdModeLicenseSettings::ResetLicense()
{
	LicenseFile.FilePath = TEXT("");
	UWorldBLDKitElementUtils::SaveConfigObject(this);
	OnLicenseChanged.Broadcast(this, LicenseFile);
}


#undef LOCTEXT_NAMESPACE

