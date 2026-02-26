// Copyright WorldBLD LLC. All rights reserved.

#include "InputAssetPickerPanel.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"
#include "AssetThumbnail.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "IContentBrowserSingleton.h"

#include "InputAssetFilterUtils.h"
#include "WorldBLDLoadedKitsAssetFilter.h"
#include "WorldBLDEditorToolkit.h"

#include "Engine/Blueprint.h"

#define LOCTEXT_NAMESPACE "WorldBLDEditor"

///////////////////////////////////////////////////////////////////////////////////////////////////

void SInputAssetPickerPanel::Construct(const FArguments& InArgs)
{
	this->TileSize = InArgs._TileSize;
	this->PanelSize = InArgs._PanelSize;
	this->CollectionSets = InArgs._CollectionSets;
	this->OnSelectionChanged = InArgs._OnSelectionChanged;
	this->OnGetAssetLabel = InArgs._OnGetAssetLabel;
	this->RecentAssetsProvider = InArgs._RecentAssetsProvider;
	this->FilterSetup = InArgs._FilterSetup;
	this->AssetThumbnailLabel = InArgs._AssetThumbnailLabel;
	this->bForceShowEngineContent = InArgs._bForceShowEngineContent;
	this->bForceShowPluginContent = InArgs._bForceShowPluginContent;
	this->bShowRecentAssets = InArgs._bShowRecentAssets;
	this->bShowCollectionFilters = InArgs._bShowCollectionFilters;

	// Get the shared thumbnail pool
	ThumbnailPool = UThumbnailManager::Get().GetSharedThumbnailPool();
	ensure(ThumbnailPool.IsValid());

	// Create the content container
	ContentContainer = SNew(SBox);
	
	// Build the picker content
	RebuildPickerContent();

	// Set initial selection if provided
	if (InArgs._InitiallySelectedAsset.IsValid())
	{
		SetSelectedAsset(InArgs._InitiallySelectedAsset);
	}

	ChildSlot
	[
		ContentContainer.ToSharedRef()
	];
}

void SInputAssetPickerPanel::RebuildPickerContent()
{
	// Configure filter for asset picker
	FAssetPickerConfig Config;
	Config.SelectionMode = ESelectionMode::Single;
	WorldBLD::Editor::InputAssetFilterUtils::ApplyFilterSetupToAssetPickerConfig(Config, FilterSetup);

	if (FilterSetup.bRestrictToAllowedPackageNames && !FilterSetup.bPresentSpecificAssets)
	{
		const UWorldBLDEdMode* EdMode = UWorldBLDEdMode::GetWorldBLDEdMode();
		const bool bHasAnyLoadedKits = IsValid(EdMode) && (EdMode->GetLoadedWorldBLDKitCount() > 0);
		Config.AssetShowWarningText = bHasAnyLoadedKits ?
			LOCTEXT("LoadedKitsOnlyNoMatchingAssets", "No matching assets were found in your Kits. Please load another, or disable Only Show Kit Assets.") :
			LOCTEXT("LoadedKitsOnlyNoKitsInLevel", "There are no Kits in your level. Please load in a new Kit.");
	}

	Config.InitialAssetViewType = EAssetViewType::Tile;
	Config.bFocusSearchBoxWhenOpened = false; // Don't focus search since we're always visible
	Config.bAllowNullSelection = true;
	Config.bAllowDragging = false;
	Config.ThumbnailLabel = AssetThumbnailLabel;
	Config.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SInputAssetPickerPanel::NewAssetSelected);
	Config.bForceShowEngineContent = bForceShowEngineContent;
	Config.bForceShowPluginContent = bForceShowPluginContent;

	// Build asset picker UI
	TSharedRef<SInputAssetPicker> AssetPickerWidget = SAssignNew(AssetPicker, SInputAssetPicker)
		.OnGetBrowserItemText_Lambda([this](const FAssetData& AssetItem) {
			if (this->OnGetAssetLabel.IsBound())
			{
				int32 Idx;
				this->FilterSetup.SpecificAssets.Find(AssetItem, Idx);
				return this->OnGetAssetLabel.Execute(AssetItem, Idx);
			}
			else
			{
				return FText::FromName(AssetItem.AssetName);
			}
		})
		.IsEnabled(true)
		.AssetPickerConfig(Config);

	TSharedRef<SVerticalBox> PanelContent = SNew(SVerticalBox);

	int32 FilterButtonBarVertPadding = 2;
	
	// Add recent assets section if enabled and provider is valid
	if (bShowRecentAssets)
	{
		UpdateRecentAssets();
		if (RecentAssetsProvider.IsValid() && RecentAssetData.Num() > 0)
		{
			TSharedRef<SListView<TSharedPtr<FRecentAssetInfo>>> RecentsListView = SNew(SListView<TSharedPtr<FRecentAssetInfo>>)
				.Orientation(Orient_Horizontal)
				.ListItemsSource(&RecentAssetData)
				.OnGenerateRow(this, &SInputAssetPickerPanel::OnGenerateWidgetForRecentList)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FRecentAssetInfo> SelectedItem, ESelectInfo::Type SelectInfo)
				{
					if (SelectedItem.IsValid())
					{
						NewAssetSelected(SelectedItem->AssetData);
					}
				})
				.ClearSelectionOnClick(false)
				.SelectionMode(ESelectionMode::Single);

			PanelContent->AddSlot()
			.AutoHeight()
			[
				SNew(SBox)
				.Padding(6.0f)
				.HeightOverride(TileSize.Y + 30.0f)
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
	}
	else
	{
		FilterButtonBarVertPadding = 10;
	}

	// Add collection filter buttons if enabled and sets are provided
	if (bShowCollectionFilters && CollectionSets.Num() > 0)
	{
		PanelContent->AddSlot()
		.AutoHeight()
		.Padding(10, FilterButtonBarVertPadding, 4, 2)
		[
			MakeCollectionSetsButtonPanel(AssetPickerWidget)
		];
	}

	// Add the main asset picker
	PanelContent->AddSlot()
	[
		SNew(SBox)
		.Padding(6.0f)
		.HeightOverride(PanelSize.Y)
		.WidthOverride(PanelSize.X)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Menu.Background"))
			[
				AssetPickerWidget
			]
		]
	];

	// Set the content
	ContentContainer->SetContent(PanelContent);
}

void SInputAssetPickerPanel::SetFilterSetup(const FAssetComboFilterSetup& InFilterSetup)
{
	FilterSetup = InFilterSetup;
	RebuildPickerContent();
}

void SInputAssetPickerPanel::SetSelectedAsset(const FAssetData& InAsset)
{
	SelectedAsset = InAsset;
	// Note: We don't fire OnSelectionChanged here since this is a programmatic set
}

TSharedRef<SWidget> SInputAssetPickerPanel::MakeCollectionSetsButtonPanel(TSharedRef<SInputAssetPicker> AssetPickerView)
{
	TArray<FNamedCollectionList> ShowCollectionSets = CollectionSets;
	ShowCollectionSets.Insert(FNamedCollectionList{ FString(), TArray<FCollectionNameType>() }, 0);

	ActiveCollectionSetIndex = 0;

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
		
	for (int32 Index = 0; Index < ShowCollectionSets.Num(); ++Index)
	{
		HorizontalBox->AddSlot()
		.AutoWidth()
		.Padding(4, 0)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "DetailsView.SectionButton")
			.HAlign(HAlign_Center)
			.OnCheckStateChanged_Lambda([this, Index, AssetPickerView](ECheckBoxState NewState)
			{
				if (NewState == ECheckBoxState::Checked)
				{
					AssetPickerView->UpdateAssetSourceCollections((Index == 0) ? 
						TArray<FCollectionNameType>() : this->CollectionSets[Index-1].CollectionNames);
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
					.Text((Index == 0) ? LOCTEXT("AllFilterLabel", "Show All") : FText::FromString(CollectionSets[Index-1].Name))
				]
			]
		];
	}

	return HorizontalBox;
}

void SInputAssetPickerPanel::UpdateRecentAssets()
{
	if (!RecentAssetsProvider.IsValid())
	{
		return;
	}
	
	TArray<FAssetData> RecentAssets = RecentAssetsProvider->GetRecentAssetsList();
	int32 NumRecent = RecentAssets.Num();

	while (RecentAssetData.Num() < NumRecent)
	{
		FAssetThumbnailConfig ThumbnailConfig;
		ThumbnailConfig.bAllowRealTimeOnHovered = false;

		TSharedPtr<FRecentAssetInfo> NewInfo = MakeShared<FRecentAssetInfo>();
		NewInfo->Index = RecentAssetData.Num();
		RecentAssetData.Add(NewInfo);

		RecentThumbnails.Add(MakeShared<FAssetThumbnail>(FAssetData(), TileSize.X, TileSize.Y, ThumbnailPool));
		RecentThumbnailWidgets.Add(SNew(SBox)
			.WidthOverride(TileSize.X)
			.HeightOverride(TileSize.Y)
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

TSharedRef<ITableRow> SInputAssetPickerPanel::OnGenerateWidgetForRecentList(TSharedPtr<FRecentAssetInfo> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FRecentAssetInfo>>, OwnerTable)
		.Padding(2.0f)
		[
			RecentThumbnailWidgets[InItem->Index]->AsShared()
		];
}

void SInputAssetPickerPanel::NewAssetSelected(const FAssetData& AssetData)
{
	SelectedAsset = AssetData;

	if (RecentAssetsProvider.IsValid())
	{
		RecentAssetsProvider->NotifyNewAsset(AssetData);
	}

	OnSelectionChanged.ExecuteIfBound(AssetData);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

UInputAssetPickerPanel::UInputAssetPickerPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TileSize = FVector2D(85, 85);
	PanelSize = FVector2D(600, 400);
	AssetClass = UObject::StaticClass();
}

TSharedRef<SWidget> UInputAssetPickerPanel::RebuildWidget()
{
	BindToWorldBLDEdMode();

	FAssetComboFilterSetup PickerFilterSetup;
	if (bPresentSpecificObjects)
	{
		PickerFilterSetup.bPresentSpecificAssets = true;
		for (UObject* Object : SpecificObjects)
		{
			if (IsValid(Object))
			{
				PickerFilterSetup.SpecificAssets.Add(FAssetData(Object, /* Allow Blueprint*/ true));
			}
		}
	}
	else
	{
		PickerFilterSetup.AssetClassType = IsValid(AssetClass) ? AssetClass.Get() : UObject::StaticClass();
		if (bLoadedKitsOnly)
		{
			PickerFilterSetup.bRestrictToAllowedPackageNames = true;
			PickerFilterSetup.AllowedPackageNames = WorldBLD::Editor::LoadedKitsAssetFilter::GetAllowedPackageNamesFromLoadedKits();
		}
	}

	MyPicker = SNew(SInputAssetPickerPanel)
		.TileSize(TileSize)
		.PanelSize(PanelSize)
		.FilterSetup(PickerFilterSetup)
		.OnSelectionChanged(SInputAssetPickerPanel::FOnSelectedAssetChanged::CreateUObject(this, &UInputAssetPickerPanel::HandleSelectionChanged))
		.OnGetAssetLabel_Lambda([this](const FAssetData& AssetData, int32 Idx) {
			if (OnGenerateLabelForAsset.IsBound() && this->SpecificObjects.IsValidIndex(Idx))
			{
				return OnGenerateLabelForAsset.Execute(this, this->SpecificObjects[Idx], Idx);
			}
			else
			{
				UObject* Asset = AssetData.GetAsset();
				return Asset ? FText::FromName(Asset->GetFName()) : FText::GetEmpty();
			}
		})
		.AssetThumbnailLabel(EThumbnailLabel::NoLabel);

	return MyPicker.ToSharedRef();
}

void UInputAssetPickerPanel::SynchronizeProperties()
{
	Super::SynchronizeProperties();
}

void UInputAssetPickerPanel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	UnbindFromWorldBLDEdMode();
	MyPicker.Reset();
}

void UInputAssetPickerPanel::SetSpecificObjects(const TArray<UObject*>& Objects)
{
	bPresentSpecificObjects = true;
	SpecificObjects = Objects;
	UpdateInterfaceState();
}

void UInputAssetPickerPanel::HandleSelectionChanged(const FAssetData& Selection)
{
	if (MyPicker.IsValid())
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

		if (MyPicker->GetFilterSetup().SpecificAssets.Find(Selection, Idx) && SpecificObjects.IsValidIndex(Idx))
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

void UInputAssetPickerPanel::AddOption(UObject* Option)
{
	bPresentSpecificObjects = true;
	SpecificObjects.Add(Option);
	UpdateInterfaceState();
}

bool UInputAssetPickerPanel::RemoveOption(UObject* Option)
{
	int32 RemIdx = SpecificObjects.Remove(Option);
	UpdateInterfaceState();
	return RemIdx != INDEX_NONE;
}

int32 UInputAssetPickerPanel::FindOptionIndex(UObject* Option) const
{
	return IsValid(Option) ? SpecificObjects.Find(Option) + 1 : 0;
}

UObject* UInputAssetPickerPanel::GetOptionAtIndex(int32 Index) const
{
	return ((Index == 0) || !SpecificObjects.IsValidIndex(Index - 1)) ? nullptr : SpecificObjects[Index - 1];
}

void UInputAssetPickerPanel::ClearOptions()
{
	SpecificObjects.Empty();
	ClearSelection();
}

void UInputAssetPickerPanel::ClearSelection()
{
	SelectedObject = nullptr;
	UpdateInterfaceState();
}

void UInputAssetPickerPanel::SetSelectedOption(UObject* Option)
{
	SelectedObject = GetOptionAtIndex(FindOptionIndex(Option));
	UpdateInterfaceState();
}

void UInputAssetPickerPanel::SetSelectedIndex(const int32 Index)
{
	SelectedObject = GetOptionAtIndex(Index);
	UpdateInterfaceState();
}

UObject* UInputAssetPickerPanel::GetSelectedOption() const
{
	return SelectedObject;
}

int32 UInputAssetPickerPanel::GetSelectedIndex() const
{
	return IsValid(SelectedObject) ? (SpecificObjects.Find(SelectedObject) + 1) : 0;
}

int32 UInputAssetPickerPanel::GetOptionCount() const
{
	return SpecificObjects.Num() + 1;
}

void UInputAssetPickerPanel::RefreshPicker()
{
	UpdateInterfaceState();
}

void UInputAssetPickerPanel::HandleWorldBLDKitsChanged(UWorldBLDEdMode* InEdMode)
{
	if (!bPresentSpecificObjects && bLoadedKitsOnly)
	{
		RefreshPicker();
	}
}

void UInputAssetPickerPanel::HandleWorldBLDKitBundlesChanged(UWorldBLDEdMode* InEdMode)
{
	if (!bPresentSpecificObjects && bLoadedKitsOnly)
	{
		RefreshPicker();
	}
}

void UInputAssetPickerPanel::BindToWorldBLDEdMode()
{
	UnbindFromWorldBLDEdMode();

	UWorldBLDEdMode* EdMode = UWorldBLDEdMode::GetWorldBLDEdMode();
	if (!IsValid(EdMode))
	{
		return;
	}

	BoundEdMode = EdMode;
	EdMode->OnWorldBLDKitListChanged.AddUniqueDynamic(this, &UInputAssetPickerPanel::HandleWorldBLDKitsChanged);
	EdMode->OnWorldBLDKitBundleListChanged.AddUniqueDynamic(this, &UInputAssetPickerPanel::HandleWorldBLDKitBundlesChanged);
}

void UInputAssetPickerPanel::UnbindFromWorldBLDEdMode()
{
	if (UWorldBLDEdMode* EdMode = BoundEdMode.Get())
	{
		EdMode->OnWorldBLDKitListChanged.RemoveDynamic(this, &UInputAssetPickerPanel::HandleWorldBLDKitsChanged);
		EdMode->OnWorldBLDKitBundleListChanged.RemoveDynamic(this, &UInputAssetPickerPanel::HandleWorldBLDKitBundlesChanged);
	}
	BoundEdMode.Reset();
}

void UInputAssetPickerPanel::UpdateInterfaceState()
{
	if (!MyPicker.IsValid())
	{
		return;
	}

	// If the filter setup changed, forward that to the widget.
	FAssetComboFilterSetup PickerFilterSetup;
	if (bPresentSpecificObjects)
	{
		PickerFilterSetup.bPresentSpecificAssets = true;
		for (UObject* Object : SpecificObjects)
		{
			if (IsValid(Object))
			{
				PickerFilterSetup.SpecificAssets.Add(FAssetData(Object, /* Allow Blueprint*/ true));
			}
		}
	}
	else
	{
		PickerFilterSetup.AssetClassType = IsValid(AssetClass) ? AssetClass.Get() : UObject::StaticClass();
		if (bLoadedKitsOnly)
		{
			PickerFilterSetup.bRestrictToAllowedPackageNames = true;
			PickerFilterSetup.AllowedPackageNames = WorldBLD::Editor::LoadedKitsAssetFilter::GetAllowedPackageNamesFromLoadedKits();
		}
	}

	MyPicker->SetFilterSetup(PickerFilterSetup);
	
	if (IsValid(SelectedObject))
	{
		MyPicker->SetSelectedAsset(FAssetData(SelectedObject, /* Allow Blueprint*/ true));
	}
	
	MyPicker->Invalidate(EInvalidateWidgetReason::Layout);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

