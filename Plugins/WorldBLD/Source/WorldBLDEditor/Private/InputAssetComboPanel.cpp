// Copyright WorldBLD LLC. All rights reserved.

#include "InputAssetComboPanel.h"

#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/SListView.h"
#include "SAssetView.h"
#include "SAssetDropTarget.h"

#include "Editor.h"

#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateTypes.h"

#include "Widgets/SBoxPanel.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Styling/AppStyle.h"
#include "FrontendFilters.h"
#include "SAssetSearchBox.h"
#include "Framework/Commands/GenericCommands.h"
#include "PropertyHandle.h"

#include "AssetViewTypes.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "IContentBrowserDataModule.h"
#include "Settings/ContentBrowserSettings.h"
#include "AssetToolsModule.h"
#include "EditorWidgetsModule.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Fonts/FontMeasure.h"
#include "Internationalization/BreakIterator.h"
#include "ContentBrowserDataSource.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "ContentBrowserModule.h"
#include "Misc/ComparisonUtility.h"
#include "Styling/DefaultStyleCache.h"
#include "Engine/Font.h"
#include "ICollectionContainer.h"
#include "UObject/Package.h"

#include "InputAssetFilterUtils.h"
#include "WorldBLDLoadedKitsAssetFilter.h"
#include "WorldBLDEditorToolkit.h"
#include "AssetLibrary/WorldBLDAssetLibraryWindow.h"

#include "Engine/Blueprint.h"

#define LOCTEXT_NAMESPACE "WorldBLDEditor"

///////////////////////////////////////////////////////////////////////////////////////////////////

UInputAssetComboPanel::UInputAssetComboPanel(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
	FallbackSelectionText = LOCTEXT("FallbackText", "Select Asset...");
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	WidgetStyle = UE::Slate::Private::FDefaultStyleCache::GetRuntime().GetComboBoxStyle();
	
#if WITH_EDITOR 
	if (IsEditorWidget())
	{
		WidgetStyle = UE::Slate::Private::FDefaultStyleCache::GetEditor().GetComboBoxStyle();

		// The CDO isn't an editor widget and thus won't use the editor style, call post edit change to mark difference from CDO
		PostEditChange();
	}
#endif
	
	// We don't want to try and load fonts on the server.
	if ( !IsRunningDedicatedServer() )
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(*UWidget::GetDefaultFontName());
		SelectionFont = FSlateFontInfo(RobotoFontObj.Object, 16, FName("Bold"));
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	AssetClass = UObject::StaticClass();
}

TSharedRef<SWidget> UInputAssetComboPanel::RebuildWidget()
{
	BindToWorldBLDEdMode();

	FAssetComboFilterSetup FilterSetup;
	if (bPresentSpecificObjects)
	{
		FilterSetup.bPresentSpecificAssets = true;
		for (UObject* Object : SpecificObjects)
		{
			if (IsValid(Object))
			{
				FilterSetup.SpecificAssets.Add(FAssetData(Object, /* Allow Blueprint*/ true));
			}
		}
	}
	else
	{
		FilterSetup.AssetClassType = IsValid(AssetClass) ? AssetClass.Get() : UObject::StaticClass();
		if (bLoadedKitsOnly)
		{
			FilterSetup.bRestrictToAllowedPackageNames = true;
			FilterSetup.AllowedPackageNames = WorldBLD::Editor::LoadedKitsAssetFilter::GetAllowedPackageNamesFromLoadedKits();
		}
	}

	MyMenu = SNew(SInputAssetComboPanel)
		.ComboButtonTileSize(FVector2D(60, 60))
		.FallbackSelectionText_Lambda([this]() {return this->FallbackSelectionText;})
		.ShowSelectedItemLabelOnly(true)
		.ForegroundColor(GetForegroundColor())
		.ComboBoxStyle(&WidgetStyle)
		.SelectedItemFont(SelectionFont)
		.FlyoutTileSize(FVector2D(80, 80))
		.FlyoutSize(FVector2D(600, 400))
		.ContentPadding(ContentPadding)
		.FilterSetup(FilterSetup)
		.OnOpening(SInputAssetComboPanel::FOnOpeningEvent::CreateUObject(this, &UInputAssetComboPanel::HandleComboOpened))
		.OnSelectionChanged(SInputAssetComboPanel::FOnSelectedAssetChanged::CreateUObject(this, &UInputAssetComboPanel::HandleSelectionChanged))
		.OnGetAssetLabel_Lambda([this](const FAssetData& AssetData, int32 Idx) {
			if (OnGenerateLabelForAsset.IsBound() && this->SpecificObjects.IsValidIndex(Idx))
			{
				return OnGenerateLabelForAsset.Execute(this, this->SpecificObjects[Idx], Idx);
			}
			else
			{
				return FText::FromName(AssetData.GetAsset()->GetFName());
			}
		})
		.AssetThumbnailLabel(EThumbnailLabel::NoLabel);
	return MyMenu.ToSharedRef();
}

void UInputAssetComboPanel::SynchronizeProperties()
{
	Super::SynchronizeProperties();
}

void UInputAssetComboPanel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	UnbindFromWorldBLDEdMode();
	MyMenu.Reset();
}

void UInputAssetComboPanel::SetSpecificObjects(const TArray<UObject*>& Objects)
{
	bPresentSpecificObjects = true;
	SpecificObjects = Objects;
	UpdateInterfaceState();
}

void UInputAssetComboPanel::HandleComboOpened()
{
	OnOpening.Broadcast(this);
	UpdateInterfaceState();
}

void UInputAssetComboPanel::HandleWorldBLDKitsChanged(UWorldBLDEdMode* InEdMode)
{
	if (!bPresentSpecificObjects && bLoadedKitsOnly)
	{
		UpdateFilterSetupOnly();
	}
}

void UInputAssetComboPanel::HandleWorldBLDKitBundlesChanged(UWorldBLDEdMode* InEdMode)
{
	if (!bPresentSpecificObjects && bLoadedKitsOnly)
	{
		UpdateFilterSetupOnly();
	}
}

void UInputAssetComboPanel::BindToWorldBLDEdMode()
{
	UnbindFromWorldBLDEdMode();

	UWorldBLDEdMode* EdMode = UWorldBLDEdMode::GetWorldBLDEdMode();
	if (!IsValid(EdMode))
	{
		return;
	}

	BoundEdMode = EdMode;
	EdMode->OnWorldBLDKitListChanged.AddUniqueDynamic(this, &UInputAssetComboPanel::HandleWorldBLDKitsChanged);
	EdMode->OnWorldBLDKitBundleListChanged.AddUniqueDynamic(this, &UInputAssetComboPanel::HandleWorldBLDKitBundlesChanged);
}

void UInputAssetComboPanel::UnbindFromWorldBLDEdMode()
{
	if (UWorldBLDEdMode* EdMode = BoundEdMode.Get())
	{
		EdMode->OnWorldBLDKitListChanged.RemoveDynamic(this, &UInputAssetComboPanel::HandleWorldBLDKitsChanged);
		EdMode->OnWorldBLDKitBundleListChanged.RemoveDynamic(this, &UInputAssetComboPanel::HandleWorldBLDKitBundlesChanged);
	}
	BoundEdMode.Reset();
}

void UInputAssetComboPanel::HandleSelectionChanged(const FAssetData& Selection)
{
	if (MyMenu.IsValid())
	{
		const auto ResolveSelectionClass = [](const FAssetData& InSelection) -> UClass*
		{
			if (!InSelection.IsValid())
			{
				return nullptr;
			}

			// If this is a blueprint(-like) asset, the class we actually care about is the generated class.
			FString GeneratedClassPathStr = InSelection.GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath);
			if (GeneratedClassPathStr.IsEmpty())
			{
				// Fallback for non-standard/legacy tags.
				GeneratedClassPathStr = InSelection.GetTagValueRef<FString>(FName(TEXT("GeneratedClass")));
				if (GeneratedClassPathStr.IsEmpty())
				{
					GeneratedClassPathStr = InSelection.GetTagValueRef<FString>(FName(TEXT("GeneratedClassPath")));
				}
			}

			if (!GeneratedClassPathStr.IsEmpty())
			{
				if (UClass* GeneratedClass = FSoftClassPath(GeneratedClassPathStr).TryLoadClass<UObject>())
				{
					return GeneratedClass;
				}
			}

			return InSelection.GetClass();
		};

		UObject* SelectedItem = nullptr;
		UClass* SelectedClass = nullptr;
		int32 Idx = INDEX_NONE;

		if (!bPresentSpecificObjects)
		{
			SelectedClass = ResolveSelectionClass(Selection);
			SelectedItem = Selection.GetAsset();

			// If we loaded a Blueprint asset, prefer its generated class (avoids returning UBlueprint::StaticClass()).
			if (UBlueprint* Blueprint = Cast<UBlueprint>(SelectedItem))
			{
				if (IsValid(Blueprint->GeneratedClass))
				{
					SelectedClass = Blueprint->GeneratedClass;
				}
			}

			SelectedObject = SelectedItem;
			OnSelectionChanged.Broadcast(this, SelectedItem, SelectedClass, INDEX_NONE, ESelectInfo::Direct);
			return;
		}

		if (MyMenu->FilterSetup.SpecificAssets.Find(Selection, Idx) && SpecificObjects.IsValidIndex(Idx))
		{
			SelectedItem = SpecificObjects[Idx];
			SelectedClass = IsValid(SelectedItem) ? SelectedItem->GetClass() : nullptr;

			SelectedObject = SelectedItem;
			OnSelectionChanged.Broadcast(this, SelectedItem, SelectedClass, Idx, ESelectInfo::Direct);
		}
		else
		{
			SelectedObject = nullptr;
			OnSelectionChanged.Broadcast(this, nullptr, nullptr, INDEX_NONE, ESelectInfo::Direct);
		}
	}
}

const FComboBoxStyle& UInputAssetComboPanel::GetWidgetStyle() const
{
	return WidgetStyle;
}

const FSlateFontInfo& UInputAssetComboPanel::GetSelectionFont() const
{
	return SelectionFont;
}

void UInputAssetComboPanel::SetWidgetStyle(const FComboBoxStyle& InWidgetStyle)
{
	WidgetStyle = InWidgetStyle;
	if (MyMenu.IsValid())
	{
		MyMenu->Invalidate(EInvalidateWidgetReason::Layout);
	}
}

void UInputAssetComboPanel::SetContentPadding(FMargin InPadding)
{
	ContentPadding = InPadding;
	if (MyMenu.IsValid())
	{
		MyMenu->ComboButton->SetButtonContentPadding(InPadding);
	}
}

FMargin UInputAssetComboPanel::GetContentPadding() const
{
	return ContentPadding;
}


FSlateColor UInputAssetComboPanel::GetForegroundColor() const
{
	return ForegroundColor;
}

void UInputAssetComboPanel::AddOption(UObject* Option)
{
	bPresentSpecificObjects = true;
	SpecificObjects.Add(Option);
	UpdateInterfaceState();
}

bool UInputAssetComboPanel::RemoveOption(UObject* Option)
{
	int32 RemIdx = SpecificObjects.Remove(Option);
	UpdateInterfaceState();
	return RemIdx != INDEX_NONE;
}

int32 UInputAssetComboPanel::FindOptionIndex(UObject* Option) const
{
	return IsValid(Option) ? SpecificObjects.Find(Option) + 1 : 0;
}

UObject* UInputAssetComboPanel::GetOptionAtIndex(int32 Index) const
{
	return ((Index == 0) || !SpecificObjects.IsValidIndex(Index - 1)) ? nullptr : SpecificObjects[Index - 1];
}

void UInputAssetComboPanel::ClearOptions()
{
	SpecificObjects.Empty();
	ClearSelection();
}

void UInputAssetComboPanel::ClearSelection()
{
	SelectedObject = nullptr;
	UpdateInterfaceState();
}

void UInputAssetComboPanel::SetSelectedOption(UObject* Option)
{
	SelectedObject = GetOptionAtIndex(FindOptionIndex(Option));
	UpdateInterfaceState();
}

void UInputAssetComboPanel::SetSelectedIndex(const int32 Index)
{
	SelectedObject = GetOptionAtIndex(Index);
	UpdateInterfaceState();
}

UObject* UInputAssetComboPanel::GetSelectedOption() const
{
	return SelectedObject;
}

int32 UInputAssetComboPanel::GetSelectedIndex() const
{
	return IsValid(SelectedObject) ? (SpecificObjects.Find(SelectedObject) + 1) : 0;
}

int32 UInputAssetComboPanel::GetOptionCount() const
{
	return SpecificObjects.Num() + 1;
}

void UInputAssetComboPanel::UpdateInterfaceState()
{
	if (!MyMenu.IsValid())
	{
		return;
	}

	// If the filter setup changed, forward that to the widget.
	FAssetComboFilterSetup FilterSetup;
	if (bPresentSpecificObjects)
	{
		FilterSetup.bPresentSpecificAssets = true;
		for (UObject* Object : SpecificObjects)
		{
			if (IsValid(Object))
			{
				FilterSetup.SpecificAssets.Add(FAssetData(Object, /* Allow Blueprint*/ true));
			}
		}
	}
	else
	{
		FilterSetup.AssetClassType = IsValid(AssetClass) ? AssetClass.Get() : UObject::StaticClass();
		if (bLoadedKitsOnly)
		{
			FilterSetup.bRestrictToAllowedPackageNames = true;
			FilterSetup.AllowedPackageNames = WorldBLD::Editor::LoadedKitsAssetFilter::GetAllowedPackageNamesFromLoadedKits();
		}
	}

	MyMenu->FilterSetup = FilterSetup;
	MyMenu->NewAssetSelected(FAssetData(SelectedObject, /* Allow Blueprint*/ true));
	MyMenu->Invalidate(EInvalidateWidgetReason::Layout);
}

void UInputAssetComboPanel::UpdateFilterSetupOnly()
{
	if (!MyMenu.IsValid())
	{
		return;
	}

	FAssetComboFilterSetup FilterSetup;
	if (!bPresentSpecificObjects)
	{
		FilterSetup.AssetClassType = IsValid(AssetClass) ? AssetClass.Get() : UObject::StaticClass();
		if (bLoadedKitsOnly)
		{
			FilterSetup.bRestrictToAllowedPackageNames = true;
			FilterSetup.AllowedPackageNames = WorldBLD::Editor::LoadedKitsAssetFilter::GetAllowedPackageNamesFromLoadedKits();
		}
	}

	MyMenu->FilterSetup = FilterSetup;
	MyMenu->Invalidate(EInvalidateWidgetReason::Layout);
}

///////////////////////////////////////////////////////////////////////////////////////////////////


void SInputAssetComboPanel::Construct(const FArguments& InArgs)
{
	this->ComboButtonTileSize = InArgs._ComboButtonTileSize;
	this->FlyoutTileSize = InArgs._FlyoutTileSize;
	this->FlyoutSize = InArgs._FlyoutSize;
	this->Property = InArgs._Property;
	this->CollectionSets = InArgs._CollectionSets;
	this->OnSelectionChanged = InArgs._OnSelectionChanged;
	this->OnOpening = InArgs._OnOpening;
	this->OnGetAssetLabel = InArgs._OnGetAssetLabel;
	RecentAssetsProvider = InArgs._RecentAssetsProvider;
	this->FilterSetup = InArgs._FilterSetup;
	this->OnShouldFilterAsset = InArgs._OnShouldFilterAsset;
	this->FallbackSelectionText = InArgs._FallbackSelectionText;
	this->SelectedItemFont = InArgs._SelectedItemFont;
	this->ForegroundColor = InArgs._ForegroundColor;
	this->ContentPadding = InArgs._ContentPadding;

	FText UseTooltipText = Property.IsValid() ? Property->GetToolTipText() : InArgs._ToolTipText;

	// create our own thumbnail pool?
	ThumbnailPool = UThumbnailManager::Get().GetSharedThumbnailPool();// MakeShared<FAssetThumbnailPool>(20, 1.0 /** must be large or thumbnails will not render?? */ );
	ensure(ThumbnailPool.IsValid());

	// make an empty asset thumbnail
	FAssetThumbnailConfig ThumbnailConfig;
	ThumbnailConfig.bAllowRealTimeOnHovered = false;
	this->AssetThumbnail = MakeShared<FAssetThumbnail>(FAssetData(), ComboButtonTileSize.X, ComboButtonTileSize.Y, ThumbnailPool);

	TSharedRef<SBox> IconBox = SNew(SBox)
		.WidthOverride(ComboButtonTileSize.X)
		.HeightOverride(ComboButtonTileSize.Y)
		[
			AssetThumbnail->MakeThumbnailWidget(ThumbnailConfig)
		];

	OurComboButtonStyle = InArgs._ComboBoxStyle->ComboButtonStyle;
	const FButtonStyle* const OurButtonStyle = InArgs._ButtonStyle ? InArgs._ButtonStyle : &OurComboButtonStyle.ButtonStyle;

	if (InArgs._ShowSelectedItemLabelOnly)
	{
		ComboButton = SNew(SComboButton)
			.ComboButtonStyle(&OurComboButtonStyle)
			.ButtonStyle(OurButtonStyle)
			.ToolTipText(UseTooltipText)
			.ForegroundColor(ForegroundColor)
			.HasDownArrow(true)
			.ContentPadding(ContentPadding)
			.ButtonContent()
			[
				SAssignNew(SelectedItemBorder, SBorder)
				.BorderImage(nullptr)
				[
					SAssignNew(SelectedItemLabel, STextBlock)
						.Font(SelectedItemFont)
						.Text_Lambda([this]() {
							int32 AssetIdx = INDEX_NONE;
							bool bValidSelection = (this->OnGetAssetLabel.IsBound() && IsValid(this->AssetThumbnail->GetAsset()));
							if (bValidSelection)
							{
								this->FilterSetup.SpecificAssets.Find(this->AssetThumbnail->GetAsset(), AssetIdx);
							}
							return bValidSelection ? 
									this->OnGetAssetLabel.Execute(this->AssetThumbnail->GetAssetData(), AssetIdx) : 
									this->FallbackSelectionText.Get();
						})
				]
			];
	}
	else
	{
		ComboButton = SNew(SComboButton)
			.ComboButtonStyle(&OurComboButtonStyle)
			.ButtonStyle(OurButtonStyle)
			.ToolTipText(UseTooltipText)
			.HasDownArrow(true)
			.ButtonContent()
			[
				SNew(SBorder)
					.Visibility(EVisibility::SelfHitTestInvisible)
					.Padding(FMargin(0.0f))
					.BorderImage(FAppStyle::Get().GetBrush("ProjectBrowser.ProjectTile.DropShadow"))
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						//.Padding(1)
						[
							SNew(SVerticalBox)
							// Thumbnail
							+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(SBox)
								.WidthOverride(ComboButtonTileSize.X)
								.HeightOverride(ComboButtonTileSize.Y)
								[
									SAssignNew(ThumbnailBorder, SBorder)
									.Padding(0)
									.BorderImage(FStyleDefaults::GetNoBrush())
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									.OnMouseDoubleClick(this, &SInputAssetComboPanel::OnAssetThumbnailDoubleClick)
									[
										IconBox
									]
								]
							]
						]

					]
			];
	}


	EThumbnailLabel::Type TileThumbnailLabel = InArgs._AssetThumbnailLabel;
	bool bForceShowEngineContent = InArgs._bForceShowEngineContent;
	bool bForceShowPluginContent = InArgs._bForceShowPluginContent;

	ComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda([this, TileThumbnailLabel,bForceShowEngineContent, bForceShowPluginContent]()
	{
		this->HandleComboOpened();

		// Configure filter for asset picker
		FAssetPickerConfig Config;
		Config.SelectionMode = ESelectionMode::Single;
		WorldBLD::Editor::InputAssetFilterUtils::ApplyFilterSetupToAssetPickerConfig(Config, this->FilterSetup);
		// IMPORTANT:
		// ApplyFilterSetupToAssetPickerConfig may have already composed a filter (eg, blueprint GeneratedClass derived-from checks).
		// Don't stomp it here; combine with any additional filter provided by the caller.
		{
			const FOnShouldFilterAsset ExistingFilter = Config.OnShouldFilterAsset;
			const FOnShouldFilterAsset AdditionalFilter = this->OnShouldFilterAsset;
			if (ExistingFilter.IsBound() || AdditionalFilter.IsBound())
			{
				Config.OnShouldFilterAsset = FOnShouldFilterAsset::CreateLambda(
					[ExistingFilter, AdditionalFilter](const FAssetData& Asset)
					{
						const bool bFilteredByExisting = ExistingFilter.IsBound() ? ExistingFilter.Execute(Asset) : false;
						const bool bFilteredByAdditional = AdditionalFilter.IsBound() ? AdditionalFilter.Execute(Asset) : false;
						return bFilteredByExisting || bFilteredByAdditional;
					});
			}
		}

		if (this->FilterSetup.bRestrictToAllowedPackageNames && !this->FilterSetup.bPresentSpecificAssets)
		{
			const UWorldBLDEdMode* EdMode = UWorldBLDEdMode::GetWorldBLDEdMode();
			const bool bHasAnyLoadedKits = IsValid(EdMode) && (EdMode->GetLoadedWorldBLDKitCount() > 0);
			Config.AssetShowWarningText = bHasAnyLoadedKits ?
				LOCTEXT("LoadedKitsOnlyNoMatchingAssets", "No matching assets were found in your Kits. Please load another, or disable Only Show Kit Assets.") :
				LOCTEXT("LoadedKitsOnlyNoKitsInLevel", "There are no Kits in your level. Please load in a new Kit.");
		}

		//Config.Filter.bIncludeOnlyOnDiskAssets = true;
		Config.InitialAssetViewType = EAssetViewType::Tile;
		Config.bFocusSearchBoxWhenOpened = true;
		Config.bAllowNullSelection = true;
		Config.bAllowDragging = false;
		Config.ThumbnailLabel = TileThumbnailLabel;
		Config.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SInputAssetComboPanel::NewAssetSelected);
		Config.bForceShowEngineContent = bForceShowEngineContent;
		Config.bForceShowPluginContent = bForceShowPluginContent;

		// build asset picker UI
		TSharedRef<SInputAssetPicker> AssetPickerWidget = SAssignNew( AssetPicker, SInputAssetPicker )
			.OnGetBrowserItemText_Lambda([this](const FAssetData& AssetItem) {
				if (this->OnGetAssetLabel.IsBound())
				{	int32 Idx;
					this->FilterSetup.SpecificAssets.Find(AssetItem, Idx);
					return this->OnGetAssetLabel.Execute(AssetItem, Idx);
				}
				else
				{
					return FText::FromName(AssetItem.AssetName);
				}
			})
			.IsEnabled( true )
			.AssetPickerConfig( Config );
		

		TSharedRef<SVerticalBox> PopupContent = SNew(SVerticalBox);

		int32 FilterButtonBarVertPadding = 2;
		UpdateRecentAssets();
		if ( RecentAssetsProvider.IsValid() && RecentAssetData.Num() > 0 )
		{
			TSharedRef<SListView<TSharedPtr<FRecentAssetInfo>>> RecentsListView = SNew(SListView<TSharedPtr<FRecentAssetInfo>>)
				.Orientation(Orient_Horizontal)
				.ListItemsSource(&RecentAssetData)
				.OnGenerateRow(this, &SInputAssetComboPanel::OnGenerateWidgetForRecentList)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FRecentAssetInfo> SelectedItem, ESelectInfo::Type SelectInfo)
				{
					NewAssetSelected(SelectedItem->AssetData);
				})
				.ClearSelectionOnClick(false)
				.SelectionMode(ESelectionMode::Single);

			PopupContent->AddSlot()
			.AutoHeight()
			[
				SNew(SBox)
				.Padding(6.0f)
				.HeightOverride(FlyoutTileSize.Y + 30.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RecentsHeaderText", "Recently Used"))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						RecentsListView->AsShared()
					]
				]
			];
		}
		else
		{
			FilterButtonBarVertPadding = 10;
		}

		if (CollectionSets.Num() > 0)
		{
			PopupContent->AddSlot()
			.AutoHeight()
			.Padding(10, FilterButtonBarVertPadding, 4, 2)
			[
				MakeCollectionSetsButtonPanel(AssetPickerWidget)
			];
		}

		PopupContent->AddSlot()
		[
			SNew(SBox)
			.Padding(6.0f)
			.HeightOverride(FlyoutSize.Y)
			.WidthOverride(FlyoutSize.X)
			[
				SNew( SBorder )
				.BorderImage( FAppStyle::GetBrush("Menu.Background") )
				[
					AssetPickerWidget
				]
			]
		];

		return PopupContent;
	}));


	// set initial thumbnail
	RefreshThumbnailFromProperty();

	if (InArgs._InitiallySelectedAsset.IsValid())
	{
		NewAssetSelected(InArgs._InitiallySelectedAsset);
	}

	ChildSlot
		[
			ComboButton->AsShared()
		];

}

void SInputAssetComboPanel::HandleComboOpened()
{
	OnOpening.ExecuteIfBound();
}

void SInputAssetComboPanel::RefreshThumbnailFromProperty()
{
	if (Property.IsValid())
	{
		FAssetData AssetData;
		if (Property->GetValue(AssetData) == FPropertyAccess::Result::Success)
		{
			AssetThumbnail->SetAsset(AssetData);
		}
	}
}


TSharedRef<SWidget> SInputAssetComboPanel::MakeCollectionSetsButtonPanel(TSharedRef<SInputAssetPicker> AssetPickerView)
{
	TArray<FNamedCollectionList> ShowCollectionSets = CollectionSets;
	ShowCollectionSets.Insert(FNamedCollectionList{ FString(), TArray<FCollectionNameType>() }, 0);

	ActiveCollectionSetIndex = 0;

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
		
	for ( int32 Index = 0; Index < ShowCollectionSets.Num(); ++Index)
	{
		const FNamedCollectionList& Collection = ShowCollectionSets[Index];

		HorizontalBox->AddSlot()
		.AutoWidth()
		.Padding(4,0)
		[
			SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.HAlign(HAlign_Center)
				.OnCheckStateChanged_Lambda([this, Index, AssetPickerView](ECheckBoxState NewState)
				{
					if ( NewState == ECheckBoxState::Checked )
					{
						AssetPickerView->UpdateAssetSourceCollections( (Index == 0) ? 
							TArray<FCollectionNameType>() : this->CollectionSets[Index-1].CollectionNames );
						ActiveCollectionSetIndex = Index;
					}
				})
				.IsChecked_Lambda([this, Index]() -> ECheckBoxState
				{
					return (this->ActiveCollectionSetIndex == Index) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(FMargin(4, 2))
					.AutoWidth()
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
						.Text( (Index == 0) ? LOCTEXT("AllFilterLabel", "Show All") :  FText::FromString(CollectionSets[Index-1].Name) )
					]
				]
		];
	}

	return HorizontalBox;
}


void SInputAssetComboPanel::UpdateRecentAssets()
{
	if (RecentAssetsProvider.IsValid() == false)
	{
		return;
	}
	TArray<FAssetData> RecentAssets = RecentAssetsProvider->GetRecentAssetsList();

	// The recent-assets strip is separate from the asset picker, so it must be filtered explicitly.
	RecentAssets.RemoveAll([this](const FAssetData& Asset)
	{
		return !WorldBLD::Editor::InputAssetFilterUtils::PassesFilterSetup(Asset, this->FilterSetup);
	});

	int32 NumRecent = RecentAssets.Num();

	while (RecentAssetData.Num() < NumRecent)
	{
		FAssetThumbnailConfig ThumbnailConfig;
		ThumbnailConfig.bAllowRealTimeOnHovered = false;

		TSharedPtr<FRecentAssetInfo> NewInfo = MakeShared<FRecentAssetInfo>();
		NewInfo->Index = RecentAssetData.Num();
		RecentAssetData.Add(NewInfo);

		RecentThumbnails.Add(MakeShared<FAssetThumbnail>(FAssetData(), FlyoutTileSize.X, FlyoutTileSize.Y, ThumbnailPool));
		RecentThumbnailWidgets.Add(SNew(SBox)
			.WidthOverride(FlyoutTileSize.X)
			.HeightOverride(FlyoutTileSize.Y)
			[
				RecentThumbnails[NewInfo->Index]->MakeThumbnailWidget(ThumbnailConfig)
			]);
	}

	for (int32 k = 0; k < NumRecent; ++k)
	{
		RecentAssetData[k]->AssetData = RecentAssets[k];
		RecentThumbnails[k]->SetAsset(RecentAssets[k]);
	}

}


TSharedRef<ITableRow> SInputAssetComboPanel::OnGenerateWidgetForRecentList(TSharedPtr<FRecentAssetInfo> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return  SNew(STableRow<TSharedPtr<FRecentAssetInfo>>, OwnerTable)
		//.Style(FAppStyle::Get(), "ProjectBrowser.TableRow")		// do not like this style...
		//.ToolTipText( InItem->AssetData )		// todo - cannot be assigned to STableRow, needs to go on nested widget
		.Padding(2.0f)
		[
			// todo: maybe add a border around this?
			RecentThumbnailWidgets[InItem->Index]->AsShared()
		];
}


void SInputAssetComboPanel::NewAssetSelected(const FAssetData& AssetData)
{
	AssetThumbnail->SetAsset(AssetData);

	if (Property.IsValid())
	{
		Property->SetValue(AssetData);
	}

	if (RecentAssetsProvider.IsValid())
	{
		RecentAssetsProvider->NotifyNewAsset(AssetData);
	}

	OnSelectionChanged.ExecuteIfBound(AssetData);

	ComboButton->SetIsOpen(false);
}


FReply SInputAssetComboPanel::OnAssetThumbnailDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	const FAssetData& CurData = AssetThumbnail->GetAssetData();
	if (CurData.IsValid())
	{
		UObject* ObjectToEdit = CurData.GetAsset();
		if( ObjectToEdit )
		{
			GEditor->EditObject( ObjectToEdit );
		}
	}

	// might be open from first click?
	ComboButton->SetIsOpen(false);

	return FReply::Handled();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

SInputAssetPicker::~SInputAssetPicker()
{
	// Empty
}

void SInputAssetPicker::Construct( const FArguments& InArgs )
{
	OnAssetsActivated = InArgs._AssetPickerConfig.OnAssetsActivated;
	OnAssetSelected = InArgs._AssetPickerConfig.OnAssetSelected;
	OnAssetEnterPressed = InArgs._AssetPickerConfig.OnAssetEnterPressed;
	bPendingFocusNextFrame = InArgs._AssetPickerConfig.bFocusSearchBoxWhenOpened;
	DefaultFilterMenuExpansion = InArgs._AssetPickerConfig.DefaultFilterMenuExpansion;

	if ( InArgs._AssetPickerConfig.bFocusSearchBoxWhenOpened )
	{
		RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &SInputAssetPicker::SetFocusPostConstruct ) );
	}

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	ChildSlot
	[
		VerticalBox
	];

	TAttribute< FText > HighlightText;
	EThumbnailLabel::Type ThumbnailLabel = InArgs._AssetPickerConfig.ThumbnailLabel;

	FrontendFilters = MakeShareable(new FAssetFilterCollectionType());

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	if (!InArgs._AssetPickerConfig.bAutohideSearchBar)
	{
		// Search box
		HighlightText = TAttribute< FText >(this, &SInputAssetPicker::GetHighlightedText);
		HorizontalBox->AddSlot()
		.FillWidth(1.0f)
		[
			SAssignNew( SearchBoxPtr, SAssetSearchBox )
			.HintText(NSLOCTEXT( "ContentBrowser", "SearchBoxHint", "Search Assets" ))
			.OnTextChanged( this, &SInputAssetPicker::OnSearchBoxChanged )
			.OnTextCommitted( this, &SInputAssetPicker::OnSearchBoxCommitted )
			.DelayChangeNotificationsWhileTyping( true )
			.OnKeyDownHandler( this, &SInputAssetPicker::HandleKeyDownFromSearchBox )
		];

	}
	else
	{
		HorizontalBox->AddSlot()
		.FillWidth(1.0)
		[
			SNew(SSpacer)
		];
	}

		
	VerticalBox->AddSlot()
	.AutoHeight()
	.Padding(8.f, 0.f, 8.f, 8.f)
	[
		HorizontalBox
	];

	// "None" button
	if (InArgs._AssetPickerConfig.bAllowNullSelection)
	{
		VerticalBox->AddSlot()
		.AutoHeight()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SButton)
						.ButtonStyle( FAppStyle::Get(), "ContentBrowser.NoneButton" )
						.TextStyle( FAppStyle::Get(), "ContentBrowser.NoneButtonText" )
						.Text( LOCTEXT("NoneButtonText", "( Clear Selection )") )
						.ToolTipText( LOCTEXT("NoneButtonTooltip", "Clears the asset selection.") )
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked(this, &SInputAssetPicker::OnNoneButtonClicked)
				]

			// "Download More Presets" button
			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SButton)
						.ButtonStyle( FAppStyle::Get(), "ContentBrowser.NoneButton" )
						.TextStyle( FAppStyle::Get(), "ContentBrowser.NoneButtonText" )
						.Text( LOCTEXT("DownloadMorePresetsButtonText", "Download More Presets") )
						.ToolTipText( LOCTEXT("DownloadMorePresetsButtonTooltip", "Opens the WorldBLD Asset Library to download more presets.") )
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked_Lambda([]()
						{
							FWorldBLDAssetLibraryWindow::OpenAssetLibrary();
							return FReply::Handled();
						})
				]

			// Trailing separator
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(SSeparator)
						.Orientation(Orient_Horizontal)
				]
		];
	}

	// Asset view
	
	// Break up the incoming filter into a sources data and backend filter.
	{
		TArray<FCollectionNameType> LegacyCollections;
		LegacyCollections.Reserve(InArgs._AssetPickerConfig.CollectionsFilter.Num());
		for (const FCollectionRef& CollectionRef : InArgs._AssetPickerConfig.CollectionsFilter)
		{
			LegacyCollections.Emplace(CollectionRef.Name, CollectionRef.Type);
		}
		CurrentSourcesData = FSourcesData(InArgs._AssetPickerConfig.Filter.PackagePaths, MoveTemp(LegacyCollections));
	}
	CurrentBackendFilter = InArgs._AssetPickerConfig.Filter;
	CurrentBackendFilter.PackagePaths.Reset();

	// Make game-specific filter
	FOnShouldFilterAsset ShouldFilterAssetDelegate;
	{
		FAssetReferenceFilterContext AssetReferenceFilterContext;
		AssetReferenceFilterContext.AddReferencingAssets(InArgs._AssetPickerConfig.AdditionalReferencingAssets);
		if (InArgs._AssetPickerConfig.PropertyHandle.IsValid())
		{
			AssetReferenceFilterContext.AddReferencingAssetsFromPropertyHandle(InArgs._AssetPickerConfig.PropertyHandle);
		}
		TSharedPtr<IAssetReferenceFilter> AssetReferenceFilter = GEditor ? GEditor->MakeAssetReferenceFilter(AssetReferenceFilterContext) : nullptr;
		if (AssetReferenceFilter.IsValid())
		{
			FOnShouldFilterAsset ConfigFilter = InArgs._AssetPickerConfig.OnShouldFilterAsset;
			ShouldFilterAssetDelegate = FOnShouldFilterAsset::CreateLambda([ConfigFilter, AssetReferenceFilter](const FAssetData& AssetData) -> bool {
				if (!AssetReferenceFilter->PassesFilter(AssetData))
				{
					return true;
				}
				if (ConfigFilter.IsBound())
				{
					return ConfigFilter.Execute(AssetData);
				}
				return false;
			});
		}
		else
		{
			ShouldFilterAssetDelegate = InArgs._AssetPickerConfig.OnShouldFilterAsset;
		}
	}

	VerticalBox->AddSlot()
	.FillHeight(1.f)
	[
		SAssignNew(AssetViewPtr, SInputAssetView)
		.InitialCategoryFilter(EContentBrowserItemCategoryFilter::IncludeAssets)
		.SelectionMode( InArgs._AssetPickerConfig.SelectionMode )
		.OnShouldFilterAsset(ShouldFilterAssetDelegate)
		.OnItemSelectionChanged(this, &SInputAssetPicker::HandleItemSelectionChanged)
		.OnItemsActivated(this, &SInputAssetPicker::HandleItemsActivated)
		.OnIsAssetValidForCustomToolTip(InArgs._AssetPickerConfig.OnIsAssetValidForCustomToolTip)
		.OnGetCustomAssetToolTip(InArgs._AssetPickerConfig.OnGetCustomAssetToolTip)
		.OnVisualizeAssetToolTip(InArgs._AssetPickerConfig.OnVisualizeAssetToolTip)
		.OnAssetToolTipClosing(InArgs._AssetPickerConfig.OnAssetToolTipClosing)
		.OnGetBrowserItemText(InArgs._OnGetBrowserItemText)
		.FrontendFilters(FrontendFilters)
		.InitialSourcesData(CurrentSourcesData)
		.InitialBackendFilter(CurrentBackendFilter)
		.InitialViewType(InArgs._AssetPickerConfig.InitialAssetViewType)
		.InitialAssetSelection(InArgs._AssetPickerConfig.InitialAssetSelection)
		.ShowBottomToolbar(InArgs._AssetPickerConfig.bShowBottomToolbar)
		.OnAssetTagWantsToBeDisplayed(InArgs._AssetPickerConfig.OnAssetTagWantsToBeDisplayed)
		.OnGetCustomSourceAssets(InArgs._AssetPickerConfig.OnGetCustomSourceAssets)
		.AllowDragging( InArgs._AssetPickerConfig.bAllowDragging )
		.CanShowClasses( InArgs._AssetPickerConfig.bCanShowClasses )
		.CanShowFolders( InArgs._AssetPickerConfig.bCanShowFolders )
		.CanShowReadOnlyFolders( InArgs._AssetPickerConfig.bCanShowReadOnlyFolders )
		.ShowPathInColumnView( InArgs._AssetPickerConfig.bShowPathInColumnView)
		.ShowTypeInColumnView( InArgs._AssetPickerConfig.bShowTypeInColumnView)
		.ShowViewOptions(false)  // We control this in the asset picker
		.SortByPathInColumnView( InArgs._AssetPickerConfig.bSortByPathInColumnView)
		.FilterRecursivelyWithBackendFilter( false )
		.CanShowRealTimeThumbnails( InArgs._AssetPickerConfig.bCanShowRealTimeThumbnails )
		.CanShowDevelopersFolder( InArgs._AssetPickerConfig.bCanShowDevelopersFolder )
		.ForceShowEngineContent( InArgs._AssetPickerConfig.bForceShowEngineContent )
		.ForceShowPluginContent( InArgs._AssetPickerConfig.bForceShowPluginContent )
		.HighlightedText( HighlightText )
		.ThumbnailLabel( ThumbnailLabel )
		.AssetShowWarningText( InArgs._AssetPickerConfig.AssetShowWarningText)
		.AllowFocusOnSync(false)	// Stop the asset view from stealing focus (we're in control of that)
		.HiddenColumnNames(InArgs._AssetPickerConfig.HiddenColumnNames)
		.CustomColumns(InArgs._AssetPickerConfig.CustomColumns)
		// custom stuff, should expose as parameters
		.InitialThumbnailSize(EThumbnailSize::Small)
		.ShowTypeInTileView(false)
		.AllowThumbnailHintLabel(false)
	];


	if (AssetViewPtr.IsValid() && !InArgs._AssetPickerConfig.bAutohideSearchBar)
	{
		TextFilter = MakeShareable(new FFrontendFilter_Text());
		bool bClassNamesProvided = (InArgs._AssetPickerConfig.Filter.ClassPaths.Num() != 1);
		TextFilter->SetIncludeClassName(bClassNamesProvided || AssetViewPtr->IsIncludingClassNames());
		TextFilter->SetIncludeAssetPath(AssetViewPtr->IsIncludingAssetPaths());
		TextFilter->SetIncludeCollectionNames(AssetViewPtr->IsIncludingCollectionNames());
	}

	AssetViewPtr->RequestSlowFullListRefresh();
}


void SInputAssetPicker::UpdateAssetSourceCollections(TArray<FCollectionNameType> Collections)
{
	CurrentSourcesData = FSourcesData(CurrentSourcesData.VirtualPaths, Collections);
	if (AssetViewPtr.IsValid())
	{
		AssetViewPtr->SetSourcesData(CurrentSourcesData);
	}
}

EActiveTimerReturnType SInputAssetPicker::SetFocusPostConstruct( double InCurrentTime, float InDeltaTime )
{
	if ( SearchBoxPtr.IsValid() )
	{
		FWidgetPath WidgetToFocusPath;
		FSlateApplication::Get().GeneratePathToWidgetUnchecked( SearchBoxPtr.ToSharedRef(), WidgetToFocusPath );
		FSlateApplication::Get().SetKeyboardFocus( WidgetToFocusPath, EFocusCause::SetDirectly );
		WidgetToFocusPath.GetWindow()->SetWidgetToFocusOnActivate(SearchBoxPtr);

		return EActiveTimerReturnType::Stop;
	}

	return EActiveTimerReturnType::Continue;
}

FReply SInputAssetPicker::HandleKeyDownFromSearchBox(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Up/Right and Down/Left move thru the filtered list
	if (InKeyEvent.GetKey() == EKeys::Up || InKeyEvent.GetKey() == EKeys::Left)
	{
		AssetViewPtr->AdjustActiveSelection(-1);
		return FReply::Handled();
	}
	else if (InKeyEvent.GetKey() == EKeys::Down || InKeyEvent.GetKey() == EKeys::Right)
	{
		AssetViewPtr->AdjustActiveSelection(+1);
	}

	return FReply::Unhandled();
}

FReply SInputAssetPicker::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		TArray<FContentBrowserItem> SelectionSet = AssetViewPtr->GetSelectedFileItems();
		if (SelectionSet.Num() == 1)
		{
			FAssetData ItemAssetData;
			SelectionSet[0].Legacy_TryGetAssetData(ItemAssetData);
			OnAssetSelected.ExecuteIfBound(ItemAssetData);
		}
		else
		{
			OnNoneButtonClicked();
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FText SInputAssetPicker::GetHighlightedText() const
{
	return TextFilter->GetRawFilterText();
}

void SInputAssetPicker::SetSearchBoxText(const FText& InSearchText)
{
	// Has anything changed? (need to test case as the operators are case-sensitive)
	if (!InSearchText.ToString().Equals(TextFilter->GetRawFilterText().ToString(), ESearchCase::CaseSensitive))
	{
		TextFilter->SetRawFilterText(InSearchText);
		if (InSearchText.IsEmpty())
		{
			FrontendFilters->Remove(TextFilter);
			AssetViewPtr->SetUserSearching(false);
		}
		else
		{
			FrontendFilters->Add(TextFilter);
			AssetViewPtr->SetUserSearching(true);
		}
	}
}

void SInputAssetPicker::OnSearchBoxChanged(const FText& InSearchText)
{
	SetSearchBoxText( InSearchText );
}

void SInputAssetPicker::OnSearchBoxCommitted(const FText& InSearchText, ETextCommit::Type CommitInfo)
{
	SetSearchBoxText( InSearchText );

	if (CommitInfo == ETextCommit::OnEnter)
	{
		TArray<FContentBrowserItem> SelectionSet = AssetViewPtr->GetSelectedFileItems();
		if ( SelectionSet.Num() == 0 )
		{
			AssetViewPtr->AdjustActiveSelection(1);
			SelectionSet = AssetViewPtr->GetSelectedFileItems();
		}
		HandleItemsActivated(SelectionSet, EAssetTypeActivationMethod::Opened);
	}
}

FReply SInputAssetPicker::OnNoneButtonClicked()
{
	OnAssetSelected.ExecuteIfBound(FAssetData());
	if (AssetViewPtr.IsValid())
	{
		AssetViewPtr->ClearSelection(true);
	}
	return FReply::Handled();
}

void SInputAssetPicker::HandleItemSelectionChanged(const FContentBrowserItem& InSelectedItem, ESelectInfo::Type InSelectInfo)
{
	if (InSelectInfo != ESelectInfo::Direct)
	{
		FAssetData ItemAssetData;
		InSelectedItem.Legacy_TryGetAssetData(ItemAssetData);
		OnAssetSelected.ExecuteIfBound(ItemAssetData);
	}
}

void SInputAssetPicker::HandleItemsActivated(TArrayView<const FContentBrowserItem> ActivatedItems, EAssetTypeActivationMethod::Type ActivationMethod)
{
	FContentBrowserItem FirstActivatedFolder;

	TArray<FAssetData> ActivatedAssets;
	for (const FContentBrowserItem& ActivatedItem : ActivatedItems)
	{
		if (ActivatedItem.IsFile())
		{
			FAssetData ItemAssetData;
			if (ActivatedItem.Legacy_TryGetAssetData(ItemAssetData))
			{
				ActivatedAssets.Add(MoveTemp(ItemAssetData));
			}
		}

		if (ActivatedItem.IsFolder() && !FirstActivatedFolder.IsValid())
		{
			FirstActivatedFolder = ActivatedItem;
		}
	}

	if (ActivatedAssets.Num() == 0)
	{
		return;
	}

	if (ActivationMethod == EAssetTypeActivationMethod::Opened)
	{
		OnAssetEnterPressed.ExecuteIfBound(ActivatedAssets);
	}

	OnAssetsActivated.ExecuteIfBound( ActivatedAssets, ActivationMethod );
}

void SInputAssetPicker::RefreshAssetView(bool bRefreshSources)
{
	if (bRefreshSources)
	{
		AssetViewPtr->RequestSlowFullListRefresh();
	}
	else
	{
		AssetViewPtr->RequestQuickFrontendListRefresh();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static int32 GenericThumbnailSizes[(int32)EThumbnailSize::MAX] = { 24, 32, 64, 128, 200 };

FText SInputAssetView::ThumbnailSizeToDisplayName(EThumbnailSize InSize)
{
	switch (InSize)
	{
	case EThumbnailSize::Tiny:
		return LOCTEXT("TinyThumbnailSize", "Tiny");
	case EThumbnailSize::Small:
		return LOCTEXT("SmallThumbnailSize", "Small");
	case EThumbnailSize::Medium:
		return LOCTEXT("MediumThumbnailSize", "Medium");
	case EThumbnailSize::Large:
		return LOCTEXT("LargeThumbnailSize", "Large");
	case EThumbnailSize::Huge:
		return LOCTEXT("HugeThumbnailSize", "Huge");
	default:
		return FText::GetEmpty();
	}
}

class FAssetViewFrontendFilterHelper
{
public:
	explicit FAssetViewFrontendFilterHelper(SInputAssetView* InAssetView)
		: AssetView(InAssetView)
		, ContentBrowserData(IContentBrowserDataModule::Get().GetSubsystem())
		, bDisplayEmptyFolders(AssetView->IsShowingEmptyFolders())
	{
	}

	bool DoesItemPassQueryFilter(const TSharedPtr<FContentBrowserItem>& InItemToFilter)
	{
		// Folders aren't subject to additional filtering
		if (InItemToFilter->IsFolder())
		{
			return true;
		}

		if (AssetView->OnShouldFilterItem.IsBound()
			&& !AssetView->OnShouldFilterItem.Execute(*InItemToFilter))
		{
			return true;
		}

		// If we have OnShouldFilterAsset then it is assumed that we really only want to see true assets and 
		// nothing else so only include things that have asset data and also pass the query filter
		FAssetData ItemAssetData;
		if (AssetView->OnShouldFilterAsset.IsBound()
			&& InItemToFilter->Legacy_TryGetAssetData(ItemAssetData))
		{
			if (!AssetView->OnShouldFilterAsset.Execute(ItemAssetData))
			{
				return true;
			}
		}

		return false;
	}

	bool DoesItemPassFrontendFilter(const TSharedPtr<FContentBrowserItem>& InItemToFilter)
	{
		// Folders are only subject to "empty" filtering
		if (InItemToFilter->IsFolder())
		{
			return false;// ContentBrowserData->IsFolderVisible(InItemToFilter->GetVirtualPath(), ContentBrowserUtils::GetIsFolderVisibleFlags(bDisplayEmptyFolders));
		}

		// Run the item through the filters
		if (!AssetView->IsFrontendFilterActive() || AssetView->PassesCurrentFrontendFilter(*InItemToFilter))
		{
			return true;
		}

		return false;
	}

private:
	SInputAssetView* AssetView = nullptr;
	UContentBrowserDataSubsystem* ContentBrowserData = nullptr;
	const bool bDisplayEmptyFolders = true;
};

SInputAssetView::~SInputAssetView()
{
	if (IContentBrowserDataModule* ContentBrowserDataModule = IContentBrowserDataModule::GetPtr())
	{
		if (UContentBrowserDataSubsystem* ContentBrowserData = ContentBrowserDataModule->GetSubsystem())
		{
			ContentBrowserData->OnItemDataUpdated().RemoveAll(this);
			ContentBrowserData->OnItemDataRefreshed().RemoveAll(this);
			ContentBrowserData->OnItemDataDiscoveryComplete().RemoveAll(this);
		}
	}

	// Remove the listener for when view settings are changed
	UContentBrowserSettings::OnSettingChanged().RemoveAll(this);

	if ( FrontendFilters.IsValid() )
	{
		// Clear the frontend filter changed delegate
		FrontendFilters->OnChanged().RemoveAll( this );
	}
}


void SInputAssetView::Construct( const FArguments& InArgs )
{
	InitialNumAmortizedTasks = 0;
	TotalAmortizeTime = 0;
	AmortizeStartTime = 0;
	MaxSecondsPerFrame = 0.015f;

	bFillEmptySpaceInTileView = InArgs._FillEmptySpaceInTileView;
	FillScale = 1.0f;

	ThumbnailHintFadeInSequence.JumpToStart();
	ThumbnailHintFadeInSequence.AddCurve(0, 0.5f, ECurveEaseFunction::Linear);

	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
	ContentBrowserData->OnItemDataUpdated().AddSP(this, &SInputAssetView::HandleItemDataUpdated);
	ContentBrowserData->OnItemDataRefreshed().AddSP(this, &SInputAssetView::RequestSlowFullListRefresh);
	ContentBrowserData->OnItemDataDiscoveryComplete().AddSP(this, &SInputAssetView::HandleItemDataDiscoveryComplete);
	FilterCacheID.Initialaze(ContentBrowserData);

	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
	{
		const TSharedRef<ICollectionContainer>& ProjectCollectionContainer = CollectionManagerModule.Get().GetProjectCollectionContainer();
		ProjectCollectionContainer->OnAssetsAddedToCollection().AddSP(this, &SInputAssetView::OnAssetsAddedToCollection);
		ProjectCollectionContainer->OnAssetsRemovedFromCollection().AddSP(this, &SInputAssetView::OnAssetsRemovedFromCollection);
		ProjectCollectionContainer->OnCollectionRenamed().AddSP(this, &SInputAssetView::OnCollectionRenamed);
		ProjectCollectionContainer->OnCollectionUpdated().AddSP(this, &SInputAssetView::OnCollectionUpdated);
	}

	// Listen for when view settings are changed
	UContentBrowserSettings::OnSettingChanged().AddSP(this, &SInputAssetView::HandleSettingChanged);

	ThumbnailSize = InArgs._InitialThumbnailSize;

	// Get desktop metrics
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetCachedDisplayMetrics( DisplayMetrics );

	const FIntPoint DisplaySize(
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Right - DisplayMetrics.PrimaryDisplayWorkAreaRect.Left,
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - DisplayMetrics.PrimaryDisplayWorkAreaRect.Top );

	ThumbnailScaleRangeScalar = (float)DisplaySize.Y / 2160.f;

	// Use the shared ThumbnailPool for the rendering of thumbnails
	AssetThumbnailPool = UThumbnailManager::Get().GetSharedThumbnailPool();
	NumOffscreenThumbnails = 64;
	ListViewThumbnailResolution = 128;
	ListViewThumbnailSize = 64;
	ListViewThumbnailPadding = 4;
	TileViewThumbnailResolution = 256;
	TileViewThumbnailSize = 150;
	TileViewThumbnailPadding = 9;

	TileViewNameHeight = 50;

	MinThumbnailScale = 0.2f * ThumbnailScaleRangeScalar;
	MaxThumbnailScale = 1.9f * ThumbnailScaleRangeScalar;

	bCanShowClasses = InArgs._CanShowClasses;

	bCanShowFolders = InArgs._CanShowFolders;
	
	bCanShowReadOnlyFolders = InArgs._CanShowReadOnlyFolders;

	bFilterRecursivelyWithBackendFilter = InArgs._FilterRecursivelyWithBackendFilter;
		
	bCanShowRealTimeThumbnails = InArgs._CanShowRealTimeThumbnails;

	bCanShowDevelopersFolder = InArgs._CanShowDevelopersFolder;

	bCanShowFavorites = InArgs._CanShowFavorites;
	bCanDockCollections = InArgs._CanDockCollections;

	SelectionMode = InArgs._SelectionMode;

	bShowPathInColumnView = InArgs._ShowPathInColumnView;
	bShowTypeInColumnView = InArgs._ShowTypeInColumnView;
	bSortByPathInColumnView = bShowPathInColumnView && InArgs._SortByPathInColumnView;
	bShowTypeInTileView = InArgs._ShowTypeInTileView;
	bForceShowEngineContent = InArgs._ForceShowEngineContent;
	bForceShowPluginContent = InArgs._ForceShowPluginContent;
	bForceHideScrollbar = InArgs._ForceHideScrollbar;
	bShowDisallowedAssetClassAsUnsupportedItems = InArgs._ShowDisallowedAssetClassAsUnsupportedItems;

	bPendingUpdateThumbnails = false;
	bShouldNotifyNextAssetSync = true;
	CurrentThumbnailSize = TileViewThumbnailSize;

	SourcesData = InArgs._InitialSourcesData;
	BackendFilter = InArgs._InitialBackendFilter;

	FrontendFilters = InArgs._FrontendFilters;
	if ( FrontendFilters.IsValid() )
	{
		FrontendFilters->OnChanged().AddSP( this, &SInputAssetView::OnFrontendFiltersChanged );
	}

	OnShouldFilterAsset = InArgs._OnShouldFilterAsset;
	OnShouldFilterItem = InArgs._OnShouldFilterItem;

	OnNewItemRequested = InArgs._OnNewItemRequested;
	OnItemSelectionChanged = InArgs._OnItemSelectionChanged;
	OnItemsActivated = InArgs._OnItemsActivated;
	OnGetItemContextMenu = InArgs._OnGetItemContextMenu;
	OnItemRenameCommitted = InArgs._OnItemRenameCommitted;
	OnAssetTagWantsToBeDisplayed = InArgs._OnAssetTagWantsToBeDisplayed;
	OnIsAssetValidForCustomToolTip = InArgs._OnIsAssetValidForCustomToolTip;
	OnGetCustomAssetToolTip = InArgs._OnGetCustomAssetToolTip;
	OnVisualizeAssetToolTip = InArgs._OnVisualizeAssetToolTip;
	OnAssetToolTipClosing = InArgs._OnAssetToolTipClosing;
	OnGetCustomSourceAssets = InArgs._OnGetCustomSourceAssets;
	HighlightedText = InArgs._HighlightedText;
	ThumbnailLabel = InArgs._ThumbnailLabel;
	AllowThumbnailHintLabel = InArgs._AllowThumbnailHintLabel;
	InitialCategoryFilter = InArgs._InitialCategoryFilter;
	AssetShowWarningText = InArgs._AssetShowWarningText;
	bAllowDragging = InArgs._AllowDragging;
	bAllowFocusOnSync = InArgs._AllowFocusOnSync;
	HiddenColumnNames = DefaultHiddenColumnNames = InArgs._HiddenColumnNames;
	CustomColumns = InArgs._CustomColumns;
	OnSearchOptionsChanged = InArgs._OnSearchOptionsChanged;
	bShowPathViewFilters = InArgs._bShowPathViewFilters;
	OnExtendAssetViewOptionsMenuContext = InArgs._OnExtendAssetViewOptionsMenuContext;
	OnGetBrowserItemText = InArgs._OnGetBrowserItemText;

	if ( InArgs._InitialViewType >= 0 && InArgs._InitialViewType < EAssetViewType::MAX )
	{
		CurrentViewType = InArgs._InitialViewType;
	}
	else
	{
		CurrentViewType = EAssetViewType::Tile;
	}

	bPendingSortFilteredItems = false;
	bQuickFrontendListRefreshRequested = false;
	bSlowFullListRefreshRequested = false;
	LastSortTime = 0;
	SortDelaySeconds = 8;

	bBulkSelecting = false;
	bAllowThumbnailEditMode = InArgs._AllowThumbnailEditMode;
	bThumbnailEditMode = false;
	bUserSearching = false;
	bPendingFocusOnSync = false;
	bWereItemsRecursivelyFiltered = false;

	OwningContentBrowser = InArgs._OwningContentBrowser;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetClassPermissionList = AssetToolsModule.Get().GetAssetClassPathPermissionList(EAssetClassAction::ViewAsset);
	FolderPermissionList = AssetToolsModule.Get().GetFolderPermissionList();
	WritableFolderPermissionList = AssetToolsModule.Get().GetWritableFolderPermissionList();

	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::LoadModuleChecked<FEditorWidgetsModule>("EditorWidgets");
	TSharedRef<SWidget> AssetDiscoveryIndicator = EditorWidgetsModule.CreateAssetDiscoveryIndicator(EAssetDiscoveryIndicatorScaleMode::Scale_Vertical);

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	ChildSlot
	.Padding(0.0f)
	[
		SNew(SBorder)
		.Padding(0.f)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		[
			VerticalBox
		]
	];

	// Assets area
	VerticalBox->AddSlot()
	.FillHeight(1.f)
	[
		SNew( SVerticalBox ) 

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SBox )
			.Visibility_Lambda([this] { return InitialNumAmortizedTasks > 0 ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			.HeightOverride( 2.f )
			[
				SNew( SProgressBar )
				.Percent( this, &SInputAssetView::GetIsWorkingProgressBarState )
				.BorderPadding( FVector2D(0,0) )
			]
		]
		
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(ViewContainer, SBox)
				.Padding(6.0f)
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.Padding(FMargin(0, 14, 0, 0))
			[
				// A warning to display when there are no assets to show
				SNew( STextBlock )
				.Justification( ETextJustify::Center )
				.Text( this, &SInputAssetView::GetAssetShowWarningText )
				.Visibility( this, &SInputAssetView::IsAssetShowWarningTextVisible )
				.AutoWrapText( true )
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(24, 0, 24, 0))
			[
				// Asset discovery indicator
				AssetDiscoveryIndicator
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(8, 0))
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ErrorReporting.EmptyBox"))
				.BorderBackgroundColor(this, &SInputAssetView::GetQuickJumpColor)
				.Visibility(this, &SInputAssetView::IsQuickJumpVisible)
				[
					SNew(STextBlock)
					.Text(this, &SInputAssetView::GetQuickJumpTerm)
				]
			]
		]
	];
	
	if (InArgs._ShowBottomToolbar)
	{
		// Bottom panel
		VerticalBox->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			// Asset count
			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(8, 5)
			[
				SNew(STextBlock)
				.Text(this, &SInputAssetView::GetAssetCountText)
			]
		];
	}

	CreateCurrentView();

	if( InArgs._InitialAssetSelection.IsValid() )
	{
		// sync to the initial item without notifying of selection
		bShouldNotifyNextAssetSync = false;
		SyncToLegacy( MakeArrayView(&InArgs._InitialAssetSelection, 1), TArrayView<const FString>() );
	}
}

TOptional< float > SInputAssetView::GetIsWorkingProgressBarState() const
{
	if (InitialNumAmortizedTasks > 0)
	{
		const int32 CompletedTasks = FMath::Max(0, InitialNumAmortizedTasks - ItemsPendingFrontendFilter.Num());
		return static_cast<float>(CompletedTasks) / static_cast<float>(InitialNumAmortizedTasks);
	}
	return 0.0f;
}

void SInputAssetView::SetSourcesData(const FSourcesData& InSourcesData)
{
	// Update the path and collection lists
	SourcesData = InSourcesData;
	RequestSlowFullListRefresh();
	ClearSelection();
}

const FSourcesData& SInputAssetView::GetSourcesData() const
{
	return SourcesData;
}

void SInputAssetView::SetBackendFilter(const FARFilter& InBackendFilter)
{
	// Update the path and collection lists
	BackendFilter = InBackendFilter;
	RequestSlowFullListRefresh();
}

void SInputAssetView::AppendBackendFilter(FARFilter& FilterToAppendTo) const
{
	FilterToAppendTo.Append(BackendFilter);
}

void SInputAssetView::SyncToItems(TArrayView<const FContentBrowserItem> ItemsToSync, const bool bFocusOnSync)
{
	PendingSyncItems.Reset();

	for (const FContentBrowserItem& Item : ItemsToSync)
	{
		PendingSyncItems.SelectedVirtualPaths.Add(Item.GetVirtualPath());
	}

	bPendingFocusOnSync = bFocusOnSync;
}

void SInputAssetView::SyncToVirtualPaths(TArrayView<const FName> VirtualPathsToSync, const bool bFocusOnSync)
{
	PendingSyncItems.Reset();
	for (const FName& VirtualPathToSync : VirtualPathsToSync)
	{
		PendingSyncItems.SelectedVirtualPaths.Add(VirtualPathToSync);
	}

	bPendingFocusOnSync = bFocusOnSync;
}

void SInputAssetView::SyncToLegacy(TArrayView<const FAssetData> AssetDataList, TArrayView<const FString> FolderList, const bool bFocusOnSync)
{
	PendingSyncItems.Reset();
	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
	auto AppendVirtualPath = [this](FName InPath)
	{
		this->PendingSyncItems.SelectedVirtualPaths.Add(InPath);
		return true;
	};
	PendingSyncItems.SelectedVirtualPaths.Reset();
	for (const FAssetData& Asset : AssetDataList)
	{
		ContentBrowserData->Legacy_TryConvertAssetDataToVirtualPaths(Asset, /* UseFolderPaths */ false, AppendVirtualPath);
	}

	bPendingFocusOnSync = bFocusOnSync;
}

void SInputAssetView::SyncToSelection( const bool bFocusOnSync )
{
	PendingSyncItems.Reset();

	TArray<TSharedPtr<FContentBrowserItem>> SelectedItems = GetSelectedViewItems();
	for (const TSharedPtr<FContentBrowserItem>& Item : SelectedItems)
	{
		if (Item.IsValid())
		{
			PendingSyncItems.SelectedVirtualPaths.Add(Item->GetVirtualPath());
		}
	}

	bPendingFocusOnSync = bFocusOnSync;
}

void SInputAssetView::ApplyHistoryData( const FHistoryData& History )
{
	FSourcesData LegacySources;
	LegacySources.VirtualPaths = History.ContentSources.GetVirtualPaths();
	LegacySources.Collections.Reserve(History.ContentSources.GetCollections().Num());
	for (const FCollectionRef& CollectionRef : History.ContentSources.GetCollections())
	{
		LegacySources.Collections.Emplace(CollectionRef.Name, CollectionRef.Type);
	}
	LegacySources.bIncludeVirtualPaths = History.ContentSources.IsIncludingVirtualPaths();
	SetSourcesData(LegacySources);
	PendingSyncItems = History.SelectionData;
	bPendingFocusOnSync = true;
}

TArray<TSharedPtr<FContentBrowserItem>> SInputAssetView::GetSelectedViewItems() const
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::Tile: return TileView->GetSelectedItems();
		default:
		ensure(0); // Unknown list type
		return TArray<TSharedPtr<FContentBrowserItem>>();
	}
}

TArray<FContentBrowserItem> SInputAssetView::GetSelectedItems() const
{
	TArray<TSharedPtr<FContentBrowserItem>> SelectedViewItems = GetSelectedViewItems();

	TArray<FContentBrowserItem> SelectedItems;
	for (const TSharedPtr<FContentBrowserItem>& SelectedViewItem : SelectedViewItems)
	{
		if (!SelectedViewItem->IsTemporary())
		{
			SelectedItems.Emplace(*SelectedViewItem);
		}
	}
	return SelectedItems;
}

TArray<FContentBrowserItem> SInputAssetView::GetSelectedFolderItems() const
{
	TArray<TSharedPtr<FContentBrowserItem>> SelectedViewItems = GetSelectedViewItems();

	TArray<FContentBrowserItem> SelectedFolders;
	for (const TSharedPtr<FContentBrowserItem>& SelectedViewItem : SelectedViewItems)
	{
		if (SelectedViewItem->IsFolder() && !SelectedViewItem->IsTemporary())
		{
			SelectedFolders.Emplace(*SelectedViewItem);
		}
	}
	return SelectedFolders;
}

TArray<FContentBrowserItem> SInputAssetView::GetSelectedFileItems() const
{
	TArray<TSharedPtr<FContentBrowserItem>> SelectedViewItems = GetSelectedViewItems();

	TArray<FContentBrowserItem> SelectedFiles;
	for (const TSharedPtr<FContentBrowserItem>& SelectedViewItem : SelectedViewItems)
	{
		if (SelectedViewItem->IsFile() && !SelectedViewItem->IsTemporary())
		{
			SelectedFiles.Emplace(*SelectedViewItem);
		}
	}
	return SelectedFiles;
}

TArray<FAssetData> SInputAssetView::GetSelectedAssets() const
{
	TArray<TSharedPtr<FContentBrowserItem>> SelectedViewItems = GetSelectedViewItems();

	// TODO: Abstract away?
	TArray<FAssetData> SelectedAssets;
	for (const TSharedPtr<FContentBrowserItem>& SelectedViewItem : SelectedViewItems)
	{
		// Only report non-temporary & non-folder items
		FAssetData ItemAssetData;
		if (!SelectedViewItem->IsTemporary() && SelectedViewItem->IsFile() && SelectedViewItem->Legacy_TryGetAssetData(ItemAssetData))
		{
			SelectedAssets.Add(MoveTemp(ItemAssetData));
		}
	}
	return SelectedAssets;
}

TArray<FString> SInputAssetView::GetSelectedFolders() const
{
	TArray<TSharedPtr<FContentBrowserItem>> SelectedViewItems = GetSelectedViewItems();

	// TODO: Abstract away?
	TArray<FString> SelectedFolders;
	for (const TSharedPtr<FContentBrowserItem>& SelectedViewItem : SelectedViewItems)
	{
		if (SelectedViewItem->IsFolder())
		{
			SelectedFolders.Emplace(SelectedViewItem->GetVirtualPath().ToString());
		}
	}
	return SelectedFolders;
}

void SInputAssetView::RequestSlowFullListRefresh()
{
	bSlowFullListRefreshRequested = true;
}

void SInputAssetView::RequestQuickFrontendListRefresh()
{
	bQuickFrontendListRefreshRequested = true;
}

FString SInputAssetView::GetThumbnailScaleSettingPath(const FString& SettingsString) const
{
	return SettingsString + TEXT(".ThumbnailSize");
}

FString SInputAssetView::GetCurrentViewTypeSettingPath(const FString& SettingsString) const
{
	return SettingsString + TEXT(".CurrentViewType");
}

// Adjusts the selected asset by the selection delta, which should be +1 or -1)
void SInputAssetView::AdjustActiveSelection(int32 SelectionDelta)
{
	// Find the index of the first selected item
	TArray<TSharedPtr<FContentBrowserItem>> SelectionSet = GetSelectedViewItems();
	
	int32 SelectedSuggestion = INDEX_NONE;

	if (SelectionSet.Num() > 0)
	{
		if (!FilteredAssetItems.Find(SelectionSet[0], /*out*/ SelectedSuggestion))
		{
			// Should never happen
			ensureMsgf(false, TEXT("SAssetView has a selected item that wasn't in the filtered list"));
			return;
		}
	}
	else
	{
		SelectedSuggestion = 0;
		SelectionDelta = 0;
	}

	if (FilteredAssetItems.Num() > 0)
	{
		// Move up or down one, wrapping around
		SelectedSuggestion = (SelectedSuggestion + SelectionDelta + FilteredAssetItems.Num()) % FilteredAssetItems.Num();

		// Pick the new asset
		const TSharedPtr<FContentBrowserItem>& NewSelection = FilteredAssetItems[SelectedSuggestion];

		RequestScrollIntoView(NewSelection);
		SetSelection(NewSelection);
	}
	else
	{
		ClearSelection();
	}
}

void SInputAssetView::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Adjust min and max thumbnail scale based on dpi
	MinThumbnailScale = (0.2f * ThumbnailScaleRangeScalar)/AllottedGeometry.Scale;
	MaxThumbnailScale = (1.9f * ThumbnailScaleRangeScalar)/AllottedGeometry.Scale;

	CalculateFillScale( AllottedGeometry );

	CurrentTime = InCurrentTime;

	if (FSlateApplication::Get().GetActiveModalWindow().IsValid())
	{
		// If we're in a model window then we need to tick the thumbnail pool in order for thumbnails to render correctly.
		AssetThumbnailPool->Tick(InDeltaTime);
	}

	CalculateThumbnailHintColorAndOpacity();

	if (bPendingUpdateThumbnails)
	{
		UpdateThumbnails();
		bPendingUpdateThumbnails = false;
	}

	if (bSlowFullListRefreshRequested)
	{
		RefreshSourceItems();
		bSlowFullListRefreshRequested = false;
		bQuickFrontendListRefreshRequested = true;
	}

	bool bForceViewUpdate = false;
	if (bQuickFrontendListRefreshRequested)
	{
		ResetQuickJump();

		RefreshFilteredItems();

		bQuickFrontendListRefreshRequested = false;
		bForceViewUpdate = true; // If HasItemsPendingFilter is empty we still need to update the view
	}

	if (HasItemsPendingFilter() || bForceViewUpdate)
	{
		bForceViewUpdate = false;

		const double TickStartTime = FPlatformTime::Seconds();
		const bool bWasWorking = InitialNumAmortizedTasks > 0;

		// Mark the first amortize time
		if (AmortizeStartTime == 0)
		{
			AmortizeStartTime = FPlatformTime::Seconds();
			InitialNumAmortizedTasks = ItemsPendingFrontendFilter.Num();
		}

		int32 PreviousFilteredAssetItems = FilteredAssetItems.Num();
		ProcessItemsPendingFilter(TickStartTime);

		if (HasItemsPendingFilter())
		{
			if (bPendingSortFilteredItems && InCurrentTime > LastSortTime + SortDelaySeconds)
			{
				// Don't sync to selection if we are just going to do it below
				SortList(!PendingSyncItems.Num());
			}

			// Need to finish processing queried items before rest of functdion is safe
			return;
		}
		else
		{
			if (bPendingSortFilteredItems && (bWasWorking || (InCurrentTime > LastSortTime + SortDelaySeconds)))
			{
				// Don't sync to selection if we are just going to do it below
				SortList(!PendingSyncItems.Num());
			}

			double AmortizeDuration = FPlatformTime::Seconds() - AmortizeStartTime;
			TotalAmortizeTime += AmortizeDuration;
			AmortizeStartTime = 0;
			InitialNumAmortizedTasks = 0;
		}
	}

	if ( PendingSyncItems.Num() > 0 )
	{
		if (bPendingSortFilteredItems)
		{
			// Don't sync to selection because we are just going to do it below
			SortList(/*bSyncToSelection=*/false);
		}
		
		bBulkSelecting = true;
		ClearSelection();
		bool bFoundScrollIntoViewTarget = false;

		for ( auto ItemIt = FilteredAssetItems.CreateConstIterator(); ItemIt; ++ItemIt )
		{
			const auto& Item = *ItemIt;
			if(Item.IsValid())
			{
				if (PendingSyncItems.SelectedVirtualPaths.Contains(Item->GetVirtualPath()))
				{
					SetItemSelection(Item, true, ESelectInfo::OnNavigation);
					
					// Scroll the first item in the list that can be shown into view
					if ( !bFoundScrollIntoViewTarget )
					{
						RequestScrollIntoView(Item);
						bFoundScrollIntoViewTarget = true;
					}
				}
			}
		}
	
		bBulkSelecting = false;

		if (bShouldNotifyNextAssetSync && !bUserSearching)
		{
			AssetSelectionChanged(TSharedPtr<FContentBrowserItem>(), ESelectInfo::Direct);
		}

		// Default to always notifying
		bShouldNotifyNextAssetSync = true;

		PendingSyncItems.Reset();

		if (bAllowFocusOnSync && bPendingFocusOnSync)
		{
			FocusList();
		}
	}

	if ( IsHovered() )
	{
		// This prevents us from sorting the view immediately after the cursor leaves it
		LastSortTime = CurrentTime;
	}
	else if ( bPendingSortFilteredItems && InCurrentTime > LastSortTime + SortDelaySeconds )
	{
		SortList();
	}

	// Do quick-jump last as the Tick function might have canceled it
	if(QuickJumpData.bHasChangedSinceLastTick)
	{
		QuickJumpData.bHasChangedSinceLastTick = false;

		const bool bWasJumping = QuickJumpData.bIsJumping;
		QuickJumpData.bIsJumping = true;

		QuickJumpData.LastJumpTime = InCurrentTime;
		QuickJumpData.bHasValidMatch = PerformQuickJump(bWasJumping);
	}
	else if(QuickJumpData.bIsJumping && InCurrentTime > QuickJumpData.LastJumpTime + /* JumpDelaySeconds */ 2.0)
	{
		ResetQuickJump();
	}

	TSharedPtr<FContentBrowserItem> AssetAwaitingRename = AwaitingRename.Pin();
	if (AssetAwaitingRename.IsValid())
	{
		TSharedPtr<SWindow> OwnerWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		if (!OwnerWindow.IsValid())
		{
			AwaitingRename = nullptr;
		}
		else if (OwnerWindow->HasAnyUserFocusOrFocusedDescendants())
		{
			AwaitingRename = nullptr;
		}
	}
}

void SInputAssetView::SortList(bool bSyncToSelection)
{
	FilteredAssetItems.Sort([](TSharedPtr<FContentBrowserItem> LHS, TSharedPtr<FContentBrowserItem> RHS) {
		int32 Result = (LHS && RHS) ? UE::ComparisonUtility::CompareWithNumericSuffix(
				LHS->GetPrimaryInternalItem()->GetDisplayName().ToString(), 
				RHS->GetPrimaryInternalItem()->GetDisplayName().ToString()) : 0;
		return Result <= 0;
	});

	// Update the thumbnails we were using since the order has changed
	bPendingUpdateThumbnails = true;

	if ( bSyncToSelection )
	{
		// Make sure the selection is in view
		const bool bFocusOnSync = false;
		SyncToSelection(bFocusOnSync);
	}

	RefreshList();
	bPendingSortFilteredItems = false;
	LastSortTime = CurrentTime;
}

void SInputAssetView::CalculateFillScale( const FGeometry& AllottedGeometry )
{
	if ( bFillEmptySpaceInTileView && CurrentViewType == EAssetViewType::Tile )
	{
		float ItemWidth = GetTileViewItemBaseWidth();

		// Scrollbars are 16, but we add 1 to deal with half pixels.
		const float ScrollbarWidth = 16 + 1;
		float TotalWidth = AllottedGeometry.GetLocalSize().X -(ScrollbarWidth);
		float Coverage = TotalWidth / ItemWidth;
		int32 Items = (int)( TotalWidth / ItemWidth );

		// If there isn't enough room to support even a single item, don't apply a fill scale.
		if ( Items > 0 )
		{
			float GapSpace = ItemWidth * ( Coverage - (float)Items );
			float ExpandAmount = GapSpace / (float)Items;
			FillScale = ( ItemWidth + ExpandAmount ) / ItemWidth;
			FillScale = FMath::Max( 1.0f, FillScale );
		}
		else
		{
			FillScale = 1.0f;
		}
	}
	else
	{
		FillScale = 1.0f;
	}
}

void SInputAssetView::CalculateThumbnailHintColorAndOpacity()
{
	if ( HighlightedText.Get().IsEmpty() )
	{
		if ( ThumbnailHintFadeInSequence.IsPlaying() )
		{
			if ( ThumbnailHintFadeInSequence.IsForward() )
			{
				ThumbnailHintFadeInSequence.Reverse();
			}
		}
		else if ( ThumbnailHintFadeInSequence.IsAtEnd() ) 
		{
			ThumbnailHintFadeInSequence.PlayReverse(this->AsShared());
		}
	}
	else 
	{
		if ( ThumbnailHintFadeInSequence.IsPlaying() )
		{
			if ( ThumbnailHintFadeInSequence.IsInReverse() )
			{
				ThumbnailHintFadeInSequence.Reverse();
			}
		}
		else if ( ThumbnailHintFadeInSequence.IsAtStart() ) 
		{
			ThumbnailHintFadeInSequence.Play(this->AsShared());
		}
	}

	const float Opacity = ThumbnailHintFadeInSequence.GetLerp();
	ThumbnailHintColorAndOpacity = FLinearColor( 1.0, 1.0, 1.0, Opacity );
}

bool SInputAssetView::HasItemsPendingFilter() const
{
	return (ItemsPendingPriorityFilter.Num() + ItemsPendingFrontendFilter.Num()) > 0;
}

void SInputAssetView::ProcessItemsPendingFilter(const double TickStartTime)
{
	const double ProcessItemsPendingFilterStartTime = FPlatformTime::Seconds();

	FAssetViewFrontendFilterHelper FrontendFilterHelper(this);

	auto UpdateFilteredAssetItemTypeCounts = [this](const TSharedPtr<FContentBrowserItem>& InItem)
	{
		if (CurrentViewType == EAssetViewType::Column)
		{
			const FContentBrowserItemDataAttributeValue TypeNameValue = InItem->GetItemAttribute(ContentBrowserItemAttributes::ItemTypeName);
			if (TypeNameValue.IsValid())
			{
				FilteredAssetItemTypeCounts.FindOrAdd(TypeNameValue.GetValue<FName>())++;
			}
		}
	};

	const bool bRunQueryFilter = OnShouldFilterAsset.IsBound() || OnShouldFilterItem.IsBound();
	const bool bFlushAllPendingItems = TickStartTime < 0;

	bool bRefreshList = false;
	bool bHasTimeRemaining = true;

	auto FilterItem = [this, bRunQueryFilter, &bRefreshList, &FrontendFilterHelper, &UpdateFilteredAssetItemTypeCounts](const TSharedPtr<FContentBrowserItem>& ItemToFilter)
	{
		// Run the query filter if required
		if (bRunQueryFilter)
		{
			const bool bPassedBackendFilter = FrontendFilterHelper.DoesItemPassQueryFilter(ItemToFilter);
			if (!bPassedBackendFilter)
			{
				AvailableBackendItems.Remove(FContentBrowserItemKey(*ItemToFilter));
				return;
			}
		}

		// Run the frontend filter
		{
			const bool bPassedFrontendFilter = FrontendFilterHelper.DoesItemPassFrontendFilter(ItemToFilter);
			if (bPassedFrontendFilter)
			{
				bRefreshList = true;
				FilteredAssetItems.Add(ItemToFilter);
				UpdateFilteredAssetItemTypeCounts(ItemToFilter);
			}
		}
	};

	// Run the prioritized set first
	// This data must be processed this frame, so skip the amortization time checks within the loop itself
	if (ItemsPendingPriorityFilter.Num() > 0)
	{
		for (const TSharedPtr<FContentBrowserItem>& ItemToFilter : ItemsPendingPriorityFilter)
		{
			// Make sure this item isn't pending in another list
			{
				const uint32 ItemToFilterHash = GetTypeHash(ItemToFilter);
				ItemsPendingFrontendFilter.RemoveByHash(ItemToFilterHash, ItemToFilter);
			}

			// Apply any filters and update the view
			FilterItem(ItemToFilter);
		}
		ItemsPendingPriorityFilter.Reset();

		// Check to see if we have run out of time in this tick
		if (!bFlushAllPendingItems && (FPlatformTime::Seconds() - TickStartTime) > MaxSecondsPerFrame)
		{
			bHasTimeRemaining = false;
		}
	}

	// Filter as many items as possible until we run out of time
	if (bHasTimeRemaining && ItemsPendingFrontendFilter.Num() > 0)
	{
		for (auto ItemIter = ItemsPendingFrontendFilter.CreateIterator(); ItemIter; ++ItemIter)
		{
			const TSharedPtr<FContentBrowserItem> ItemToFilter = *ItemIter;
			ItemIter.RemoveCurrent();

			// Apply any filters and update the view
			FilterItem(ItemToFilter);

			// Check to see if we have run out of time in this tick
			if (!bFlushAllPendingItems && (FPlatformTime::Seconds() - TickStartTime) > MaxSecondsPerFrame)
			{
				bHasTimeRemaining = false;
				break;
			}
		}
	}

	if (bRefreshList)
	{
		bPendingSortFilteredItems = true;
		RefreshList();
	}
}

FReply SInputAssetView::OnKeyChar( const FGeometry& MyGeometry,const FCharacterEvent& InCharacterEvent )
{
	const bool bIsControlOrCommandDown = InCharacterEvent.IsControlDown() || InCharacterEvent.IsCommandDown();
	
	const bool bTestOnly = false;
	if(HandleQuickJumpKeyDown(InCharacterEvent.GetCharacter(), bIsControlOrCommandDown, InCharacterEvent.IsAltDown(), bTestOnly).IsEventHandled())
	{
		return FReply::Handled();
	}

	// If the user pressed a key we couldn't handle, reset the quick-jump search
	ResetQuickJump();

	return FReply::Unhandled();
}

static bool IsValidObjectPath(const FString& Path, FString& OutObjectClassName, FString& OutObjectPath, FString& OutPackageName)
{
	if (FPackageName::ParseExportTextPath(Path, &OutObjectClassName, &OutObjectPath))
	{
		if (UClass* ObjectClass = UClass::TryFindTypeSlow<UClass>(OutObjectClassName, EFindFirstObjectOptions::ExactClass))
		{
			OutPackageName = FPackageName::ObjectPathToPackageName(OutObjectPath);
			if (FPackageName::IsValidLongPackageName(OutPackageName))
			{
				return true;
			}
		}
	}

	return false;
}

FReply SInputAssetView::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	const bool bIsControlOrCommandDown = InKeyEvent.IsControlDown() || InKeyEvent.IsCommandDown();
	
	// Swallow the key-presses used by the quick-jump in OnKeyChar to avoid other things (such as the viewport commands) getting them instead
	// eg) Pressing "W" without this would set the viewport to "translate" mode
	if (HandleQuickJumpKeyDown((TCHAR)InKeyEvent.GetCharacter(), bIsControlOrCommandDown, InKeyEvent.IsAltDown(), /*bTestOnly*/true).IsEventHandled())
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SInputAssetView::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Make sure to not change the thumbnail scaling when we're in Columns view since thumbnail scaling isn't applicable there.
	if( MouseEvent.IsControlDown() && IsThumbnailScalingAllowed() )
	{
		// Step up/down a level depending on the scroll wheel direction.
		// Clamp value to enum min/max before updating.
		const int32 Delta = MouseEvent.GetWheelDelta() > 0 ? 1 : -1;
		const EThumbnailSize DesiredThumbnailSize = (EThumbnailSize)FMath::Clamp<int32>((int32)ThumbnailSize + Delta, 0, (int32)EThumbnailSize::MAX - 1);
		if ( DesiredThumbnailSize != ThumbnailSize )
		{
			OnThumbnailSizeChanged(DesiredThumbnailSize);
		}		
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SInputAssetView::OnFocusChanging( const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	ResetQuickJump();
}

TSharedRef<SInputAssetView::STileViewType> SInputAssetView::CreateTileView()
{
	return SNew(STileViewType)
		.SelectionMode( SelectionMode )
		.ListItemsSource(&FilteredAssetItems)
		.OnGenerateTile(this, &SInputAssetView::MakeTileViewWidget)
		.OnItemScrolledIntoView(this, &SInputAssetView::ItemScrolledIntoView)
		.OnContextMenuOpening(this, &SInputAssetView::OnGetContextMenuContent)
		.OnMouseButtonDoubleClick(this, &SInputAssetView::OnListMouseButtonDoubleClick)
		.OnSelectionChanged(this, &SInputAssetView::AssetSelectionChanged)
		.ItemHeight(this, &SInputAssetView::GetTileViewItemHeight)
		.ItemWidth(this, &SInputAssetView::GetTileViewItemWidth)
		.ScrollbarVisibility(bForceHideScrollbar ? EVisibility::Collapsed : EVisibility::Visible);
}

bool SInputAssetView::IsValidSearchToken(const FString& Token) const
{
	if ( Token.Len() == 0 )
	{
		return false;
	}

	// A token may not be only apostrophe only, or it will match every asset because the text filter compares against the pattern Class'ObjectPath'
	if ( Token.Len() == 1 && Token[0] == '\'' )
	{
		return false;
	}

	return true;
}

static void AppendAssetFilterToContentBrowserFilter(const FARFilter& InAssetFilter, const TSharedPtr<FPathPermissionList>& InAssetClassPermissionList, const TSharedPtr<FPathPermissionList>& InFolderPermissionList, FContentBrowserDataFilter& OutDataFilter)
{
	if (InAssetFilter.SoftObjectPaths.Num() > 0 || InAssetFilter.TagsAndValues.Num() > 0 || InAssetFilter.bIncludeOnlyOnDiskAssets)
	{
		FContentBrowserDataObjectFilter& ObjectFilter = OutDataFilter.ExtraFilters.FindOrAddFilter<FContentBrowserDataObjectFilter>();
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		// TODO: Modify this API to also use FSoftObjectPath with deprecation
		ObjectFilter.ObjectNamesToInclude = UE::SoftObjectPath::Private::ConvertSoftObjectPaths(InAssetFilter.SoftObjectPaths);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		ObjectFilter.TagsAndValuesToInclude = InAssetFilter.TagsAndValues;
		ObjectFilter.bOnDiskObjectsOnly = InAssetFilter.bIncludeOnlyOnDiskAssets;
	}

	if (InAssetFilter.PackageNames.Num() > 0 || InAssetFilter.PackagePaths.Num() > 0 || (InFolderPermissionList && InFolderPermissionList->HasFiltering()))
	{
		FContentBrowserDataPackageFilter& PackageFilter = OutDataFilter.ExtraFilters.FindOrAddFilter<FContentBrowserDataPackageFilter>();
		PackageFilter.PackageNamesToInclude = InAssetFilter.PackageNames;
		PackageFilter.PackagePathsToInclude = InAssetFilter.PackagePaths;
		PackageFilter.bRecursivePackagePathsToInclude = InAssetFilter.bRecursivePaths;
		PackageFilter.PathPermissionList = InFolderPermissionList;
	}

	if (InAssetFilter.ClassPaths.Num() > 0 || (InAssetClassPermissionList && InAssetClassPermissionList->HasFiltering()))
	{
		FContentBrowserDataClassFilter& ClassFilter = OutDataFilter.ExtraFilters.FindOrAddFilter<FContentBrowserDataClassFilter>();
		for (FTopLevelAssetPath ClassPathName : InAssetFilter.ClassPaths)
		{
			// ContentBrowserData -> AssetRegistry conversion expects full top-level class path strings here (eg "/Script/Engine.Blueprint").
			ClassFilter.ClassNamesToInclude.Add(ClassPathName.ToString());
		}
		ClassFilter.bRecursiveClassNamesToInclude = InAssetFilter.bRecursiveClasses;
		if (InAssetFilter.bRecursiveClasses)
		{
			for (FTopLevelAssetPath ClassPathName : InAssetFilter.RecursiveClassPathsExclusionSet)
			{
				ClassFilter.ClassNamesToExclude.Add(ClassPathName.ToString());
			}
			ClassFilter.bRecursiveClassNamesToExclude = false;
		}
		ClassFilter.ClassPermissionList = InAssetClassPermissionList;
	}
}

static TSharedPtr<FPathPermissionList> GetCombinedFolderPermissionList(const TSharedPtr<FPathPermissionList>& FolderPermissionList, const TSharedPtr<FPathPermissionList>& WritableFolderPermissionList)
{
	TSharedPtr<FPathPermissionList> CombinedFolderPermissionList;

	const bool bHidingFolders = FolderPermissionList && FolderPermissionList->HasFiltering();
	const bool bHidingReadOnlyFolders = WritableFolderPermissionList && WritableFolderPermissionList->HasFiltering();
	if (bHidingFolders || bHidingReadOnlyFolders)
	{
		CombinedFolderPermissionList = MakeShared<FPathPermissionList>();

		if (bHidingReadOnlyFolders && bHidingFolders)
		{
			FPathPermissionList IntersectedFilter = FolderPermissionList->CombinePathFilters(*WritableFolderPermissionList.Get());
			CombinedFolderPermissionList->Append(IntersectedFilter);
		}
		else if (bHidingReadOnlyFolders)
		{
			CombinedFolderPermissionList->Append(*WritableFolderPermissionList);
		}
		else if (bHidingFolders)
		{
			CombinedFolderPermissionList->Append(*FolderPermissionList);
		}
	}

	return CombinedFolderPermissionList;
}

FContentBrowserDataFilter SInputAssetView::CreateBackendDataFilter(bool bInvalidateCache) const
{
	// Assemble the filter using the current sources
	// Force recursion when the user is searching
	const bool bHasCollections = SourcesData.HasCollections();
	const bool bRecurse = ShouldFilterRecursively();
	const bool bUsingFolders = IsShowingFolders() && !bRecurse;

	// Check whether any legacy delegates are bound (the Content Browser doesn't use these, only pickers do)
	// These limit the view to things that might use FAssetData
	const bool bHasLegacyDelegateBindings 
		=  OnIsAssetValidForCustomToolTip.IsBound()
		|| OnGetCustomAssetToolTip.IsBound()
		|| OnVisualizeAssetToolTip.IsBound()
		|| OnAssetToolTipClosing.IsBound()
		|| OnShouldFilterAsset.IsBound();

	FContentBrowserDataFilter DataFilter;
	DataFilter.bRecursivePaths = bRecurse || !bUsingFolders || bHasCollections;

	DataFilter.ItemTypeFilter = EContentBrowserItemTypeFilter::IncludeFiles
		| ((bUsingFolders && !bHasCollections) ? EContentBrowserItemTypeFilter::IncludeFolders : EContentBrowserItemTypeFilter::IncludeNone);

	DataFilter.ItemCategoryFilter = bHasLegacyDelegateBindings ? EContentBrowserItemCategoryFilter::IncludeAssets : InitialCategoryFilter;
	if (IsShowingCppContent())
	{
		DataFilter.ItemCategoryFilter |= EContentBrowserItemCategoryFilter::IncludeClasses;
	}
	else
	{
		DataFilter.ItemCategoryFilter &= ~EContentBrowserItemCategoryFilter::IncludeClasses;
	}
	DataFilter.ItemCategoryFilter |= EContentBrowserItemCategoryFilter::IncludeCollections;

	DataFilter.ItemAttributeFilter = EContentBrowserItemAttributeFilter::IncludeProject
		| (IsShowingEngineContent() ? EContentBrowserItemAttributeFilter::IncludeEngine : EContentBrowserItemAttributeFilter::IncludeNone)
		| (IsShowingPluginContent() ? EContentBrowserItemAttributeFilter::IncludePlugins : EContentBrowserItemAttributeFilter::IncludeNone)
		| (IsShowingDevelopersContent() ? EContentBrowserItemAttributeFilter::IncludeDeveloper : EContentBrowserItemAttributeFilter::IncludeNone)
		| (IsShowingLocalizedContent() ? EContentBrowserItemAttributeFilter::IncludeLocalized : EContentBrowserItemAttributeFilter::IncludeNone);

	TSharedPtr<FPathPermissionList> CombinedFolderPermissionList = GetCombinedFolderPermissionList(FolderPermissionList, IsShowingReadOnlyFolders() ? nullptr : WritableFolderPermissionList);

	if (bShowDisallowedAssetClassAsUnsupportedItems && AssetClassPermissionList && AssetClassPermissionList->HasFiltering())
	{
		// The unsupported item will created as an unsupported asset item instead of normal asset item for the writable folders
		FContentBrowserDataUnsupportedClassFilter& UnsupportedClassFilter = DataFilter.ExtraFilters.FindOrAddFilter<FContentBrowserDataUnsupportedClassFilter>();
		UnsupportedClassFilter.ClassPermissionList = AssetClassPermissionList;
		UnsupportedClassFilter.FolderPermissionList = WritableFolderPermissionList;
	}

	AppendAssetFilterToContentBrowserFilter(BackendFilter, AssetClassPermissionList, CombinedFolderPermissionList, DataFilter);

	if (bHasCollections && !SourcesData.IsDynamicCollection())
	{
		FContentBrowserDataCollectionFilter& CollectionFilter = DataFilter.ExtraFilters.FindOrAddFilter<FContentBrowserDataCollectionFilter>();
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
			const TSharedPtr<ICollectionContainer> ProjectCollectionContainer = CollectionManagerModule.Get().GetProjectCollectionContainer();
			CollectionFilter.Collections.Reserve(SourcesData.Collections.Num());
			for (const FCollectionNameType& Collection : SourcesData.Collections)
			{
				CollectionFilter.Collections.Emplace(ProjectCollectionContainer, Collection);
			}
		}
		CollectionFilter.bIncludeChildCollections = !bUsingFolders;
	}

	if (OnGetCustomSourceAssets.IsBound())
	{
		FContentBrowserDataLegacyFilter& LegacyFilter = DataFilter.ExtraFilters.FindOrAddFilter<FContentBrowserDataLegacyFilter>();
		LegacyFilter.OnGetCustomSourceAssets = OnGetCustomSourceAssets;
	}

	DataFilter.CacheID = FilterCacheID;

	if (bInvalidateCache)
	{
		if (SourcesData.IsIncludingVirtualPaths())
		{
			static const FName RootPath = "/";
			const TArrayView<const FName> DataSourcePaths = SourcesData.HasVirtualPaths() ? MakeArrayView(SourcesData.VirtualPaths) : MakeArrayView(&RootPath, 1);
			FilterCacheID.RemoveUnusedCachedData(DataSourcePaths, DataFilter);
		}
		else
		{
			// Not sure what is the right thing to do here so clear the cache
			FilterCacheID.ClearCachedData();
		}
	}

	return DataFilter;
}

void SInputAssetView::RefreshSourceItems()
{
	TRACE_CPUPROFILER_EVENT_SCOPE("SInputAssetView::RefreshSourceItems");
	const double RefreshSourceItemsStartTime = FPlatformTime::Seconds();
	
	FilteredAssetItems.Reset();
	FilteredAssetItemTypeCounts.Reset();
	VisibleItems.Reset();
	RelevantThumbnails.Reset();

	TMap<FContentBrowserItemKey, TSharedPtr<FContentBrowserItem>> PreviousAvailableBackendItems = MoveTemp(AvailableBackendItems);
	AvailableBackendItems.Reset();
	ItemsPendingPriorityFilter.Reset();
	ItemsPendingFrontendFilter.Reset();
	{
		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

		auto AddNewItem = [this, &PreviousAvailableBackendItems](FContentBrowserItemData&& InItemData)
		{
			const FContentBrowserItemKey ItemDataKey(InItemData);
			const uint32 ItemDataKeyHash = GetTypeHash(ItemDataKey);

			TSharedPtr<FContentBrowserItem>& NewItem = AvailableBackendItems.FindOrAddByHash(ItemDataKeyHash, ItemDataKey);
			if (!NewItem && InItemData.IsFile())
			{
				// Re-use the old view item where possible to avoid list churn when our backend view already included the item
				if (TSharedPtr<FContentBrowserItem>* PreviousItem = PreviousAvailableBackendItems.FindByHash(ItemDataKeyHash, ItemDataKey))
				{
					NewItem = *PreviousItem;
				}
			}
			if (NewItem)
			{
				NewItem->Append(MoveTemp(InItemData));
			}
			else
			{
				NewItem = MakeShared<FContentBrowserItem>(MoveTemp(InItemData));
			}

			return true;
		};

		if (SourcesData.OnEnumerateCustomSourceItemDatas.IsBound())
		{
			SourcesData.OnEnumerateCustomSourceItemDatas.Execute(AddNewItem);
		}

		FContentBrowserDataFilter DataFilter; // Must live long enough to provide telemetry
		if (SourcesData.IsIncludingVirtualPaths() || SourcesData.HasCollections()) 
		{
			const bool bInvalidateFilterCache = true;
			DataFilter = CreateBackendDataFilter(bInvalidateFilterCache);

			bWereItemsRecursivelyFiltered = DataFilter.bRecursivePaths;

			if (SourcesData.HasCollections() && EnumHasAnyFlags(DataFilter.ItemCategoryFilter, EContentBrowserItemCategoryFilter::IncludeCollections))
			{
				// If we are showing collections then we may need to add dummy folder items for the child collections
				// Note: We don't check the IncludeFolders flag here, as that is forced to false when collections are selected,
				// instead we check the state of bIncludeChildCollections which will be false when we want to show collection folders
				const FContentBrowserDataCollectionFilter* CollectionFilter = DataFilter.ExtraFilters.FindFilter<FContentBrowserDataCollectionFilter>();
				if (CollectionFilter && !CollectionFilter->bIncludeChildCollections)
				{
					FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
				
					TArray<FCollectionNameType> ChildCollections;
					for(const FCollectionNameType& Collection : SourcesData.Collections)
					{
						ChildCollections.Reset();
						{
							const TSharedRef<ICollectionContainer>& ProjectCollectionContainer = CollectionManagerModule.Get().GetProjectCollectionContainer();
							ProjectCollectionContainer->GetChildCollections(Collection.Name, Collection.Type, ChildCollections);
						}

						for (const FCollectionNameType& ChildCollection : ChildCollections)
						{
							// Use "Collections" as the root of the path to avoid this being confused with other view folders - see ContentBrowserUtils::IsCollectionPath
							FContentBrowserItemData FolderItemData(
								nullptr, 
								EContentBrowserItemFlags::Type_Folder | EContentBrowserItemFlags::Category_Collection, 
								*FString::Printf(TEXT("/Collections/%s/%s"), ECollectionShareType::ToString(ChildCollection.Type), *ChildCollection.Name.ToString()), 
								ChildCollection.Name, 
								FText::FromName(ChildCollection.Name), 
								nullptr,
								NAME_None
								);

							const FContentBrowserItemKey FolderItemDataKey(FolderItemData);
							AvailableBackendItems.Add(FolderItemDataKey, MakeShared<FContentBrowserItem>(MoveTemp(FolderItemData)));
						}
					}
				}
			}

			if (SourcesData.IsIncludingVirtualPaths())
			{
				static const FName RootPath = "/";
				const TArrayView<const FName> DataSourcePaths = SourcesData.HasVirtualPaths() ? MakeArrayView(SourcesData.VirtualPaths) : MakeArrayView(&RootPath, 1);
				for (const FName& DataSourcePath : DataSourcePaths)
				{
					// Ensure paths do not contain trailing slash
					ensure(DataSourcePath == RootPath || !FStringView(FNameBuilder(DataSourcePath)).EndsWith(TEXT('/')));
					ContentBrowserData->EnumerateItemsUnderPath(DataSourcePath, DataFilter, AddNewItem);
				}
			}
		}
	}
}

bool SInputAssetView::IsFilteringRecursively() const
{
	if (!bFilterRecursivelyWithBackendFilter)
	{
		return false;
	}

	return true;
}

bool SInputAssetView::IsToggleFilteringRecursivelyAllowed() const
{
	return bFilterRecursivelyWithBackendFilter;
}

bool SInputAssetView::ShouldFilterRecursively() const
{
	// Quick check for conditions which force recursive filtering
	if (bUserSearching)
	{
		return true;
	}

	if (IsFilteringRecursively() && !BackendFilter.IsEmpty() )
	{
		return true;
	}

	// Otherwise, check if there are any non-inverse frontend filters selected
	if (FrontendFilters.IsValid())
	{
		for (int32 FilterIndex = 0; FilterIndex < FrontendFilters->Num(); ++FilterIndex)
		{
			const auto* Filter = static_cast<FFrontendFilter*>(FrontendFilters->GetFilterAtIndex(FilterIndex).Get());
			if (Filter)
			{
				if (!Filter->IsInverseFilter())
				{
					return true;
				}
			}
		}
	}

	// No sources - view will show everything
	if (SourcesData.IsEmpty())
	{
		return true;
	}

	// No filters, do not override folder view with recursive filtering
	return false;
}

void SInputAssetView::RefreshFilteredItems()
{
	const double RefreshFilteredItemsStartTime = FPlatformTime::Seconds();
	
	ItemsPendingFrontendFilter.Reset();
	FilteredAssetItems.Reset();
	FilteredAssetItemTypeCounts.Reset();
	RelevantThumbnails.Reset();

	AmortizeStartTime = 0;
	InitialNumAmortizedTasks = 0;

	LastSortTime = 0;
	bPendingSortFilteredItems = true;

	ItemsPendingFrontendFilter.Reserve(AvailableBackendItems.Num());
	for (const auto& AvailableBackendItemPair : AvailableBackendItems)
	{
		ItemsPendingFrontendFilter.Add(AvailableBackendItemPair.Value);
	}

	// Let the frontend filters know the currently used asset filter in case it is necessary to conditionally filter based on path or class filters
	if (IsFrontendFilterActive() && FrontendFilters.IsValid())
	{
		static const FName RootPath = "/";
		const TArrayView<const FName> DataSourcePaths = SourcesData.HasVirtualPaths() ? MakeArrayView(SourcesData.VirtualPaths) : MakeArrayView(&RootPath, 1);

		const bool bInvalidateFilterCache = false;
		const FContentBrowserDataFilter DataFilter = CreateBackendDataFilter(bInvalidateFilterCache);

		for (int32 FilterIdx = 0; FilterIdx < FrontendFilters->Num(); ++FilterIdx)
		{
			// There are only FFrontendFilters in this collection
			const TSharedPtr<FFrontendFilter>& Filter = StaticCastSharedPtr<FFrontendFilter>(FrontendFilters->GetFilterAtIndex(FilterIdx));
			if (Filter.IsValid())
			{
				Filter->SetCurrentFilter(DataSourcePaths, DataFilter);
			}
		}
	}
}

bool SInputAssetView::IsShowingAllFolder() const
{
	return false;
}

bool SInputAssetView::IsOrganizingFolders() const
{
	return false;
}

void SInputAssetView::OnAssetsAddedToCollection(ICollectionContainer& CollectionContainer, const FCollectionNameType& Collection, TConstArrayView<FSoftObjectPath> ObjectPaths)
{
	if ( !SourcesData.Collections.Contains( Collection ) )
	{
		return;
	}

	RequestSlowFullListRefresh();
}

void SInputAssetView::OnAssetsRemovedFromCollection(ICollectionContainer& CollectionContainer, const FCollectionNameType& Collection, TConstArrayView<FSoftObjectPath> ObjectPaths)
{
	if ( !SourcesData.Collections.Contains( Collection ) )
	{
		return;
	}

	RequestSlowFullListRefresh();
}

void SInputAssetView::OnCollectionRenamed(ICollectionContainer& CollectionContainer, const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection)
{
	int32 FoundIndex = INDEX_NONE;
	if ( SourcesData.Collections.Find( OriginalCollection, FoundIndex ) )
	{
		SourcesData.Collections[ FoundIndex ] = NewCollection;
	}
}

void SInputAssetView::OnCollectionUpdated(ICollectionContainer& CollectionContainer, const FCollectionNameType& Collection)
{
	// A collection has changed in some way, so we need to refresh our backend list
	RequestSlowFullListRefresh();
}

void SInputAssetView::OnFrontendFiltersChanged()
{
	RequestQuickFrontendListRefresh();

	// If we're not operating on recursively filtered data, we need to ensure a full slow
	// refresh is performed.
	if ( ShouldFilterRecursively() && !bWereItemsRecursivelyFiltered )
	{
		RequestSlowFullListRefresh();
	}
}

bool SInputAssetView::IsFrontendFilterActive() const
{
	return ( FrontendFilters.IsValid() && FrontendFilters->Num() > 0 );
}

bool SInputAssetView::PassesCurrentFrontendFilter(const FContentBrowserItem& Item) const
{
	return !FrontendFilters.IsValid() || FrontendFilters->PassesAllFilters(Item);
}

FLinearColor SInputAssetView::GetThumbnailHintColorAndOpacity() const
{
	//We update this color in tick instead of here as an optimization
	return ThumbnailHintColorAndOpacity;
}

bool SInputAssetView::IsToggleShowFoldersAllowed() const
{
	return false;
}

bool SInputAssetView::IsShowingFolders() const
{
	return false;
}

bool SInputAssetView::IsShowingReadOnlyFolders() const
{
	return bCanShowReadOnlyFolders;
}

void SInputAssetView::ToggleShowEmptyFolders()
{
	// Empty
}

bool SInputAssetView::IsToggleShowEmptyFoldersAllowed() const
{
	return false;
}

bool SInputAssetView::IsShowingEmptyFolders() const
{
	return false;
}

bool SInputAssetView::CanShowRealTimeThumbnails() const
{
	return false;
}

bool SInputAssetView::IsShowingRealTimeThumbnails() const
{
	return false;
}

bool SInputAssetView::IsShowingPluginContent() const
{
	return true;
}

bool SInputAssetView::IsShowingEngineContent() const
{
	return true;
}

bool SInputAssetView::IsToggleShowDevelopersContentAllowed() const
{
	return false;
}

bool SInputAssetView::IsToggleShowEngineContentAllowed() const
{
	return !bForceShowEngineContent;
}

bool SInputAssetView::IsToggleShowPluginContentAllowed() const
{
	return !bForceShowPluginContent;
}

bool SInputAssetView::IsShowingDevelopersContent() const
{
	return false;
}

bool SInputAssetView::IsToggleShowLocalizedContentAllowed() const
{
	return true;
}

bool SInputAssetView::IsShowingLocalizedContent() const
{
	return false;
}

bool SInputAssetView::IsToggleShowFavoritesAllowed() const
{
	return false;
}

bool SInputAssetView::IsShowingFavorites() const
{
	return false;
}

bool SInputAssetView::IsToggleDockCollectionsAllowed() const
{
	return false;
}

bool SInputAssetView::HasDockedCollections() const
{
	return false;
}

bool SInputAssetView::IsToggleShowCppContentAllowed() const
{
	return bCanShowClasses;
}

bool SInputAssetView::IsShowingCppContent() const
{
	return false;
}

bool SInputAssetView::IsToggleIncludeClassNamesAllowed() const
{
	return true;
}

bool SInputAssetView::IsIncludingClassNames() const
{
	return true;
}

bool SInputAssetView::IsToggleIncludeAssetPathsAllowed() const
{
	return true;
}

bool SInputAssetView::IsIncludingAssetPaths() const
{
	return true;
}


bool SInputAssetView::IsToggleIncludeCollectionNamesAllowed() const
{
	return true;
}

bool SInputAssetView::IsIncludingCollectionNames() const
{
	return false;
}

void SInputAssetView::SetCurrentViewType(EAssetViewType::Type NewType)
{
	if ( ensure(NewType != EAssetViewType::MAX) && NewType != CurrentViewType )
	{
		ResetQuickJump();

		CurrentViewType = NewType;
		CreateCurrentView();

		SyncToSelection();

		// Clear relevant thumbnails to render fresh ones in the new view if needed
		RelevantThumbnails.Reset();
		VisibleItems.Reset();

		if ( NewType == EAssetViewType::Tile )
		{
			CurrentThumbnailSize = TileViewThumbnailSize;
			bPendingUpdateThumbnails = true;
		}
		else if ( NewType == EAssetViewType::List )
		{
			CurrentThumbnailSize = ListViewThumbnailSize;
			bPendingUpdateThumbnails = true;
		}
	}
}

void SInputAssetView::SetCurrentThumbnailSize(EThumbnailSize NewThumbnailSize)
{
	if (ThumbnailSize != NewThumbnailSize)
	{
		OnThumbnailSizeChanged(NewThumbnailSize);
	}
}

void SInputAssetView::SetCurrentViewTypeFromMenu(EAssetViewType::Type NewType)
{
	if (NewType != CurrentViewType)
	{
		SetCurrentViewType(NewType);
	}
}

void SInputAssetView::CreateCurrentView()
{
	TileView.Reset();

	TSharedRef<SWidget> NewView = SNullWidget::NullWidget;
	switch (CurrentViewType)
	{
		case EAssetViewType::Tile:
			TileView = CreateTileView();
			NewView = CreateShadowOverlay(TileView.ToSharedRef());
			break;
		case EAssetViewType::List:
			check(false);
			break;
		case EAssetViewType::Column:
			check(false)
			break;
	}
	
	ViewContainer->SetContent( NewView );
}

TSharedRef<SWidget> SInputAssetView::CreateShadowOverlay( TSharedRef<STableViewBase> Table )
{
	if (bForceHideScrollbar)
	{
		return Table;
	}

	return SNew(SScrollBorder, Table)
		[
			Table
		];
}

EAssetViewType::Type SInputAssetView::GetCurrentViewType() const
{
	return CurrentViewType;
}

bool SInputAssetView::IsCurrentViewType(EAssetViewType::Type ViewType) const
{
	return GetCurrentViewType() == ViewType;
}

void SInputAssetView::FocusList() const
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::Tile: FSlateApplication::Get().SetKeyboardFocus(TileView, EFocusCause::SetDirectly); break;
	}
}

void SInputAssetView::RefreshList()
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::Tile: TileView->RequestListRefresh(); break;
	}
}

void SInputAssetView::SetSelection(const TSharedPtr<FContentBrowserItem>& Item)
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::Tile: TileView->SetSelection(Item); break;
	}
}

void SInputAssetView::SetItemSelection(const TSharedPtr<FContentBrowserItem>& Item, bool bSelected, const ESelectInfo::Type SelectInfo)
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::Tile: TileView->SetItemSelection(Item, bSelected, SelectInfo); break;
	}
}

void SInputAssetView::RequestScrollIntoView(const TSharedPtr<FContentBrowserItem>& Item)
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::Tile: TileView->RequestScrollIntoView(Item); break;
	}
}

void SInputAssetView::OnOpenAssetsOrFolders()
{
	if (OnItemsActivated.IsBound())
	{
		const TArray<FContentBrowserItem> SelectedItems = GetSelectedItems();
		OnItemsActivated.Execute(SelectedItems, EAssetTypeActivationMethod::Opened);
	}
}

void SInputAssetView::OnPreviewAssets()
{
	if (OnItemsActivated.IsBound())
	{
		const TArray<FContentBrowserItem> SelectedItems = GetSelectedItems();
		OnItemsActivated.Execute(SelectedItems, EAssetTypeActivationMethod::Previewed);
	}
}

void SInputAssetView::ClearSelection(bool bForceSilent)
{
	const bool bTempBulkSelectingValue = bForceSilent ? true : bBulkSelecting;
	TGuardValue<bool> Guard(bBulkSelecting, bTempBulkSelectingValue);
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::Tile: TileView->ClearSelection(); break;
	}
}

TSharedRef<ITableRow> SInputAssetView::MakeTileViewWidget(TSharedPtr<FContentBrowserItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if ( !ensure(AssetItem.IsValid()) )
	{
		return SNew( STableRow<TSharedPtr<FContentBrowserItem>>, OwnerTable );
	}

	VisibleItems.Add(AssetItem);
	bPendingUpdateThumbnails = true;

	TWeakPtr<FContentBrowserItem> WeakItem = AssetItem;
	{
		TSharedPtr<FAssetThumbnail>& AssetThumbnail = RelevantThumbnails.FindOrAdd(AssetItem);
		if (!AssetThumbnail)
		{
			AssetThumbnail = MakeShared<FAssetThumbnail>(FAssetData(), TileViewThumbnailResolution, TileViewThumbnailResolution, AssetThumbnailPool);
			AssetItem->UpdateThumbnail(*AssetThumbnail);
			AssetThumbnail->GetViewportRenderTargetTexture(); // Access the texture once to trigger it to render
		}

		TSharedPtr< STableRow<TSharedPtr<FContentBrowserItem>> > TableRowWidget;
		SAssignNew( TableRowWidget, STableRow<TSharedPtr<FContentBrowserItem>>, OwnerTable )
		.Style(FAppStyle::Get(), "ContentBrowser.AssetListView.TileTableRow")
		.Cursor( EMouseCursor::Default );

		TSharedRef<SInputAssetTileItem> Item =
			SNew(SInputAssetTileItem)
			.AssetThumbnail(AssetThumbnail)
			.AssetItem(AssetItem)
			.AssetOverrideLabelText(TAttribute<FText>::CreateLambda([WeakItem, this](){
				if (auto StrongItem = WeakItem.Pin())
				{
					FAssetData ItemAssetData;
					StrongItem->Legacy_TryGetAssetData(ItemAssetData);
					return this->OnGetBrowserItemText.IsBound() ? this->OnGetBrowserItemText.Execute(ItemAssetData) : StrongItem->GetDisplayName();
				}
				return FText::FromString(TEXT("(No Name)"));
			}))
			.ThumbnailPadding((float)TileViewThumbnailPadding)
			.CurrentThumbnailSize(this, &SInputAssetView::GetThumbnailSize)
			.ItemWidth(this, &SInputAssetView::GetTileViewItemWidth)
			.OnItemDestroyed(this, &SInputAssetView::AssetItemWidgetDestroyed)
			.ShouldAllowToolTip(this, &SInputAssetView::ShouldAllowToolTips)
			.HighlightText( HighlightedText )
			.ThumbnailEditMode(this, &SInputAssetView::IsThumbnailEditMode)
			.ThumbnailLabel( ThumbnailLabel )
			.ThumbnailHintColorAndOpacity( this, &SInputAssetView::GetThumbnailHintColorAndOpacity )
			.AllowThumbnailHintLabel( AllowThumbnailHintLabel )
			.IsSelected(FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FContentBrowserItem>>::IsSelected))
			.IsSelectedExclusively( FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FContentBrowserItem>>::IsSelectedExclusively) )
			.OnIsAssetValidForCustomToolTip(OnIsAssetValidForCustomToolTip)
			.OnGetCustomAssetToolTip(OnGetCustomAssetToolTip)
			.OnVisualizeAssetToolTip( OnVisualizeAssetToolTip )
			.OnAssetToolTipClosing( OnAssetToolTipClosing )
			.ShowType(bShowTypeInTileView);

		TableRowWidget->SetContent(Item);

		return TableRowWidget.ToSharedRef();
	}
}

void SInputAssetView::AssetItemWidgetDestroyed(const TSharedPtr<FContentBrowserItem>& Item)
{
	if ( VisibleItems.Remove(Item) != INDEX_NONE )
	{
		bPendingUpdateThumbnails = true;
	}
}

void SInputAssetView::UpdateThumbnails()
{
	int32 MinItemIdx = INDEX_NONE;
	int32 MaxItemIdx = INDEX_NONE;
	int32 MinVisibleItemIdx = INDEX_NONE;
	int32 MaxVisibleItemIdx = INDEX_NONE;

	const int32 HalfNumOffscreenThumbnails = NumOffscreenThumbnails / 2;
	for ( auto ItemIt = VisibleItems.CreateConstIterator(); ItemIt; ++ItemIt )
	{
		int32 ItemIdx = FilteredAssetItems.Find(*ItemIt);
		if ( ItemIdx != INDEX_NONE )
		{
			const int32 ItemIdxLow = FMath::Max<int32>(0, ItemIdx - HalfNumOffscreenThumbnails);
			const int32 ItemIdxHigh = FMath::Min<int32>(FilteredAssetItems.Num() - 1, ItemIdx + HalfNumOffscreenThumbnails);
			if ( MinItemIdx == INDEX_NONE || ItemIdxLow < MinItemIdx )
			{
				MinItemIdx = ItemIdxLow;
			}
			if ( MaxItemIdx == INDEX_NONE || ItemIdxHigh > MaxItemIdx )
			{
				MaxItemIdx = ItemIdxHigh;
			}
			if ( MinVisibleItemIdx == INDEX_NONE || ItemIdx < MinVisibleItemIdx )
			{
				MinVisibleItemIdx = ItemIdx;
			}
			if ( MaxVisibleItemIdx == INDEX_NONE || ItemIdx > MaxVisibleItemIdx )
			{
				MaxVisibleItemIdx = ItemIdx;
			}
		}
	}

	if ( MinItemIdx != INDEX_NONE && MaxItemIdx != INDEX_NONE && MinVisibleItemIdx != INDEX_NONE && MaxVisibleItemIdx != INDEX_NONE )
	{
		// We have a new min and a new max, compare it to the old min and max so we can create new thumbnails
		// when appropriate and remove old thumbnails that are far away from the view area.
		TMap< TSharedPtr<FContentBrowserItem>, TSharedPtr<FAssetThumbnail> > NewRelevantThumbnails;

		// Operate on offscreen items that are furthest away from the visible items first since the thumbnail pool processes render requests in a LIFO order.
		while (MinItemIdx < MinVisibleItemIdx || MaxItemIdx > MaxVisibleItemIdx)
		{
			const int32 LowEndDistance = MinVisibleItemIdx - MinItemIdx;
			const int32 HighEndDistance = MaxItemIdx - MaxVisibleItemIdx;

			if ( HighEndDistance > LowEndDistance )
			{
				if(FilteredAssetItems.IsValidIndex(MaxItemIdx) && FilteredAssetItems[MaxItemIdx]->IsFile())
				{
					AddItemToNewThumbnailRelevancyMap(FilteredAssetItems[MaxItemIdx], NewRelevantThumbnails);
				}
				MaxItemIdx--;
			}
			else
			{
				if(FilteredAssetItems.IsValidIndex(MinItemIdx) && FilteredAssetItems[MinItemIdx]->IsFile())
				{
					AddItemToNewThumbnailRelevancyMap(FilteredAssetItems[MinItemIdx], NewRelevantThumbnails);
				}
				MinItemIdx++;
			}
		}

		// Now operate on VISIBLE items then prioritize them so they are rendered first
		TArray< TSharedPtr<FAssetThumbnail> > ThumbnailsToPrioritize;
		for ( int32 ItemIdx = MinVisibleItemIdx; ItemIdx <= MaxVisibleItemIdx; ++ItemIdx )
		{
			if(FilteredAssetItems.IsValidIndex(ItemIdx) && FilteredAssetItems[ItemIdx]->IsFile())
			{
				TSharedPtr<FAssetThumbnail> Thumbnail = AddItemToNewThumbnailRelevancyMap( FilteredAssetItems[ItemIdx], NewRelevantThumbnails );
				if ( Thumbnail.IsValid() )
				{
					ThumbnailsToPrioritize.Add(Thumbnail);
				}
			}
		}

		// Now prioritize all thumbnails there were in the visible range
		if ( ThumbnailsToPrioritize.Num() > 0 )
		{
			AssetThumbnailPool->PrioritizeThumbnails(ThumbnailsToPrioritize, CurrentThumbnailSize, CurrentThumbnailSize);
		}

		// Assign the new map of relevant thumbnails. This will remove any entries that were no longer relevant.
		RelevantThumbnails = NewRelevantThumbnails;
	}
}

TSharedPtr<FAssetThumbnail> SInputAssetView::AddItemToNewThumbnailRelevancyMap(const TSharedPtr<FContentBrowserItem>& Item, TMap< TSharedPtr<FContentBrowserItem>, TSharedPtr<FAssetThumbnail> >& NewRelevantThumbnails)
{
	checkf(Item->IsFile(), TEXT("Only files can have thumbnails!"));

	TSharedPtr<FAssetThumbnail> Thumbnail = RelevantThumbnails.FindRef(Item);
	if (!Thumbnail)
	{
		if (!ensure(CurrentThumbnailSize > 0 && CurrentThumbnailSize <=/* MAX_THUMBNAIL_SIZE */ 4096))
		{
			// Thumbnail size must be in a sane range
			CurrentThumbnailSize = 64;
		}

		// The thumbnail newly relevant, create a new thumbnail
		const int32 ThumbnailResolution = FMath::TruncToInt((float)CurrentThumbnailSize * MaxThumbnailScale);
		Thumbnail = MakeShared<FAssetThumbnail>(FAssetData(), ThumbnailResolution, ThumbnailResolution, AssetThumbnailPool);
		Item->UpdateThumbnail(*Thumbnail);
		Thumbnail->GetViewportRenderTargetTexture(); // Access the texture once to trigger it to render
	}

	if (Thumbnail)
	{
		NewRelevantThumbnails.Add(Item, Thumbnail);
	}

	return Thumbnail;
}

void SInputAssetView::AssetSelectionChanged( TSharedPtr< FContentBrowserItem > AssetItem, ESelectInfo::Type SelectInfo )
{
	if (!bBulkSelecting)
	{
		if (AssetItem)
		{
			OnItemSelectionChanged.ExecuteIfBound(*AssetItem, SelectInfo);
		}
		else
		{
			OnItemSelectionChanged.ExecuteIfBound(FContentBrowserItem(), SelectInfo);
		}
	}
}

void SInputAssetView::ItemScrolledIntoView(TSharedPtr<FContentBrowserItem> AssetItem, const TSharedPtr<ITableRow>& Widget )
{
	// Empty
}

TSharedPtr<SWidget> SInputAssetView::OnGetContextMenuContent()
{
	if (CanOpenContextMenu())
	{
		const TArray<FContentBrowserItem> SelectedItems = GetSelectedItems();
		return OnGetItemContextMenu.Execute(SelectedItems);
	}

	return nullptr;
}

bool SInputAssetView::CanOpenContextMenu() const
{
	if (!OnGetItemContextMenu.IsBound())
	{
		// You can only a summon a context menu if one is set up
		return false;
	}

	if (IsThumbnailEditMode())
	{
		// You can not summon a context menu for assets when in thumbnail edit mode because right clicking may happen inadvertently while adjusting thumbnails.
		return false;
	}

	TArray<TSharedPtr<FContentBrowserItem>> SelectedItems = GetSelectedViewItems();

	// Detect if at least one temporary item was selected. If there is only a temporary item selected, then deny the context menu.
	int32 NumTemporaryItemsSelected = 0;
	int32 NumCollectionFoldersSelected = 0;
	for (const TSharedPtr<FContentBrowserItem>& Item : SelectedItems)
	{
		if (Item->IsTemporary())
		{
			++NumTemporaryItemsSelected;
		}

		if (Item->IsFolder() && EnumHasAnyFlags(Item->GetItemCategory(), EContentBrowserItemFlags::Category_Collection))
		{
			++NumCollectionFoldersSelected;
		}
	}

	// If there are only a temporary items selected, deny the context menu
	if (SelectedItems.Num() > 0 && SelectedItems.Num() == NumTemporaryItemsSelected)
	{
		return false;
	}

	// If there are any collection folders selected, deny the context menu
	if (NumCollectionFoldersSelected > 0)
	{
		return false;
	}

	return true;
}

void SInputAssetView::OnListMouseButtonDoubleClick(TSharedPtr<FContentBrowserItem> AssetItem)
{
	if ( !ensure(AssetItem.IsValid()) )
	{
		return;
	}

	if ( IsThumbnailEditMode() )
	{
		// You can not activate assets when in thumbnail edit mode because double clicking may happen inadvertently while adjusting thumbnails.
		return;
	}

	if ( AssetItem->IsTemporary() )
	{
		// You may not activate temporary items, they are just for display.
		return;
	}

	if (OnItemsActivated.IsBound())
	{
		OnItemsActivated.Execute(MakeArrayView(&*AssetItem, 1), EAssetTypeActivationMethod::DoubleClicked);
	}
}

bool SInputAssetView::ShouldAllowToolTips() const
{
	bool bIsRightClickScrolling = false;
	switch( CurrentViewType )
	{
		case EAssetViewType::Tile:
			bIsRightClickScrolling = TileView->IsRightClickScrolling();
			break;

		default:
			bIsRightClickScrolling = false;
			break;
	}

	return !bIsRightClickScrolling && !IsThumbnailEditMode();
}

bool SInputAssetView::IsThumbnailEditMode() const
{
	return IsThumbnailEditModeAllowed() && bThumbnailEditMode;
}

bool SInputAssetView::IsThumbnailEditModeAllowed() const
{
	return bAllowThumbnailEditMode && GetCurrentViewType() != EAssetViewType::Column;
}

FReply SInputAssetView::EndThumbnailEditModeClicked()
{
	bThumbnailEditMode = false;

	return FReply::Handled();
}

FText SInputAssetView::GetAssetCountText() const
{
	const int32 NumAssets = FilteredAssetItems.Num();
	const int32 NumSelectedAssets = GetSelectedViewItems().Num();

	FText AssetCount = FText::GetEmpty();
	if ( NumSelectedAssets == 0 )
	{
		if ( NumAssets == 1 )
		{
			AssetCount = LOCTEXT("AssetCountLabelSingular", "1 item");
		}
		else
		{
			AssetCount = FText::Format( LOCTEXT("AssetCountLabelPlural", "{0} items"), FText::AsNumber(NumAssets) );
		}
	}
	else
	{
		if ( NumAssets == 1 )
		{
			AssetCount = FText::Format( LOCTEXT("AssetCountLabelSingularPlusSelection", "1 item ({0} selected)"), FText::AsNumber(NumSelectedAssets) );
		}
		else
		{
			AssetCount = FText::Format( LOCTEXT("AssetCountLabelPluralPlusSelection", "{0} items ({1} selected)"), FText::AsNumber(NumAssets), FText::AsNumber(NumSelectedAssets) );
		}
	}

	return AssetCount;
}

EVisibility SInputAssetView::GetEditModeLabelVisibility() const
{
	return IsThumbnailEditMode() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SInputAssetView::GetListViewVisibility() const
{
	return EVisibility::Collapsed;
}

EVisibility SInputAssetView::GetTileViewVisibility() const
{
	return GetCurrentViewType() == EAssetViewType::Tile ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SInputAssetView::GetColumnViewVisibility() const
{
	return EVisibility::Collapsed;
}

void SInputAssetView::ToggleThumbnailEditMode()
{
	bThumbnailEditMode = !bThumbnailEditMode;
}

void SInputAssetView::OnThumbnailSizeChanged(EThumbnailSize NewThumbnailSize)
{
	ThumbnailSize = NewThumbnailSize;
	RefreshList();
}

bool SInputAssetView::IsThumbnailSizeChecked(EThumbnailSize InThumbnailSize) const
{
	return ThumbnailSize == InThumbnailSize;
}

float SInputAssetView::GetThumbnailScale() const
{
	float BaseScale;
	switch (ThumbnailSize)
	{
	case EThumbnailSize::Tiny:
		BaseScale = 0.1f;
		break;
	case EThumbnailSize::Small:
		BaseScale = 0.25f;
		break;
	case EThumbnailSize::Medium:
		BaseScale = 0.5f;
		break;
	case EThumbnailSize::Large:
		BaseScale = 0.75f;
		break;
	case EThumbnailSize::Huge:
		BaseScale = 1.0f;
		break;
	default:
		BaseScale = 0.5f;
		break;
	}

	return BaseScale * GetTickSpaceGeometry().Scale;
}

bool SInputAssetView::IsThumbnailScalingAllowed() const
{
	return GetCurrentViewType() != EAssetViewType::Column;
}

float SInputAssetView::GetTileViewTypeNameHeight() const
{
	float TypeNameHeight = 0;

	if (bShowTypeInTileView)
	{
		TypeNameHeight = 50;
	}
	else
	{
		if (ThumbnailSize == EThumbnailSize::Small)
		{
			TypeNameHeight = 25;
		}
		else if (ThumbnailSize == EThumbnailSize::Medium)
		{
			TypeNameHeight = -5;
		}
		else if (ThumbnailSize > EThumbnailSize::Medium)
		{
			TypeNameHeight = -25;
		}
		else
		{
			TypeNameHeight = -40;
		}
	}
	return TypeNameHeight;
}

float SInputAssetView::GetListViewItemHeight() const
{
	return (float)(ListViewThumbnailSize + ListViewThumbnailPadding * 2) * FMath::Lerp(MinThumbnailScale, MaxThumbnailScale, GetThumbnailScale());
}

float SInputAssetView::GetTileViewItemHeight() const
{
	return SInputAssetTileItem::AssetNameHeights[(int32)ThumbnailSize] 
			+ GetTileViewItemBaseHeight() * FillScale + 4.0f;
}

float SInputAssetView::GetTileViewItemBaseHeight() const
{
	return (float)(TileViewThumbnailSize + TileViewThumbnailPadding * 2) * FMath::Lerp(MinThumbnailScale, MaxThumbnailScale, GetThumbnailScale());
}

float SInputAssetView::GetTileViewItemWidth() const
{
	return GetTileViewItemBaseWidth() * FillScale;
}

float SInputAssetView::GetTileViewItemBaseWidth() const //-V524
{
	return (float)( TileViewThumbnailSize + TileViewThumbnailPadding * 2 ) * FMath::Lerp( MinThumbnailScale, MaxThumbnailScale, GetThumbnailScale() );
}

EVisibility SInputAssetView::IsAssetShowWarningTextVisible() const
{
	return (FilteredAssetItems.Num() > 0 || bQuickFrontendListRefreshRequested) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

FText SInputAssetView::GetAssetShowWarningText() const
{
	if (AssetShowWarningText.IsSet())
	{
		return AssetShowWarningText.Get();
	}
	
	if (InitialNumAmortizedTasks > 0)
	{
		return LOCTEXT("ApplyingFilter", "Applying filter...");
	}

	FText NothingToShowText, DropText;
	if (ShouldFilterRecursively())
	{
		NothingToShowText = LOCTEXT( "NothingToShowCheckFilter", "No results, check your filter." );
	}

	if ( SourcesData.HasCollections() && !SourcesData.IsDynamicCollection() )
	{
		if (SourcesData.Collections[0].Name.IsNone())
		{
			DropText = LOCTEXT("NoCollectionSelected", "No collection selected.");
		}
		else
		{
			DropText = LOCTEXT("DragAssetsHere", "Drag and drop assets here to add them to the collection.");
		}
	}
	else if ( OnGetItemContextMenu.IsBound() )
	{
		DropText = LOCTEXT( "DropFilesOrRightClick", "Drop files here or right click to create content." );
	}
	
	return NothingToShowText.IsEmpty() ? DropText : FText::Format(LOCTEXT("NothingToShowPattern", "{0}\n\n{1}"), NothingToShowText, DropText);
}

bool SInputAssetView::HasSingleCollectionSource() const
{
	return ( SourcesData.Collections.Num() == 1 && SourcesData.VirtualPaths.Num() == 0 );
}

void SInputAssetView::SetUserSearching(bool bInSearching)
{
	if(bUserSearching != bInSearching)
	{
		RequestSlowFullListRefresh();
	}
	bUserSearching = bInSearching;
}

void SInputAssetView::HandleSettingChanged(FName PropertyName)
{
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UContentBrowserSettings, DisplayFolders)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UContentBrowserSettings, DisplayEmptyFolders)) ||
		(PropertyName == "DisplayDevelopersFolder") ||
		(PropertyName == "DisplayEngineFolder") ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UContentBrowserSettings, bDisplayContentFolderSuffix)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UContentBrowserSettings, bDisplayFriendlyNameForPluginFolders)) ||
		(PropertyName == NAME_None))	// @todo: Needed if PostEditChange was called manually, for now
	{
		RequestSlowFullListRefresh();
	}
}

FText SInputAssetView::GetQuickJumpTerm() const
{
	return FText::FromString(QuickJumpData.JumpTerm);
}

EVisibility SInputAssetView::IsQuickJumpVisible() const
{
	return (QuickJumpData.JumpTerm.IsEmpty()) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

FSlateColor SInputAssetView::GetQuickJumpColor() const
{
	return FAppStyle::GetColor((QuickJumpData.bHasValidMatch) ? "InfoReporting.BackgroundColor" : "ErrorReporting.BackgroundColor");
}

void SInputAssetView::ResetQuickJump()
{
	QuickJumpData.JumpTerm.Empty();
	QuickJumpData.bIsJumping = false;
	QuickJumpData.bHasChangedSinceLastTick = false;
	QuickJumpData.bHasValidMatch = false;
}

FReply SInputAssetView::HandleQuickJumpKeyDown(const TCHAR InCharacter, const bool bIsControlDown, const bool bIsAltDown, const bool bTestOnly)
{
	// Check for special characters
	if(bIsControlDown || bIsAltDown)
	{
		return FReply::Unhandled();
	}

	// Check for invalid characters
	for(int InvalidCharIndex = 0; InvalidCharIndex < UE_ARRAY_COUNT(INVALID_OBJECTNAME_CHARACTERS) - 1; ++InvalidCharIndex)
	{
		if(InCharacter == INVALID_OBJECTNAME_CHARACTERS[InvalidCharIndex])
		{
			return FReply::Unhandled();
		}
	}

	switch(InCharacter)
	{
	// Ignore some other special characters that we don't want to be entered into the buffer
	case 0:		// Any non-character key press, e.g. f1-f12, Delete, Pause/Break, etc.
				// These should be explicitly not handled so that their input bindings are handled higher up the chain.

	case 8:		// Backspace
	case 13:	// Enter
	case 27:	// Esc
		return FReply::Unhandled();

	default:
		break;
	}

	// Any other character!
	if(!bTestOnly)
	{
		QuickJumpData.JumpTerm.AppendChar(InCharacter);
		QuickJumpData.bHasChangedSinceLastTick = true;
	}

	return FReply::Handled();
}

bool SInputAssetView::PerformQuickJump(const bool bWasJumping)
{
	auto JumpToNextMatch = [this](const int StartIndex, const int EndIndex) -> bool
	{
		check(StartIndex >= 0);
		check(EndIndex <= FilteredAssetItems.Num());

		for(int NewSelectedItemIndex = StartIndex; NewSelectedItemIndex < EndIndex; ++NewSelectedItemIndex)
		{
			TSharedPtr<FContentBrowserItem>& NewSelectedItem = FilteredAssetItems[NewSelectedItemIndex];
			const FString& NewSelectedItemName = NewSelectedItem->GetDisplayName().ToString();
			if(NewSelectedItemName.StartsWith(QuickJumpData.JumpTerm, ESearchCase::IgnoreCase))
			{
				ClearSelection(true);
				RequestScrollIntoView(NewSelectedItem);
				ClearSelection();
				// Consider it derived from a keypress because otherwise it won't update the navigation selector
				SetItemSelection(NewSelectedItem, true, ESelectInfo::Type::OnKeyPress);
				return true;
			}
		}

		return false;
	};

	TArray<TSharedPtr<FContentBrowserItem>> SelectedItems = GetSelectedViewItems();
	TSharedPtr<FContentBrowserItem> SelectedItem = (SelectedItems.Num()) ? SelectedItems[0] : nullptr;

	// If we have a selection, and we were already jumping, first check to see whether 
	// the current selection still matches the quick-jump term; if it does, we do nothing
	if(bWasJumping && SelectedItem.IsValid())
	{
		const FString& SelectedItemName = SelectedItem->GetDisplayName().ToString();
		if(SelectedItemName.StartsWith(QuickJumpData.JumpTerm, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	// We need to move on to the next match in FilteredAssetItems that starts with the given quick-jump term
	const int SelectedItemIndex = (SelectedItem.IsValid()) ? FilteredAssetItems.Find(SelectedItem) : INDEX_NONE;
	const int StartIndex = (SelectedItemIndex == INDEX_NONE) ? 0 : SelectedItemIndex + 1;
	
	bool ValidMatch = JumpToNextMatch(StartIndex, FilteredAssetItems.Num());
	if(!ValidMatch && StartIndex > 0)
	{
		// If we didn't find a match, we need to loop around and look again from the start (assuming we weren't already)
		return JumpToNextMatch(0, StartIndex);
	}

	return ValidMatch;
}

bool SInputAssetView::ShouldColumnGenerateWidget(const FString ColumnName) const
{
	return !HiddenColumnNames.Contains(ColumnName);
}

void SInputAssetView::HandleItemDataUpdated(TArrayView<const FContentBrowserItemDataUpdate> InUpdatedItems)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SInputAssetView::HandleItemDataUpdated);

	if (InUpdatedItems.Num() == 0)
	{
		return;
	}

	const double HandleItemDataUpdatedStartTime = FPlatformTime::Seconds();

	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

	TArray<FContentBrowserDataCompiledFilter> CompiledDataFilters;
	if (SourcesData.IsIncludingVirtualPaths())
	{
		const bool bInvalidateFilterCache = false;
		const FContentBrowserDataFilter DataFilter = CreateBackendDataFilter(bInvalidateFilterCache);

		static const FName RootPath = "/";
		const TArrayView<const FName> DataSourcePaths = SourcesData.HasVirtualPaths() ? MakeArrayView(SourcesData.VirtualPaths) : MakeArrayView(&RootPath, 1);
		for (const FName& DataSourcePath : DataSourcePaths)
		{
			FContentBrowserDataCompiledFilter& CompiledDataFilter = CompiledDataFilters.AddDefaulted_GetRef();
			ContentBrowserData->CompileFilter(DataSourcePath, DataFilter, CompiledDataFilter);
		}
	}

	bool bRefreshView = false;
	TSet<TSharedPtr<FContentBrowserItem>> ItemsPendingInplaceFrontendFilter;

	auto AddItem = [this, &ItemsPendingInplaceFrontendFilter](const FContentBrowserItemKey& InItemDataKey, FContentBrowserItemData&& InItemData)
	{
		TSharedPtr<FContentBrowserItem>& ItemToUpdate = AvailableBackendItems.FindOrAdd(InItemDataKey);
		if (ItemToUpdate)
		{
			// Update the item
			ItemToUpdate->Append(MoveTemp(InItemData));

			// This item was modified, so put it in the list of items to be in-place re-tested against the active frontend filter (this can avoid a costly re-sort of the view)
			// If the item can't be queried in-place (because the item isn't in the view) then it will be added to ItemsPendingPriorityFilter instead
			ItemsPendingInplaceFrontendFilter.Add(ItemToUpdate);
		}
		else
		{
			ItemToUpdate = MakeShared<FContentBrowserItem>(MoveTemp(InItemData));

			// This item is new so put it in the pending set to be processed over time
			ItemsPendingFrontendFilter.Add(ItemToUpdate);
		}
	};

	auto RemoveItem = [this, &bRefreshView, &ItemsPendingInplaceFrontendFilter](const FContentBrowserItemKey& InItemDataKey, const FContentBrowserItemData& InItemData)
	{
		const uint32 ItemDataKeyHash = GetTypeHash(InItemDataKey);

		if (const TSharedPtr<FContentBrowserItem>* ItemToRemovePtr = AvailableBackendItems.FindByHash(ItemDataKeyHash, InItemDataKey))
		{
			TSharedPtr<FContentBrowserItem> ItemToRemove = *ItemToRemovePtr;
			check(ItemToRemove);

			// Only fully remove this item if every sub-item is removed (items become invalid when empty)
			ItemToRemove->Remove(InItemData);
			if (ItemToRemove->IsValid())
			{
				return;
			}

			AvailableBackendItems.RemoveByHash(ItemDataKeyHash, InItemDataKey);

			const uint32 ItemToRemoveHash = GetTypeHash(ItemToRemove);

			// Also ensure this item has been removed from the pending filter lists and the current list view data
			FilteredAssetItems.RemoveSingle(ItemToRemove);
			ItemsPendingPriorityFilter.RemoveByHash(ItemToRemoveHash, ItemToRemove);
			ItemsPendingFrontendFilter.RemoveByHash(ItemToRemoveHash, ItemToRemove);
			ItemsPendingInplaceFrontendFilter.RemoveByHash(ItemToRemoveHash, ItemToRemove);

			// Need to refresh manually after removing items, as adding relies on the pending filter lists to trigger this
			bRefreshView = true;
		}
	};

	auto GetBackendFilterCompliantItem = [this, &CompiledDataFilters](const FContentBrowserItemData& InItemData, bool& bOutPassFilter)
	{
		UContentBrowserDataSource* ItemDataSource = InItemData.GetOwnerDataSource();
		FContentBrowserItemData ItemData = InItemData;
		for (const FContentBrowserDataCompiledFilter& DataFilter : CompiledDataFilters)
		{
			// We only convert the item if this is the right filter for the data source
			if (ItemDataSource->ConvertItemForFilter(ItemData, DataFilter))
			{
				bOutPassFilter = ItemDataSource->DoesItemPassFilter(ItemData, DataFilter);

				return ItemData;
			}

			if (ItemDataSource->DoesItemPassFilter(ItemData, DataFilter))
			{
				bOutPassFilter = true;
				return ItemData;
			}
		}

		bOutPassFilter = false;
		return ItemData;
	};

	// Process the main set of updates
	for (const FContentBrowserItemDataUpdate& ItemDataUpdate : InUpdatedItems)
	{
		bool bItemPassFilter = false;
		FContentBrowserItemData ItemData = GetBackendFilterCompliantItem(ItemDataUpdate.GetItemData(), bItemPassFilter);
		const FContentBrowserItemKey ItemDataKey(ItemData);

		switch (ItemDataUpdate.GetUpdateType())
		{
		case EContentBrowserItemUpdateType::Added:
			if (bItemPassFilter)
			{
				AddItem(ItemDataKey, MoveTemp(ItemData));
			}
			break;

		case EContentBrowserItemUpdateType::Modified:
			if (bItemPassFilter)
			{
				AddItem(ItemDataKey, MoveTemp(ItemData));
			}
			else
			{
				RemoveItem(ItemDataKey, ItemData);
			}
			break;

		case EContentBrowserItemUpdateType::Moved:
			{
				const FContentBrowserItemData OldMinimalItemData(ItemData.GetOwnerDataSource(), ItemData.GetItemType(), ItemDataUpdate.GetPreviousVirtualPath(), NAME_None, FText(), nullptr, NAME_None);
				const FContentBrowserItemKey OldItemDataKey(OldMinimalItemData);
				RemoveItem(OldItemDataKey, OldMinimalItemData);

				if (bItemPassFilter)
				{
					AddItem(ItemDataKey, MoveTemp(ItemData));
				}
				else
				{
					check(!AvailableBackendItems.Contains(ItemDataKey));
				}
			}
			break;

		case EContentBrowserItemUpdateType::Removed:
			RemoveItem(ItemDataKey, ItemData);
			break;

		default:
			checkf(false, TEXT("Unexpected EContentBrowserItemUpdateType!"));
			break;
		}
	}

	// Now patch in the in-place frontend filter requests (if possible)
	if (ItemsPendingInplaceFrontendFilter.Num() > 0)
	{
		FAssetViewFrontendFilterHelper FrontendFilterHelper(this);
		const bool bRunQueryFilter = OnShouldFilterAsset.IsBound() || OnShouldFilterItem.IsBound();

		for (auto It = FilteredAssetItems.CreateIterator(); It && ItemsPendingInplaceFrontendFilter.Num() > 0; ++It)
		{
			const TSharedPtr<FContentBrowserItem> ItemToFilter = *It;

			if (ItemsPendingInplaceFrontendFilter.Remove(ItemToFilter) > 0)
			{
				bool bRemoveItem = false;

				// Run the query filter if required
				if (bRunQueryFilter)
				{
					const bool bPassedBackendFilter = FrontendFilterHelper.DoesItemPassQueryFilter(ItemToFilter);
					if (!bPassedBackendFilter)
					{
						bRemoveItem = true;
						AvailableBackendItems.Remove(FContentBrowserItemKey(*ItemToFilter));
					}
				}

				// Run the frontend filter
				if (!bRemoveItem)
				{
					const bool bPassedFrontendFilter = FrontendFilterHelper.DoesItemPassFrontendFilter(ItemToFilter);
					if (!bPassedFrontendFilter)
					{
						bRemoveItem = true;
					}
				}

				// Remove this item?
				if (bRemoveItem)
				{
					bRefreshView = true;
					It.RemoveCurrent();
				}
			}
		}

		// Do we still have items that could not be in-place filtered?
		// If so, add them to ItemsPendingPriorityFilter so they are processed into the view ASAP
		if (ItemsPendingInplaceFrontendFilter.Num() > 0)
		{
			ItemsPendingPriorityFilter.Append(MoveTemp(ItemsPendingInplaceFrontendFilter));
			ItemsPendingInplaceFrontendFilter.Reset();
		}
	}

	if (bRefreshView)
	{
		RefreshList();
	}
}

void SInputAssetView::HandleItemDataDiscoveryComplete()
{
	if (bPendingSortFilteredItems)
	{
		// If we have a sort pending, then force this to happen next frame now that discovery has finished
		LastSortTime = 0;
	}
}

void SInputAssetView::SetFilterBar(TSharedPtr<SFilterList> InFilterBar)
{
	FilterBar = InFilterBar;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

struct FInternalAssetViewItemHelper
{
	template <typename T>
	static TSharedRef<SWidget> CreateListTileItemContents(T* const InTileOrListItem, const TSharedRef<SWidget>& InThumbnail, FName& OutItemShadowBorder)
	{
		TSharedRef<SOverlay> ItemContentsOverlay = SNew(SOverlay);

		OutItemShadowBorder = FName("ContentBrowser.AssetTileItem.DropShadow");
		{
			// The actual thumbnail
			ItemContentsOverlay->AddSlot()
				[
					InThumbnail
				];
		}

		return ItemContentsOverlay;
	}
};

class SInputAssetViewItemToolTip : public SToolTip
{
public:
	SLATE_BEGIN_ARGS(SInputAssetViewItemToolTip)
		: _AssetViewItem()
	{ }

		SLATE_ARGUMENT(TSharedPtr<SInputAssetTileItem>, AssetViewItem)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		AssetViewItem = InArgs._AssetViewItem;

		SToolTip::Construct(
			SToolTip::FArguments()
			.TextMargin(1.0f)
			.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ToolTipBorder"))
			);
	}

	// IToolTip interface
	virtual bool IsEmpty() const override
	{
		return !AssetViewItem.IsValid();
	}

	virtual void OnOpening() override
	{
		TSharedPtr<SInputAssetTileItem> AssetViewItemPin = AssetViewItem.Pin();
		if (AssetViewItemPin.IsValid())
		{
			SetContentWidget(AssetViewItemPin->CreateToolTipWidget());
		}
	}

	virtual void OnClosed() override
	{
		ResetContentWidget();
	}

private:
	TWeakPtr<SInputAssetTileItem> AssetViewItem;
};

SInputAssetTileItem::~SInputAssetTileItem()
{
	OnItemDestroyed.ExecuteIfBound(AssetItem);
}

void SInputAssetTileItem::Construct( const FArguments& InArgs )
{
	AssetItem = InArgs._AssetItem;
	OnItemDestroyed = InArgs._OnItemDestroyed;
	ShouldAllowToolTip = InArgs._ShouldAllowToolTip;
	ThumbnailEditMode = InArgs._ThumbnailEditMode;
	HighlightText = InArgs._HighlightText;
	OnIsAssetValidForCustomToolTip = InArgs._OnIsAssetValidForCustomToolTip;
	OnGetCustomAssetToolTip = InArgs._OnGetCustomAssetToolTip;
	OnVisualizeAssetToolTip = InArgs._OnVisualizeAssetToolTip;
	OnAssetToolTipClosing = InArgs._OnAssetToolTipClosing;
	AssetOverrideLabelText = InArgs._AssetOverrideLabelText;
	IsSelected = InArgs._IsSelected;

	bDraggedOver = false;

	bItemDirty = false;
	OnAssetDataChanged();
	
	AssetDirtyBrush = FAppStyle::GetBrush("ContentBrowser.ContentDirty");

	// Set our tooltip - this will refresh each time it's opened to make sure it's up-to-date
	SetToolTip(SNew(SInputAssetViewItemToolTip).AssetViewItem(SharedThis(this)));
	
	bShowType = InArgs._ShowType;
	AssetThumbnail = InArgs._AssetThumbnail;
	ItemWidth = InArgs._ItemWidth;
	ThumbnailPadding = InArgs._ThumbnailPadding;

	CurrentThumbnailSize = InArgs._CurrentThumbnailSize;

	InitializeAssetNameHeights();

	TSharedPtr<SWidget> Thumbnail;
	if ( AssetItem.IsValid() && AssetThumbnail.IsValid() )
	{
		FAssetThumbnailConfig ThumbnailConfig;
		ThumbnailConfig.bAllowFadeIn = true;
		ThumbnailConfig.bAllowHintText = InArgs._AllowThumbnailHintLabel;
		ThumbnailConfig.bAllowRealTimeOnHovered = false; // we use our own OnMouseEnter/Leave for logical asset item
		ThumbnailConfig.bForceGenericThumbnail = AssetItem->GetItemTemporaryReason() == EContentBrowserItemFlags::Temporary_Creation;
		ThumbnailConfig.AllowAssetSpecificThumbnailOverlay = !ThumbnailConfig.bForceGenericThumbnail;
		ThumbnailConfig.AllowAssetSpecificThumbnailOverlayIndicator = ThumbnailConfig.AllowAssetSpecificThumbnailOverlay;
		ThumbnailConfig.ThumbnailLabel = InArgs._ThumbnailLabel;
		ThumbnailConfig.HighlightedText = InArgs._HighlightText;
		ThumbnailConfig.HintColorAndOpacity = InArgs._ThumbnailHintColorAndOpacity;
		ThumbnailConfig.BorderPadding = FMargin(2.0f);
		ThumbnailConfig.GenericThumbnailSize = MakeAttributeSP(this, &SInputAssetTileItem::GetGenericThumbnailSize);
		
		if (!AssetItem->IsSupported())
		{
			ThumbnailConfig.ClassThumbnailBrushOverride = FName("Icons.WarningWithColor.Thumbnail");
		}
		
		{
			FContentBrowserItemDataAttributeValue ColorAttributeValue = AssetItem->GetItemAttribute(ContentBrowserItemAttributes::ItemColor);
			if (ColorAttributeValue.IsValid())
			{
				const FString ColorStr = ColorAttributeValue.GetValue<FString>();

				FLinearColor Color;
				if (Color.InitFromString(ColorStr))
				{
					ThumbnailConfig.AssetTypeColorOverride = Color;
				}
			}
		}

		Thumbnail = AssetThumbnail->MakeThumbnailWidget(ThumbnailConfig);
	}
	else
	{
		Thumbnail = SNew(SImage).Image(FAppStyle::GetDefaultBrush());
	}

	FName ItemShadowBorderName;
	TSharedRef<SWidget> ItemContents = FInternalAssetViewItemHelper::CreateListTileItemContents(this, Thumbnail.ToSharedRef(), ItemShadowBorderName);

	if (bShowType)
	{
		SAssignNew(ItemLabelWidget, STextBlock)
			.Font(this, &SInputAssetTileItem::GetThumbnailFont)
			.Text(GetNameText())
			.HighlightText(InArgs._HighlightText)
			.AutoWrapText(true)
			.WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
			.LineBreakPolicy(FBreakIterator::CreateCamelCaseBreakIterator())
			.ColorAndOpacity(this, &SInputAssetTileItem::GetNameAreaTextColor);
	}
	else
	{
		SAssignNew(ItemLabelWidget, STextBlock)
			.Font(this, &SInputAssetTileItem::GetThumbnailFont)
			.Text(GetNameText())
			.HighlightText(InArgs._HighlightText)
			.AutoWrapText(true)
			.WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
			.LineBreakPolicy(FBreakIterator::CreateCamelCaseBreakIterator())
			.ColorAndOpacity(this, &SInputAssetTileItem::GetNameAreaTextColor);
	}

	ChildSlot
	.Padding(FMargin(0.0f, 0.0f, 4.0f, 4.0f))
	[				
		// Drop shadow border
		SNew(SBorder)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
		.BorderImage(FAppStyle::Get().GetBrush(ItemShadowBorderName))
		[
			SNew(SOverlay)
			.AddMetaData<FTagMetaData>(FTagMetaData(AssetItem->GetVirtualPath()))
			+SOverlay::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(FAppStyle::Get().GetBrush("ContentBrowser.AssetTileItem.ThumbnailAreaBackground"))
				[
					SNew(SVerticalBox)
					// Thumbnail
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						// The remainder of the space is reserved for the name.
						SNew(SBox)
						.Padding(0)
						.WidthOverride(this, &SInputAssetTileItem::GetThumbnailBoxSize)
						.HeightOverride(this, &SInputAssetTileItem::GetThumbnailBoxSize)
						[
							ItemContents
						]
					]

					+SVerticalBox::Slot()
					.VAlign(VAlign_Fill)
					.AutoHeight()
					[
						SNew(SBorder)
						.Padding(FMargin(2.0f, 3.0f))
						.BorderImage(this, &SInputAssetTileItem::GetNameAreaBackgroundImage)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.Padding(2.0f,2.0f,0.0f,0.0f)
							.VAlign(VAlign_Fill)
							.HAlign(HAlign_Fill)
							.AutoHeight()
							[
								SNew(SBox)
								.HeightOverride(this, &SInputAssetTileItem::GetNameAreaMaxDesiredHeight)
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Fill)
								[
									ItemLabelWidget.ToSharedRef()
								]
							]
							
						]
					]
				]
			]
			+SOverlay::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SImage)
				.Image(this, &SInputAssetTileItem::GetBorderImage)
				.Visibility(EVisibility::HitTestInvisible)
			]
		]
	];
}

void SInputAssetTileItem::OnAssetDataChanged()
{
	UpdateDirtyState();

	if ( ItemLabelWidget.IsValid() )
	{
		ItemLabelWidget->SetText( GetNameText() );
	}

	if (ClassTextWidget.IsValid())
	{
		ClassTextWidget->SetText(GetAssetClassText());
	}
	
	CacheDisplayTags();

	if (AssetThumbnail)
	{
		bool bSetThumbnail = false;
		if (AssetItem)
		{
			bSetThumbnail = AssetItem->UpdateThumbnail(*AssetThumbnail);
		}
		if (!bSetThumbnail)
		{
			AssetThumbnail->SetAsset(FAssetData());
		}
	}
}

void SInputAssetTileItem::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Super::OnMouseEnter(MyGeometry, MouseEvent);

	if (AssetThumbnail.IsValid())
	{
		AssetThumbnail->SetRealTime( true );
	}
}

void SInputAssetTileItem::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	Super::OnMouseLeave(MouseEvent);

	if (AssetThumbnail.IsValid())
	{
		AssetThumbnail->SetRealTime( false );
	}
}

const FSlateBrush* SInputAssetTileItem::GetBorderImage() const
{
	const bool bIsSelected = IsSelected.IsBound() ? IsSelected.Execute() : false;
	const bool bIsHoveredOrDraggedOver = IsHovered() || bDraggedOver;
	if (bIsSelected && bIsHoveredOrDraggedOver)
	{
		static const FName SelectedHover("ContentBrowser.AssetTileItem.SelectedHoverBorder");
		return FAppStyle::Get().GetBrush(SelectedHover);
	}
	else if (bIsSelected)
	{
		static const FName Selected("ContentBrowser.AssetTileItem.SelectedBorder");
		return FAppStyle::Get().GetBrush(Selected);
	}
	else if (bIsHoveredOrDraggedOver && !IsFolder())
	{
		static const FName Hovered("ContentBrowser.AssetTileItem.HoverBorder");
		return FAppStyle::Get().GetBrush(Hovered);
	}

	return FStyleDefaults::GetNoBrush();
}

float SInputAssetTileItem::GetExtraStateIconWidth() const
{
	return GetStateIconImageSize().Get();
}

FOptionalSize SInputAssetTileItem::GetExtraStateIconMaxWidth() const
{
	return GetThumbnailBoxSize().Get() * 0.8f;
}

FOptionalSize SInputAssetTileItem::GetStateIconImageSize() const
{
	float IconSize = FMath::TruncToFloat(GetThumbnailBoxSize().Get() * 0.2f);
	return IconSize > 12 ? IconSize : 12;
}

FOptionalSize SInputAssetTileItem::GetThumbnailBoxSize() const
{
	return FOptionalSize(ItemWidth.Get()- ThumbnailPadding);
}

EVisibility SInputAssetTileItem::GetAssetClassLabelVisibility() const
{
	return (!IsFolder() && bShowType)
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

FSlateColor SInputAssetTileItem::GetAssetClassLabelTextColor() const
{
	const bool bIsSelected = IsSelected.IsBound() ? IsSelected.Execute() : false;
	const bool bIsHoveredOrDraggedOver = IsHovered() || bDraggedOver;
	if (bIsSelected || bIsHoveredOrDraggedOver)
	{
		return FStyleColors::White;
	}

	return FSlateColor::UseSubduedForeground();
}

FSlateFontInfo SInputAssetTileItem::GetThumbnailFont() const
{
	FOptionalSize ThumbSize = GetThumbnailBoxSize();
	if ( ThumbSize.IsSet() )
	{
		float Size = ThumbSize.Get();
		if ( Size < 50 )
		{
			const static FName SmallFontName("ContentBrowser.AssetTileViewNameFontVerySmall");
			return FAppStyle::GetFontStyle(SmallFontName);
		}
		else if ( Size < 85 )
		{
			const static FName SmallFontName("ContentBrowser.AssetTileViewNameFontSmall");
			return FAppStyle::GetFontStyle(SmallFontName);
		}
	}

	const static FName RegularFont("ContentBrowser.AssetTileViewNameFont");
	return FAppStyle::GetFontStyle(RegularFont);
}

const FSlateBrush* SInputAssetTileItem::GetFolderBackgroundImage() const
{
	const bool bIsSelected = IsSelected.IsBound() ? IsSelected.Execute() : false;
	const bool bIsHoveredOrDraggedOver = IsHovered() || bDraggedOver;

	if (bIsSelected && bIsHoveredOrDraggedOver)
	{
		static const FName SelectedHoverBackground("ContentBrowser.AssetTileItem.FolderAreaSelectedHoverBackground");
		return FAppStyle::Get().GetBrush(SelectedHoverBackground);
	}
	else if (bIsSelected)
	{
		static const FName SelectedBackground("ContentBrowser.AssetTileItem.FolderAreaSelectedBackground");
		return FAppStyle::Get().GetBrush(SelectedBackground);
	}
	else if (bIsHoveredOrDraggedOver)
	{
		static const FName HoveredBackground("ContentBrowser.AssetTileItem.FolderAreaHoveredBackground");
		return FAppStyle::Get().GetBrush(HoveredBackground);
	}

	return FStyleDefaults::GetNoBrush();
}

const FSlateBrush* SInputAssetTileItem::GetFolderBackgroundShadowImage() const
{
	const bool bIsSelected = IsSelected.IsBound() ? IsSelected.Execute() : false;
	const bool bIsHoveredOrDraggedOver = IsHovered() || bDraggedOver;

	if (bIsSelected || bIsHoveredOrDraggedOver)
	{
		static const FName DropShadowName("ContentBrowser.AssetTileItem.DropShadow");
		return FAppStyle::Get().GetBrush(DropShadowName);
	}

	return FStyleDefaults::GetNoBrush();
}

const FSlateBrush* SInputAssetTileItem::GetNameAreaBackgroundImage() const
{
	const bool bIsSelected = IsSelected.IsBound() ? IsSelected.Execute() : false;
	const bool bIsHoveredOrDraggedOver = IsHovered() || bDraggedOver;
	if (bIsSelected && bIsHoveredOrDraggedOver)
	{
		static const FName SelectedHover("ContentBrowser.AssetTileItem.NameAreaSelectedHoverBackground");
		return FAppStyle::Get().GetBrush(SelectedHover);
	}
	else if (bIsSelected)
	{
		static const FName Selected("ContentBrowser.AssetTileItem.NameAreaSelectedBackground");
		return FAppStyle::Get().GetBrush(Selected);
	}
	else if (bIsHoveredOrDraggedOver && !IsFolder())
	{
		static const FName Hovered("ContentBrowser.AssetTileItem.NameAreaHoverBackground");
		return FAppStyle::Get().GetBrush(Hovered);
	}
	else if (!IsFolder())
	{
		static const FName Normal("ContentBrowser.AssetTileItem.NameAreaBackground");
		return FAppStyle::Get().GetBrush(Normal);
	}

	return FStyleDefaults::GetNoBrush();
}

FSlateColor SInputAssetTileItem::GetNameAreaTextColor() const
{
	const bool bIsSelected = IsSelected.IsBound() ? IsSelected.Execute() : false;
	const bool bIsHoveredOrDraggedOver = IsHovered() || bDraggedOver;
	if (bIsSelected || bIsHoveredOrDraggedOver)
	{
		return FStyleColors::White;
	}

	return FSlateColor::UseForeground();
}

FOptionalSize SInputAssetTileItem::GetNameAreaMaxDesiredHeight() const
{
	return AssetNameHeights[(int32)CurrentThumbnailSize.Get()];
}

int32 SInputAssetTileItem::GetGenericThumbnailSize() const
{
	return GenericThumbnailSizes[(int32)CurrentThumbnailSize.Get()];
}

EVisibility SInputAssetTileItem::GetSCCIconVisibility() const
{
	// Hide the scc state icon when there is no brush or in tiny size since there isn't enough space
	return EVisibility::Collapsed;
}

void SInputAssetTileItem::InitializeAssetNameHeights()
{
	// The height of the asset name field for each thumbnail size
	static bool bInitializedHeights = false;

	if (!bInitializedHeights)
	{
		AssetNameHeights[(int32)EThumbnailSize::Tiny] = 0;

		{
			const static FName SmallFontName("ContentBrowser.AssetTileViewNameFontSmall");
			FSlateFontInfo Font = FAppStyle::GetFontStyle(SmallFontName);
			TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			SmallFontHeight = FontMeasureService->GetMaxCharacterHeight(Font);
			AssetNameHeights[(int32)EThumbnailSize::Small] = SmallFontHeight * 2 ;
		}

		{
			const static FName SmallFontName("ContentBrowser.AssetTileViewNameFont");
			FSlateFontInfo Font = FAppStyle::GetFontStyle(SmallFontName);
			TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			RegularFontHeight = FontMeasureService->GetMaxCharacterHeight(Font);
			AssetNameHeights[(int32)EThumbnailSize::Medium] = RegularFontHeight * 3;
			AssetNameHeights[(int32)EThumbnailSize::Large] = RegularFontHeight * 4;
			AssetNameHeights[(int32)EThumbnailSize::Huge] = RegularFontHeight * 5;
		}

		bInitializedHeights = true;
	}

}

float SInputAssetTileItem::AssetNameHeights[(int32)EThumbnailSize::MAX];
float SInputAssetTileItem::RegularFontHeight(0);
float SInputAssetTileItem::SmallFontHeight(0);

void SInputAssetTileItem::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	LastGeometry = AllottedGeometry;
	UpdateDirtyState();
}

TSharedPtr<IToolTip> SInputAssetTileItem::GetToolTip()
{
	return ShouldAllowToolTip.Get() ? SCompoundWidget::GetToolTip() : NULL;
}

bool SInputAssetTileItem::IsNameReadOnly() const
{
	if (ThumbnailEditMode.Get())
	{
		// Read-only while editing thumbnails
		return true;
	}

	if (!AssetItem.IsValid())
	{
		// Read-only if no valid asset item
		return true;
	}

	if (AssetItem->IsTemporary())
	{
		// Temporary items can always be renamed (required for creation/duplication, etc)
		return false;
	}

	// Read-only if we can't be renamed
	return !AssetItem->CanRename(nullptr);
}

void SInputAssetTileItem::DirtyStateChanged()
{
}

FText SInputAssetTileItem::GetAssetClassText() const
{
	if (!AssetItem)
	{
		return FText();
	}
	
	if (AssetItem->IsFolder())
	{
		return LOCTEXT("FolderName", "Folder");
	}

	FContentBrowserItemDataAttributeValue DisplayNameAttributeValue = AssetItem->GetItemAttribute(ContentBrowserItemAttributes::ItemTypeDisplayName);
	if (!DisplayNameAttributeValue.IsValid())
	{
		DisplayNameAttributeValue = AssetItem->GetItemAttribute(ContentBrowserItemAttributes::ItemTypeName);
	}
	return DisplayNameAttributeValue.IsValid() ? DisplayNameAttributeValue.GetValue<FText>() : FText();
}

const FSlateBrush* SInputAssetTileItem::GetDirtyImage() const
{
	return IsDirty() ? AssetDirtyBrush : NULL;
}

TSharedRef<SWidget> SInputAssetTileItem::GenerateExtraStateIconWidget(TAttribute<float> InMaxExtraStateIconWidth) const
{
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SInputAssetTileItem::GenerateExtraStateTooltipWidget() const
{
	return SNullWidget::NullWidget;
}

EVisibility SInputAssetTileItem::GetThumbnailEditModeUIVisibility() const
{
	return !IsFolder() && ThumbnailEditMode.Get() ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SInputAssetTileItem::CreateToolTipWidget() const
{
	if ( AssetItem.IsValid() )
	{
		// Legacy custom asset tooltips
		if (OnGetCustomAssetToolTip.IsBound())
		{
			FAssetData ItemAssetData;
			if (AssetItem->Legacy_TryGetAssetData(ItemAssetData))
			{
				const bool bTryCustomAssetToolTip = !OnIsAssetValidForCustomToolTip.IsBound() || OnIsAssetValidForCustomToolTip.Execute(ItemAssetData);
				if (bTryCustomAssetToolTip)
				{
					return OnGetCustomAssetToolTip.Execute(ItemAssetData);
				}
			}
		}

		// TODO: Remove this special caseness so that folders can also have visible attributes
		if(AssetItem->IsFile())
		{
			// The tooltip contains the name, class, path, asset registry tags and source control status
			const FText NameText = GetNameText();
			const FText ClassText = FText::Format(LOCTEXT("ClassName", "({0})"), GetAssetClassText());

			FText PublicStateText;
			FName PublicStateTextBorder = "ContentBrowser.TileViewTooltip.PillBorder";

			// Create a box to hold every line of info in the body of the tooltip
			TSharedRef<SVerticalBox> InfoBox = SNew(SVerticalBox);

			FAssetData ItemAssetData;
			AssetItem->Legacy_TryGetAssetData(ItemAssetData);

			// TODO: Always use the virtual path?
			if (ItemAssetData.IsValid())
			{
				AddToToolTipInfoBox(InfoBox, LOCTEXT("TileViewTooltipPath", "Path"), FText::FromName(ItemAssetData.PackagePath), false);
			}
			else
			{
				AddToToolTipInfoBox(InfoBox, LOCTEXT("TileViewTooltipPath", "Path"), FText::FromName(AssetItem->GetVirtualPath()), false);
			}

		if (ItemAssetData.IsValid() && ItemAssetData.PackageName != NAME_None)
		{
			if (ItemAssetData.PackageFlags)
			{
				PublicStateText = LOCTEXT("PrivateAssetState", "Private");
			}
				else
				{
					PublicStateText = LOCTEXT("PublicAssetState", "Public");
				}
			}

			if (!AssetItem->CanEdit())
			{
				PublicStateText = LOCTEXT("ReadOnlyAssetState", "Read Only");
			}

			if(!AssetItem->IsSupported())
			{
				PublicStateText = LOCTEXT("UnsupportedAssetState", "Unsupported");
				PublicStateTextBorder = "ContentBrowser.TileViewTooltip.UnsupportedAssetPillBorder";
			}

			// Add tags
			for (const auto& DisplayTagItem : CachedDisplayTags)
			{
				AddToToolTipInfoBox(InfoBox, DisplayTagItem.DisplayKey, DisplayTagItem.DisplayValue, DisplayTagItem.bImportant);
			}

			TSharedRef<SVerticalBox> OverallTooltipVBox = SNew(SVerticalBox);

			static const auto PublicAssetUIEnabledCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ContentBrowser.PublicAsset.EnablePublicAssetFeature"));
			const bool bIsPublicAssetUIEnabled = PublicAssetUIEnabledCVar && PublicAssetUIEnabledCVar->GetBool();

			// Top section (asset name, type, is checked out)
			OverallTooltipVBox->AddSlot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(SBorder)
					.Padding(6)
					.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0, 0, 4, 0)
							[
								SNew(STextBlock)
								.Text(NameText)
								.Font(FAppStyle::GetFontStyle("ContentBrowser.TileViewTooltip.NameFont"))
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(ClassText)
								.HighlightText(HighlightText)
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(10.0f, 4.0f)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Right) 
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush(PublicStateTextBorder))
								.Padding(FMargin(12.0f, 2.0f, 12.0f, 2.0f))
								.Visibility(bIsPublicAssetUIEnabled && !PublicStateText.IsEmpty() ? EVisibility::Visible : EVisibility::Hidden)
								[
									SNew(STextBlock)
									.Text(PublicStateText)
									.HighlightText(HighlightText)
								]
								
							]
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Visibility_Lambda([this]()
							{
								return AssetItem->IsSupported() ? EVisibility::Collapsed : EVisibility::Visible;
							})
							.Text(LOCTEXT("UnsupportedAssetDescriptionText", "This type of asset is not allowed in this project. Delete unsupported assets to avoid errors."))
							.ColorAndOpacity(FStyleColors::Warning)
						]
					]
				];

			// Middle section (user description, if present)
			FText UserDescription = GetAssetUserDescription();
			if (!UserDescription.IsEmpty())
			{
				OverallTooltipVBox->AddSlot()
					.AutoHeight()
					.Padding(0, 0, 0, 4)
					[
						SNew(SBorder)
						.Padding(6)
						.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
						[
							SNew(STextBlock)
							.WrapTextAt(700.0f)
							.Font(FAppStyle::GetFontStyle("ContentBrowser.TileViewTooltip.AssetUserDescriptionFont"))
							.Text(UserDescription)
						]
					];
			}

			// Bottom section (asset registry tags)
			OverallTooltipVBox->AddSlot()
				.AutoHeight()
				[
					SNew(SBorder)
					.Padding(6)
					.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
					[
						InfoBox
					]
				];

			// Final section (collection pips)
			if (ItemAssetData.IsValid())
			{
				ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();

			TArray<FCollectionNameType> CollectionsContainingObject;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			CollectionManager.GetCollectionsContainingObject(FSoftObjectPath(ItemAssetData.GetSoftObjectPath()), CollectionsContainingObject);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}

			return SNew(SBorder)
				.Padding(6)
				.BorderImage( FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.NonContentBorder") )
				[
					OverallTooltipVBox
				];
		}
		else
		{
			const FText FolderName = GetNameText();
			const FText FolderPath = FText::FromName(AssetItem->GetVirtualPath());

			// Create a box to hold every line of info in the body of the tooltip
			TSharedRef<SVerticalBox> InfoBox = SNew(SVerticalBox);

			AddToToolTipInfoBox( InfoBox, LOCTEXT("TileViewTooltipPath", "Path"), FolderPath, false );

			return SNew(SBorder)
				.Padding(6)
				.BorderImage( FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.NonContentBorder") )
				[
					SNew(SVerticalBox)

					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 0, 0, 4)
					[
						SNew(SBorder)
						.Padding(6)
						.BorderImage( FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder") )
						[
							SNew(SVerticalBox)

							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)

								+SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0, 0, 4, 0)
								[
									SNew(STextBlock)
									.Text( FolderName )
									.Font( FAppStyle::GetFontStyle("ContentBrowser.TileViewTooltip.NameFont") )
								]

								+SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock) 
									.Text( LOCTEXT("FolderNameBracketed", "(Folder)") )
								]
							]
						]
					]

					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.Padding(6)
						.BorderImage( FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder") )
						[
							InfoBox
						]
					]
				];
		}
	}
	else
	{
		// Return an empty tooltip since the asset item wasn't valid
		return SNullWidget::NullWidget;
	}
}

FText SInputAssetTileItem::GetAssetUserDescription() const
{
	if (AssetItem && AssetItem->IsFile())
	{
		FContentBrowserItemDataAttributeValue DescriptionAttributeValue = AssetItem->GetItemAttribute(ContentBrowserItemAttributes::ItemDescription);
		if (DescriptionAttributeValue.IsValid())
		{
			return DescriptionAttributeValue.GetValue<FText>();
		}
	}

	return FText::GetEmpty();
}

void SInputAssetTileItem::AddToToolTipInfoBox(const TSharedRef<SVerticalBox>& InfoBox, const FText& Key, const FText& Value, bool bImportant) const
{
	FWidgetStyle ImportantStyle;
	ImportantStyle.SetForegroundColor(FLinearColor(1, 0.5, 0, 1));

	InfoBox->AddSlot()
	.AutoHeight()
	.Padding(0, 1)
	[
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 4, 0)
		[
			SNew(STextBlock) .Text( FText::Format(LOCTEXT("AssetViewTooltipFormat", "{0}:"), Key ) )
			.ColorAndOpacity(bImportant ? ImportantStyle.GetSubduedForegroundColor() : FSlateColor::UseSubduedForeground())
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock) .Text( Value )
			.ColorAndOpacity(bImportant ? ImportantStyle.GetForegroundColor() : FSlateColor::UseForeground())
			.HighlightText((Key.ToString() == TEXT("Path")) ? HighlightText : FText())
			.WrapTextAt(700.0f)
		]
	];
}

void SInputAssetTileItem::UpdateDirtyState()
{
	bool bNewIsDirty = false;

	// Only update the dirty state for non-temporary items
	if (AssetItem && !AssetItem->IsTemporary())
	{
		bNewIsDirty = AssetItem->IsDirty();
	}

	if (bNewIsDirty != bItemDirty)
	{
		bItemDirty = bNewIsDirty;
		DirtyStateChanged();
	}
}

bool SInputAssetTileItem::IsDirty() const
{
	return bItemDirty;
}

void SInputAssetTileItem::CacheDisplayTags()
{
	CachedDisplayTags.Reset();

	const FContentBrowserItemDataAttributeValues AssetItemAttributes = AssetItem->GetItemAttributes(/*bIncludeMetaData*/true);
	
	FAssetData ItemAssetData;
	AssetItem->Legacy_TryGetAssetData(ItemAssetData);

	// Add all visible attributes
	for (const auto& AssetItemAttributePair : AssetItemAttributes)
	{
		const FName AttributeName = AssetItemAttributePair.Key;
		const FContentBrowserItemDataAttributeValue& AttributeValue = AssetItemAttributePair.Value;
		const FContentBrowserItemDataAttributeMetaData& AttributeMetaData = AttributeValue.GetMetaData();

		if (AttributeMetaData.AttributeType == UObject::FAssetRegistryTag::TT_Hidden)
		{
			continue;
		}
	
		// Build the display value for this attribute
		FText DisplayValue;
		if (AttributeValue.GetValueType() == EContentBrowserItemDataAttributeValueType::Text)
		{
			DisplayValue = AttributeValue.GetValueText();
		}
		else
		{
			const FString AttributeValueStr = AttributeValue.GetValue<FString>();

			auto ReformatNumberStringForDisplay = [](const FString& InNumberString) -> FText
			{
				FText Result = FText::GetEmpty();

				// Respect the number of decimal places in the source string when converting for display
				int32 NumDecimalPlaces = 0;
				{
					int32 DotIndex = INDEX_NONE;
					if (InNumberString.FindChar(TEXT('.'), DotIndex))
					{
						NumDecimalPlaces = InNumberString.Len() - DotIndex - 1;
					}
				}

				if (NumDecimalPlaces > 0)
				{
					// Convert the number as a double
					double Num = 0.0;
					LexFromString(Num, *InNumberString);

					const FNumberFormattingOptions NumFormatOpts = FNumberFormattingOptions()
						.SetMinimumFractionalDigits(NumDecimalPlaces)
						.SetMaximumFractionalDigits(NumDecimalPlaces);

					Result = FText::AsNumber(Num, &NumFormatOpts);
				}
				else
				{
					const bool bIsSigned = InNumberString.Len() > 0 && (InNumberString[0] == TEXT('-') || InNumberString[0] == TEXT('+'));

					if (bIsSigned)
					{
						// Convert the number as a signed int
						int64 Num = 0;
						LexFromString(Num, *InNumberString);

						Result = FText::AsNumber(Num);
					}
					else
					{
						// Convert the number as an unsigned int
						uint64 Num = 0;
						LexFromString(Num, *InNumberString);

						Result = FText::AsNumber(Num);
					}
				}

				return Result;
			};
	
			bool bHasSetDisplayValue = false;
	
			// Numerical tags need to format the specified number based on the display flags
			if (!bHasSetDisplayValue && AttributeMetaData.AttributeType == UObject::FAssetRegistryTag::TT_Numerical && AttributeValueStr.IsNumeric())
			{
				bHasSetDisplayValue = true;
	
				const bool bAsMemory = !!(AttributeMetaData.DisplayFlags & UObject::FAssetRegistryTag::TD_Memory);
	
				if (bAsMemory)
				{
					// Memory should be a 64-bit unsigned number of bytes
					uint64 NumBytes = 0;
					LexFromString(NumBytes, *AttributeValueStr);
	
					DisplayValue = FText::AsMemory(NumBytes);
				}
				else
				{
					DisplayValue = ReformatNumberStringForDisplay(AttributeValueStr);
				}
			}
	
			// Dimensional tags need to be split into their component numbers, with each component number re-format
			if (!bHasSetDisplayValue && AttributeMetaData.AttributeType == UObject::FAssetRegistryTag::TT_Dimensional)
			{
				TArray<FString> NumberStrTokens;
				AttributeValueStr.ParseIntoArray(NumberStrTokens, TEXT("x"), true);
	
				if (NumberStrTokens.Num() > 0 && NumberStrTokens.Num() <= 3)
				{
					bHasSetDisplayValue = true;
	
					switch (NumberStrTokens.Num())
					{
					case 1:
						DisplayValue = ReformatNumberStringForDisplay(NumberStrTokens[0]);
						break;
	
					case 2:
						DisplayValue = FText::Format(LOCTEXT("DisplayTag2xFmt", "{0} \u00D7 {1}"), ReformatNumberStringForDisplay(NumberStrTokens[0]), ReformatNumberStringForDisplay(NumberStrTokens[1]));
						break;
	
					case 3:
						DisplayValue = FText::Format(LOCTEXT("DisplayTag3xFmt", "{0} \u00D7 {1} \u00D7 {2}"), ReformatNumberStringForDisplay(NumberStrTokens[0]), ReformatNumberStringForDisplay(NumberStrTokens[1]), ReformatNumberStringForDisplay(NumberStrTokens[2]));
						break;
	
					default:
						break;
					}
				}
			}
	
			// Chronological tags need to format the specified timestamp based on the display flags
			if (!bHasSetDisplayValue && AttributeMetaData.AttributeType == UObject::FAssetRegistryTag::TT_Chronological)
			{
				bHasSetDisplayValue = true;
	
				FDateTime Timestamp;
				if (FDateTime::Parse(AttributeValueStr, Timestamp))
				{
					const bool bDisplayDate = !!(AttributeMetaData.DisplayFlags & UObject::FAssetRegistryTag::TD_Date);
					const bool bDisplayTime = !!(AttributeMetaData.DisplayFlags & UObject::FAssetRegistryTag::TD_Time);
					const FString TimeZone = (AttributeMetaData.DisplayFlags & UObject::FAssetRegistryTag::TD_InvariantTz) ? FText::GetInvariantTimeZone() : FString();
	
					if (bDisplayDate && bDisplayTime)
					{
						DisplayValue = FText::AsDateTime(Timestamp, EDateTimeStyle::Short, EDateTimeStyle::Short, TimeZone);
					}
					else if (bDisplayDate)
					{
						DisplayValue = FText::AsDate(Timestamp, EDateTimeStyle::Short, TimeZone);
					}
					else if (bDisplayTime)
					{
						DisplayValue = FText::AsTime(Timestamp, EDateTimeStyle::Short, TimeZone);
					}
				}
			}
	
			// The tag value might be localized text, so we need to parse it for display
			if (!bHasSetDisplayValue && FTextStringHelper::IsComplexText(*AttributeValueStr))
			{
				bHasSetDisplayValue = FTextStringHelper::ReadFromBuffer(*AttributeValueStr, DisplayValue) != nullptr;
			}
	
			// Do our best to build something valid from the string value
			if (!bHasSetDisplayValue)
			{
				bHasSetDisplayValue = true;
	
				// Since all we have at this point is a string, we can't be very smart here.
				// We need to strip some noise off class paths in some cases, but can't load the asset to inspect its UPROPERTYs manually due to performance concerns.
				FString ValueString = FPackageName::ExportTextPathToObjectPath(AttributeValueStr);
	
				const TCHAR StringToRemove[] = TEXT("/Script/");
			if (ValueString.StartsWith(StringToRemove))
			{
				// Remove the class path for native classes, and also remove Engine. for engine classes
				const int32 SizeOfPrefix = UE_ARRAY_COUNT(StringToRemove) - 1;
				ValueString.MidInline(SizeOfPrefix, ValueString.Len() - SizeOfPrefix, EAllowShrinking::No);
				ValueString.ReplaceInline(TEXT("Engine."), TEXT(""));
			}
	
				if (ItemAssetData.IsValid())
				{
					if (const UClass* AssetClass = ItemAssetData.GetClass())
					{
						if (const FProperty* TagField = FindFProperty<FProperty>(AssetClass, AttributeName))
						{
							const FProperty* TagProp = nullptr;
							const UEnum* TagEnum = nullptr;
							if (const FByteProperty* ByteProp = CastField<FByteProperty>(TagField))
							{
								TagProp = ByteProp;
								TagEnum = ByteProp->Enum;
							}
							else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(TagField))
							{
								TagProp = EnumProp;
								TagEnum = EnumProp->GetEnum();
							}

							// Strip off enum prefixes if they exist
							if (TagProp)
							{
							if (TagEnum)
							{
								const FString EnumPrefix = TagEnum->GenerateEnumPrefix();
								if (EnumPrefix.Len() && ValueString.StartsWith(EnumPrefix))
								{
									ValueString.RightChopInline(EnumPrefix.Len() + 1, EAllowShrinking::No);	// +1 to skip over the underscore
								}
							}

								ValueString = FName::NameToDisplayString(ValueString, false);
							}
						}
					}
				}
	
				DisplayValue = FText::AsCultureInvariant(MoveTemp(ValueString));
			}
			
			// Add suffix to the value, if one is defined for this tag
			if (!AttributeMetaData.Suffix.IsEmpty())
			{
				DisplayValue = FText::Format(LOCTEXT("DisplayTagSuffixFmt", "{0} {1}"), DisplayValue, AttributeMetaData.Suffix);
			}
		}
	
		if (!DisplayValue.IsEmpty())
		{
			CachedDisplayTags.Add(FTagDisplayItem(AttributeName, AttributeMetaData.DisplayName, DisplayValue, AttributeMetaData.bIsImportant));
		}
	}
}

bool SInputAssetTileItem::IsFolder() const
{
	return AssetItem && AssetItem->IsFolder();
}

FText SInputAssetTileItem::GetNameText() const
{
	if (AssetOverrideLabelText.IsSet())
	{
		return AssetOverrideLabelText.Get();
	}
	return AssetItem
		? AssetItem->GetDisplayName()
		: FText();
}

FSlateColor SInputAssetTileItem::GetAssetColor() const
{
	if (AssetItem)
	{
		FContentBrowserItemDataAttributeValue ColorAttributeValue = AssetItem->GetItemAttribute(ContentBrowserItemAttributes::ItemColor);
		if (ColorAttributeValue.IsValid())
		{
			const FString ColorStr = ColorAttributeValue.GetValue<FString>();

			FLinearColor Color;
			if (Color.InitFromString(ColorStr))
			{
				return Color;
			}
		}

		if(!AssetItem->IsSupported())
		{
			return FSlateColor::UseForeground();
		}
	}

	// The default tint the folder should appear as
	static const FName FolderColorName("ContentBrowser.DefaultFolderColor");
	return FAppStyle::Get().GetSlateColor(FolderColorName).GetSpecifiedColor();
}

bool SInputAssetTileItem::OnVisualizeTooltip(const TSharedPtr<SWidget>& TooltipContent)
{
	// OnVisualizeTooltip will be called when tooltips are opening for any children of SAssetColumnViewRow
	// So we only want custom visualization for the parent row's SInputAssetTileItemToolTip
	TSharedPtr<IToolTip> ThisTooltip = GetToolTip();
	if (!ThisTooltip.IsValid() || (ThisTooltip->AsWidget() != TooltipContent))
	{
		return false;
	}

	if(OnVisualizeAssetToolTip.IsBound() && TooltipContent.IsValid() && AssetItem && AssetItem->IsFile())
	{
		FAssetData ItemAssetData;
		if (AssetItem->Legacy_TryGetAssetData(ItemAssetData))
		{
			return OnVisualizeAssetToolTip.Execute(TooltipContent, ItemAssetData);
		}
	}

	// No custom behavior, return false to allow slate to visualize the widget
	return false;
}

void SInputAssetTileItem::OnToolTipClosing()
{
	OnAssetToolTipClosing.ExecuteIfBound();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
