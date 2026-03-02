#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI_PauseGame.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPauseContinue);

UCLASS()
class LVL_AGENT_DEMO_API UUI_PauseGame : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Pause")
    void ShowPauseUI();

    UFUNCTION(BlueprintCallable, Category = "Pause")
    void HidePauseUI();

    UPROPERTY(BlueprintAssignable, Category = "Pause")
    FOnPauseContinue OnContinue;

protected:
    virtual void NativeOnInitialized() override;

    UFUNCTION()
    void HandleContinueClicked();

protected:

    UPROPERTY(meta = (BindWidget))
    UButton* ContinueButton;
};
