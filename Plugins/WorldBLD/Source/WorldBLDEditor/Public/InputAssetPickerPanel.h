// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "Misc/Attribute.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "AssetRegistry/AssetData.h"
#include "CollectionManagerTypes.h"
#include "AssetThumbnail.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "IContentBrowserSingleton.h"

#include "Blueprint/UserWidget.h"

#include "InputAssetPanelTypes.h"

// Forward declarations
class FAssetThumbnailPool;
class FAssetThumbnail;
class SBorder;
class SBox;
class ITableRow;
class STableViewBase;
class SInputAssetPicker;
class SCheckBox;
class UWorldBLDEdMode;

#include "InputAssetPickerPanel.generated.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * SInputAssetPickerPanel is an "always open" version of SInputAssetComboPanel.
 * It displays the asset picker panel content directly without a combo button wrapper,
 * making it suitable for use as MenuContent in WorldBLDMenuAnchor widgets.
 * 
 * Features:
 * - Asset picker tile view (SInputAssetPicker)
 * - Optional recent assets list (via IRecentAssetsProvider)
 * - Optional collection set filter buttons
 * - Direct display without flyout/popup behavior
 */
class SInputAssetPickerPanel : public SCompoundWidget
{
	friend class UInputAssetPickerPanel;
public:

	DECLARE_DELEGATE_OneParam(FOnSelectedAssetChanged, const FAssetData& AssetData);
	DECLARE_DELEGATE_RetVal_TwoParams(FText, FOnGetAssetLabel, const FAssetData& AssetData, int32 Idx);

	/** List of Collections with associated Name, used to provide pickable Collection filters */
	struct FNamedCollectionList
	{
		FString Name;
		TArray<FCollectionNameType> CollectionNames;
	};

	/** IRecentAssetsProvider allows the Client to specify a set of "recently-used" Assets */
	class IRecentAssetsProvider
	{
	public:
		virtual ~IRecentAssetsProvider() {}
		virtual TArray<FAssetData> GetRecentAssetsList() = 0;
		virtual void NotifyNewAsset(const FAssetData& NewAsset) = 0;
	};

public:

	SLATE_BEGIN_ARGS( SInputAssetPickerPanel )
		: _TileSize(85, 85)
		, _PanelSize(600, 400)
		, _FilterSetup()
		, _OnSelectionChanged()
		, _InitiallySelectedAsset()
		, _AssetThumbnailLabel(EThumbnailLabel::NoLabel)
		, _bForceShowEngineContent(false)
		, _bForceShowPluginContent(false)
		, _bShowRecentAssets(true)
		, _bShowCollectionFilters(true)
	{}

		/** The size of the icon tiles in the picker */
		SLATE_ARGUMENT(FVector2D, TileSize)

		/** Size of the Panel */
		SLATE_ARGUMENT(FVector2D, PanelSize)

		/** UClass of Asset to pick or specific assets filter */
		SLATE_ARGUMENT( FAssetComboFilterSetup, FilterSetup )

		/** (Optional) external provider/tracker of Recently-Used Assets. If not provided, Recent Assets area will not be shown. */
		SLATE_ARGUMENT(TSharedPtr<IRecentAssetsProvider>, RecentAssetsProvider)

		/** (Optional) set of collection-lists, if provided, button bar will be shown with each CollectionSet as an option */
		SLATE_ARGUMENT(TArray<FNamedCollectionList>, CollectionSets)
			
		/** This delegate is executed each time the Selected Asset is modified */
		SLATE_EVENT( FOnSelectedAssetChanged, OnSelectionChanged )

		SLATE_EVENT( FOnGetAssetLabel, OnGetAssetLabel )

		/** Sets the asset selected by the widget before any user made selection occurs. */
		SLATE_ARGUMENT( FAssetData, InitiallySelectedAsset)

		/** Sets the type of label used for the asset picker tiles */
		SLATE_ARGUMENT( EThumbnailLabel::Type, AssetThumbnailLabel)

		/** Indicates if engine content should always be shown */
		SLATE_ARGUMENT(bool, bForceShowEngineContent)

		/** Indicates if plugin content should always be shown */
		SLATE_ARGUMENT(bool, bForceShowPluginContent)

		/** Whether to show the recent assets section (requires RecentAssetsProvider) */
		SLATE_ARGUMENT(bool, bShowRecentAssets)

		/** Whether to show collection filter buttons (requires CollectionSets) */
		SLATE_ARGUMENT(bool, bShowCollectionFilters)

	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/** Get the current filter setup */
	const FAssetComboFilterSetup& GetFilterSetup() const { return FilterSetup; }

	/** Update the filter setup and refresh the view */
	void SetFilterSetup(const FAssetComboFilterSetup& InFilterSetup);

	/** Get the currently selected asset */
	const FAssetData& GetSelectedAsset() const { return SelectedAsset; }

	/** Set the selected asset programmatically */
	void SetSelectedAsset(const FAssetData& InAsset);

protected:

	FVector2D TileSize;
	FVector2D PanelSize;
	FAssetComboFilterSetup FilterSetup;

	/** Delegate to invoke when selection changes. */
	FOnSelectedAssetChanged OnSelectionChanged;
	FOnGetAssetLabel OnGetAssetLabel;

	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TSharedPtr<class SInputAssetPicker> AssetPicker;

	TSharedPtr<IRecentAssetsProvider> RecentAssetsProvider;

	struct FRecentAssetInfo
	{
		int32 Index;
		FAssetData AssetData;
	};
	TArray<TSharedPtr<FRecentAssetInfo>> RecentAssetData;

	TArray<TSharedPtr<FAssetThumbnail>> RecentThumbnails;
	TArray<TSharedPtr<SBox>> RecentThumbnailWidgets;

	void UpdateRecentAssets();
	void RebuildPickerContent();

	virtual void NewAssetSelected(const FAssetData& AssetData);

	TSharedRef<ITableRow> OnGenerateWidgetForRecentList(TSharedPtr<FRecentAssetInfo> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	TArray<FNamedCollectionList> CollectionSets;
	int32 ActiveCollectionSetIndex = -1;
	TSharedRef<SWidget> MakeCollectionSetsButtonPanel(TSharedRef<SInputAssetPicker> AssetPickerView);

	EThumbnailLabel::Type AssetThumbnailLabel;
	bool bForceShowEngineContent;
	bool bForceShowPluginContent;
	bool bShowRecentAssets;
	bool bShowCollectionFilters;

	FAssetData SelectedAsset;

	/** Container for the picker content - allows rebuilding without reconstructing the entire widget */
	TSharedPtr<SBox> ContentContainer;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * UInputAssetPickerPanel is a UMG widget wrapper for SInputAssetPickerPanel.
 * It provides an always-visible asset picker panel suitable for use as
 * MenuContent in WorldBLDMenuAnchor widgets.
 * 
 * Unlike UInputAssetComboPanel, this widget does not have a combo button -
 * the asset picker is always displayed directly.
 */
UCLASS(meta=( DisplayName="Asset Picker Panel"), MinimalAPI)
class UInputAssetPickerPanel : public UWidget
{
	GENERATED_BODY()

public:

	UInputAssetPickerPanel(const FObjectInitializer& ObjectInitializer);

	/**
	 * Called when the selected item changes.
	 *
	 * - When bPresentSpecificObjects is true: SelectedItem/SelectionIdx map into SpecificObjects.
	 * - When bPresentSpecificObjects is false: SelectionIdx will be INDEX_NONE and SelectedClass will be the class of the selected asset.
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnPickerSelectionChangedEvent, UInputAssetPickerPanel*, Picker, UObject*, SelectedItem, UClass*, SelectedClass, int32, SelectionIdx, ESelectInfo::Type, SelectionType);

	//////////////////////////////////////////////////////////////////////////////////////////////////

	/** The size of the tiles in the asset picker */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Style, meta=(DisplayName="Tile Size"))
	FVector2D TileSize;

	/** The size of the picker panel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Style, meta=(DisplayName="Panel Size"))
	FVector2D PanelSize;

	//////////////////////////////////////////////////////////////////////////////////////////////////

	/** Whether to present specific objects instead of filtering by class */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Content")
	bool bPresentSpecificObjects {false};

	/**
	 * Class filter used when bPresentSpecificObjects is false.
	 * Default is UObject which effectively shows all asset types (recursive).
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Content")
	TSubclassOf<UObject> AssetClass;

	/**
	 * If true (and bPresentSpecificObjects is false), only show assets referenced (hard/soft/management)
	 * by the WorldBLD kits/bundles currently loaded in the active WorldBLD editor mode.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Content")
	bool bLoadedKitsOnly {false};

	/** The specific objects to present (when bPresentSpecificObjects is true) */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Content", DuplicateTransient, Transient)
	TArray<UObject*> SpecificObjects;

	//////////////////////////////////////////////////////////////////////////////////////////////////

	/** Set the specific objects to display in the picker */
	UFUNCTION(BlueprintCallable, Category="Content")
	void SetSpecificObjects(const TArray<UObject*>& Objects);

	/** Called when a new item is selected in the picker. */
	UPROPERTY(BlueprintAssignable, Category=Events)
	FOnPickerSelectionChangedEvent OnSelectionChanged;

	/** Called when we want to generate a label for the asset. */
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FGenerateLabelForAsset OnGenerateLabelForAsset;

	//////////////////////////////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category="Picker")
	void AddOption(UObject* Option);

	UFUNCTION(BlueprintCallable, Category="Picker")
	bool RemoveOption(UObject* Option);

	UFUNCTION(BlueprintCallable, Category="Picker")
	int32 FindOptionIndex(UObject* Option) const;

	UFUNCTION(BlueprintCallable, Category="Picker")
	UObject* GetOptionAtIndex(int32 Index) const;

	UFUNCTION(BlueprintCallable, Category="Picker")
	void ClearOptions();

	/** Resets the selection to nullptr (index 0). */
	UFUNCTION(BlueprintCallable, Category="Picker")
	void ClearSelection();

	UFUNCTION(BlueprintCallable, Category="Picker")
	void SetSelectedOption(UObject* Option);

	UFUNCTION(BlueprintCallable, Category="Picker")
	void SetSelectedIndex(const int32 Index);

	UFUNCTION(BlueprintCallable, Category="Picker")
	UObject* GetSelectedOption() const;

	UFUNCTION(BlueprintCallable, Category="Picker")
	int32 GetSelectedIndex() const;

	/** Returns the number of options. NOTE: There is always at least one option (nullptr) at the start. */
	UFUNCTION(BlueprintCallable, Category="Picker")
	int32 GetOptionCount() const;

	/** Refresh the picker display */
	UFUNCTION(BlueprintCallable, Category="Picker")
	void RefreshPicker();

protected:
	//~ UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	//~ End of UWidget interface

	//~ UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End of UVisual interface

	TSharedPtr<class SInputAssetPickerPanel> MyPicker;

	void HandleSelectionChanged(const FAssetData& Selection);

	UFUNCTION()
	void HandleWorldBLDKitsChanged(UWorldBLDEdMode* InEdMode);

	UFUNCTION()
	void HandleWorldBLDKitBundlesChanged(UWorldBLDEdMode* InEdMode);

	void BindToWorldBLDEdMode();
	void UnbindFromWorldBLDEdMode();

	UPROPERTY()
	UObject* SelectedObject {nullptr};

	UPROPERTY(Transient)
	TWeakObjectPtr<UWorldBLDEdMode> BoundEdMode;

	void UpdateInterfaceState();
};

///////////////////////////////////////////////////////////////////////////////////////////////////

