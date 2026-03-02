// Copyright WorldBLD. All rights reserved.

#include "BlueprintAssetSelectorWidget.h"
#include "BlueprintAssetItemButtonWidget.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Misc/Paths.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"

void UBlueprintAssetSelectorWidget::ForceRescanAssetRegistry()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	UE_LOG(LogTemp, Log, TEXT("UBlueprintAssetSelectorWidget::ForceRescanAssetRegistry - Forcing Asset Registry scan..."));
	
	// Scan all game content paths
	TArray<FString> PathsToScan;
	AssetRegistry.GetAllCachedPaths(PathsToScan);
	
	if (PathsToScan.Num() > 0)
	{
		AssetRegistry.ScanPathsSynchronous(PathsToScan, true);
		UE_LOG(LogTemp, Log, TEXT("UBlueprintAssetSelectorWidget::ForceRescanAssetRegistry - Scanned %d paths"), PathsToScan.Num());
	}
	
	UE_LOG(LogTemp, Log, TEXT("UBlueprintAssetSelectorWidget::ForceRescanAssetRegistry - Scan complete!"));
}

void UBlueprintAssetSelectorWidget::LoadAndDisplayBlueprintAssets(const FString& SearchPath, bool bRecursive)
{
	if (!ItemContainer)
	{
		UE_LOG(LogTemp, Error, TEXT("UBlueprintAssetSelectorWidget::LoadAndDisplayBlueprintAssets - ItemContainer is not bound!"));
		return;
	}

	// Validate that BaseClassFilter is set
	if (!BaseClassFilter || !IsValid(BaseClassFilter))
	{
		UE_LOG(LogTemp, Error, TEXT("UBlueprintAssetSelectorWidget::LoadAndDisplayBlueprintAssets - BaseClassFilter is not set! Please set BaseClassFilter to the base class you want to filter by."));
		return;
	}

	// Clear existing widgets and classes
	ItemContainer->ClearChildren();
	ItemWidgets.Empty();
	LoadedAssetClasses.Empty();

	// Validate that we have an item widget class to instantiate
	if (!ItemWidgetClass || !IsValid(ItemWidgetClass))
	{
		UE_LOG(LogTemp, Error, TEXT("UBlueprintAssetSelectorWidget::LoadAndDisplayBlueprintAssets - ItemWidgetClass is not set!"));
		return;
	}

	// Get the Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Create filter for Blueprint assets
	FARFilter Filter;
	
	// If a search path is specified, use it; otherwise search everywhere
	if (!SearchPath.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*SearchPath));
	}
	
	// Filter by Blueprint class
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = bRecursive;
	Filter.bRecursiveClasses = false; // We only want Blueprints, not subclasses of Blueprint class

	// Get all Blueprint assets matching the filter
	TArray<FAssetData> BlueprintAssetDataList;
	AssetRegistry.GetAssets(Filter, BlueprintAssetDataList);

	// Debug logging
	FString SearchLocation = SearchPath.IsEmpty() ? TEXT("entire project") : SearchPath;
	UE_LOG(LogTemp, Log, TEXT("UBlueprintAssetSelectorWidget::LoadAndDisplayBlueprintAssets - Searching for Blueprints of class '%s' in: %s"), 
		*BaseClassFilter->GetName(), 
		*SearchLocation);
	UE_LOG(LogTemp, Log, TEXT("UBlueprintAssetSelectorWidget::LoadAndDisplayBlueprintAssets - Found %d Blueprint asset(s)"), BlueprintAssetDataList.Num());

	// Filter and load each Blueprint asset
	int32 MatchingCount = 0;
	for (const FAssetData& AssetData : BlueprintAssetDataList)
	{
		// Get the Blueprint's generated class
		const FString GeneratedClassPath = AssetData.GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath);
		if (GeneratedClassPath.IsEmpty())
		{
			continue;
		}

		// Load the generated class
		FSoftClassPath ClassPath(GeneratedClassPath);
		UClass* BlueprintClass = ClassPath.TryLoadClass<UObject>();
		
		if (!BlueprintClass)
		{
			continue;
		}

		// Check if this Blueprint class inherits from our base class filter
		if (!BlueprintClass->IsChildOf(BaseClassFilter))
		{
			continue;
		}

		MatchingCount++;

		// Debug: Log each matching asset
		UE_LOG(LogTemp, Log, TEXT("  - Found matching Blueprint: %s (Class: %s, Path: %s)"), 
			*AssetData.AssetName.ToString(), 
			*BlueprintClass->GetName(),
			*AssetData.GetObjectPathString());

		// Store the loaded class
		LoadedAssetClasses.Add(BlueprintClass);

		// Create the item widget
		UBlueprintAssetItemButtonWidget* ItemWidget = CreateWidget<UBlueprintAssetItemButtonWidget>(this, ItemWidgetClass);
		
		if (ItemWidget)
		{
			// Store asset info in the widget
			ItemWidget->AssetName = AssetData.AssetName.ToString();
			ItemWidget->AssetPath = AssetData.GetObjectPathString();

			// Configure the item with the Blueprint class
			ItemWidget->SetItem(BlueprintClass);

			// Bind the click delegate
			ItemWidget->OnItemClicked.BindUObject(this, &UBlueprintAssetSelectorWidget::HandleItemClicked);

			// Add to the WrapBox
			UWrapBoxSlot* WrapBoxSlot = ItemContainer->AddChildToWrapBox(ItemWidget);
			
			if (WrapBoxSlot)
			{
				// Configure slot padding (matching base class defaults)
				WrapBoxSlot->SetPadding(FMargin(4.0f));
				WrapBoxSlot->SetHorizontalAlignment(HAlign_Center);
				WrapBoxSlot->SetVerticalAlignment(VAlign_Center);
			}

			// Keep track of created widgets
			ItemWidgets.Add(ItemWidget);
		}
	}

	// Log the final count
	UE_LOG(LogTemp, Log, TEXT("UBlueprintAssetSelectorWidget::LoadAndDisplayBlueprintAssets - Displayed %d Blueprint(s) matching class '%s'"), 
		MatchingCount, 
		*BaseClassFilter->GetName());

	// If nothing was found, provide helpful debug info
	if (MatchingCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBlueprintAssetSelectorWidget::LoadAndDisplayBlueprintAssets - No Blueprints found matching class '%s'"), 
			*BaseClassFilter->GetName());
		UE_LOG(LogTemp, Warning, TEXT("  Possible reasons:"));
		UE_LOG(LogTemp, Warning, TEXT("  1. No Blueprints of this class exist in the search path"));
		UE_LOG(LogTemp, Warning, TEXT("  2. Blueprints haven't been saved yet"));
		UE_LOG(LogTemp, Warning, TEXT("  3. Asset Registry might need to be rescanned - try calling ForceRescanAssetRegistry()"));
		UE_LOG(LogTemp, Warning, TEXT("  4. Search path might be incorrect (current: %s)"), *SearchLocation);
		
		// Try to find ANY Blueprints in the search path to help debug
		if (!SearchPath.IsEmpty())
		{
			FARFilter DebugFilter;
			DebugFilter.PackagePaths.Add(FName(*SearchPath));
			DebugFilter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
			DebugFilter.bRecursivePaths = bRecursive;
			
			TArray<FAssetData> AllBlueprints;
			AssetRegistry.GetAssets(DebugFilter, AllBlueprints);
			
			if (AllBlueprints.Num() > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("  Found %d total Blueprint(s) in search path (but none match the base class filter):"), AllBlueprints.Num());
				for (int32 i = 0; i < FMath::Min(AllBlueprints.Num(), 5); i++)
				{
					UE_LOG(LogTemp, Warning, TEXT("    - %s"), *AllBlueprints[i].AssetName.ToString());
				}
				if (AllBlueprints.Num() > 5)
				{
					UE_LOG(LogTemp, Warning, TEXT("    ... and %d more"), AllBlueprints.Num() - 5);
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("  No Blueprints found at all in path: %s"), *SearchPath);
			}
		}
	}
}

void UBlueprintAssetSelectorWidget::OnItemClicked_Implementation(UClass* ClickedItemClass)
{
	// Forward to the Blueprint asset specific event
	OnBlueprintAssetClicked(ClickedItemClass);
}

void UBlueprintAssetSelectorWidget::OnBlueprintAssetClicked_Implementation(UClass* ClickedAssetClass)
{
	// Default implementation does nothing
	// Override this in Blueprint or subclasses to handle Blueprint asset selection
	if (ClickedAssetClass)
	{
		UE_LOG(LogTemp, Log, TEXT("UBlueprintAssetSelectorWidget::OnBlueprintAssetClicked - Clicked: %s"), *ClickedAssetClass->GetName());
	}
}

