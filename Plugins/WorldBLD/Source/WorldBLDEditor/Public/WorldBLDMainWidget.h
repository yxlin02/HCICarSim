// Copyright WorldBLD LLC. All rights reserved.

#pragma once

class UWorldBLDToolPaletteWidget;
class UWorldBLDToolButtonWidget;
class UWorldBLDEditController;

#include "Blueprint/UserWidget.h"
#include "Components/WrapBox.h"
#include "Components/WidgetSwitcher.h"
#include "WorldBLDMainWidget.generated.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

// The main widget for the WorldBLD Edit Mode. It mainly consists of a tab switcher and a
// set of tab content bodies (instances of UWorldBLDToolPaletteWidget). 
UCLASS(BlueprintType, Blueprintable, Abstract)
class UWorldBLDMainWidget : public UUserWidget
{
    GENERATED_BODY()

/* friends: */
    friend class UWorldBLDEdMode;

public:
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta=(BindWidget), Category="Tool")
    UPanelWidget* TabButtonContainer;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta=(BindWidget), Category="Tool")
    UWidgetSwitcher* MainWidgetSwitcher;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tool|Design")
    TSubclassOf<UWorldBLDToolButtonWidget> ButtonWidgetClass;

    void SetTools(TArray<UWorldBLDToolPaletteWidget*> Tools);
    void SetActiveTool(const FName& NewActiveTool);
    UWorldBLDToolPaletteWidget* GetActiveTool() const;
    FName GetFirstToolName() const;

    UFUNCTION(BlueprintImplementableEvent, Category="Tool")
    void NewToolTabSelected(UWorldBLDToolPaletteWidget* NewTool);

    UFUNCTION(BlueprintImplementableEvent, Category="Tool")
    void OnToolsSet();

    UFUNCTION(BlueprintCallable, Category="Tool")
    void ResetToDefaultController();

    ///////////////////////////////////////////////////////////////////////////////////////////////

    UPROPERTY(Transient, BlueprintReadOnly, VisibleAnywhere, Category="Tool")
    class UWorldBLDEdMode* EdMode;

    void ResetContent();
    void MakeFirstEnabledPaletteActive();

protected:
    struct FLocalWidgetSet
    {
        FName UniqueId {NAME_None};
        TWeakObjectPtr<UWorldBLDToolPaletteWidget> BodyContent;
        TWeakObjectPtr<UWorldBLDToolButtonWidget> ButtonContent;
    };

    TArray<FLocalWidgetSet> AssignedWidgets;
    FName ActiveTool {NAME_None};

    UFUNCTION()
    void OnButtonClicked(UWidget* Widget);

    UFUNCTION()
    void OnLoadedWorldBLDKitsChanged(class UWorldBLDEdMode* InEdMode);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
