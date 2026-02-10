// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RecommendationWidget.generated.h"

class UTextBlock;
class UImage;

/**
 * 
 */
UCLASS()
class LVL_AGENT_DEMO_API URecommendationWidget : public UUserWidget
{
    GENERATED_BODY()
    
public:
    UFUNCTION(BlueprintCallable)
    void ShowReaction(
        const FText& InText = FText::GetEmpty(),
        UTexture2D* InImage = nullptr
    );
    
    UFUNCTION(BlueprintCallable)
    void ShowContent(UTexture2D* InImage);

protected:
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TextBlock_RecommendationContent;

    UPROPERTY(meta = (BindWidget))
    UImage* Image_ContentContent;

    UPROPERTY(meta = (BindWidget))
    UImage* Image_ReactionContent;
};
