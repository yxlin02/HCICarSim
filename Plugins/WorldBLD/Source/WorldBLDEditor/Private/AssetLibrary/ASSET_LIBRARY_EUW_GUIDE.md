## WorldBLD Asset Library (Editor-only) — EUW integration guide

This folder implements the **WorldBLD Asset Library** UI and backend used to browse, purchase, download, and import WorldBLD assets/kits **in-editor only** (UE `UEditorSubsystem`).

The intended integration point for **Editor Utility Widgets (EUWs)** is a single Blueprint-callable function that opens the stock Asset Library window.

### Quick start (Editor Utility Widget)

If you want to launch the stock Asset Library from an EUW:

- Add a Button (or any UI action) in your EUW blueprint.
- On Clicked, call:
  - **`UWorldBLDAssetLibraryBlueprintLibrary::OpenAssetLibraryWindow()`** (Blueprint node: `Open Asset Library Window`)

That’s it—this spawns the dedicated Asset Library Slate window and handles the rest.

Relevant files:
- `.../Public/AssetLibrary/WorldBLDAssetLibraryBlueprintLibrary.h`
- `.../Private/AssetLibrary/WorldBLDAssetLibraryBlueprintLibrary.cpp`
- `.../Public/AssetLibrary/WorldBLDAssetLibraryWindow.h`
- `.../Private/AssetLibrary/WorldBLDAssetLibraryWindow.cpp`

### High-level architecture (how the system is wired)

The Asset Library is split into three layers:

- **Backend (Blueprint-friendly, editor subsystem)**:
  - `UWorldBLDAssetLibrarySubsystem` (`.../Public/AssetLibrary/WorldBLDAssetLibrarySubsystem.h`)
    - Makes HTTP requests to the WorldBLD API (`BaseURL = https://worldbld.com/api`)
    - Exposes `BlueprintCallable` functions like `FetchAssets`, `FetchKits`, `FetchAssetDetails`, `PurchaseAsset`
    - Broadcasts results via dynamic multicast delegates (`OnAssetsFetched`, `OnAPIError`, ...)
  - `UWorldBLDAssetDownloadManager` (`.../Public/AssetLibrary/WorldBLDAssetDownloadManager.h`)
    - Downloads + caches archives, extracts, imports into the project Content folder
    - Tracks per-asset state (`EWorldBLDAssetStatus`) and broadcasts `OnAssetStatusChanged` / `OnImportComplete`

- **UI (Slate)**:
  - `SWorldBLDAssetLibraryWindow`
    - The main UI widget (search, category tree, grid, details)
    - On construct, it grabs editor subsystems via `GEditor->GetEditorSubsystem<>()` and kicks off initial fetches.
  - `SWorldBLDAssetDetailPanel`
    - The right-side expandable details panel
    - Triggers `Fetch*Details()`, purchase, download and import actions based on selection + status

- **Bridge (UObject → Slate glue for dynamic delegates)**:
  - `UWorldBLDAssetLibrarySlateBridge`
    - Dynamic multicast delegates (`AddDynamic`) require a `UObject` receiver.
    - The bridge receives subsystem events and forwards them to `SWorldBLDAssetLibraryWindow`.

### What happens when an EUW opens the window

Call chain:

- EUW calls `UWorldBLDAssetLibraryBlueprintLibrary::OpenAssetLibraryWindow()`
- Which calls `FWorldBLDAssetLibraryWindow::OpenAssetLibrary()`
- Which spawns an `SWindow` and sets its content to `SNew(SWorldBLDAssetLibraryWindow)`

Inside `SWorldBLDAssetLibraryWindow::Construct`:

- Retrieves:
  - `UWorldBLDAssetLibrarySubsystem`
  - `UWorldBLDAssetDownloadManager`
- Creates a `UWorldBLDAssetLibrarySlateBridge` and binds:
  - `AssetLibrarySubsystem->OnAssetsFetched` → `Bridge->HandleAssetsFetched` → `SWorldBLDAssetLibraryWindow::OnAssetsFetched`
  - `DownloadManager->OnAssetStatusChanged` → `Bridge->HandleAssetStatusChanged` → `SWorldBLDAssetLibraryWindow::RefreshAssetGrid`
  - etc.
- Starts loading by calling:
  - `AssetLibrarySubsystem->FetchAssets(CurrentPage, 20, "")`
  - `AssetLibrarySubsystem->FetchKits(1, 20)`

### Data model (types you’ll see in Blueprint/C++)

Types are defined in `.../Public/AssetLibrary/WorldBLDAssetLibraryTypes.h`:

- `FWorldBLDAssetInfo`, `FWorldBLDKitInfo`
- `FWorldBLDAssetListResponse`, `FWorldBLDKitListResponse`
- `EWorldBLDAssetStatus` (`NotOwned`, `Owned`, `Downloading`, `Downloaded`, `Imported`)
- `FWorldBLDAssetDownloadState` and `FWorldBLDAssetCacheManifest`

### Authentication / ownership notes

- Purchasing requires a session token via `UWorldBLDAuthSubsystem` (used internally by `UWorldBLDAssetLibrarySubsystem`).
- If not authenticated, `PurchaseAsset()` will broadcast `OnAPIError` with `"Not authenticated"`.
- The legacy `.../auth/desktop/owned-apps` ownership endpoint is currently inactive; UI should prefer tool license checks via `UWorldBLDAuthSubsystem::HasLicenseForTool()` for "Get Plan" vs "Get Credits" UX.

### Download + import behavior (where files go)

Download/cache:

- Cached downloads live under the user settings dir:
  - `.../WorldBLD/AssetLibrary/Cache/<AssetId>/`
  - With a persistent manifest:
    - `.../WorldBLD/AssetLibrary/manifest.json`

Import destination:

- `UWorldBLDAssetDownloadManager::ImportAsset()` copies extracted content into:
  - **Assets**: `/Game/WorldBLD/Assets/<SanitizedAssetName>_<AssetId>`
  - **Kits**: `/Game/WorldBLD/Kits/<SanitizedAssetName>_<AssetId>`
- After importing a kit, `WorldBLDEditor` triggers a rescan for kit bundles (`ScanContentForKitBundles()`).

Note:

- The importer performs **SHA1-based deduplication** for supported asset types (meshes/materials/textures). If a newly imported `.uasset` is identical to one already present in `/Game/WorldBLD/Assets` (same SHA1), references are consolidated to the existing asset and the duplicate asset is removed.

### Building a custom EUW Asset Browser (optional)

If you want your own EUW UI (instead of opening the stock Slate window), you can work directly with the subsystems from Blueprint:

- **Fetch lists**:
  - `Get Editor Subsystem (WorldBLDAssetLibrarySubsystem)`
  - Bind to:
    - `OnAssetsFetched`
    - `OnKitsFetched`
    - `OnAPIError`
  - Call:
    - `FetchAssets(Page, PerPage, CategoryString)`
    - `FetchKits(Page, PerPage)`

- **Fetch details** (for richer description/preview images):
  - Call:
    - `FetchAssetDetails(AssetID)` or `FetchKitDetails(KitID)`
  - Bind to:
    - `OnAssetDetailsFetched` / `OnKitDetailsFetched`

- **Purchase**:
  - Call:
    - `PurchaseAsset(AssetId)`
  - Bind to:
    - `OnPurchaseComplete` and `OnAPIError`

- **Download / import / status**:
  - `Get Editor Subsystem (WorldBLDAssetDownloadManager)`
  - Use:
    - `GetAssetStatus(AssetId)`
    - `DownloadAsset(AssetId, DownloadUrl, bIsKit, OnProgress, OnComplete)`
    - `ImportAsset(AssetId)`
  - Bind to:
    - `OnAssetStatusChanged`
    - `OnImportComplete`

### Implementation map (where to look when extending)

- **Window spawn / singleton behavior**: `.../Private/AssetLibrary/WorldBLDAssetLibraryWindow.cpp`
- **Main UI + wiring**: `.../Private/AssetLibrary/SWorldBLDAssetLibraryWindow.cpp`
- **Delegates forwarding**: `.../Private/AssetLibrary/WorldBLDAssetLibrarySlateBridge.cpp`
- **API requests + parsing**: `.../Private/AssetLibrary/WorldBLDAssetLibrarySubsystem.cpp`
- **Download/cache/import**: `.../Private/AssetLibrary/WorldBLDAssetDownloadManager.cpp`
- **Details panel actions (purchase/download/import)**: `.../Private/AssetLibrary/SWorldBLDAssetDetailPanel.cpp`


