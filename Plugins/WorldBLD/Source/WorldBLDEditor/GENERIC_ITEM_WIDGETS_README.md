# Generic Item Selector Widgets - Implementation Summary

## Overview
Generic UMG widget system for displaying and selecting items in the WorldBLD plugin. This implementation provides base classes that can be subclassed to work with any item type by overriding virtual methods for thumbnail and tooltip extraction.

## Created Files

### 1. UWorldBLDItemButtonWidget
- **Header**: `Public/WorldBLDItemButtonWidget.h`
- **Source**: `Private/WorldBLDItemButtonWidget.cpp`

Individual widget representing a single clickable item with customizable thumbnail and tooltip extraction.

**Key Features**:
- Inherits from `UUserWidget`
- Contains `UButton* ItemButton` (BindWidget) for click handling
- Stores reference to `UClass* ItemClass` (generic class pointer)
- Hard-coded image size: 128x128 pixels
- Delegate `FOnItemClicked` to notify parent when clicked
- Uses virtual methods for customization

**Virtual Methods for Customization**:
```cpp
// Override to extract thumbnail from the target class
virtual UTexture2D* GetThumbnailFromClass(UClass* InClass);

// Override to extract tooltip text from the target class  
virtual FText GetTooltipTextFromClass(UClass* InClass);
```

**API**:
```cpp
void SetItem(UClass* InItemClass); // Configure widget with any class
```

### 2. UWorldBLDItemSelectorWidget
- **Header**: `Public/WorldBLDItemSelectorWidget.h`
- **Source**: `Private/WorldBLDItemSelectorWidget.cpp`

Container widget that manages a collection of item widgets in a scrollable WrapBox.

**Key Features**:
- Inherits from `UUserWidget`
- Contains `UScrollBox* ItemScrollBox` (BindWidget) for vertical scrolling
- Contains `UWrapBox* ItemContainer` (BindWidget) nested inside ScrollBox to hold item widgets
- Property `TSubclassOf<UWorldBLDItemButtonWidget> ItemWidgetClass` for item instantiation (EditAnywhere)
- Hard-coded WrapBox settings: 8px inner slot padding, 4px item padding
- Scrollbar visibility configured to show when needed
- Blueprint native event for item selection
- Built-in validation with `IsWidgetConfigured()` helper

**Virtual Method for Customization**:
```cpp
// Override to handle item clicks (BlueprintNativeEvent)
virtual void OnItemClicked(UClass* ClickedItemClass);
```

**API**:
```cpp
// C++ callable function to populate the gallery
void SetItems(const TArray<UClass*>& InItemClasses);

// Blueprint native event (override in Blueprint or C++)
void OnItemClicked(UClass* ClickedItemClass);

// Helper to validate widget setup
bool IsWidgetConfigured() const;
```

## Usage Examples

### Example 1: Creating a RoadPreset Subclass

```cpp
// RoadPresetItemWidget.h (future refactoring)
class URoadPresetItemWidget : public UWorldBLDItemButtonWidget
{
    GENERATED_BODY()
    
protected:
    virtual UTexture2D* GetThumbnailFromClass_Implementation(UClass* InClass) override
    {
        if (InClass)
        {
            UDynamicRoadDrawPreset* PresetCDO = InClass->GetDefaultObject<UDynamicRoadDrawPreset>();
            if (PresetCDO)
            {
                return PresetCDO->PresetImage;
            }
        }
        return nullptr;
    }
    
    virtual FText GetTooltipTextFromClass_Implementation(UClass* InClass) override
    {
        if (InClass)
        {
            UDynamicRoadDrawPreset* PresetCDO = InClass->GetDefaultObject<UDynamicRoadDrawPreset>();
            if (PresetCDO && !PresetCDO->PresetName.IsNone())
            {
                return FText::FromName(PresetCDO->PresetName);
            }
        }
        return FText::GetEmpty();
    }
};
```

### Example 2: Using in C++

```cpp
// Create the selector widget
UWorldBLDItemSelectorWidget* Selector = CreateWidget<UWorldBLDItemSelectorWidget>(GetWorld(), SelectorWidgetClass);

// Populate with any item classes
TArray<UClass*> MyItemClasses;
MyItemClasses.Add(MyPreset1Class);
MyItemClasses.Add(MyPreset2Class);
Selector->SetItems(MyItemClasses);

// Add to viewport
Selector->AddToViewport();
```

### Example 3: Using in Blueprint

1. Create a Blueprint subclass of `UWorldBLDItemButtonWidget`
2. In the UMG Designer, create the widget hierarchy:
   - Add a `Button` named "ItemButton" (exact name required)
3. Override the `GetThumbnailFromClass` event to return the appropriate texture
4. Override the `GetTooltipTextFromClass` event to return the appropriate text

For the selector:
1. Create a Blueprint subclass of `UWorldBLDItemSelectorWidget`
2. In the UMG Designer, create the widget hierarchy:
   - Add a `ScrollBox` named "ItemScrollBox" (exact name required)
   - Inside the ScrollBox, add a `WrapBox` named "ItemContainer" (exact name required)
3. Set the `ItemWidgetClass` property to your button widget subclass
4. Override the `OnItemClicked` event to handle selection

## Widget Hierarchy

```
UWorldBLDItemSelectorWidget
└── ItemScrollBox (UScrollBox)
    └── ItemContainer (UWrapBox)
        ├── UWorldBLDItemButtonWidget #1
        │   └── ItemButton (UButton)
        ├── UWorldBLDItemButtonWidget #2
        │   └── ItemButton (UButton)
        └── ... (more items)
```

## Implementation Details

### Hard-Coded Values
- **Image Size**: 128x128 pixels per item image
- **WrapBox Inner Padding**: 8x8 pixels
- **Item Slot Padding**: 4 pixels (all sides)

### Key Design Decisions

1. **Generic UClass* Instead of Templates**: Uses `UClass*` instead of templated types to maintain compatibility with Blueprint and UMG widget system, which doesn't support templates well.

2. **Virtual Methods for Customization**: Subclasses override virtual methods to specify how to extract thumbnails and tooltips from their specific target class types. This provides flexibility while maintaining type safety in subclasses.

3. **BlueprintNativeEvent**: The virtual methods use `BlueprintNativeEvent` specifier, allowing them to be overridden in both C++ (via `_Implementation` methods) and Blueprint.

4. **Preserved Button Styling**: When setting thumbnails, the implementation preserves existing button style properties (outlines, corner radius, tint) set in Blueprint, only modifying the image resource and size.

5. **Protected Members for Extensibility**: The `ItemWidgets` array and `HandleItemClicked` method are protected (not private) to allow derived classes to extend functionality. For example, a derived class might need to directly manage widgets when working with DataAsset instances instead of Blueprint subclasses.

## Future Refactoring Path

The existing `URoadPresetItemWidget` and `URoadPresetSelectorWidget` can be refactored to inherit from these base classes:

1. Change `URoadPresetItemWidget` to inherit from `UWorldBLDItemButtonWidget`
2. Override `GetThumbnailFromClass()` to access `UDynamicRoadDrawPreset::PresetImage`
3. Override `GetTooltipTextFromClass()` to access `UDynamicRoadDrawPreset::PresetName`
4. Change `URoadPresetSelectorWidget` to inherit from `UWorldBLDItemSelectorWidget`
5. Update method signatures from `TSubclassOf<UDynamicRoadDrawPreset>` to `UClass*`
6. Keep the existing `OnPresetClicked` blueprint event for backwards compatibility, forwarding from `OnItemClicked`

This refactoring will reduce code duplication and make it easier to create new item selector widgets for other types (building presets, material presets, etc.).

