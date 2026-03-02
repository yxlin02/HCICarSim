// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "EditorUtilityWidget.h"
#include "TextureResource.h"
#include "WorldBLDToolPaletteWidget.generated.h"

class UWorldBLDToolViewportWidget;

///////////////////////////////////////////////////////////////////////////////////////////////////

// Widgets that is a logically grouped set of tools/functionality. For example, the Road Tools would
// all be organized under one of these widgets.
UCLASS(BlueprintType, Blueprintable, Abstract)
class WORLDBLDEDITOR_API UWorldBLDToolPaletteWidget : public UEditorUtilityWidget
{
    GENERATED_BODY()
public:

    ///////////////////////////////////////////////////////////////////////////////////////////////
    
    // The unique ID of the tool.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Tool")
    FName ToolId {NAME_None};

    // The display name for the tool.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Tool")
    FText ToolDisplayName;

    // The tab icon for this tool
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Tool")
    TSoftObjectPtr<class UTexture2D> ToolTabIcon;

    // Whether or not this tool palette requires a loaded kit in the map.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Tool")
    bool bRequiresLoadedWorldBLDKits {false};

    // If this tool is always enabled regardless of the number of kits
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Tool")
    bool bIsAlwaysEnabled {false};

	UFUNCTION(BlueprintPure, Category="License")
	bool IsLicensed(class UWorldBLDAuthSubsystem* Subsystem);
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // Whether or not this tool is usable for the provided Kit.
    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Tool") 
    bool IsToolPaletteSupportedByWorldBLDKit(class AWorldBLDKitBase* Kit) const;

    // The user switched to this tab.
    UFUNCTION(BlueprintNativeEvent, Category="Tool")
    void ToolPaletteActivate();

    UFUNCTION(BlueprintNativeEvent, Category="Tool")
    void ToolPaletteActorSelectionChanged(class UWorldBLDEditController* ActiveTool, 
            const TArray<AActor*>& NewlySelected, const TArray<AActor*>& Deselected,
            const TArray<AActor*>& CurrentSelectionSet);

    // The user switched away from this tab.
    UFUNCTION(BlueprintNativeEvent, Category="Tool")
    void ToolPaletteDeactivate();

    // Makes this tool the foremost one in the EdMode.
    UFUNCTION(BlueprintCallable, Category="Tool")
    void ToolMakeForemostPalette(bool bForceFocused = false);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    UPROPERTY(Transient, BlueprintReadOnly, VisibleAnywhere, Category="Tool")
    class UWorldBLDEdMode* EdMode;

    ///////////////////////////////////////////////////////////////////////////////////////////////

protected:
    virtual void ToolPaletteActivate_Implementation() {};
    virtual void ToolPaletteDeactivate_Implementation() {};
    virtual bool IsToolPaletteSupportedByWorldBLDKit_Implementation(class AWorldBLDKitBase* Kit) const {return true;}
    virtual void ToolPaletteActorSelectionChanged_Implementation(class UWorldBLDEditController* ActiveTool, 
            const TArray<AActor*>& NewlySelected, const TArray<AActor*>& Deselected,
            const TArray<AActor*>& CurrentSelectionSet) {}
};

UCLASS(BlueprintType, Blueprintable)
class WORLDBLDEDITOR_API UWorldBLDToolPaletteWidgetExtended : public UWorldBLDToolPaletteWidget
{
    GENERATED_BODY()
public:

    UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Tool")
    TSubclassOf<UWorldBLDEditController> MainSubtoolClass;

protected:

    UPROPERTY(BlueprintReadOnly, Category = "Tool")
    UWorldBLDEditController* MainSubtool {nullptr};

    virtual void ToolPaletteActivate_Implementation() override;
    virtual void ToolPaletteDeactivate_Implementation() override;

};


// Data that we need to use a given tool, like the widgets it uses and its icon.
UCLASS(BlueprintType)
class WORLDBLDEDITOR_API UWorldBLDToolBundle : public UDataAsset
{
	GENERATED_BODY()
public:
	
	// The icon that appears in the sidebar
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD", Meta=(Untracked))
	UTexture2D* ToolIcon;
	
	// The tool palettes associated with this tool.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD", Meta=(Untracked))
	TArray<UWorldBLDToolPaletteWidget*> ToolWidgets;
};


///////////////////////////////////////////////////////////////////////////////////////////////////
