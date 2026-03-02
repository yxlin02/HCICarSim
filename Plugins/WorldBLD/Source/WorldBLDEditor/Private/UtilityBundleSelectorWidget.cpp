// Copyright WorldBLD. All rights reserved.

#include "UtilityBundleSelectorWidget.h"
#include "UtilityBundleItemButtonWidget.h"
#include "WorldBLDEditController.h"
#include "WorldBLDEditorToolkit.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/Widget.h"
#include "Editor.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"

void UUtilityBundleSelectorWidget::ForceRescanAssetRegistry()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	UE_LOG(LogTemp, Log, TEXT("UUtilityBundleSelectorWidget::ForceRescanAssetRegistry - Forcing Asset Registry to scan plugin paths..."));
	
	// Force a scan of all plugin content directories
	TArray<TSharedRef<IPlugin>> Plugins = IPluginManager::Get().GetEnabledPlugins();
	for (const TSharedRef<IPlugin>& Plugin : Plugins)
	{
		FString PluginContentDir = Plugin->GetContentDir();
		if (!PluginContentDir.IsEmpty() && FPaths::DirectoryExists(PluginContentDir))
		{
			UE_LOG(LogTemp, Log, TEXT("  Scanning plugin: %s (Path: %s)"), *Plugin->GetName(), *PluginContentDir);
			TArray<FString> PathsToScan;
			PathsToScan.Add(PluginContentDir);
			AssetRegistry.ScanPathsSynchronous(PathsToScan, true);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("UUtilityBundleSelectorWidget::ForceRescanAssetRegistry - Scan complete!"));
}

void UUtilityBundleSelectorWidget::LoadAndDisplayUtilityBundles(const FString& SearchPath, bool bTryCommonPaths)
{
	if (!ItemContainer)
	{
		UE_LOG(LogTemp, Error, TEXT("UUtilityBundleSelectorWidget::LoadAndDisplayUtilityBundles - ItemContainer is not bound!"));
		return;
	}

	// Clear existing widgets and bundles
	ItemContainer->ClearChildren();
	ItemWidgets.Empty();
	LoadedBundles.Empty();

	// Validate that we have an item widget class to instantiate
	if (!ItemWidgetClass || !IsValid(ItemWidgetClass))
	{
		UE_LOG(LogTemp, Error, TEXT("UUtilityBundleSelectorWidget::LoadAndDisplayUtilityBundles - ItemWidgetClass is not set!"));
		return;
	}

	// Ensure the ItemWidgetClass is a subclass of UUtilityBundleItemButtonWidget
	if (!ItemWidgetClass->IsChildOf(UUtilityBundleItemButtonWidget::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("UUtilityBundleSelectorWidget::LoadAndDisplayUtilityBundles - ItemWidgetClass must be a subclass of UUtilityBundleItemButtonWidget!"));
		return;
	}

	// Get the Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Define the path to search in (package path format)
	// Use provided path, or default to /WorldBLD/Utilities/Data
	const FString ActualSearchPath = SearchPath.IsEmpty() ? TEXT("/WorldBLD/Utilities/Data") : SearchPath;

	// Create filter for UWorldBLDUtilityBundle assets and Blueprint subclasses
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*ActualSearchPath));
	Filter.ClassPaths.Add(UWorldBLDUtilityBundle::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;     // Search recursively in subdirectories
	Filter.bRecursiveClasses = true;   // Include Blueprint subclasses of UWorldBLDUtilityBundle

	// Get all assets matching the filter
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	// Debug logging to help diagnose issues
	UE_LOG(LogTemp, Log, TEXT("UUtilityBundleSelectorWidget::LoadAndDisplayUtilityBundles - Searching in path: %s"), *ActualSearchPath);
	UE_LOG(LogTemp, Log, TEXT("UUtilityBundleSelectorWidget::LoadAndDisplayUtilityBundles - Asset Registry returned %d asset(s)"), AssetDataList.Num());

	// Load and create widgets for each asset
	for (const FAssetData& AssetData : AssetDataList)
	{
		// Debug: Log each asset found
		UE_LOG(LogTemp, Log, TEXT("  - Found asset: %s (Class: %s, Path: %s)"), 
			*AssetData.AssetName.ToString(), 
			*AssetData.AssetClassPath.ToString(), 
			*AssetData.GetObjectPathString());

		// Load the asset instance
		UWorldBLDUtilityBundle* BundleAsset = Cast<UWorldBLDUtilityBundle>(AssetData.GetAsset());
		if (!BundleAsset)
		{
			UE_LOG(LogTemp, Warning, TEXT("  - Failed to cast asset %s to UWorldBLDUtilityBundle"), *AssetData.AssetName.ToString());
			continue;
		}

		// Store the loaded bundle
		LoadedBundles.Add(BundleAsset);

		// Create the item widget
		UUtilityBundleItemButtonWidget* ItemWidget = CreateWidget<UUtilityBundleItemButtonWidget>(this, ItemWidgetClass);
		
		if (ItemWidget)
		{
			// Configure the item with the bundle asset instance
			ItemWidget->SetUtilityBundle(BundleAsset);

			// Bind the click delegate
			ItemWidget->OnItemClicked.BindUObject(this, &UUtilityBundleSelectorWidget::HandleItemClicked);

			// Add to the WrapBox
			UWrapBoxSlot* WrapBoxSlot = ItemContainer->AddChildToWrapBox(ItemWidget);
			
			if (WrapBoxSlot)
			{
				// Configure slot padding (hard-coded defaults matching base class)
				WrapBoxSlot->SetPadding(FMargin(4.0f));
				WrapBoxSlot->SetHorizontalAlignment(HAlign_Center);
				WrapBoxSlot->SetVerticalAlignment(VAlign_Center);
			}

			// Keep track of created widgets
			ItemWidgets.Add(ItemWidget);
		}
	}

	// Log the number of utility bundles found
	UE_LOG(LogTemp, Log, TEXT("UUtilityBundleSelectorWidget::LoadAndDisplayUtilityBundles - Found and displayed %d utility bundle(s) in %s"), 
		LoadedBundles.Num(), *ActualSearchPath);

	if (NoBundlesFoundText)
	{
		NoBundlesFoundText->SetVisibility(LoadedBundles.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	// If nothing was found, try common path variations or help debug
	if (LoadedBundles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UUtilityBundleSelectorWidget::LoadAndDisplayUtilityBundles - No utility bundles found in %s"), *ActualSearchPath);
		
		// If enabled, try common path variations automatically
		if (bTryCommonPaths && SearchPath.IsEmpty()) // Only try alternatives if user didn't specify a custom path
		{
			TArray<FString> CommonPaths = {
				TEXT("/Game/WorldBLD/Utilities/Data"),
				TEXT("/Plugins/WorldBLD/Utilities/Data"),
				TEXT("/WorldBLD/Content/Utilities/Data"),
				TEXT("/Game/Plugins/WorldBLD/Utilities/Data")
			};
			
			UE_LOG(LogTemp, Log, TEXT("  Trying common path variations..."));
			
			for (const FString& AlternatePath : CommonPaths)
			{
				FARFilter AltFilter;
				AltFilter.PackagePaths.Add(FName(*AlternatePath));
				AltFilter.ClassPaths.Add(UWorldBLDUtilityBundle::StaticClass()->GetClassPathName());
				AltFilter.bRecursivePaths = true;
				AltFilter.bRecursiveClasses = true;
				
				TArray<FAssetData> AltAssets;
				AssetRegistry.GetAssets(AltFilter, AltAssets);
				
				if (AltAssets.Num() > 0)
				{
					UE_LOG(LogTemp, Log, TEXT("  SUCCESS! Found %d asset(s) in alternate path: %s"), AltAssets.Num(), *AlternatePath);
					UE_LOG(LogTemp, Log, TEXT("  Retrying with this path..."));
					
					// Recursively call with the found path, but disable auto-retry to avoid infinite loop
					LoadAndDisplayUtilityBundles(AlternatePath, false);
					return;
				}
			}
			
			UE_LOG(LogTemp, Warning, TEXT("  No assets found in any common path variation."));
		}
		
		UE_LOG(LogTemp, Warning, TEXT("  Possible reasons:"));
		UE_LOG(LogTemp, Warning, TEXT("  1. Assets might not be saved/loaded yet"));
		UE_LOG(LogTemp, Warning, TEXT("  2. Asset Registry might need to be rescanned - try calling ForceRescanAssetRegistry()"));
		UE_LOG(LogTemp, Warning, TEXT("  3. Assets might have a different base class"));
		
		// Try to find ANY UWorldBLDUtilityBundle assets in the entire project to help debug
		FARFilter DebugFilter;
		DebugFilter.ClassPaths.Add(UWorldBLDUtilityBundle::StaticClass()->GetClassPathName());
		DebugFilter.bRecursiveClasses = true;
		DebugFilter.bRecursivePaths = true;
		
		TArray<FAssetData> AllBundles;
		AssetRegistry.GetAssets(DebugFilter, AllBundles);
		
		if (AllBundles.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Found %d UWorldBLDUtilityBundle asset(s) elsewhere in project:"), AllBundles.Num());
			for (const FAssetData& Bundle : AllBundles)
			{
				UE_LOG(LogTemp, Warning, TEXT("    - %s"), *Bundle.GetObjectPathString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  No UWorldBLDUtilityBundle assets found anywhere in the project!"));
			UE_LOG(LogTemp, Warning, TEXT("  Try:"));
			UE_LOG(LogTemp, Warning, TEXT("    1. Calling ForceRescanAssetRegistry() before LoadAndDisplayUtilityBundles()"));
			UE_LOG(LogTemp, Warning, TEXT("    2. Saving your utility bundle assets"));
			UE_LOG(LogTemp, Warning, TEXT("    3. Verifying they are actually UWorldBLDUtilityBundle data assets"));
		}
	}
}

void UUtilityBundleSelectorWidget::OnItemClicked_Implementation(UClass* ClickedItemClass)
{
	// Find the utility bundle asset associated with the clicked item
	// We need to iterate through the item widgets to find which one was clicked
	for (UWorldBLDItemButtonWidget* Widget : ItemWidgets)
	{
		if (Widget && Widget->ItemClass == ClickedItemClass)
		{
			// Cast to our specialized widget type to get the bundle asset
			UUtilityBundleItemButtonWidget* BundleWidget = Cast<UUtilityBundleItemButtonWidget>(Widget);
			if (BundleWidget && BundleWidget->UtilityBundleAsset)
			{
				// Forward to the utility bundle specific event with the asset instance
				OnUtilityBundleClicked(BundleWidget->UtilityBundleAsset);
				return;
			}
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("UUtilityBundleSelectorWidget::OnItemClicked - Could not find utility bundle asset for clicked item"));
}

void UUtilityBundleSelectorWidget::OnUtilityBundleClicked_Implementation(UWorldBLDUtilityBundle* ClickedBundle)
{
	if (!ClickedBundle)
	{
		return;
	}

	if (!ClickedBundle->PushWidgetToViewport)
	{
		return;
	}

	// Create the widget in the editor world (mirrors UWorldBLDEdMode::TryConstructUserWidget).
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	if (!IsValid(ClickedBundle->WidgetClass))
	{
		return;
	}

	UEditorUtilityWidget* UtilityWidget = CreateWidget<UEditorUtilityWidget>(World, ClickedBundle->WidgetClass);
	if (!IsValid(UtilityWidget))
	{
		return;
	}

	// Mark transient to prevent dirtying the editor world.
	UtilityWidget->SetFlags(RF_Transient);

	// Also mark nested utility widgets as transient (see UWorldBLDEdMode::TryConstructUserWidget).
	if (UtilityWidget->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		UtilityWidget->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->IsA(UEditorUtilityWidget::StaticClass()))
			{
				Widget->SetFlags(RF_Transient);
				if (Widget->Slot)
				{
					Widget->Slot->SetFlags(RF_Transient);
				}
			}
		}
	}

	// Utilities that are also WorldBLD tools should be pushed via the EdMode tool stack.
	if (UWorldBLDEditController* ToolController = Cast<UWorldBLDEditController>(UtilityWidget))
	{
		if (UWorldBLDEdMode* EdMode = UWorldBLDEdMode::GetWorldBLDEdMode())
		{
			EdMode->PushEditController(ToolController);
		}
		else
		{
			// Fallback for cases where the EdMode isn't active, but we still want to display the tool in-viewport.
			ToolController->AddToEditorLevelViewport();
			ToolController->ToolBegin();
		}
	}
}

