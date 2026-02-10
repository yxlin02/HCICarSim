#include "MyPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

#include "DecisionManagerComponent.h"

void AMyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (ULocalPlayer* LP = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            if (IMC_Decision)
            {
                Subsystem->AddMappingContext(IMC_Decision, 10);
            }
        }
    }
}

void AMyPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EIC =
        Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (IA_DecisionAccept)
        {
            EIC->BindAction(
                IA_DecisionAccept,
                ETriggerEvent::Triggered,
                this,
                &AMyPlayerController::OnDecisionAccept
            );
        }

        if (IA_DecisionReject)
        {
            EIC->BindAction(
                IA_DecisionReject,
                ETriggerEvent::Triggered,
                this,
                &AMyPlayerController::OnDecisionReject
            );
        }
    }
}

void AMyPlayerController::OnDecisionAccept(const FInputActionValue&)
{
    if (APawn* PawnPtr = GetPawn())
    {
        if (UDecisionManagerComponent* Manager =
            PawnPtr->FindComponentByClass<UDecisionManagerComponent>())
        {
            Manager->OnAccept();
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("No UDecisionManagerComponent found on pawn"));
        }
    }
}

void AMyPlayerController::OnDecisionReject(const FInputActionValue&)
{
    if (APawn* PawnPtr = GetPawn())
    {
        if (UDecisionManagerComponent* Manager =
            PawnPtr->FindComponentByClass<UDecisionManagerComponent>())
        {
            Manager->OnReject();
        }
    }
}
