// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "RecommendationTypes.h"
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
    /** 当前被试编号 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Design")
    int32 SubNum = 1;
    
    /** 当前模式（1/2） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Design")
    ERecommendMode Mode = ERecommendMode::Mode1;

    /** 当前 SceneID（1/2/3） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Design")
    int32 Block = 1;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Block")
    UDataTable* PhaseBlockTable;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recommendation")
    EExperimentType CurrentExperimentType = EExperimentType::Default;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recommendation|Personalized")
    UDataTable* PZRecommendationTable;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recommendation|Personalified")
    UDataTable* PFRecommendationTable;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recommendation|Default")
    UDataTable* DefaultRecommendationTable;
    
    
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recommendation|Patterns")
    UDataTable* RecommendationPatternTable;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Traffic|DefaultMapping")
    UDataTable* DefaultTrafficMappingTable;
    
    

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recommendation")
    TSubclassOf<class UUserWidget> RecommendationWidgetClass;
};
