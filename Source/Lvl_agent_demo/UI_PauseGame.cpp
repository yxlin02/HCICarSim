#include "UI_PauseGame.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void UUI_PauseGame::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (ContinueButton)
    {
        ContinueButton->OnClicked.AddDynamic(this, &UUI_PauseGame::HandleContinueClicked);
    }
}

void UUI_PauseGame::ShowPauseUI()
{
    if (!IsInViewport())
    {
        AddToViewport(100);
    }

    if (UWorld* World = GetWorld())
    {
        UGameplayStatics::SetGamePaused(World, true);

        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            PC->bShowMouseCursor = true;

            FInputModeUIOnly InputMode;
            InputMode.SetWidgetToFocus(TakeWidget());
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(InputMode);
        }
    }
}

void UUI_PauseGame::HidePauseUI()
{
    if (UWorld* World = GetWorld())
    {
        UGameplayStatics::SetGamePaused(World, false);

        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            PC->bShowMouseCursor = false;

            FInputModeGameOnly InputMode;
            PC->SetInputMode(InputMode);
        }
    }

    RemoveFromParent();
}

void UUI_PauseGame::HandleContinueClicked()
{
    HidePauseUI();

    OnContinue.Broadcast();
}
