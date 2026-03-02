# Blueprint Asset Selector Widget - Implementation Summary

## Overview
This implementation provides a specialized widget system for displaying and selecting Blueprint assets of a user-specified base class. It extends the generic item selector widget system to work with any Blueprint type, using the Asset Registry to find assets and displaying them with their asset thumbnails and names.

## Created Files

### 1. UBlueprintAssetItemButtonWidget
**Header**: `Public/BlueprintAssetItemButtonWidget.h`  
**Source**: `Private/BlueprintAssetItemButtonWidget.cpp`

Individual widget representing a single clickable Blueprint asset with its thumbnail.

**Key Features**:
- Inherits from `UWorldBLDItemButtonWidget`
- Stores asset name and path for each Blueprint
- Overrides `GetThumbnailFromClass()` to store asset info (returns nullptr for thumbnail by default)
- Overrides `GetTooltipTextFromClass()` to extract asset name from the Asset Registry
- Works with any Blueprint class type
- Can be extended in Blueprint to provide custom thumbnail logic

**Properties**:
```cpp
FString AssetName;  // The asset name for display
FString AssetPath;  // The asset path for thumbnail lookup
```

### 2. UBlueprintAssetSelectorWidget
**Header**: `Public/BlueprintAssetSelectorWidget.h`  
**Source**: `Private/BlueprintAssetSelectorWidget.cpp`

Container widget that automatically loads and displays all Blueprint assets of a specified base class.

**Key Features**:
- Inherits from `UWorldBLDItemSelectorWidget`
- User-configurable base class filter (set in Blueprint)
- Automatically discovers Blueprint assets using the Asset Registry
- Can search entire project or specific path
- Supports recursive subdirectory searching
- Blueprint native event `OnBlueprintAssetClicked` provides the clicked Blueprint class

**Properties**:
```cpp
TSubclassOf<UObject> BaseClassFilter;  // Set this to filter Blueprint assets
```

**API**:
```cpp
// Load and display Blueprint assets matching BaseClassFilter
void LoadAndDisplayBlueprintAssets(const FString& SearchPath = "", bool bRecursive = true);

// Force rescan of Asset Registry if assets aren't found
static void ForceRescanAssetRegistry();

// Blueprint native event (override in Blueprint or C++)
void OnBlueprintAssetClicked(UClass* ClickedAssetClass);
```

## Usage Guide

### Setting Up in Blueprint

#### Step 1: Create Item Button Widget Blueprint
1. Create a Blueprint subclass of `UBlueprintAssetItemButtonWidget`
2. In the UMG Designer, create the widget hierarchy:
   - Add a `Button` named **"ItemButton"** (exact name required - must match BindWidget)
3. Configure the button's appearance (style, colors, size, etc.)
4. No need to override virtual methods - they're already implemented in C++

#### Step 2: Create Selector Widget Blueprint
1. Create a Blueprint subclass of `UBlueprintAssetSelectorWidget`
2. In the UMG Designer, create the widget hierarchy:
   - Add a `ScrollBox` named **"ItemScrollBox"** (exact name required - must match BindWidget)
   - Inside the ScrollBox, add a `WrapBox` named **"ItemContainer"** (exact name required - must match BindWidget)
3. In the Details panel:
   - Set `ItemWidgetClass` to your item button widget Blueprint created in Step 1
   - Set `BaseClassFilter` to the base class you want to filter by (e.g., Actor, Pawn, UserWidget, etc.)
4. Override the `OnBlueprintAssetClicked` event to handle selection:
   ```
   Event OnBlueprintAssetClicked
   ├─ Input: Clicked Asset Class (UClass)
   └─ Your logic here (e.g., spawn the actor, open the Blueprint, etc.)
   ```

#### Step 3: Load and Display the Blueprint Assets
Call `LoadAndDisplayBlueprintAssets()` after the widget is constructed (e.g., in the `Construct` event):

**Option A: Search entire project**
```
Event Construct
└─ LoadAndDisplayBlueprintAssets (leave SearchPath empty)
```

**Option B: Search specific path**
```
Event Construct
└─ LoadAndDisplayBlueprintAssets
    ├─ SearchPath: "/Game/Blueprints/Characters"
    └─ bRecursive: true
```

## Example Use Cases

### Example 1: Actor Blueprint Selector
Display all Blueprint Actors in a specific folder:

1. Set `BaseClassFilter` = `Actor`
2. Call `LoadAndDisplayBlueprintAssets("/Game/Characters", true)`
3. In `OnBlueprintAssetClicked`, spawn the clicked actor:
   ```blueprint
   Event OnBlueprintAssetClicked
   ├─ Clicked Asset Class -> Spawn Actor from Class
   └─ Location: Player location
   ```

### Example 2: Widget Blueprint Selector
Display all UMG widgets in the project:

1. Set `BaseClassFilter` = `UserWidget`
2. Call `LoadAndDisplayBlueprintAssets("", true)` (empty path = entire project)
3. In `OnBlueprintAssetClicked`, create and display the widget:
   ```blueprint
   Event OnBlueprintAssetClicked
   ├─ Clicked Asset Class -> Create Widget
   ├─ Add to Viewport
   └─ Set as focus
   ```

### Example 3: Custom Component Selector
Display all custom components inheriting from a base class:

1. Create a base class in C++: `UMyCustomComponent`
2. Set `BaseClassFilter` = `MyCustomComponent`
3. Call `LoadAndDisplayBlueprintAssets("", true)`
4. In `OnBlueprintAssetClicked`, add the component to an actor

## Technical Details

### Asset Discovery
The widget uses the Unreal Engine Asset Registry to find Blueprint assets:
- Searches for assets of type `UBlueprint`
- Filters by the `GeneratedClassPath` tag to find Blueprints of the specified base class
- Only includes Blueprints that inherit from `BaseClassFilter`
- Supports recursive subdirectory searching

### Thumbnail Extraction
**Current Implementation**: The widget uses asset names for display instead of thumbnails. Extracting asset thumbnails to UTexture2D for UMG widgets is complex and requires additional rendering infrastructure.

**How it works**:
1. Get asset data from Asset Registry
2. Extract asset name and path
3. Return `nullptr` for thumbnail (base widget displays using class icon)
4. Asset name is used as the primary identifier and tooltip

**For custom thumbnails**, users can:
- Override `GetThumbnailFromClass_Implementation` in Blueprint
- Store custom thumbnail textures as properties on their Blueprint classes
- Use a pre-generated thumbnail texture asset
- Implement a custom thumbnail generation system similar to `RoadPresetThumbnailGenerator`

### Performance Considerations
- Asset Registry queries are fast (indexed)
- Thumbnail loading may take time for many assets
- Thumbnails are loaded on-demand when the widget is created
- Consider limiting search paths for large projects

## Troubleshooting

### Issue: "BaseClassFilter is not set" error
**Solution:** Ensure you set the `BaseClassFilter` property in the Blueprint Details panel to the base class you want to filter by

### Issue: No Blueprint assets are displayed
**Possible causes:**
1. No Blueprints of the specified class exist in the search path
2. Asset Registry hasn't indexed the assets yet
3. Search path is incorrect

**Solutions:**
- Call `ForceRescanAssetRegistry()` before `LoadAndDisplayBlueprintAssets()`
- Verify the search path is correct (use package path format like "/Game/Blueprints")
- Check the Output Log for detailed debug information

### Issue: Thumbnails not showing
**Current behavior:** Thumbnails are not extracted by default. The widget displays asset names instead.

**To add custom thumbnails:**
1. Override `GetThumbnailFromClass` in your Blueprint button widget
2. Add a UTexture2D property to your Blueprint class and set it manually
3. Implement a thumbnail generation system (see `RoadPresetThumbnailGenerator` for reference)
4. The base widget will use the class icon if no thumbnail is provided

### Issue: "ItemContainer is not bound" error
**Solution:** Ensure you have:
- A `ScrollBox` named "ItemScrollBox" in your widget
- A `WrapBox` named "ItemContainer" inside the ScrollBox
- Both widgets are properly added in the UMG Designer

### Issue: Wrong Blueprints are showing
**Solution:** Verify your `BaseClassFilter` is set to the correct class. The widget shows all Blueprints that inherit from this class (directly or indirectly)

## Differences from UtilityBundleSelectorWidget

This widget differs from `UUtilityBundleSelectorWidget` in several key ways:

| Feature | UBlueprintAssetSelectorWidget | UUtilityBundleSelectorWidget |
|---------|-------------------------------|------------------------------|
| Asset Type | Blueprint classes (any type) | DataAsset instances (UWorldBLDUtilityBundle) |
| Base Class | User-configurable in Blueprint | Fixed (UWorldBLDUtilityBundle) |
| Thumbnail Source | None (class icon fallback) | Property on DataAsset (ToolImage) |
| Name Source | Asset name from Asset Registry | Property on DataAsset (UtilityName) |
| Search Flexibility | Entire project or custom path | Fixed folder with fallback paths |
| Use Case | Generic Blueprint selection | Specific utility bundle selection |
| Custom Thumbnails | Can be added via Blueprint override | Built-in via ToolImage property |

## Integration with Other Systems

### With Editor Utility Widgets
This selector can be used in Editor Utility Widgets to create custom asset browser panels:

```blueprint
Editor Utility Widget
├─ Blueprint Asset Selector (BaseClassFilter: Actor)
└─ Details Panel to show selected asset properties
```

### With Runtime Widget Systems
While designed for editor use, this can potentially work in runtime if:
- Asset Registry is available
- The Blueprints are loaded/referenced in the build

### With Custom Content Browser
Use this as a filtered content browser for specific Blueprint types:

```blueprint
Custom Content Browser
├─ Tab 1: Character Blueprints (BaseClassFilter: ACharacter)
├─ Tab 2: Weapon Blueprints (BaseClassFilter: AWeaponBase)
└─ Tab 3: UI Widgets (BaseClassFilter: UUserWidget)
```

## Blueprint Event Flow

```
Widget Construct
    ↓
Set BaseClassFilter (in Blueprint)
    ↓
Call LoadAndDisplayBlueprintAssets()
    ↓
[Asset Registry Search]
    ↓
[Filter by BaseClassFilter]
    ↓
[Create Item Widgets]
    ↓
[Load Thumbnails]
    ↓
[Display in WrapBox]
    ↓
User Clicks Item
    ↓
OnBlueprintAssetClicked Event
    ↓
[Your Custom Logic]
```

## Future Enhancements

Possible future improvements:
1. Async thumbnail loading for better performance
2. Search/filter functionality by asset name
3. Sorting options (by name, date, etc.)
4. Category/folder grouping
5. Drag-and-drop support
6. Context menu for asset operations
7. Preview panel showing larger thumbnail and asset details
8. Favorites/recent items tracking

## See Also

- `WorldBLDItemSelectorWidget.h` - Base class documentation
- `WorldBLDItemButtonWidget.h` - Base button widget documentation
- `UtilityBundleSelectorWidget.h` - Similar widget for DataAssets
- `GENERIC_ITEM_WIDGETS_README.md` - Generic widget system overview

