# Utility Bundle Selector Widget - Implementation Summary

## Overview
This implementation provides a specialized widget system for displaying and selecting UWorldBLDUtilityBundle data assets. It extends the generic item selector widget system to work with DataAsset instances rather than Blueprint subclasses.

## Changes Made

### 1. Added ToolImage Property to UWorldBLDUtilityBundle
**File**: `Public/WorldBLDEditController.h`

Added a new texture property to the `UWorldBLDUtilityBundle` class:
```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Utility")
UTexture2D* ToolImage;
```

This property serves as the thumbnail image source for each utility bundle in the selector widget.

### 2. Created UUtilityBundleItemButtonWidget
**Header**: `Public/UtilityBundleItemButtonWidget.h`  
**Source**: `Private/UtilityBundleItemButtonWidget.cpp`

Individual widget representing a single clickable utility bundle with its thumbnail.

**Key Features**:
- Inherits from `UWorldBLDItemButtonWidget`
- Stores reference to `UWorldBLDUtilityBundle* UtilityBundleAsset` (the actual DataAsset instance)
- Overrides `GetThumbnailFromClass()` to extract `ToolImage` from the bundle asset
- Overrides `GetTooltipTextFromClass()` to extract `UtilityName` from the bundle asset

**API**:
```cpp
void SetUtilityBundle(UWorldBLDUtilityBundle* InBundleAsset); // Configure widget with a bundle asset
```

**Important Design Note**: Since UWorldBLDUtilityBundle is a DataAsset (instances, not subclasses), this widget stores the actual asset instance and extracts data from it, rather than using the CDO pattern that works for Blueprint subclasses.

### 3. Created UUtilityBundleSelectorWidget
**Header**: `Public/UtilityBundleSelectorWidget.h`  
**Source**: `Private/UtilityBundleSelectorWidget.cpp`

Container widget that automatically loads and displays all utility bundle assets from `/WorldBLD/Utilities/Data/`.

**Key Features**:
- Inherits from `UWorldBLDItemSelectorWidget`
- Automatically discovers all `UWorldBLDUtilityBundle` assets using the Asset Registry
- Searches recursively in `/WorldBLD/Utilities/Data/` folder
- Stores loaded bundle instances in `LoadedBundles` array
- Blueprint native event `OnUtilityBundleClicked` provides the clicked bundle asset instance

**API**:
```cpp
// Load and display all utility bundles from /WorldBLD/Utilities/Data/
void LoadAndDisplayUtilityBundles();

// Blueprint native event (override in Blueprint or C++)
void OnUtilityBundleClicked(UWorldBLDUtilityBundle* ClickedBundle);
```

## Usage Guide

### Setting Up in Blueprint

#### Step 1: Create Item Button Widget Blueprint
1. Create a Blueprint subclass of `UUtilityBundleItemButtonWidget`
2. In the UMG Designer, create the widget hierarchy:
   - Add a `Button` named **"ItemButton"** (exact name required - must match BindWidget)
3. Configure the button's appearance (style, colors, etc.)
4. No need to override virtual methods - they're already implemented in C++

#### Step 2: Create Selector Widget Blueprint
1. Create a Blueprint subclass of `UUtilityBundleSelectorWidget`
2. In the UMG Designer, create the widget hierarchy:
   - Add a `ScrollBox` named **"ItemScrollBox"** (exact name required - must match BindWidget)
   - Inside the ScrollBox, add a `WrapBox` named **"ItemContainer"** (exact name required - must match BindWidget)
3. In the Details panel, set `ItemWidgetClass` to your item button widget Blueprint created in Step 1
4. Override the `OnUtilityBundleClicked` event to handle selection:
   ```
   Event OnUtilityBundleClicked
   ├─ Input: Clicked Bundle (UWorldBLDUtilityBundle)
   └─ Your logic here (e.g., open the utility, log the selection, etc.)
   ```

#### Step 3: Load and Display the Bundles
Call `LoadAndDisplayUtilityBundles()` after the widget is constructed (e.g., in the `Construct` event):
```
Event Construct
└─ LoadAndDisplayUtilityBundles
```

### Using in C++

```cpp
// Create the selector widget
UUtilityBundleSelectorWidget* Selector = CreateWidget<UUtilityBundleSelectorWidget>(GetWorld(), SelectorWidgetClass);

// Load and display all utility bundles
Selector->LoadAndDisplayUtilityBundles();

// Add to viewport
Selector->AddToViewport();
```

To handle clicks in C++, override `OnUtilityBundleClicked_Implementation`:
```cpp
void UMyCustomSelectorWidget::OnUtilityBundleClicked_Implementation(UWorldBLDUtilityBundle* ClickedBundle)
{
    if (ClickedBundle)
    {
        // Handle the clicked bundle
        UE_LOG(LogTemp, Log, TEXT("User selected: %s"), *ClickedBundle->UtilityName.ToString());
        
        // Example: Launch the utility widget
        if (ClickedBundle->WidgetClass)
        {
            UEditorUtilityWidget* UtilityWidget = CreateWidget<UEditorUtilityWidget>(GetWorld(), ClickedBundle->WidgetClass);
            if (UtilityWidget && ClickedBundle->PushWidgetToViewport)
            {
                UtilityWidget->AddToViewport();
            }
        }
    }
}
```

## Widget Hierarchy

```
UUtilityBundleSelectorWidget
└── ItemScrollBox (UScrollBox)
    └── ItemContainer (UWrapBox)
        ├── UUtilityBundleItemButtonWidget #1
        │   ├── UtilityBundleAsset (stores DA_UtilityBundle_1)
        │   └── ItemButton (UButton with ToolImage)
        ├── UUtilityBundleItemButtonWidget #2
        │   ├── UtilityBundleAsset (stores DA_UtilityBundle_2)
        │   └── ItemButton (UButton with ToolImage)
        └── ... (more bundles)
```

## Implementation Details

### DataAsset vs Blueprint Subclass Pattern

The generic item widget system (`UWorldBLDItemSelectorWidget` and `UWorldBLDItemButtonWidget`) was designed to work with Blueprint subclasses where each item is a different UClass with a CDO containing the data.

However, `UWorldBLDUtilityBundle` assets are DataAsset instances, not subclasses. To adapt the generic system for DataAssets:

1. **Item Button Widget** stores both:
   - `ItemClass` (UClass*) - For compatibility with base widget system
   - `UtilityBundleAsset` (UWorldBLDUtilityBundle*) - The actual asset instance with data

2. **Selector Widget**:
   - Loads asset instances using Asset Registry
   - Creates widgets and calls `SetUtilityBundle()` with the asset instance
   - Maintains `LoadedBundles` array to keep references to loaded assets
   - When clicked, retrieves the asset instance from the widget and passes it to the event

3. **Thumbnail/Tooltip Extraction**:
   - Instead of accessing CDO via `InClass->GetDefaultObject()`
   - Directly accesses the stored `UtilityBundleAsset` instance

### Asset Loading

The selector uses the Unreal Asset Registry to discover and load assets:
- **Search Path**: `/WorldBLD/Utilities/Data/` (package path format)
- **Recursive**: Yes, searches all subdirectories
- **Filter**: Only `UWorldBLDUtilityBundle` assets
- **Timing**: Assets are loaded synchronously when `LoadAndDisplayUtilityBundles()` is called

### Required Setup for Utility Bundle Assets

For utility bundles to display correctly in the selector:
1. Create `UWorldBLDUtilityBundle` DataAsset instances in `/WorldBLD/Utilities/Data/` (or subdirectories)
2. Set the `ToolImage` property to a texture (for thumbnail display)
3. Set the `UtilityName` property to a descriptive name (for tooltip)
4. Optionally set `WidgetClass`, `PushWidgetToViewport`, and `UtilityDescription`

### Hard-Coded Values
- **Image Size**: 128x128 pixels per item image (inherited from base class)
- **WrapBox Inner Padding**: 8x8 pixels (inherited from base class)
- **Item Slot Padding**: 4 pixels all sides (inherited from base class)

## Example: Complete Blueprint Setup

1. **WBP_UtilityBundleButton** (Blueprint subclass of `UUtilityBundleItemButtonWidget`):
   - Root: Button named "ItemButton"
   - Style: Configure appearance as desired

2. **WBP_UtilityBundleSelector** (Blueprint subclass of `UUtilityBundleSelectorWidget`):
   - Root: Canvas Panel or Overlay
   - Child: ScrollBox named "ItemScrollBox"
     - Child: WrapBox named "ItemContainer"
   - Details: Set `ItemWidgetClass` = WBP_UtilityBundleButton
   - Event Graph:
     ```
     Event Construct
     └─ LoadAndDisplayUtilityBundles
     
     Event OnUtilityBundleClicked (Clicked Bundle)
     └─ [Your logic: open utility, log selection, etc.]
     ```

## Notes

- All widget names ("ItemButton", "ItemScrollBox", "ItemContainer") must match exactly due to `BindWidget` meta specifier
- The selector automatically handles layout and scrolling
- Item widgets are created dynamically at runtime
- Assets are loaded when `LoadAndDisplayUtilityBundles()` is called, not at widget construction
- The `LoadedBundles` array keeps strong references to prevent garbage collection

