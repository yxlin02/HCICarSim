# Blueprint Asset Selector - Quick Setup Guide

This guide will walk you through creating a complete Blueprint Asset Selector widget from scratch.

## Prerequisites
- Unreal Engine project with WorldBLD plugin
- Basic knowledge of UMG (Unreal Motion Graphics) widget creation

## Step-by-Step Setup

### Part 1: Create the Item Button Widget (Individual Asset Display)

1. **Create a new Widget Blueprint**
   - Right-click in Content Browser → User Interface → Widget Blueprint
   - Name it: `WBP_BlueprintAssetItemButton`
   - Open the Blueprint

2. **Set the Parent Class**
   - File → Reparent Blueprint
   - Search for: `BlueprintAssetItemButtonWidget`
   - Select it and click "Reparent"

3. **Design the Widget Layout**
   - In the Designer tab, create this hierarchy:
   ```
   Canvas Panel (Root)
   └── Button (Name: "ItemButton")  ← IMPORTANT: Must be named exactly "ItemButton"
       └── Overlay
           └── Image (for displaying the thumbnail)
               - Name: Can be anything
               - Size: 128x128 (or your preferred size)
   ```

4. **Configure the Button**
   - Select the Button widget
   - In Details panel:
     - Style → Normal: Set background color/image
     - Style → Hovered: Set hover state appearance
     - Style → Pressed: Set pressed state appearance
   - Make sure the button is named **"ItemButton"** (BindWidget requirement)

5. **Optional: Add Asset Name Label**
   ```
   Button ("ItemButton")
   └── Overlay
       ├── Image (thumbnail)
       └── Text Block (asset name) ← Shows below or overlaid on thumbnail
   ```

6. **Configure Button Display**
   - The widget will display using the class icon by default (thumbnails not extracted)
   - For custom display, you can:
     - Add an Image widget and bind it to show asset-specific visuals
     - Use the Button's style to set a default appearance
     - Override `GetThumbnailFromClass` in Blueprint to provide custom thumbnails

7. **Compile and Save**

### Part 2: Create the Selector Widget (Main Gallery)

1. **Create a new Widget Blueprint**
   - Right-click in Content Browser → User Interface → Widget Blueprint
   - Name it: `WBP_BlueprintAssetSelector`
   - Open the Blueprint

2. **Set the Parent Class**
   - File → Reparent Blueprint
   - Search for: `BlueprintAssetSelectorWidget`
   - Select it and click "Reparent"

3. **Design the Widget Layout**
   - In the Designer tab, create this hierarchy:
   ```
   Canvas Panel (Root)
   └── Scroll Box (Name: "ItemScrollBox")  ← IMPORTANT: Must be named exactly "ItemScrollBox"
       └── Wrap Box (Name: "ItemContainer")  ← IMPORTANT: Must be named exactly "ItemContainer"
   ```

4. **Configure the Scroll Box**
   - Select "ItemScrollBox"
   - In Details panel:
     - Scroll Bar Visibility: Visible When Needed
     - Always Show Scrollbar: False
     - Allow Overscroll: True
     - Orientation: Vertical
   - Anchors: Stretch to fill parent
   - Fill entire canvas or desired area

5. **Configure the Wrap Box**
   - Select "ItemContainer"
   - In Details panel:
     - Inner Slot Padding: 8, 8 (space between items)
     - Wrap Size: Set to your desired width (e.g., 1000 for 5-6 items per row with 128px items)
   - Or leave Wrap Size at 0 to fill available width

6. **Set Widget Properties**
   - Select the root (WBP_BlueprintAssetSelector) in the Hierarchy
   - In Details panel, find:
     - **Item Widget Class**: Select `WBP_BlueprintAssetItemButton` (created in Part 1)
     - **Base Class Filter**: Choose the class you want to filter by
       - For example: `Actor`, `Pawn`, `Character`, `UserWidget`, etc.
       - This determines which Blueprint types will be shown

7. **Implement the Construct Event**
   - Switch to the Graph tab
   - Find or create the `Event Construct` node
   - Add logic:
   ```
   Event Construct
   └── Load And Display Blueprint Assets
       ├── Search Path: "" (empty = entire project) or "/Game/YourFolder"
       └── Recursive: true (checked)
   ```

8. **Implement Click Handling**
   - Override the `OnBlueprintAssetClicked` event:
   - Right-click → Add Event → Override → On Blueprint Asset Clicked
   ```
   Event OnBlueprintAssetClicked
   ├── Clicked Asset Class (input pin) → Your custom logic
   └── Example: Print String (show asset name for testing)
   ```

9. **Compile and Save**

### Part 3: Use the Selector Widget

#### Option A: In Level Editor (Editor Utility Widget)

1. **Create an Editor Utility Widget**
   - Right-click in Content Browser → Editor Utilities → Editor Utility Widget
   - Name it: `EUW_AssetBrowser`
   - Open it

2. **Add the Selector Widget**
   ```
   Canvas Panel
   └── WBP_BlueprintAssetSelector
       - Anchors: Stretch to fill
       - Size: Fill screen
   ```

3. **Run the Editor Utility Widget**
   - Right-click on `EUW_AssetBrowser` in Content Browser
   - Select "Run Editor Utility Widget"
   - Your asset selector should appear!

#### Option B: In Another Widget

1. **Add to Any UMG Widget**
   - Open your main widget Blueprint
   - Drag `WBP_BlueprintAssetSelector` from Content Browser into your Designer
   - Position and size as needed

2. **Set Properties at Runtime** (if needed)
   - In the Graph, you can change `BaseClassFilter` before calling `LoadAndDisplayBlueprintAssets`:
   ```
   Event Construct
   ├── Set Base Class Filter → Actor (or your desired class)
   └── Load And Display Blueprint Assets
   ```

## Example Configurations

### Example 1: Character Browser
**Use Case:** Browse and spawn all character Blueprints

**Setup:**
- Base Class Filter: `Character`
- Search Path: `/Game/Characters`
- OnBlueprintAssetClicked:
  ```blueprint
  Spawn Actor from Class
  ├── Class: Clicked Asset Class
  ├── Location: Get Player Location + Offset
  └── Collision Handling Override: Always Spawn
  ```

### Example 2: Widget Palette
**Use Case:** Show all custom widgets and create them on click

**Setup:**
- Base Class Filter: `UserWidget`  
- Search Path: `/Game/UI/CustomWidgets`
- OnBlueprintAssetClicked:
  ```blueprint
  Create Widget
  ├── Class: Clicked Asset Class
  ├── Owning Player: Get Player Controller
  └── Add to Viewport
  ```

### Example 3: Weapon Selector
**Use Case:** Show all weapon Blueprints (custom base class)

**Setup:**
- Create a C++ or Blueprint base class: `BP_WeaponBase`
- Base Class Filter: `BP_WeaponBase`
- Search Path: `/Game/Weapons`
- OnBlueprintAssetClicked:
  ```blueprint
  Spawn Actor from Class
  ├── Class: Clicked Asset Class
  └── Attach to Component (player's hand)
  ```

## Styling Tips

### Make Items Look Better

1. **Add hover effects to the button**
   - Button → Style → Hovered → Tint: Lighter color
   - Button → Style → Hovered → Add outline/border

2. **Add asset name label**
   ```
   Button
   └── Overlay
       ├── Image (class icon or custom image)
       └── Vertical Box (Bottom-aligned)
           └── Text Block (asset name)
               - Background: Semi-transparent black
               - Text Color: White
               - Padding: 5
   ```
   
3. **Add custom thumbnails** (optional, requires additional setup)
   - Create a UTexture2D property on your Blueprint class
   - Set it manually for each Blueprint you want to display
   - Override `GetThumbnailFromClass` to return this texture

4. **Add selection state**
   - Store the last clicked item
   - Change its button style to show it's selected
   - Use a variable: `SelectedAssetClass`

5. **Responsive sizing**
   - Make item buttons scale with screen size
   - Use Size Box to constrain min/max size
   - Adjust Wrap Box wrap width dynamically

## Troubleshooting Quick Fixes

| Problem | Solution |
|---------|----------|
| Nothing shows up | Check Output Log for errors. Verify BaseClassFilter is set. |
| "ItemButton" not bound | Rename your button to exactly "ItemButton" (case-sensitive) |
| "ItemScrollBox" not bound | Rename your ScrollBox to exactly "ItemScrollBox" |
| "ItemContainer" not bound | Rename your WrapBox to exactly "ItemContainer" |
| No thumbnails | By default, thumbnails are not extracted. Widget uses class icons and names. |
| Want custom thumbnails | Override GetThumbnailFromClass in Blueprint or add texture properties. |
| Wrong assets showing | Verify BaseClassFilter is correct. Check Search Path. |
| Items too small/large | Adjust button size in WBP_BlueprintAssetItemButton |
| Items not wrapping | Set Wrap Box → Wrap Size to a specific value (e.g., 1000) |

## Advanced: Filtering and Searching

To add search functionality, you can:

1. **Add a Text Box** above the selector:
   ```
   Vertical Box
   ├── Horizontal Box (Search bar)
   │   ├── Text Box (SearchInput)
   │   └── Button (Search)
   └── WBP_BlueprintAssetSelector
   ```

2. **Filter on text change**:
   ```blueprint
   On Text Changed (SearchInput)
   ├── Get all item widgets from selector
   ├── For each widget:
   │   ├── Get asset name
   │   ├── Contains substring (search text)?
   │   └── Set Visibility (Visible/Collapsed)
   ```

## Performance Notes

- **Large projects**: Use specific search paths instead of searching entire project
- **Many assets**: Consider pagination or lazy loading
- **Thumbnail loading**: May be slow on first load. Thumbnails are cached afterward.
- **Asset Registry**: Call `ForceRescanAssetRegistry()` if assets not appearing after creation

## Complete Blueprint Graph Example

Here's what your selector widget's Event Graph should look like:

```
Event Construct
└── Load And Display Blueprint Assets
    ├── Search Path: "" 
    └── Recursive: true

Event On Blueprint Asset Clicked
├── Print String
│   └── Clicked Asset Class → Get Display Name
└── Branch (Is Valid?)
    └── TRUE → Your custom action
        ├── Spawn Actor from Class
        └── ... your logic
```

## Next Steps

Once you have the basic selector working, you can extend it with:
- ✅ Custom styling and animations
- ✅ Search and filter functionality  
- ✅ Context menus (right-click options)
- ✅ Drag-and-drop support
- ✅ Multi-selection
- ✅ Sorting options
- ✅ Category grouping
- ✅ Favorites system

Happy Blueprint browsing! 🎨

