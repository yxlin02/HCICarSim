// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "Misc/Attribute.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "PropertyHandle.h"
#include "AssetViewWidgets.h"
#include "AssetRegistry/AssetData.h"
#include "CollectionManagerTypes.h"
#include "AssetThumbnail.h"

#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "AssetRegistry/ARFilter.h"
#include "IContentBrowserSingleton.h"
#include "SourcesData.h"
#include "SAssetView.h"

#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"

#include "InputAssetPanelTypes.h"

class SComboButton;
class FAssetThumbnailPool;
class FAssetThumbnail;
class SBorder;
class SBox;
class ITableRow;
class STableViewBase;
class SInputAssetPicker;

class FFrontendFilter_Text;
class FUICommandList;
class SAssetSearchBox;
class SAssetView;
class SComboButton;
class ICollectionContainer;
enum class ECheckBoxState : uint8;
class UWorldBLDEdMode;

#include "InputAssetComboPanel.generated.h"

class FContentBrowserItemData;
class FString;
class FText;
struct FAssetViewCustomColumn;

///////////////////////////////////////////////////////////////////////////////////////////////////

// A ComboBox specialized for picking a single asset shown in a Tile view.
UCLASS(meta=( DisplayName="ComboBox (Asset)"), MinimalAPI)
class UInputAssetComboPanel : public UWidget
{
    GENERATED_BODY()

public:

	UInputAssetComboPanel(const FObjectInitializer& ObjectInitializer);

	/**
	 * Called when the selected item changes.
	 *
	 * - When bPresentSpecificObjects is true: SelectedItem/SelectionIdx map into SpecificObjects.
	 * - When bPresentSpecificObjects is false: SelectionIdx will be INDEX_NONE and SelectedClass will be the class of the selected asset.
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnSelectionChangedEvent, UInputAssetComboPanel*, Combo, UObject*, SelectedItem, UClass*, SelectedClass, int32, SelectionIdx, ESelectInfo::Type, SelectionType);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOpeningEvent, UInputAssetComboPanel*, Combo);
   
   //////////////////////////////////////////////////////////////////////////////////////////////////

	/** The style. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category=Style, meta=( DisplayName="Style" ))
	FComboBoxStyle WidgetStyle;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Getter, Category=Style)
	FSlateFontInfo SelectionFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category=Content)
	FMargin ContentPadding;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Getter, Category=Style, meta=(DesignerRebuild))
	FSlateColor ForegroundColor;

	//////////////////////////////////////////////////////////////////////////////////////////////////

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Content")
	FText FallbackSelectionText;

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

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Content", DuplicateTransient, Transient)
	TArray<UObject*> SpecificObjects;

	//////////////////////////////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category="Content")
	void SetSpecificObjects(const TArray<UObject*>& Objects);

	/** Called when a new item is selected in the combobox. */
	UPROPERTY(BlueprintAssignable, Category=Events)
	FOnSelectionChangedEvent OnSelectionChanged;

	/** Called when the combobox is opening */
	UPROPERTY(BlueprintAssignable, Category=Events)
	FOnOpeningEvent OnOpening;

	/** Called when we want to generate a label for the asset. */
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FGenerateLabelForAsset OnGenerateLabelForAsset;

	//////////////////////////////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	void AddOption(UObject* Option);

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	bool RemoveOption(UObject* Option);

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	int32 FindOptionIndex(UObject* Option) const;

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	UObject* GetOptionAtIndex(int32 Index) const;

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	void ClearOptions();

	// Resets the selection to nullptr (index 0).
	UFUNCTION(BlueprintCallable, Category="ComboBox")
	void ClearSelection();

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	void SetSelectedOption(UObject* Option);

	UFUNCTION(BlueprintCallable, Category = "ComboBox")
	void SetSelectedIndex(const int32 Index);

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	UObject* GetSelectedOption() const;

	UFUNCTION(BlueprintCallable, Category="ComboBox")
	int32 GetSelectedIndex() const;

	/** Returns the number of options. NOTE: There is always at least one option (nullptr) at the start. */
	UFUNCTION(BlueprintCallable, Category="ComboBox")
	int32 GetOptionCount() const;

public:

	/** Get the style of the combobox. */
	const FComboBoxStyle& GetWidgetStyle() const;

	/** Set the style of the combobox. */
	void SetWidgetStyle(const FComboBoxStyle& InWidgetStyle);

	/** Get the default font for Combobox selection. */
	const FSlateFontInfo& GetSelectionFont() const;

	/** Set the padding for content. */
	void SetContentPadding(FMargin InPadding);

	/** Get the padding for content. */
	FMargin GetContentPadding() const;

	/** Get the foreground color of the button. */
	FSlateColor GetForegroundColor() const;

protected:
	//~ UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	//~ End of UWidget interface

	//~ UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End of UVisual interface

    TSharedPtr<class SInputAssetComboPanel> MyMenu;

	UFUNCTION()
	void HandleComboOpened();
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
	void UpdateFilterSetupOnly();
};

DECLARE_DELEGATE_RetVal_OneParam(FText, FOnGetBrowserItemText, const FAssetData& /*AssetItem*/);

/**
* SInputAssetComboPanel provides a similar UI to SComboPanel but
* specifically for picking Assets. The standard widget is a SComboButton
* that displays a thumbnail of the selected Asset, and on click a flyout
* panel is shown that has an Asset Picker tile view, as well as (optionally)
* a list of recently-used Assets, and also Collection-based filters.
* 
* Drag-and-drop onto the SComboButton is also supported, and the "selected Asset"
* can be mapped to/from a PropertyHandle. However note that a PropertyHandle is *not* required,
* each time the selection is modified the OnSelectionChanged(AssetData) delegate will also fire.
* 
* Note that "No Selection" is valid option by default
*/
// SEE: Engine\Plugins\Experimental\MeshModelingToolsetExp\Source\ModelingEditorUI\Public\ModelingWidgets\SToolInputAssetComboPanel.h
class SInputAssetComboPanel : public SCompoundWidget
{
	friend class UInputAssetComboPanel;
public:

	DECLARE_DELEGATE(FOnOpeningEvent);
	DECLARE_DELEGATE_OneParam(FOnSelectedAssetChanged, const FAssetData& AssetData);
	DECLARE_DELEGATE_RetVal_TwoParams(FText, FOnGetAssetLabel, const FAssetData& AssetData, int32 Idx);

	/** List of Collections with associated Name, used to provide pickable Collection filters */
	struct FNamedCollectionList
	{
		FString Name;
		TArray<FCollectionNameType> CollectionNames;
	};

	/** IRecentAssetsProvider allows the Client to specify a set of "recently-used" Assets which the SInputAssetComboPanel will try to update as the selected Asset changes */
	class IRecentAssetsProvider
	{
	public:
		virtual ~IRecentAssetsProvider() {}
		// SInputAssetComboPanel calls this to get the recent-assets list each time the flyout is opened
		virtual TArray<FAssetData> GetRecentAssetsList() = 0;
		// SInputAssetComboPanel calls this whenever the selected asset changes
		virtual void NotifyNewAsset(const FAssetData& NewAsset) = 0;
	};

public:

	SLATE_BEGIN_ARGS( SInputAssetComboPanel )
		: _ComboBoxStyle(&FAppStyle::Get().GetWidgetStyle< FComboBoxStyle >("ComboBox"))
		, _ButtonStyle(nullptr)
		, _ShowSelectedItemLabelOnly(false)
		, _ComboButtonTileSize(50, 50)
		, _FlyoutTileSize(85, 85)
		, _FlyoutSize(600, 400)
		, _FilterSetup()
		, _OnSelectionChanged()
		, _InitiallySelectedAsset()
		, _AssetThumbnailLabel(EThumbnailLabel::NoLabel)
		, _bForceShowEngineContent(false)
		, _bForceShowPluginContent(false)
	{}

		SLATE_STYLE_ARGUMENT( FComboBoxStyle, ComboBoxStyle )

		/** The visual style of the button part of the combo box (overrides ComboBoxStyle) */
		SLATE_STYLE_ARGUMENT( FButtonStyle, ButtonStyle )

		SLATE_ATTRIBUTE( FSlateColor, ForegroundColor )

		SLATE_ARGUMENT( bool, ShowSelectedItemLabelOnly )

		SLATE_ATTRIBUTE(FText, FallbackSelectionText)

		SLATE_ARGUMENT(FSlateFontInfo, SelectedItemFont)

		SLATE_ATTRIBUTE( FMargin, ContentPadding )

		/** The size of the combo button icon tile */
		SLATE_ARGUMENT(FVector2D, ComboButtonTileSize)

		/** The size of the icon tiles in the flyout */
		SLATE_ARGUMENT(FVector2D, FlyoutTileSize)

		/** Size of the Flyout Panel */
		SLATE_ARGUMENT(FVector2D, FlyoutSize)

		/** Target PropertyHandle, selected value will be written here (Note: not required, selected  */
		SLATE_ARGUMENT( TSharedPtr<IPropertyHandle>, Property )

		/** Tooltip for the ComboButton. If Property is defined, this will be ignored. */
		SLATE_ARGUMENT( FText, ToolTipText )

		/** UClass of Asset to pick. Required, and only one class is supported */
		SLATE_ARGUMENT( FAssetComboFilterSetup, FilterSetup )

		/** (Optional) external provider/tracker of Recently-Used Assets. If not provided, Recent Assets area will not be shown. */
		SLATE_ARGUMENT(TSharedPtr<IRecentAssetsProvider>, RecentAssetsProvider)

		/** (Optional) set of collection-lists, if provided, button bar will be shown with each CollectionSet as an option */
		SLATE_ARGUMENT(TArray<FNamedCollectionList>, CollectionSets)
			
		/** (Optional) additional filter used by the flyout asset picker. Return true to filter OUT an asset. */
		SLATE_EVENT(FOnShouldFilterAsset, OnShouldFilterAsset)

		/** This delegate is executed each time the Selected Asset is modified */
		SLATE_EVENT( FOnSelectedAssetChanged, OnSelectionChanged )

		SLATE_EVENT( FOnOpeningEvent, OnOpening )

		SLATE_EVENT( FOnGetAssetLabel, OnGetAssetLabel )

		/** Sets the asset selected by the widget before any user made selection occurs. */
		SLATE_ARGUMENT( FAssetData, InitiallySelectedAsset)

		/** Sets the type of label used for the asset picker tiles */
		SLATE_ARGUMENT( EThumbnailLabel::Type, AssetThumbnailLabel)

		/** Indicates if engine content should always be shown */
		SLATE_ARGUMENT(bool, bForceShowEngineContent)

		/** Indicates if plugin content should always be shown */
		SLATE_ARGUMENT(bool, bForceShowPluginContent)

	SLATE_END_ARGS()


	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	void RefreshThumbnailFromProperty();

protected:

	FVector2D ComboButtonTileSize;
	FVector2D FlyoutTileSize;
	FVector2D FlyoutSize;
	TSharedPtr<IPropertyHandle> Property;
	FAssetComboFilterSetup FilterSetup;
	FOnShouldFilterAsset OnShouldFilterAsset;


	/** Delegate to invoke when selection changes. */
	FOnSelectedAssetChanged OnSelectionChanged;
	FOnOpeningEvent OnOpening;
	FOnGetAssetLabel OnGetAssetLabel;

	TSharedPtr<SComboButton> ComboButton;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TSharedPtr<FAssetThumbnail> AssetThumbnail;
	TSharedPtr<SBorder> ThumbnailBorder;

	TSharedPtr<IRecentAssetsProvider> RecentAssetsProvider;
	TSharedPtr<class SInputAssetPicker> AssetPicker;

	struct FRecentAssetInfo
	{
		int Index;
		FAssetData AssetData;
	};
	TArray<TSharedPtr<FRecentAssetInfo>> RecentAssetData;

	TArray<TSharedPtr<FAssetThumbnail>> RecentThumbnails;
	TArray<TSharedPtr<SBox>> RecentThumbnailWidgets;

	void UpdateRecentAssets();

	virtual void NewAssetSelected(const FAssetData& AssetData);
	virtual FReply OnAssetThumbnailDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	TSharedRef<ITableRow> OnGenerateWidgetForRecentList(TSharedPtr<FRecentAssetInfo> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	virtual void HandleComboOpened();

	TArray<FNamedCollectionList> CollectionSets;
	int32 ActiveCollectionSetIndex = -1;
	TSharedRef<SWidget> MakeCollectionSetsButtonPanel(TSharedRef<SInputAssetPicker> AssetPickerView);
	TSharedPtr<STextBlock> SelectedItemLabel;
	TAttribute<FText> FallbackSelectionText;
	FSlateFontInfo SelectedItemFont;
	TSharedPtr<SBorder> SelectedItemBorder;
	TAttribute<FSlateColor> ForegroundColor;
	TAttribute<FMargin> ContentPadding;
	FComboButtonStyle OurComboButtonStyle;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * SInputAssetPicker is designed to support Asset picking in Modeling Tools, 
 * where the Assets in question are input parameters/options to Tools, eg such as
 * a brush alpha texture for use in a Painting/Sculpting Tool. 
 * 
 * Implementation is derived from SAssetPicker (private class in the ContentBrowser module).
 * However many optional features have been stripped out as they are not relevant
 * in the Modeling-Tool Parameters context. 
 * 
 * Most settings are provided via the FAssetPickerConfig, which is passed to an SAssetView internally.
 * 
 * Unless you are really certain you need to use this class directly, it's likely that 
 * you are looking for SToolInputAssetComboPanel, which provides a combobox/flyout-style
 * widget suitable for user interface panels.
 */
 // SEE: Engine\Plugins\Experimental\MeshModelingToolsetExp\Source\ModelingEditorUI\Public\ModelingWidgets\SToolInputAssetPicker.h
class SInputAssetPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SInputAssetPicker ){}

		/** A struct containing details about how the asset picker should behave */
		SLATE_ARGUMENT(FAssetPickerConfig, AssetPickerConfig)

		SLATE_EVENT( FOnGetBrowserItemText, OnGetBrowserItemText )

	SLATE_END_ARGS()

	virtual ~SInputAssetPicker();

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	// SWidget implementation
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	// End of SWidget implementation

	/** Return the associated AssetView */
	const TSharedPtr<class SInputAssetView>& GetAssetView() const { return AssetViewPtr; }

	/** Sorts the contents of the asset view alphabetically */
	void SortList(bool bSyncToSelection = true);

	/**
	 * Update the set of input Assets to be only based on the given set of Collections
	 * (or all Assets, if the Collections list is empty)
	 */
	virtual void UpdateAssetSourceCollections(TArray<FCollectionNameType> Collections);

private:
	/** Focuses the search box post-construct */
	EActiveTimerReturnType SetFocusPostConstruct( double InCurrentTime, float InDeltaTime );

	/** Special case handling for SAssetSearchBox key commands */
	FReply HandleKeyDownFromSearchBox(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	/** Called when the editable text needs to be set or cleared */
	void SetSearchBoxText(const FText& InSearchText);

	/** Called by the editable text control when the search text is changed by the user */
	void OnSearchBoxChanged(const FText& InSearchText);

	/** Called by the editable text control when the user commits a text change */
	void OnSearchBoxCommitted(const FText& InSearchText, ETextCommit::Type CommitInfo);

	/** Handler for when the "None" button is clicked */
	FReply OnNoneButtonClicked();

	/** Handle forwarding picking events. We wrap OnAssetSelected here to prevent 'Direct' selections being identified as user actions */
	void HandleItemSelectionChanged(const FContentBrowserItem& InSelectedItem, ESelectInfo::Type InSelectInfo);

	/** Handler for when the user double clicks, presses enter, or presses space on an asset */
	void HandleItemsActivated(TArrayView<const FContentBrowserItem> ActivatedItems, EAssetTypeActivationMethod::Type ActivationMethod);

	/** Forces a refresh */
	void RefreshAssetView(bool bRefreshSources);

	/** @return The text to highlight on the assets  */
	FText GetHighlightedText() const;

private:

	/** The list of FrontendFilters currently applied to the asset view */
	TSharedPtr<FAssetFilterCollectionType> FrontendFilters;

	/** The asset view widget */
	TSharedPtr<class SInputAssetView> AssetViewPtr;

	/** The search box */
	TSharedPtr<SAssetSearchBox> SearchBoxPtr;

	/** Called to when an asset is selected or the none button is pressed */
	FOnAssetSelected OnAssetSelected;

	/** Called when enter is pressed while an asset is selected */
	FOnAssetEnterPressed OnAssetEnterPressed;

	/** Called when any number of assets are activated */
	FOnAssetsActivated OnAssetsActivated;

	/** True if the search box will take keyboard focus next frame */
	bool bPendingFocusNextFrame;

	/** Filters needed for filtering the assets */
	TSharedPtr< FAssetFilterCollectionType > FilterCollection;
	TSharedPtr< FFrontendFilter_Text > TextFilter;

	EAssetTypeCategories::Type DefaultFilterMenuExpansion;

	/** The sources data currently used by the picker */
	FSourcesData CurrentSourcesData;

	/** Current filter we are using, needed reset asset view after we have custom filtered */
	FARFilter CurrentBackendFilter;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

// Copy of SAssetView. Removed extra stuff we don't need to focus on an asset picker.
class SInputAssetView : public SCompoundWidget
{
public:
	typedef STileView<TSharedPtr<FContentBrowserItem>> STileViewType;
	
	SLATE_BEGIN_ARGS( SInputAssetView )
		: _InitialCategoryFilter(EContentBrowserItemCategoryFilter::IncludeAll)
		, _ThumbnailLabel( EThumbnailLabel::ClassName )
		, _AllowThumbnailHintLabel(true)
		, _bShowPathViewFilters(false)
		, _InitialViewType(EAssetViewType::Tile)
		, _InitialThumbnailSize(EThumbnailSize::Medium)
		, _ShowBottomToolbar(true)
		, _ShowViewOptions(true)
		, _AllowThumbnailEditMode(false)
		, _CanShowClasses(true)
		, _CanShowFolders(false)
		, _CanShowReadOnlyFolders(true)
		, _FilterRecursivelyWithBackendFilter(true)
		, _CanShowRealTimeThumbnails(false)
		, _CanShowDevelopersFolder(false)
		, _CanShowFavorites(false)
		, _CanDockCollections(false)
		, _SelectionMode( ESelectionMode::Multi )
		, _AllowDragging(true)
		, _AllowFocusOnSync(true)
		, _FillEmptySpaceInTileView(true)
		, _ShowPathInColumnView(false)
		, _ShowTypeInColumnView(true)
		, _SortByPathInColumnView(false)
		, _ShowTypeInTileView(true)
		, _ForceShowEngineContent(false)
		, _ForceShowPluginContent(false)
		, _ForceHideScrollbar(false)
		, _ShowDisallowedAssetClassAsUnsupportedItems(false)
		{}

		/** Called to check if an asset should be filtered out by external code */
		SLATE_EVENT( FOnShouldFilterAsset, OnShouldFilterAsset )

		/** Called to check if an item should be filtered out by external code */
		SLATE_EVENT(FOnShouldFilterItem, OnShouldFilterItem)

		/** Called when the asset view is asked to start to create a temporary item */
		SLATE_EVENT( FOnAssetViewNewItemRequested, OnNewItemRequested )

		/** Called to when an item is selected */
		SLATE_EVENT( FOnContentBrowserItemSelectionChanged, OnItemSelectionChanged )

		/** Called when the user double clicks, presses enter, or presses space on an Content Browser item */
		SLATE_EVENT( FOnContentBrowserItemsActivated, OnItemsActivated )

		SLATE_EVENT( FOnGetBrowserItemText, OnGetBrowserItemText )

		/** Delegate to invoke when a context menu for an item is opening. */
		SLATE_EVENT( FOnGetContentBrowserItemContextMenu, OnGetItemContextMenu )

		/** Called when the user has committed a rename of one or more items */
		SLATE_EVENT( FOnContentBrowserItemRenameCommitted, OnItemRenameCommitted )

		/** Delegate to call (if bound) to check if it is valid to get a custom tooltip for this asset item */
		SLATE_EVENT(FOnIsAssetValidForCustomToolTip, OnIsAssetValidForCustomToolTip)

		/** Called to get a custom asset item tool tip (if necessary) */
		SLATE_EVENT( FOnGetCustomAssetToolTip, OnGetCustomAssetToolTip )

		/** Called when an asset item is about to show a tooltip */
		SLATE_EVENT( FOnVisualizeAssetToolTip, OnVisualizeAssetToolTip )

		/** Called when an asset item's tooltip is closing */
		SLATE_EVENT(FOnAssetToolTipClosing, OnAssetToolTipClosing)

		/** Called when opening view options menu */
		SLATE_EVENT(FOnExtendAssetViewOptionsMenuContext, OnExtendAssetViewOptionsMenuContext)

		/** Initial set of item categories that this view should show - may be adjusted further by things like CanShowClasses or legacy delegate bindings */
		SLATE_ARGUMENT( EContentBrowserItemCategoryFilter, InitialCategoryFilter )

		/** The warning text to display when there are no assets to show */
		SLATE_ATTRIBUTE( FText, AssetShowWarningText )

		/** Attribute to determine what text should be highlighted */
		SLATE_ATTRIBUTE( FText, HighlightedText )

		/** What the label on the asset thumbnails should be */
		SLATE_ARGUMENT( EThumbnailLabel::Type, ThumbnailLabel )

		/** Whether to ever show the hint label on thumbnails */
		SLATE_ARGUMENT( bool, AllowThumbnailHintLabel )

		/** The filter collection used to further filter down assets returned from the backend */
		SLATE_ARGUMENT( TSharedPtr<FAssetFilterCollectionType>, FrontendFilters )

		/** Show path view filters submenu in view options menu */
		SLATE_ARGUMENT( bool, bShowPathViewFilters )

		/** The initial base sources filter */
		SLATE_ARGUMENT( FSourcesData, InitialSourcesData )

		/** The initial backend filter */
		SLATE_ARGUMENT( FARFilter, InitialBackendFilter )

		/** The asset that should be initially selected */
		SLATE_ARGUMENT( FAssetData, InitialAssetSelection )

		/** The initial view type */
		SLATE_ARGUMENT( EAssetViewType::Type, InitialViewType )

		/** Initial thumbnail size */
		SLATE_ARGUMENT( EThumbnailSize, InitialThumbnailSize )

		/** Should the toolbar indicating number of selected assets, mode switch buttons, etc... be shown? */
		SLATE_ARGUMENT( bool, ShowBottomToolbar )

		/** Should view options be shown. Note: If ShowBottomToolbar is false, then view options are not shown regardless of this setting */
		SLATE_ARGUMENT( bool, ShowViewOptions)

		/** True if the asset view may edit thumbnails */
		SLATE_ARGUMENT( bool, AllowThumbnailEditMode )

		/** Indicates if this view is allowed to show classes */
		SLATE_ARGUMENT( bool, CanShowClasses )

		/** Indicates if the 'Show Folders' option should be enabled or disabled */
		SLATE_ARGUMENT( bool, CanShowFolders )

		/** Indicates if this view is allowed to show folders that cannot be written to */
		SLATE_ARGUMENT( bool, CanShowReadOnlyFolders )

		/** If true, recursive filtering will be caused by applying a backend filter */
		SLATE_ARGUMENT( bool, FilterRecursivelyWithBackendFilter )

		/** Indicates if the 'Real-Time Thumbnails' option should be enabled or disabled */
		SLATE_ARGUMENT( bool, CanShowRealTimeThumbnails )

		/** Indicates if the 'Show Developers' option should be enabled or disabled */
		SLATE_ARGUMENT( bool, CanShowDevelopersFolder )

		/** Indicates if the 'Show Favorites' option should be enabled or disabled */
		SLATE_ARGUMENT(bool, CanShowFavorites)

		/** Indicates if the 'Dock Collections' option should be enabled or disabled */
		SLATE_ARGUMENT(bool, CanDockCollections)

		/** The selection mode the asset view should use */
		SLATE_ARGUMENT( ESelectionMode::Type, SelectionMode )

		/** Whether to allow dragging of items */
		SLATE_ARGUMENT( bool, AllowDragging )

		/** Whether this asset view should allow focus on sync or not */
		SLATE_ARGUMENT( bool, AllowFocusOnSync )

		/** Whether this asset view should allow the thumbnails to consume empty space after the user scale is applied */
		SLATE_ARGUMENT( bool, FillEmptySpaceInTileView )

		/** Should show Path in column view if true */
		SLATE_ARGUMENT(bool, ShowPathInColumnView)

		/** Should show Type in column view if true */
		SLATE_ARGUMENT(bool, ShowTypeInColumnView)

		/** Sort by path in the column view. Only works if the initial view type is Column */
		SLATE_ARGUMENT(bool, SortByPathInColumnView)
		
		/** Should show Type in column view if true */
		SLATE_ARGUMENT(bool, ShowTypeInTileView)

		/** Should always show engine content */
		SLATE_ARGUMENT(bool, ForceShowEngineContent)

		/** Should always show plugin content */
		SLATE_ARGUMENT(bool, ForceShowPluginContent)

		/** Should always hide scrollbar (Removes scrollbar) */
		SLATE_ARGUMENT(bool, ForceHideScrollbar)

		/** Allow the asset view to display the hidden asset class as unsupported items */
		SLATE_ARGUMENT(bool, ShowDisallowedAssetClassAsUnsupportedItems)

		/** Called to check if an asset tag should be display in details view. */
		SLATE_EVENT( FOnShouldDisplayAssetTag, OnAssetTagWantsToBeDisplayed )

		/** Called to add extra asset data to the asset view, to display virtual assets. These get treated similar to Class assets */
		SLATE_EVENT( FOnGetCustomSourceAssets, OnGetCustomSourceAssets )

		/** Columns to hide by default */
		SLATE_ARGUMENT( TArray<FString>, HiddenColumnNames )
			
		/** Custom columns that can be use specific */
		SLATE_ARGUMENT( TArray<FAssetViewCustomColumn>, CustomColumns )

		/** Custom columns that can be use specific */
		SLATE_EVENT( FOnSearchOptionChanged, OnSearchOptionsChanged)

		/** The content browser that owns this view if any */
		SLATE_ARGUMENT(TSharedPtr<SContentBrowser>, OwningContentBrowser)
	SLATE_END_ARGS()

	~SInputAssetView();

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	/** Changes the base sources for this view */
	void SetSourcesData(const FSourcesData& InSourcesData);

	/** Returns the sources filter applied to this asset view */
	const FSourcesData& GetSourcesData() const;

	/** Notifies the asset view that the filter-list filter has changed */
	void SetBackendFilter(const FARFilter& InBackendFilter);

	/** Get the current backend filter */
	const FARFilter& GetBackendFilter() const { return BackendFilter; }

	/** Selects the specified items. */
	void SyncToItems( TArrayView<const FContentBrowserItem> ItemsToSync, const bool bFocusOnSync = true );

	/** Selects the specified virtual paths. */
	void SyncToVirtualPaths( TArrayView<const FName> VirtualPathsToSync, const bool bFocusOnSync = true );

	/** Selects the specified assets and paths. */
	void SyncToLegacy( TArrayView<const FAssetData> AssetDataList, TArrayView<const FString> FolderList, const bool bFocusOnSync = true );

	/** Sets the state of the asset view to the one described by the history data */
	void ApplyHistoryData( const FHistoryData& History );

	/** Returns all the items currently selected in the view */
	TArray<TSharedPtr<FContentBrowserItem>> GetSelectedViewItems() const;

	/** Returns all the items currently selected in the view */
	TArray<FContentBrowserItem> GetSelectedItems() const;

	/** Returns all the folder items currently selected in the view */
	TArray<FContentBrowserItem> GetSelectedFolderItems() const;

	/** Returns all the file items currently selected in the view */
	TArray<FContentBrowserItem> GetSelectedFileItems() const;

	/** Returns all the asset data objects in items currently selected in the view */
	TArray<FAssetData> GetSelectedAssets() const;

	/** Returns all the folders currently selected in the view */
	TArray<FString> GetSelectedFolders() const;

	/** Requests that the asset view refreshes all it's source items. This is slow and should only be used if the source items change. */
	void RequestSlowFullListRefresh();

	/** Requests that the asset view refreshes only items that are filtered through frontend sources. This should be used when possible. */
	void RequestQuickFrontendListRefresh();

	/** Adjusts the selected asset by the selection delta, which should be +1 or -1) */
	void AdjustActiveSelection(int32 SelectionDelta);

	// SWidget inherited
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual FReply OnKeyChar( const FGeometry& MyGeometry,const FCharacterEvent& InCharacterEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnFocusChanging( const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent ) override;

	/** Opens the selected assets or folders, depending on the selection */
	void OnOpenAssetsOrFolders();

	/** Loads the selected assets and previews them if possible */
	void OnPreviewAssets();

	/** Clears the selection of all the lists in the view */
	void ClearSelection(bool bForceSilent = false);

	/** Returns true if the asset view is in thumbnail editing mode */
	bool IsThumbnailEditMode() const;

	/** Delegate called when an editor setting is changed */
	void HandleSettingChanged(FName PropertyName);

	/** Set whether the user is currently searching or not */
	void SetUserSearching(bool bInSearching);

	/**
	 * Forces the plugin content folder to be shown.
	 *
	 * @param bEnginePlugin		If true, also forces the engine folder to be shown.
	 */
	void ForceShowPluginFolder( bool bEnginePlugin );

	/** Enables the Show Engine Content setting for the active Content Browser. The user can still toggle the setting manually. */
	void OverrideShowEngineContent();

	/** Enables the Show Developer Content setting for the active Content Browser. The user can still toggle the setting manually. */
	void OverrideShowDeveloperContent();

	/** Enables the Show Plugin Content setting for the active Content Browser. The user can still toggle the setting manually. */
	void OverrideShowPluginContent();

	/** Enables the Show Localized Content setting for the active Content Browser. The user can still toggle the setting manually. */
	void OverrideShowLocalizedContent();

	/** @return true when we are including class names in search criteria */
	bool IsIncludingClassNames() const;

	/** @return true when we are including the entire asset path in search criteria */
	bool IsIncludingAssetPaths() const;

	/** @return true when we are including collection names in search criteria */
	bool IsIncludingCollectionNames() const;

	/** Sets the view type and updates lists accordingly */
	void SetCurrentViewType(EAssetViewType::Type NewType);

	/** Sets the thumbnail size and updates lists accordingly */
	void SetCurrentThumbnailSize(EThumbnailSize NewThumbnailSize);

	/** Gets the text for the asset count label */
	FText GetAssetCountText() const;

	/** Gets text name for given thumbnail */
	static FText ThumbnailSizeToDisplayName(EThumbnailSize InSize);

	/** Set the filter list attached to this asset view - allows toggling of the the filter bar layout from the view options */
	void SetFilterBar(TSharedPtr<SFilterList> InFilterBar);

private:

	/** Sets the pending selection to the current selection (used when changing views or refreshing the view). */
	void SyncToSelection( const bool bFocusOnSync = true );

	/** @return the thumbnail scale setting path to use when looking up the setting in an ini. */
	FString GetThumbnailScaleSettingPath(const FString& SettingsString) const;

	/** @return the view type setting path to use when looking up the setting in an ini. */
	FString GetCurrentViewTypeSettingPath(const FString& SettingsString) const;

	/** Calculates a new filler scale used to adjust the thumbnails to fill empty space. */
	void CalculateFillScale( const FGeometry& AllottedGeometry );

	/** Calculates the latest color and opacity for the hint on thumbnails */
	void CalculateThumbnailHintColorAndOpacity();

	/** True if we have items pending that ProcessItemsPendingFilter still needs to process */
	bool HasItemsPendingFilter() const;

	/** Handles amortizing the additional filters */
	void ProcessItemsPendingFilter(const double TickStartTime);

	/** Creates a new tile view */
	TSharedRef<STileViewType> CreateTileView();

	/** Returns true if the specified search token is allowed */
	bool IsValidSearchToken(const FString& Token) const;

	/** Regenerates the AssetItems list from the AssetRegistry */
	void RefreshSourceItems();

	/** Regenerates the FilteredAssetItems list from the AssetItems list */
	void RefreshFilteredItems();

	/** Handler for when an asset is added to a collection */
	void OnAssetsAddedToCollection(ICollectionContainer& CollectionContainer, const FCollectionNameType& Collection, TConstArrayView<FSoftObjectPath> ObjectPaths);

	/** Handler for when an asset is removed from a collection */
	void OnAssetsRemovedFromCollection(ICollectionContainer& CollectionContainer, const FCollectionNameType& Collection, TConstArrayView<FSoftObjectPath> ObjectPaths);

	/** Handler for when a collection is renamed */
	void OnCollectionRenamed(ICollectionContainer& CollectionContainer, const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection);

	/** Handler for when a collection is updated */
	void OnCollectionUpdated(ICollectionContainer& CollectionContainer, const FCollectionNameType& Collection);

	/** Handler for when any frontend filters have been changed */
	void OnFrontendFiltersChanged();

	/** Returns true if there is any frontend filter active */
	bool IsFrontendFilterActive() const;

	/** Returns true if the specified Content Browser item passes all applied frontend (non asset registry) filters */
	bool PassesCurrentFrontendFilter(const FContentBrowserItem& Item) const;

	/** Returns true if the current filters deem that the asset view should be filtered recursively (overriding folder view) */
	bool ShouldFilterRecursively() const;

	/** Sorts the contents of the asset view alphabetically */
	void SortList(bool bSyncToSelection = true);

	/** Returns the thumbnails hint color and opacity */
	FLinearColor GetThumbnailHintColorAndOpacity() const;

	/** Whether or not it's possible to show folders */
	bool IsToggleShowFoldersAllowed() const;

	/** @return true when we are showing folders */
	bool IsShowingFolders() const;

	/** @return true when we are showing read-only folders */
	bool IsShowingReadOnlyFolders() const;

	/** Toggle whether empty folders should be shown or not */
	void ToggleShowEmptyFolders();

	/** Whether or not it's possible to show empty folders */
	bool IsToggleShowEmptyFoldersAllowed() const;

	/** @return true when we are showing empty folders */
	bool IsShowingEmptyFolders() const;

	/** Whether or not it's possible to show localized content */
	bool IsToggleShowLocalizedContentAllowed() const;

	/** @return true when we are showing folders */
	bool IsShowingLocalizedContent() const;

	/** Whether it is possible to show real-time thumbnails */
	bool CanShowRealTimeThumbnails() const;

	/** @return true if we are showing real-time thumbnails */
	bool IsShowingRealTimeThumbnails() const;

	/** @return true when we are showing plugin content */
	bool IsShowingPluginContent() const;

	/** @return true when we are showing engine content */
	bool IsShowingEngineContent() const;

	/** Whether or not it's possible to toggle developers content */
	bool IsToggleShowDevelopersContentAllowed() const;

	/** Whether or not it's possible to toggle engine content */
	bool IsToggleShowEngineContentAllowed() const;

	/** Whether or not it's possible to toggle plugin content */
	bool IsToggleShowPluginContentAllowed() const;
	
	/** @return true when we are showing the developers content */
	bool IsShowingDevelopersContent() const;

	/** Whether or not it's possible to toggle favorites */
	bool IsToggleShowFavoritesAllowed() const;

	/** @return true when we are showing favorites */
	bool IsShowingFavorites() const;

	/** Whether or not it's possible to dock the collections view */
	bool IsToggleDockCollectionsAllowed() const;

	/** @return true when the collections view is docked */
	bool HasDockedCollections() const;

	/** Whether or not it's possible to show C++ content */
	bool IsToggleShowCppContentAllowed() const;

	/** @return true when we are showing C++ content */
	bool IsShowingCppContent() const;

	/** Whether or not it's possible to include class names in search criteria */
	bool IsToggleIncludeClassNamesAllowed() const;

	/** Whether or not it's possible to include the entire asset path in search criteria */
	bool IsToggleIncludeAssetPathsAllowed() const;

	/** Whether or not it's possible to include collection names in search criteria */
	bool IsToggleIncludeCollectionNamesAllowed() const;

	/** @return true when we are filtering recursively when we have an asset path */
	bool IsFilteringRecursively() const;

	/** Whether or not it's possible to toggle how to filtering recursively */
	bool IsToggleFilteringRecursivelyAllowed() const;

	/** @return true when we are showing the all virtual folder */
	bool IsShowingAllFolder() const;

	/** @return true when we are organizing folders into virtual locations */
	bool IsOrganizingFolders() const;

	/** Sets the view type and forcibly dismisses all currently open context menus */
	void SetCurrentViewTypeFromMenu(EAssetViewType::Type NewType);

	/** Clears the reference to the current view and creates a new one, based on CurrentViewType */
	void CreateCurrentView();

	/** Gets the current view type (list or tile) */
	EAssetViewType::Type GetCurrentViewType() const;

	TSharedRef<SWidget> CreateShadowOverlay( TSharedRef<STableViewBase> Table );

	/** Returns true if ViewType is the current view type */
	bool IsCurrentViewType(EAssetViewType::Type ViewType) const;

	/** Set the keyboard focus to the correct list view that should be active */
	void FocusList() const;

	/** Refreshes the list view to display any changes made to the non-filtered assets */
	void RefreshList();

	/** Sets the sole selection for all lists in the view */
	void SetSelection(const TSharedPtr<FContentBrowserItem>& Item);

	/** Sets selection for an item in all lists in the view */
	void SetItemSelection(const TSharedPtr<FContentBrowserItem>& Item, bool bSelected, const ESelectInfo::Type SelectInfo = ESelectInfo::Direct);

	/** Scrolls the selected item into view for all lists in the view */
	void RequestScrollIntoView(const TSharedPtr<FContentBrowserItem>& Item);

	/** Handler for tile view widget creation */
	TSharedRef<ITableRow> MakeTileViewWidget(TSharedPtr<FContentBrowserItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Handler for when any asset item widget gets destroyed */
	void AssetItemWidgetDestroyed(const TSharedPtr<FContentBrowserItem>& Item);
	
	/** Creates new thumbnails that are near the view area and deletes old thumbnails that are no longer relevant. */
	void UpdateThumbnails();

	/**  Helper function for UpdateThumbnails. Adds the specified item to the new thumbnail relevancy map and creates any thumbnails for new items. Returns the thumbnail. */
	TSharedPtr<FAssetThumbnail> AddItemToNewThumbnailRelevancyMap(const TSharedPtr<FContentBrowserItem>& Item, TMap< TSharedPtr<FContentBrowserItem>, TSharedPtr<FAssetThumbnail> >& NewRelevantThumbnails);

	/** Handler for tree view selection changes */
	void AssetSelectionChanged( TSharedPtr<FContentBrowserItem > AssetItem, ESelectInfo::Type SelectInfo );

	/** Handler for when an item has scrolled into view after having been requested to do so */
	void ItemScrolledIntoView(TSharedPtr<FContentBrowserItem> AssetItem, const TSharedPtr<ITableRow>& Widget);

	/** Handler for context menus */
	TSharedPtr<SWidget> OnGetContextMenuContent();

	/** Handler called when an asset context menu is about to open */
	bool CanOpenContextMenu() const;

	/** Handler for double clicking an item */
	void OnListMouseButtonDoubleClick(TSharedPtr<FContentBrowserItem> AssetItem);

	/** Returns true if tooltips should be allowed right now. Tooltips are typically disabled while right click scrolling. */
	bool ShouldAllowToolTips() const;

	/** Returns true if the asset view is currently allowing the user to edit thumbnails */
	bool IsThumbnailEditModeAllowed() const;

	/** The "Done Editing" button was pressed in the thumbnail edit mode strip */
	FReply EndThumbnailEditModeClicked();

	/** Gets the visibility of the Thumbnail Edit Mode label */
	EVisibility GetEditModeLabelVisibility() const;

	/** Gets the visibility of the list view */
	EVisibility GetListViewVisibility() const;

	/** Gets the visibility of the tile view */
	EVisibility GetTileViewVisibility() const;

	/** Gets the visibility of the column view */
	EVisibility GetColumnViewVisibility() const;

	/** Toggles thumbnail editing mode */
	void ToggleThumbnailEditMode();

	/** Called when  thumbnail size is changed */
	void OnThumbnailSizeChanged(EThumbnailSize NewThumbnailSize);

	/** Is the current thumbnail size menu option checked */
	bool IsThumbnailSizeChecked(EThumbnailSize InThumbnailSize) const;

	/** Are thumbnails allowed to be scaled by the user */
	bool IsThumbnailScalingAllowed() const;

	/** Gets the current thumbnail scale */
	float GetThumbnailScale() const;

	/** Gets the current thumbnail size enum */
	EThumbnailSize GetThumbnailSize() const { return ThumbnailSize; }

	/** Gets the spacing dedicated to support type names */
	float GetTileViewTypeNameHeight() const;

	/** Gets the scaled item height for the list view */
	float GetListViewItemHeight() const;
	
	/** Gets the final scaled item height for the tile view */
	float GetTileViewItemHeight() const;

	/** Gets the scaled item height for the tile view before the filler scale is applied */
	float GetTileViewItemBaseHeight() const;

	/** Gets the final scaled item width for the tile view */
	float GetTileViewItemWidth() const;

	/** Gets the scaled item width for the tile view before the filler scale is applied */
	float GetTileViewItemBaseWidth() const;

	/** Gets the sort mode for the supplied ColumnId */
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;

	/** Gets the sort order for the supplied ColumnId */
	EColumnSortPriority::Type GetColumnSortPriority(const FName ColumnId) const;

	/** Handler for when a column header is clicked */
	void OnSortColumnHeader(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type NewSortMode);

	/** @return The state of the is working progress bar */
	TOptional< float > GetIsWorkingProgressBarState() const;

	/** Is the no assets to show warning visible? */
	EVisibility IsAssetShowWarningTextVisible() const;

	/** Gets the text for displaying no assets to show warning */
	FText GetAssetShowWarningText() const;

	/** Whether we have a single source collection selected */
	bool HasSingleCollectionSource() const;

	/** @return The current quick-jump term */
	FText GetQuickJumpTerm() const;

	/** @return Whether the quick-jump term is currently visible? */
	EVisibility IsQuickJumpVisible() const;

	/** @return The color that should be used for the quick-jump term */
	FSlateColor GetQuickJumpColor() const;

	/** Reset the quick-jump to its empty state */
	void ResetQuickJump();

	/**
	 * Called from OnKeyChar and OnKeyDown to handle quick-jump key presses
	 * @param InCharacter		The character that was typed
	 * @param bIsControlDown	Was the control key pressed?
	 * @param bIsAltDown		Was the alt key pressed?
	 * @param bTestOnly			True if we only want to test whether the key press would be handled, but not actually update the quick-jump term
	 * @return FReply::Handled or FReply::Unhandled
	 */
	FReply HandleQuickJumpKeyDown(const TCHAR InCharacter, const bool bIsControlDown, const bool bIsAltDown, const bool bTestOnly);

	/**
	 * Perform a quick-jump to the next available asset in FilteredAssetItems that matches the current term
	 * @param bWasJumping		True if we were performing an ongoing quick-jump last Tick
	 * @return True if the quick-jump found a valid match, false otherwise
	 */
	bool PerformQuickJump(const bool bWasJumping);

	/** Append the current effective backend filter (intersection of BackendFilter and SupportedFilter) to the given filter. */
	void AppendBackendFilter(FARFilter& FilterToAppendTo) const;

	FContentBrowserDataFilter CreateBackendDataFilter(bool bInvalidateCache) const;

	/** Handles updating the view when content items are changed */
	void HandleItemDataUpdated(TArrayView<const FContentBrowserItemDataUpdate> InUpdatedItems);

	/** Notification for when the content browser has completed it's initial search */
	void HandleItemDataDiscoveryComplete();

private:
	friend class FAssetViewFrontendFilterHelper;

	/** The available items from querying the backend data sources */
	TMap<FContentBrowserItemKey, TSharedPtr<FContentBrowserItem>> AvailableBackendItems;

	/**
	 * The items from AvailableBackendItems that are pending a run through any additional filtering before they can be shown in the filtered view list.
	 * @note This filtering will run without amortization via ProcessItemsPendingFilter, so only use it for items that *must* be processed this frame.
	 */
	TSet<TSharedPtr<FContentBrowserItem>> ItemsPendingPriorityFilter;

	/**
	 * The items from AvailableBackendItems that are pending a run through any additional frontend filters before they can be shown in the filtered view list.
	 * @note This filtering will run amortized on the game thread via ProcessItemsPendingFilter.
	 */
	TSet<TSharedPtr<FContentBrowserItem>> ItemsPendingFrontendFilter;

	/** The items that are being shown in the filtered view list */
	TArray<TSharedPtr<FContentBrowserItem>> FilteredAssetItems;

	/* Map of an item name to the current count of FilteredAssetItems that are of that type */ 
	TMap<FName, int32> FilteredAssetItemTypeCounts;

	/** The list view that is displaying the assets */
	EAssetViewType::Type CurrentViewType {EAssetViewType::Tile};
	TSharedPtr<STileViewType> TileView;
	TSharedPtr<SBox> ViewContainer;

	/** The content browser that created this asset view if any */
	TWeakPtr<SContentBrowser> OwningContentBrowser;

	/** The Filter Bar attached to this asset view if any */
	TWeakPtr<SFilterList> FilterBar;

	/** The current base source filter for the view */
	FSourcesData SourcesData;
	FARFilter BackendFilter;
	TSharedPtr<FPathPermissionList> AssetClassPermissionList;
	TSharedPtr<FPathPermissionList> FolderPermissionList;
	TSharedPtr<FPathPermissionList> WritableFolderPermissionList;
	TSharedPtr<FAssetFilterCollectionType> FrontendFilters;

	/** Show path view filters submenu in view options menu  */
	bool bShowPathViewFilters;

	/** If true, the source items will be refreshed next frame. Very slow. */
	bool bSlowFullListRefreshRequested;

	/** If true, the frontend items will be refreshed next frame. Much faster. */
	bool bQuickFrontendListRefreshRequested;

	/** The list of items to sync next frame */
	FSelectionData PendingSyncItems;

	/** Should we take focus when the PendingSyncAssets are processed? */
	bool bPendingFocusOnSync;

	/** Was recursive filtering enabled the last time a full slow refresh performed? */
	bool bWereItemsRecursivelyFiltered;

	/** Called to check if an asset should be filtered out by external code */
	FOnShouldFilterAsset OnShouldFilterAsset;

	/** Called to check if an item should be filtered out by external code */
	FOnShouldFilterItem OnShouldFilterItem;

	/** Called when the asset view is asked to start to create a temporary item */
	FOnAssetViewNewItemRequested OnNewItemRequested;

	FOnGetBrowserItemText OnGetBrowserItemText;

	/** Called when an item was selected in the list. Provides more context than OnAssetSelected. */
	FOnContentBrowserItemSelectionChanged OnItemSelectionChanged;

	/** Called when the user double clicks, presses enter, or presses space on a Content Browser item */
	FOnContentBrowserItemsActivated OnItemsActivated;

	/** Delegate to invoke when generating the context menu for an item */
	FOnGetContentBrowserItemContextMenu OnGetItemContextMenu;

	/** Called when the user has committed a rename of one or more items */
	FOnContentBrowserItemRenameCommitted OnItemRenameCommitted;

	/** Called to check if an asset tag should be display in details view. */
	FOnShouldDisplayAssetTag OnAssetTagWantsToBeDisplayed;

	/** Called to see if it is valid to get a custom asset tool tip */
	FOnIsAssetValidForCustomToolTip OnIsAssetValidForCustomToolTip;

	/** Called to get a custom asset item tooltip (If necessary) */
	FOnGetCustomAssetToolTip OnGetCustomAssetToolTip;

	/** Called when a custom asset item is about to show a tooltip */
	FOnVisualizeAssetToolTip OnVisualizeAssetToolTip;

	/** Called when a custom asset item's tooltip is closing */
	FOnAssetToolTipClosing OnAssetToolTipClosing;

	/** Called to add extra asset data to the asset view, to display virtual assets. These get treated similar to Class assets */
	FOnGetCustomSourceAssets OnGetCustomSourceAssets;

	/** Called when a search option changes to notify that results should be rebuilt */
	FOnSearchOptionChanged OnSearchOptionsChanged;

	/** Called when opening view options menu */
	FOnExtendAssetViewOptionsMenuContext OnExtendAssetViewOptionsMenuContext;

	/** When true, filtered list items will be sorted next tick. Provided another sort hasn't happened recently or we are renaming an asset */
	bool bPendingSortFilteredItems;
	double CurrentTime;
	double LastSortTime;
	double SortDelaySeconds;

	/** Weak ptr to the asset that is waiting to be renamed when scrolled into view, and the window is active */
	TWeakPtr<FContentBrowserItem> AwaitingRename;

	/** Set when the user is in the process of naming an asset */
	TWeakPtr<FContentBrowserItem> RenamingAsset;

	/** Pool for maintaining and rendering thumbnails */
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;

	/** A map of FAssetViewAsset to the thumbnail that represents it. Only items that are currently visible or within half of the FilteredAssetItems array index distance described by NumOffscreenThumbnails are in this list */
	TMap< TSharedPtr<FContentBrowserItem>, TSharedPtr<FAssetThumbnail> > RelevantThumbnails;

	/** The set of FAssetItems that currently have widgets displaying them. */
	TArray< TSharedPtr<FContentBrowserItem> > VisibleItems;

	/** The number of thumbnails to keep for asset items that are not currently visible. Half of the thumbnails will be before the earliest item and half will be after the latest. */
	int32 NumOffscreenThumbnails;

	/** The current size of relevant thumbnails */
	int32 CurrentThumbnailSize;

	/** Flag to defer thumbnail updates until the next frame */
	bool bPendingUpdateThumbnails;

	/** The size of thumbnails */
	int32 ListViewThumbnailResolution;
	int32 ListViewThumbnailSize;
	int32 ListViewThumbnailPadding;
	int32 TileViewThumbnailResolution;
	int32 TileViewThumbnailSize;
	int32 TileViewThumbnailPadding;
	int32 TileViewNameHeight;

	/** The max and min thumbnail scales as a fraction of the rendered size */
	float MinThumbnailScale;
	float MaxThumbnailScale;

	/** Scalar applied to thumbnail sizes so that users thumbnails are scaled based on users display area size*/
	float ThumbnailScaleRangeScalar;

	/** Current thumbnail size */
	EThumbnailSize ThumbnailSize;

	/** The amount to scale each thumbnail so that the empty space is filled. */
	float FillScale;

	/** When in columns view, this is the name of the asset type which is most commonly found in the recent results */
	FName MajorityAssetType;

	/** The manager responsible for sorting assets in the view */
	FAssetViewSortManager SortManager;

	/** Flag indicating if we will be filling the empty space in the tile view. */
	bool bFillEmptySpaceInTileView;

	/** When true, selection change notifications will not be sent */
	bool bBulkSelecting;

	/** When true, the user may edit thumbnails */
	bool bAllowThumbnailEditMode : 1;

	/** True when the asset view is currently allowing the user to edit thumbnails */
	bool bThumbnailEditMode : 1;

	/** Indicates if this view is allowed to show classes */
	bool bCanShowClasses : 1;

	/** Indicates if the 'Show Folders' option should be enabled or disabled */
	bool bCanShowFolders : 1;

	/** Indicates if this view is allowed to show folders that cannot be written to */
	bool bCanShowReadOnlyFolders : 1;

	/** If true, recursive filtering will be caused by applying a backend filter */
	bool bFilterRecursivelyWithBackendFilter : 1;

	/** Indicates if the 'Real-Time Thumbnails' option should be enabled or disabled */
	bool bCanShowRealTimeThumbnails : 1;

	/** Indicates if the 'Show Developers' option should be enabled or disabled */
	bool bCanShowDevelopersFolder : 1;

	/** Indicates if the 'Show Favorites' option should be enabled or disabled */
	bool bCanShowFavorites : 1;

	/** Indicates if the 'Dock Collections' option should be enabled or disabled */
	bool bCanDockCollections : 1;

	/** If true, it will show path column in the asset view */
	bool bShowPathInColumnView : 1;

	/** If true, it will show type in the asset view */
	bool bShowTypeInColumnView : 1;

	/** If true, it sorts by path and then name */
	bool bSortByPathInColumnView : 1;

	/** If true, it will show type in the tile view */
	bool bShowTypeInTileView : 1;

	/** If true, engine content is always shown */
	bool bForceShowEngineContent : 1;

	/** If true, plugin content is always shown */
	bool bForceShowPluginContent : 1;

	/** If true, scrollbar is never shown, removes scroll border entirely */
	bool bForceHideScrollbar : 1;

	/** Whether to allow dragging of items */
	bool bAllowDragging : 1;

	/** Whether this asset view should allow focus on sync or not */
	bool bAllowFocusOnSync : 1;

	/** Flag set if the user is currently searching */
	bool bUserSearching : 1;
	
	/** Whether or not to notify about newly selected items on on the next asset sync */
	bool bShouldNotifyNextAssetSync : 1;

	/** The current selection mode used by the asset view */
	ESelectionMode::Type SelectionMode;

	/** The max number of results to process per tick */
	float MaxSecondsPerFrame;

	/** When delegate amortization began */
	double AmortizeStartTime;

	/** The total time spent amortizing the delegate filter */
	double TotalAmortizeTime;

	/** The initial number of tasks when we started performing amortized work (used to display a progress cue to the user) */
	int32 InitialNumAmortizedTasks;

	/** The text to highlight on the assets */
	TAttribute< FText > HighlightedText;

	/** What the label on the thumbnails should be */
	EThumbnailLabel::Type ThumbnailLabel;

	/** Whether to ever show the hint label on thumbnails */
	bool AllowThumbnailHintLabel;

	/** The sequence used to generate the opacity of the thumbnail hint */
	FCurveSequence ThumbnailHintFadeInSequence;

	/** The current thumbnail hint color and opacity*/
	FLinearColor ThumbnailHintColorAndOpacity;

	/** The text to show when there are no assets to show */
	TAttribute< FText > AssetShowWarningText;

	/** Initial set of item categories that this view should show - may be adjusted further by things like CanShowClasses or legacy delegate bindings */
	EContentBrowserItemCategoryFilter InitialCategoryFilter;

	bool bShowDisallowedAssetClassAsUnsupportedItems = false;

	/** A struct to hold data for the deferred creation of a file or folder item */
	struct FCreateDeferredItemData
	{
		FContentBrowserItemTemporaryContext ItemContext;

		bool bWasAddedToView = false;
	};

	/** File or folder item pending deferred creation */
	TUniquePtr<FCreateDeferredItemData> DeferredItemToCreate;

	/** Struct holding the data for the asset quick-jump */
	struct FQuickJumpData
	{
		FQuickJumpData()
			: bIsJumping(false)
			, bHasChangedSinceLastTick(false)
			, bHasValidMatch(false)
			, LastJumpTime(0)
		{
		}

		/** True if we're currently performing an ongoing quick-jump */
		bool bIsJumping;

		/** True if the jump data has changed since the last Tick */
		bool bHasChangedSinceLastTick;

		/** True if the jump term found a valid match */
		bool bHasValidMatch;

		/** Time (taken from Tick) that we last performed a quick-jump */
		double LastJumpTime;

		/** The string we should be be looking for */
		FString JumpTerm;
	};

	/** Data for the asset quick-jump */
	FQuickJumpData QuickJumpData;
	
	/** Column filtering state */
	TArray<FString> DefaultHiddenColumnNames;
	TArray<FString> HiddenColumnNames;

	TArray<FAssetViewCustomColumn> CustomColumns;

	/** An Id for the cache of the data sources for the filters compilation */
	FContentBrowserDataFilterCacheIDOwner FilterCacheID;

public:
	bool ShouldColumnGenerateWidget(const FString ColumnName) const;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

DECLARE_DELEGATE_OneParam( FOnBrowserItemDestroyed, const TSharedPtr<FContentBrowserItem>& /*AssetItem*/);

/** An item in the asset tile view */
class SInputAssetTileItem : public SCompoundWidget
{
	friend struct FInternalAssetViewItemHelper;
	friend class SInputAssetViewItemToolTip;
	friend class SInputAssetView;
	using Super = SCompoundWidget;

public:
	SLATE_BEGIN_ARGS( SInputAssetTileItem )
		: _ThumbnailPadding(0)
		, _ThumbnailLabel( EThumbnailLabel::ClassName )
		, _ThumbnailHintColorAndOpacity( FLinearColor( 0.0f, 0.0f, 0.0f, 0.0f ) )
		, _AllowThumbnailHintLabel(true)
		, _CurrentThumbnailSize(EThumbnailSize::Medium)
		, _ItemWidth(16)
		, _ShouldAllowToolTip(true)
		, _ShowType(true)
		, _ThumbnailEditMode(false)
	{}

		/** The handle to the thumbnail this item should render */
		SLATE_ARGUMENT( TSharedPtr<FAssetThumbnail>, AssetThumbnail)

		/** Data for the asset this item represents */
		SLATE_ARGUMENT( TSharedPtr<FContentBrowserItem>, AssetItem )

		/** How much padding to allow around the thumbnail */
		SLATE_ARGUMENT( float, ThumbnailPadding )

		/** The contents of the label displayed on the thumbnail */
		SLATE_ARGUMENT( EThumbnailLabel::Type, ThumbnailLabel )

		/**  */
		SLATE_ATTRIBUTE( FLinearColor, ThumbnailHintColorAndOpacity )

		/** Whether the thumbnail should ever show it's hint label */
		SLATE_ARGUMENT( bool, AllowThumbnailHintLabel )

		/** Current size of the thumbnail that was generated */
		SLATE_ATTRIBUTE(EThumbnailSize, CurrentThumbnailSize)

		/** The width of the item */
		SLATE_ATTRIBUTE( float, ItemWidth )

		/** Called when any asset item is destroyed. Used in thumbnail management */
		SLATE_EVENT( FOnBrowserItemDestroyed, OnItemDestroyed )

		/** If false, the tooltip will not be displayed */
		SLATE_ATTRIBUTE( bool, ShouldAllowToolTip )

		/** If false, will not show type */
		SLATE_ARGUMENT( bool, ShowType )

		/** The string in the title to highlight (used when searching by string) */
		SLATE_ATTRIBUTE( FText, HighlightText )

		/** The string to present as the text for this item */
		SLATE_ATTRIBUTE( FText, AssetOverrideLabelText )

		/** If true, the thumbnail in this item can be edited */
		SLATE_ATTRIBUTE( bool, ThumbnailEditMode )

		/** Whether the item is selected in the view */
		SLATE_ARGUMENT(FIsSelected, IsSelected)

		/** Whether the item is selected in the view without anything else being selected*/
		SLATE_ARGUMENT(FIsSelected, IsSelectedExclusively)

		/** Delegate to call (if bound) to check if it is valid to get a custom tooltip for this view item */
		SLATE_EVENT(FOnIsAssetValidForCustomToolTip, OnIsAssetValidForCustomToolTip)

		/** Delegate to request a custom tool tip if necessary */
		SLATE_EVENT(FOnGetCustomAssetToolTip, OnGetCustomAssetToolTip)

		/* Delegate to signal when the item is about to show a tooltip */
		SLATE_EVENT(FOnVisualizeAssetToolTip, OnVisualizeAssetToolTip)

		/** Delegate for when an item's tooltip is about to close */
		SLATE_EVENT( FOnAssetToolTipClosing, OnAssetToolTipClosing )

	SLATE_END_ARGS()

	/** Destructor */
	~SInputAssetTileItem();

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	/** Handles realtime thumbnails */
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** Handles realtime thumbnails */
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	/** Whether the widget should allow primitive tools to be displayed */
	bool CanDisplayPrimitiveTools() const { return true; }

	/** Get the border image to display */
	virtual const FSlateBrush* GetBorderImage() const;

	static void InitializeAssetNameHeights();
	static float GetRegularFontHeight() { return RegularFontHeight; }
	static float GetSmallFontHeight() { return SmallFontHeight; }

protected:
	/** SAssetViewItem interface */
	virtual float GetNameTextWrapWidth() const { return LastGeometry.GetLocalSize().X - 15.f; }

	/** Get the expected width of an extra state icon. */
	float GetExtraStateIconWidth() const;

	/** Returns the max width size to be used by extra state icons. */
	FOptionalSize GetExtraStateIconMaxWidth() const;

	/** Returns the size of the state icon box widget (i.e dirty image, scc)*/
	FOptionalSize GetStateIconImageSize() const;

	/** Returns the size of the thumbnail widget */
	FOptionalSize GetThumbnailBoxSize() const;

	/** Gets the visibility of the asset class label in thumbnails */
	EVisibility GetAssetClassLabelVisibility() const;

	/** Gets the color of the asset class label in thumbnails */
	FSlateColor GetAssetClassLabelTextColor() const;

	/** Returns the font to use for the thumbnail label */
	FSlateFontInfo GetThumbnailFont() const;

	const FSlateBrush* GetFolderBackgroundImage() const;
	const FSlateBrush* GetFolderBackgroundShadowImage() const;

	const FSlateBrush* GetNameAreaBackgroundImage() const;
	FSlateColor GetNameAreaTextColor() const;

	FOptionalSize GetNameAreaMaxDesiredHeight() const;

	int32 GetGenericThumbnailSize() const;

	/** Gets the visibility of the SCC icons */
	EVisibility GetSCCIconVisibility() const;


private:
	/** If false, the tooltip will not be displayed */
	bool bShowType;

	/** The handle to the thumbnail that this item is rendering */
	TSharedPtr<FAssetThumbnail> AssetThumbnail;

	/** The width of the item. Used to enforce a square thumbnail. */
	TAttribute<float> ItemWidth;

	/** Max name height for each thumbnail size */
	static float AssetNameHeights[(int32)EThumbnailSize::MAX];

	/** Regular thumbnail font size */
	static float RegularFontHeight;

	/** Small thumbnail font size */
	static float SmallFontHeight;

	/** The padding allotted for the thumbnail */
	float ThumbnailPadding;

	/** Current thumbnail size when this widget was generated */
	TAttribute<EThumbnailSize> CurrentThumbnailSize;

public:
	/** NOTE: Any functions overridden from the base widget classes *must* also be overridden by SAssetColumnViewRow and forwarded on to its internal item */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual TSharedPtr<IToolTip> GetToolTip() override;
	virtual bool OnVisualizeTooltip( const TSharedPtr<SWidget>& TooltipContent ) override;
	virtual void OnToolTipClosing() override;

	/** Returns the color this item should be tinted with */
	virtual FSlateColor GetAssetColor() const;

	/** Get the name text to be displayed for this item */
	FText GetNameText() const;

protected:
	/** Check to see if the name should be read-only */
	bool IsNameReadOnly() const;

	/** Handles committing a name change */
	virtual void OnAssetDataChanged();

	/** Notification for when the dirty flag changes */
	virtual void DirtyStateChanged();

	/** Gets the name of the class of this asset */
	FText GetAssetClassText() const;

	/** Gets the brush for the dirty indicator image */
	const FSlateBrush* GetDirtyImage() const;

	/** Generate a widget to inject extra external state indicator on the asset. */
	TSharedRef<SWidget> GenerateExtraStateIconWidget(TAttribute<float> InMaxExtraStateIconWidth) const;

	/** Generate a widget to inject extra external state indicator on the asset tooltip. */
	TSharedRef<SWidget> GenerateExtraStateTooltipWidget() const;

	/** Gets the visibility for the thumbnail edit mode UI */
	EVisibility GetThumbnailEditModeUIVisibility() const;

	/** Creates a tooltip widget for this item */
	TSharedRef<SWidget> CreateToolTipWidget() const;

	/** Gets the visibility of the source control text block in the tooltip */
	EVisibility GetSourceControlTextVisibility() const;

	/** Helper function for CreateToolTipWidget. Gets the user description for the asset, if it exists. */
	FText GetAssetUserDescription() const;

	/** Helper function for CreateToolTipWidget. Adds a key value pair to the info box of the tooltip */
	void AddToToolTipInfoBox(const TSharedRef<SVerticalBox>& InfoBox, const FText& Key, const FText& Value, bool bImportant) const;

	/** Updates the bPackageDirty flag */
	void UpdateDirtyState();

	/** Returns true if the item is dirty. */
	bool IsDirty() const;
	
	/** Cache the display tags for this item */
	void CacheDisplayTags();

	/** Whether this item is a folder */
	bool IsFolder() const;

protected:
	/** Data for a cached display tag for this item (used in the tooltip, and also as the display string in column views) */
	struct FTagDisplayItem
	{
		FTagDisplayItem(FName InTagKey, FText InDisplayKey, FText InDisplayValue, const bool InImportant)
			: TagKey(InTagKey)
			, DisplayKey(MoveTemp(InDisplayKey))
			, DisplayValue(MoveTemp(InDisplayValue))
			, bImportant(InImportant)
		{
		}

		FName TagKey;
		FText DisplayKey;
		FText DisplayValue;
		bool bImportant;
	};

	TSharedPtr<STextBlock> ItemLabelWidget;

	TSharedPtr<STextBlock> ClassTextWidget;

	/** The data for this item */
	TSharedPtr<FContentBrowserItem> AssetItem;

	/** The cached display tags for this item */
	TArray<FTagDisplayItem> CachedDisplayTags;

	/** Called when any asset item is destroyed. Used in thumbnail management */
	FOnBrowserItemDestroyed OnItemDestroyed;

	/** Called to test if it is valid to make a custom tool tip for that asset */
	FOnIsAssetValidForCustomToolTip OnIsAssetValidForCustomToolTip;

	/** Called if bound to get a custom asset item tooltip */
	FOnGetCustomAssetToolTip OnGetCustomAssetToolTip;

	/** Called if bound when about to show a tooltip */
	FOnVisualizeAssetToolTip OnVisualizeAssetToolTip;

	/** Called if bound when a tooltip is closing */
	FOnAssetToolTipClosing OnAssetToolTipClosing;
	
	/** Delegate for getting the selection state of this item */ 
	FIsSelected IsSelected;

	FGeometry LastGeometry;

	/** If false, the tooltip will not be displayed */
	TAttribute<bool> ShouldAllowToolTip;

	/** If true, display the thumbnail edit mode UI */
	TAttribute<bool> ThumbnailEditMode;

	/** The substring to be highlighted in the name and tooltip path */
	TAttribute<FText> HighlightText;

	TAttribute<FText> AssetOverrideLabelText;

	/** Cached brushes for the dirty state */
	const FSlateBrush* AssetDirtyBrush;

	/** Cached flag describing if the item is dirty */
	bool bItemDirty;

	/** True when a drag is over this item with a drag operation that we know how to handle. The operation itself may not be valid to drop. */
	bool bDraggedOver;
};

///////////////////////////////////////////////////////////////////////////////////////////////////


