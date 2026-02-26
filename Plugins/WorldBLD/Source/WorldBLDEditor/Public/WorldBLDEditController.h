// Copyright WorldBLD. All rights reserved. 

#pragma once

class AActor;
struct FCollisionQueryParams;

#include "Blueprint/UserWidget.h"
#include "PrimitiveDrawWrapper.h"
#include "EditorUtilityWidget.h"
#include "Engine/DataAsset.h"
#include "WorldBLDEditController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldBLDEditControllerBegin, const UWorldBLDEditController*, Controller);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldBLDEditControllerExit, const UWorldBLDEditController*, Controller);

///////////////////////////////////////////////////////////////////////////////////////////////////

USTRUCT(BlueprintType)
struct FKeyHandlingSpec
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings")
    FKey InterceptedKey;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings")
    bool bConsumesKey {true};
};

///////////////////////////////////////////////////////////////////////////////////////////////////

//A WorldBLDUtilityBundle contains data on a WorldBLD Utility(an editor tool contained in the WorldBLD plugin because it didn't have enough code to justify putting it in its own plugin)
UCLASS(BlueprintType, Blueprintable)
class WORLDBLDEDITOR_API UWorldBLDUtilityBundle : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Utility")
    TSubclassOf<UEditorUtilityWidget> WidgetClass;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Utility")
    bool PushWidgetToViewport;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Utility")
    FText UtilityName;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Utility")
    FText UtilityDescription;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Utility")
    UTexture2D* ToolImage;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

// Changes how the WorldBLDEdMode functions in terms of user interaction.
UCLASS(BlueprintType, Blueprintable, Abstract)
class WORLDBLDEDITOR_API UWorldBLDEditController : public UEditorUtilityWidget
{
    GENERATED_BODY()

/* friends: */
    friend class UWorldBLDEdMode;
    friend class FWorldBLDSelectModeWidgetHelper;

public:
    
    ///////////////////////////////////////////////////////////////////////////////////////////////

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Tool")
    TArray<FKeyHandlingSpec> InterceptedKeyEvents;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    // Whether or not to allow standard selection behavior. Defaults to NO.
    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Tool")
    bool AllowStandardSelectionControls() const;
    
    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Tool")
    bool PressingEscapeTriggersExit() const;

    // Called when the tool becomes active for the first time.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Tool")
    void ToolBegin();

    // Called when the tool becomes active (from a resigned state).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Tool")
    void ToolActivate();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Tool")
    void ToolRender(FPrimitiveDrawWrapper Renderer);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Tool")
    void ToolPostUndoRedo(bool bIsUndo);

    // Called when the tool is still available, but no longer top of the stack.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Tool")
    void ToolResign();

    // Tries to trigger this tool to exit.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Tool")
    void TryExitTool(bool bForceEnd);

    // Actually causes this tool to exit. 
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Tool")
    void ToolExit(bool bForceEnd);

    UFUNCTION(BlueprintCallable, BlueprintCallable, Category="Tool")
	bool AddToEditorLevelViewport();

    UFUNCTION(BlueprintCallable, BlueprintCallable, Category="Tool")
    void RemoveFromEditorLevelViewport();

    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category="Tool|Runtime")
    AActor* ActiveBrushActor {nullptr};

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Tool")
    bool bActiveBrushIsForceSelected {true};

    UFUNCTION(BlueprintCallable, Category="Tool")
    bool ForceEditorSelectionToActiveBrushActor();

    UFUNCTION(BlueprintNativeEvent, Category="Tool")
    void ToolActorSelectionChanged(const TArray<AActor*>& NewlySelected, const TArray<AActor*>& Deselected,
            const TArray<AActor*>& CurrentSelections);

    UFUNCTION(BlueprintPure, Category="WorldBLDEditor|Util")
    static bool EditorViewportHasFocus();

    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Tool")
    bool ConsumesMouseScroll() const;
    
    UFUNCTION(BlueprintNativeEvent, Category="Tool")
    void ToolMouseScroll(float ScrollDelta, bool bControl, bool bAlt, bool bShift);

    UFUNCTION(BlueprintNativeEvent, Category="Tool")
    void ToolKeyEvent(FKey Key, bool bPressed, bool bControl, bool bAlt, bool bShift);

    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Tool")
    bool ConsumesMouseClick(int32 Which, bool& bIsPassive) const;

    UFUNCTION(BlueprintNativeEvent, Category="Tool")
    void ToolMouseClick(int32 WhichButton, bool bPressed);

    UFUNCTION(BlueprintNativeEvent, Category = "Tool")
    bool ToolMouseMove(int32 X, int32 Y);

    UFUNCTION(BlueprintNativeEvent, Category = "Tool")
    void ToolHitProxyClick(FElementHitProxyContext Element, const FKey& MouseKey);

    // Customization hooks for the editor transform widget while this controller is active
    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Tool|Widget")
    bool WantsCustomTransformWidget() const;

    // Axis mask for drawing (EAxisList flags). Only used if WantsCustomTransformWidget returns true
    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Tool|Widget")
    EAxisList::Type GetWidgetAxisMask() const;

    // Desired widget location. Only used if WantsCustomTransformWidget returns true
    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Tool|Widget")
    FVector GetDesiredWidgetLocation() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Tool")
    bool ConsumeSelectionControls() const;

    UPROPERTY(Instanced, EditAnywhere, BlueprintReadOnly, Category=Tool)
	class UTraceHitFilterBase* TracePlacementFilter;

    UPROPERTY(BlueprintAssignable, Category = Tool)
    FOnWorldBLDEditControllerBegin OnToolBegin;

    UPROPERTY(BlueprintAssignable, Category = Tool)
    FOnWorldBLDEditControllerExit OnToolExit;

    /** Warning text displayed as a tooltip near the mouse cursor. Set to non-empty to show, clear to hide. */
    UPROPERTY(BlueprintReadWrite, Category = "Tool")
    FText UserWarning;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tool)
    UObject* WidgetStyle {nullptr};

    /** Optional secondary style root. Widgets with bUseAlternateStyle enabled will pull their style objects from here. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tool, meta=(DisplayName="Alternate Style"))
    UObject* AlternateStyle {nullptr};

protected:
    void ApplyStylesToChildWidgets();
    virtual bool AllowStandardSelectionControls_Implementation() const {return false;}
    virtual bool PressingEscapeTriggersExit_Implementation() const {return false;}
    virtual void ToolBegin_Implementation();
    virtual void ToolActivate_Implementation();
    virtual void ToolPostUndoRedo_Implementation(bool bIsUndo) {}
    virtual void ToolResign_Implementation();
    virtual void TryExitTool_Implementation(bool bForceEnd);
    virtual void ToolExit_Implementation(bool bForceEnd);
    virtual bool ConsumesMouseScroll_Implementation() const {return false;}
    virtual void ToolMouseScroll_Implementation(float ScrollDelta, bool bControl, bool bAlt, bool bShift) {};
    virtual void ToolKeyEvent_Implementation(FKey Key, bool bPressed, bool bControl, bool bAlt, bool bShift) {};
    virtual bool ConsumesMouseClick_Implementation(int32 Which, bool& bIsPassive) const {bIsPassive = false; return false;}
    virtual void ToolMouseClick_Implementation(int32 WhichButton, bool bPressed) {};
    virtual bool ToolMouseMove_Implementation(int32 X, int32 Y) { return false; };
    virtual void ToolHitProxyClick_Implementation(FElementHitProxyContext Element, const FKey& MouseKey) {};
    virtual void ToolRender_Implementation(FPrimitiveDrawWrapper Renderer) {};
    virtual void RenderInView(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI);
    virtual void ToolActorSelectionChanged_Implementation(const TArray<AActor*>& NewlySelected, const TArray<AActor*>& Deselected, const TArray<AActor*>& CurrentSelections) {}
    virtual bool ConsumeSelectionControls_Implementation() const {return true;}

    // Defaults for transform widget customization hooks
    virtual bool WantsCustomTransformWidget_Implementation() const { return false; }
    virtual EAxisList::Type GetWidgetAxisMask_Implementation() const { return EAxisList::All; }
    virtual FVector GetDesiredWidgetLocation_Implementation() const { return FVector::ZeroVector; }

    virtual void NativePreConstruct() override;

    void CleanupSelection() const;

    /** Allows derived tools to add extra ignored actors for the mouse ray trace (e.g. ignore brush child actors). */
    virtual void AddIgnoredActorsForMouseTrace(FCollisionQueryParams& TraceParams) const;

    /**
     * Gets the world location from mouse coordinates by deprojecting and ray tracing.
     * @param X The mouse X coordinate
     * @param Y The mouse Y coordinate
     * @return The world location. If nothing is hit, returns a point along the ray.
     */
    FVector GetMouseHitPoint(int32 X, int32 Y) const;

    /**
     * Attempts to ray trace under the mouse cursor.
     * @param X The mouse X coordinate
     * @param Y The mouse Y coordinate
     * @param OutHitPoint The impact point if something is hit, otherwise ZeroVector.
     * @return True if the trace hit something, false otherwise.
     */
    bool TryGetMouseHitPoint(int32 X, int32 Y, FVector& OutHitPoint) const;

    UFUNCTION()
    void OnBrushActorDestroyed(AActor* DestroyedActor);

    void OnActorSelectionSetChanged();

    UFUNCTION(BlueprintPure, Category="Tool")
    class UEditorActorSubsystem* InternalGetEditorActorSubsystem() const;
    

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tool|Runtime")
    bool bHasBegun {false};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tool|Runtime")
    bool bIsActive {false};

private:
    bool bForceSelectionGuard {false};
    TArray<TWeakObjectPtr<AActor>> TrackedSelections;
};
