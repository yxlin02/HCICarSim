// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "WorldBLDToolButtonWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnButtonClickedEventEx, UWidget*, Widget);

// Special button widget for the WorldBLD interface. Has custom style and icon and can be
// highlighted (for use in a select-one tab system).
UCLASS(BlueprintType, Blueprintable, Abstract)
class UWorldBLDToolButtonWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta=(BindWidget), Category="Tool")
    class UButton* PrimaryButton;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta=(BindWidget), Category="Tool")
    class UImage* PrimaryIcon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tool")
    FName OptionalId {NAME_None};

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Tool")
    void SetIcon(class UTexture2D* NewIcon);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Tool")
    void SetCustomTooltip(const FText& NewTooltip);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Tool")
    void SetHighlighted(bool bHighlighted);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Tool")
    void SetEnabled(bool bEnabled);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tool")
    bool bButtonIsEnabled {true};

    UPROPERTY(BlueprintAssignable, BlueprintCallable, Category="Tool")
    FOnButtonClickedEventEx OnClicked;

protected:
    virtual void SetIcon_Implementation(class UTexture2D* NewIcon) {}
    virtual void SetCustomTooltip_Implementation(const FText& NewTooltip) {}
    virtual void SetHighlighted_Implementation(bool bHighlighted) {}
    virtual void SetEnabled_Implementation(bool bEnabled) {bButtonIsEnabled = bEnabled;}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
