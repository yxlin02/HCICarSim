// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RecommendationWidget.generated.h"

class UTextBlock;
class UImage;
class UBorder;

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

    // ===== 新增：清除所有显示内容的函数 =====
    UFUNCTION(BlueprintCallable)
    void ClearAllContent();
    // ===== 结束新增 =====

    // ===== 新增：只清除反应和推荐内容，保留背景 =====
    UFUNCTION(BlueprintCallable)
    void ClearReactionAndRecommendation();
    // ===== 结束新增 =====

protected:
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TextBlock_RecommendationContent;

    UPROPERTY(meta = (BindWidget))
    UImage* Image_ContentContent;

    UPROPERTY(meta = (BindWidget))
    UImage* Image_ReactionContent;

    UPROPERTY(meta = (BindWidget))
    UBorder* Border_ReactionBackground;

    UPROPERTY(meta = (BindWidget))
    UBorder* Border_TextBackground;
};