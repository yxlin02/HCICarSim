// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "AgentGameInstance.generated.h"

/**
 * 
 */
UENUM(BlueprintType)
enum class EExperimentType : uint8
{
    Default      UMETA(DisplayName = "Default"),
    Personalize  UMETA(DisplayName = "Personalize"),
    Personalify  UMETA(DisplayName = "Personalify")
};

UCLASS()
class LVL_AGENT_DEMO_API UAgentGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recommendation")
    EExperimentType CurrentExperimentType = EExperimentType::Default;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recommendation|Personalized")
    UDataTable* PZRecommendationTable;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recommendation|Personalified")
    UDataTable* PFRecommendationTable;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recommendation|Default")
    UDataTable* DefaultRecommendationTable;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recommendation")
    TSubclassOf<class UUserWidget> RecommendationWidgetClass;
};
