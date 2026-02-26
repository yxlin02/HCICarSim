// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "Tools/LegacyEdModeWidgetHelpers.h"
#include "Toolkits/BaseToolkit.h"

#include "Blueprint/UserWidget.h"
#include "UnrealEdMisc.h"
#include "WorldBLDEditorToolkit.generated.h"

WORLDBLDEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogWorldBLDEditor, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldBLDKitListChanged, class UWorldBLDEdMode*, EdMode);

class AWorldBLDKitBase;
class AActor;
class SLevelViewport;
class SWidget;
class UWorldBLDKitBundle;
class UWorldBLDToolPaletteWidget;
class UWorldBLDMainWidget;

USTRUCT(BlueprintType)
struct FPluginToolData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="Available Tool")
    FString ToolName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="Available Tool")
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="Available Tool")
    FString DownloadLink;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="Available Tool")
    FString Version;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="Available Tool")
    FString SHA1;
};

UENUM(BlueprintType)
enum class EWorldBLDToolUpdateState : uint8
{
	/** Tool/plugin is not currently installed in this project. */
	NotInstalled UMETA(DisplayName="Not Installed"),

	/** Tool/plugin is installed, but we don't have a reliable version string to compare against the server. */
	InstalledUnknownSha1 UMETA(DisplayName="Installed (Unknown Version)"),

	/** Tool/plugin is installed and the installed version matches (or exceeds) the server version. */
	UpToDate UMETA(DisplayName="Up To Date"),

	/** Tool/plugin is installed and the server version is newer than the installed version. */
	UpdateAvailable UMETA(DisplayName="Update Available")
};
///////////////////////////////////////////////////////////////////////////////////////////////////

// The root of the Edit Mode for the WorldBLDEditorToolkit. This is mostly a place to expose functionality
// to a Blueprint (see UWorldBLDEdModeSettings::EditModeClass) that actually is the real Editor Mode.
UCLASS(BlueprintType, Blueprintable, Abstract)
class WORLDBLDEDITOR_API UWorldBLDEdMode : public UBaseLegacyWidgetEdMode
{
	GENERATED_BODY()

/* friends: */
    friend class FWorldBLDEdModeToolkit;
    friend class UWorldBLDMainWidget;
    friend class UWorldBLDKitEditorUtils;
    friend class FWorldBLDSelectModeWidgetHelper;

public:
    static const FEditorModeID EditorModeID;

	UWorldBLDEdMode();

    ///////////////////////////////////////////////////////////////////////////////////////////////

    UFUNCTION(BlueprintPure, Category="WorldBLDEditor")
    bool IsThisEdModeActive() const;

    UFUNCTION(BlueprintPure, Category="WorldBLDEditor")
    static UWorldBLDEdMode* GetWorldBLDEdMode();

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Tool update helpers (version-based)

	/** Returns the SHA1 that was last recorded as installed for ToolId (empty if unknown). */
	UFUNCTION(BlueprintPure, Category="WorldBLD|ToolUpdates")
	FString GetRecordedInstalledToolSha1(const FString& ToolId) const;

	/** Returns update state for ToolId using installed plugin version and (optionally) server SHA1 metadata. */
	UFUNCTION(BlueprintPure, Category="WorldBLD|ToolUpdates")
	EWorldBLDToolUpdateState GetToolUpdateState(const FString& ToolId, const FString& ServerVersion, const FString& ServerSHA1) const;

	/** Triggers a refresh of the server tool list and re-evaluates update state. */
	UFUNCTION(BlueprintCallable, Category="WorldBLD|ToolUpdates")
	void RefreshAvailableToolsFromServer();

	/** Returns true if the locally installed WorldBLD plugin matches the latest server version (when known). */
	UFUNCTION(BlueprintPure, Category="WorldBLD|ToolUpdates")
	bool IsWorldBLDPluginUpToDate() const;

	/** Returns the WorldBLD tool id as reported by the server tools endpoint (empty if unknown). */
	const FString& GetServerWorldBLDToolId() const { return ServerWorldBLDToolId; }

    UFUNCTION(BlueprintPure, Category="WorldBLDEditor")
    UWorldBLDToolPaletteWidget* GetToolPaletteByClass(TSubclassOf<UWorldBLDToolPaletteWidget> ToolClass) const;

    UFUNCTION(BlueprintPure, Category="WorldBLDEditor")
    UWorldBLDToolPaletteWidget* GetActiveToolPalette() const;

    UFUNCTION(BlueprintPure, Category="WorldBLDEditor")
    TArray<UWorldBLDToolPaletteWidget*> GetAvailableToolPalettes() const;

    UFUNCTION(BlueprintCallable, Category="WorldBLDEditor")
    void SwitchToToolPaletteByClass(TSubclassOf<UWorldBLDToolPaletteWidget> ToolClass);

    UFUNCTION(BlueprintCallable, Category="WorldBLDEditor")
    void SwitchToToolPalette(UWorldBLDToolPaletteWidget* Palette, bool bForceFocused = false) const;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    // The user interface for this Edit Mode. Will be spawned a supplied as the "inline content" of this widget.
    // This interface will be given instances of `CustomToolClasses` to display.
    UPROPERTY(EditDefaultsOnly, BlueprintReadonly, Category="Config", Meta=(Untracked))
    TSoftClassPtr<UWorldBLDMainWidget> UserInterfaceClass;

    // Custom tools used by the Edit Mode.
    UPROPERTY(EditDefaultsOnly, BlueprintReadonly, Category="Config", Meta=(Untracked))
    TArray<TSoftClassPtr<UWorldBLDToolPaletteWidget>> CustomToolClasses;

    UPROPERTY(EditDefaultsOnly, BlueprintReadonly, Category="Config", Meta=(Untracked))
    TSoftClassPtr<class UWorldBLDEditController> DefaultToolClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadonly, Category="Config", Meta=(Untracked))
    bool bCreateToolkit {false};

    UPROPERTY(BlueprintReadOnly, Category="WorldBLDEditor", Meta=(Untracked))
    TArray<FPluginToolData> AvailableTools;
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // Registers a new actively used WorldBLD Kit. Triggers `OnWorldBLDKitListChanged()`.
    UFUNCTION(BlueprintCallable, Category="WorldBLD|Kits")
    void RegisterWorldBLDKit(AWorldBLDKitBase* Kit);

    // Unregisters a WorldBLD Kit. Triggers `OnWorldBLDKitListChanged()`.
    UFUNCTION(BlueprintCallable, Category="WorldBLD|Kits")
    void UnregisterWorldBLDKit(AWorldBLDKitBase* Kit);

    // Removes all registered WorldBLDKits
    UFUNCTION(BlueprintCallable, Category="WorldBLD|Kits")
    void ResetRegisteredWorldBLDKits();

    UFUNCTION(BlueprintPure, Category="WorldBLD|Kits")
    int32 GetLoadedWorldBLDKitCount() const { return LoadedWorldBLDKits.Num(); }

	/** Read-only access for systems that need to inspect loaded kits (eg asset filtering). */
	const TArray<AWorldBLDKitBase*>& GetLoadedWorldBLDKits() const { return LoadedWorldBLDKits; }

    // Called when in-map spawned WorldBLD kits have changed
    UPROPERTY(BlueprintAssignable, Category="WorldBLD|Kits")
    FOnWorldBLDKitListChanged OnWorldBLDKitListChanged;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    void RegisterWorldBLDKitBundle(UWorldBLDKitBundle* Bundle);
    void UnregisterWorldBLDKitBundle(UWorldBLDKitBundle* Bundle);

	int32 GetLoadedWorldBLDKitBundleCount() const { return LoadedWorldBLDKitBundles.Num(); }

	/** Read-only access for systems that need to inspect loaded bundles (eg asset filtering). */
	const TArray<UWorldBLDKitBundle*>& GetLoadedWorldBLDKitBundles() const { return LoadedWorldBLDKitBundles; }

    // Called when available WorldBLD Kit classes have changed
    UPROPERTY(BlueprintAssignable, Category="WorldBLD|Kits")
    FOnWorldBLDKitListChanged OnWorldBLDKitBundleListChanged;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    void InstallDefaultRecompileHandler();

    UFUNCTION(BlueprintCallable, Category="WorldBLDEditor")
    void MovePrimaryTabToSidebar() const;

    UFUNCTION(BlueprintCallable, Category="WorldBLDEditor")
    void RestorePrimaryTabFromSidebar() const;

    UFUNCTION(BlueprintPure, Category="WorldBLDEditor")
    bool IsPrimaryTabInSidebar() const;

    UFUNCTION(BlueprintCallable, Category="WorldBLDEditor")
    void TryOpenPrimaryTabSidebarDrawer() const;

    UFUNCTION(BlueprintCallable, Category="WorldBLDEditor")
    bool TryHideTabWell(bool bHide) const;

protected:
    // The spawned kits currently in the map
    UPROPERTY(Transient, BlueprintReadOnly, Category="WorldBLD|Kits", Meta=(Untracked))
    TArray<AWorldBLDKitBase*> LoadedWorldBLDKits;

    UFUNCTION()
    void OnKitDestroyed(AActor* DestroyedActor);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    void OnKitBundlesChanged();
    

    // These are WorldBLD Kit bundles available that could be used in a map
    UPROPERTY(Transient, BlueprintReadOnly, Category="WorldBLD|Kits", Meta=(Untracked))
    TArray<UWorldBLDKitBundle*> LoadedWorldBLDKitBundles;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    UPROPERTY(Transient, BlueprintReadOnly, Category="WorldBLD|Tool")
    TArray<UWorldBLDEditController*> ToolModeStack;

    void ClearToolModeStack();
    void InstallDefaultSelectionTool();

public:

    UFUNCTION(BlueprintPure, Category="WorldBLD|Tool")
    UWorldBLDEditController* GetActiveEditController() const;

    //BP callable version of the function to revert to the default tool
    UFUNCTION(BlueprintCallable, Category="WorldBLD|Tool")
    void SwitchToMainWorldBLDWidget();

    UFUNCTION(BlueprintCallable, Category="WorldBLD|Tool")
    void PushEditController(UWorldBLDEditController* Controller);

    UFUNCTION(BlueprintCallable, Category="WorldBLD|Tool")
    void PopEditController(UWorldBLDEditController* OptionalController = nullptr);

    UFUNCTION(BlueprintCallable, Category="WorldBLD|Tool")
    bool IsLicensed(const FString& Credentials, const FString& ToolName) const;
    
    ///////////////////////////////////////////////////////////////////////////////////////////////
	// Context Menu (floating window)

	/** Create and display the context menu for the given actor selection (expects same-class selection). */
	void ShowContextMenuForActors(const TArray<AActor*>& Actors);

	/** Close the context menu window (if any) and clear tracking state. */
	void HideContextMenu();

	/** Returns true if a context menu window is currently active. */
	bool HasActiveContextMenu() const;

	/** Returns true if the given selection should show a context menu (provider + factory + can-create). */
	bool IsContextMenuRelevantForSelection(const TArray<AActor*>& SelectedActors) const;

    virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
    virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override;    
    virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override;

public:
    // >> UEdMode
    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
    virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
protected:
	virtual void CreateToolkit() override;
    // << UEdMode

    void OnBlueprintCompiled(UBlueprint* Blueprint);

public:
    // >> UBaseLegacyWidgetEdMode
    virtual bool UsesToolkits() const override;
    virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;    
protected:
    virtual TSharedRef<FLegacyEdModeWidgetHelper> CreateWidgetHelper() override;
    // << UBaseLegacyWidgetEdMode

protected:
    UUserWidget* TryConstructInlineContent(bool bForce = false);
    void TryConstructUserWidget(UUserWidget*& WidgetRef, TSoftClassPtr<UUserWidget> WidgetClass, bool bForce = false);
    void DestructWidget(UUserWidget*& WidgetRef);
    void DestroyAllWidgets();
    void UpdateFocusWidget(bool bForce = false);
    void ConstructCustomToolWidgets(bool bForce = false);

    void RegenerateContents(UBlueprint* RecompiledBlueprint);
    void OnMapChanged(UWorld* World, EMapChangeType MapChangeType);

    void OnActorSpawned(AActor* InActor);
    void OnActorDeleted(AActor* InActor);

    void PostEditorUndo(const struct FTransactionContext& TransactionContext, bool bSucceeded) const;
    void PostEditorRedo(const struct FTransactionContext& TransactionContext, bool bSucceeded) const;

private:
	void EnsureLumenPostProcessVolumeExists();
    void FetchAvailableTools();
	void EvaluateWorldBLDPluginVersionAndNotify() const;

	// Context menu helpers
	bool ValidateContextMenuTargets() const;
	static bool AreSameActorSelection(const TArray<AActor*>& Actors, const TArray<TWeakObjectPtr<AActor>>& WeakActors);

	// Context menu state
	TWeakPtr<SLevelViewport> ActiveContextMenuHostViewport;
	TSharedPtr<SWidget> ActiveContextMenuOverlayWidget;
	TWeakObjectPtr<AActor> ContextMenuTargetActor;
	TArray<TWeakObjectPtr<AActor>> ContextMenuTargetActors;
	bool bContextMenuHiddenByTool = false;

	// Warning tooltip overlay (follows mouse cursor, shows active controller's UserWarning)
	void ShowWarningTooltipOverlay();
	void HideWarningTooltipOverlay();
	TSharedPtr<SWidget> WarningTooltipOverlayWidget;
	TWeakPtr<SLevelViewport> WarningTooltipHostViewport;

    UPROPERTY(Transient)
    UUserWidget* FocusInlineWidget {nullptr};
    TSharedRef<class SWidget> TabWidget {SNullWidget::NullWidget};
    UPROPERTY(Transient)
    TArray<UUserWidget*> CustomTools;

	// Cached server-side WorldBLD plugin version (from the tools API); used to warn when it differs from the local plugin version.
	FString ServerWorldBLDPluginVersion;
	// Cached server-side WorldBLD tool id (from the tools API); used to resolve requiredTools.toolId -> "WorldBLD" for asset gating.
	FString ServerWorldBLDToolId;
	bool bCheckWorldBLDPluginVersionOnNextToolsFetch = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

// These settings are loaded directly from the Plugin's INI file.
UCLASS(Config=WorldBLD)
class UWorldBLDEdModeSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:

    // The Edit Mode class used. This shouldn't change.
    UPROPERTY(Config, Meta=(Untracked))
    TSoftClassPtr<UWorldBLDEdMode> EditModeClass;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldBLDLicChanged, class UWorldBLDEdModeLicenseSettings*, Settings, const FFilePath&, FilePath);

UCLASS(Config = EditorPerProjectUserSettings, BlueprintType)
class UWorldBLDEdModeLicenseSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:

    // Point this to your CityBLD License File (.lic) generated from worldbld.com
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "WorldBLD", Meta = (FilePathFilter = "WorldBLD License File (*.lic)|*.lic"))
    FFilePath LicenseFile;

    UFUNCTION(BlueprintCallable, Category = "WorldBLD")
    void ResetLicense();

    UPROPERTY(BlueprintAssignable)
    FOnWorldBLDLicChanged OnLicenseChanged;

    virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
};

///////////////////////////////////////////////////////////////////////////////////////////////////


class FWorldBLDEdModeToolkit: public FModeToolkit
{
    using Super = FModeToolkit;
public:

    FWorldBLDEdModeToolkit();

    // >> IToolkit
    virtual FName GetToolkitFName() const override;
    virtual FText GetBaseToolkitName() const override;
    virtual TSharedPtr<class SWidget> GetInlineContent() const override;
    virtual void InvokeUI() override;
    // << IToolkit

    TSharedPtr<SDockTab> GetPrimaryTab() const;

    TWeakObjectPtr<UWorldBLDEdMode> EditModeRef;
    void HACK_RemoveScrollView() const;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

