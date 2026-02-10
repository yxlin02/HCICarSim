// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Lvl_agent_demoPlayerController.h"
#include "MyPlayerController.generated.h"

/**
 * 
 */
class UInputMappingContext;
class UInputAction;

UCLASS()
class LVL_AGENT_DEMO_API AMyPlayerController : public ALvl_agent_demoPlayerController
{
	GENERATED_BODY()
    
protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    UFUNCTION()
    void OnDecisionAccept(const FInputActionValue& Value);

    UFUNCTION()
    void OnDecisionReject(const FInputActionValue& Value);

    UPROPERTY(EditAnywhere, Category="Decision|Input")
    UInputMappingContext* IMC_Decision;

    UPROPERTY(EditAnywhere, Category="Decision|Input")
    UInputAction* IA_DecisionAccept;

    UPROPERTY(EditAnywhere, Category="Decision|Input")
    UInputAction* IA_DecisionReject;
	
};
