# WorldBLD Context Menu Widgets

This folder contains reusable Slate widgets intended for WorldBLD context menu implementations.

## SWorldBLDPropertySlider

Example (single value):

```cpp
SNew(SWorldBLDPropertySlider)
  .Label(LOCTEXT("Width", "Width"))
  .UnitLabel(LOCTEXT("Centimeters", "cm"))
  .Value_Lambda([this]() { return TOptional<float>(CurrentWidth); })
  .MinValue(0.0f)
  .MaxSliderValue(1000.0f)
  .OnBeginSliderMovement(this, &FMyClass::OnBeginTransaction)
  .OnValueChanged(this, &FMyClass::OnWidthChanged)
  .OnEndSliderMovement(this, &FMyClass::OnEndTransaction);
```

Example (multi-selection with mixed values):

```cpp
TOptional<float> WidthValue;
for (AActor* Actor : SelectedActors)
{
  if (!UpdateMultipleValue(WidthValue, Actor->GetWidth())) break;
}

SNew(SWorldBLDPropertySlider)
  .Value(WidthValue) // Shows "Mixed" if unset
  .Label(LOCTEXT("Width", "Width"));
```

## SWorldBLDAssetPicker

Example (single asset):

```cpp
SNew(SWorldBLDAssetPicker)
  .Label(LOCTEXT("Preset", "Preset"))
  .AssetClass(UMyPresetClass::StaticClass())
  .SelectedAsset_Lambda([this]()
  {
    return TOptional<FAssetData>(FAssetData(CurrentPreset));
  })
  .OnAssetSelected(this, &FMyClass::OnPresetChanged);
```

Example (multi-selection with mixed assets):

```cpp
TOptional<FAssetData> PresetValue;
for (AActor* Actor : SelectedActors)
{
  FAssetData AssetData(Actor->GetPreset());
  if (!UpdateMultipleValue(PresetValue, AssetData)) break;
}

SNew(SWorldBLDAssetPicker)
  .SelectedAsset(PresetValue) // Shows "Mixed" overlay if unset
  .Label(LOCTEXT("Preset", "Preset"))
  .AssetClass(UMyPresetClass::StaticClass());
```


