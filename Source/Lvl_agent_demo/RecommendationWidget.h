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
    // ===== 新增：重写 NativeConstruct 在初始化时保存原始图片 =====
    virtual void NativeConstruct() override;
    // ===== 结束新增 =====

    UFUNCTION(BlueprintCallable)
    void ShowReaction(
        const FText& InText = FText::GetEmpty(),
        UTexture2D* InImage = nullptr
    );

    UFUNCTION(BlueprintCallable)
    void ShowContent(UTexture2D* InImage);

    UFUNCTION(BlueprintCallable)
    void ClearAllContent();

    UFUNCTION(BlueprintCallable)
    void ClearReactionAndRecommendation();

    UFUNCTION(BlueprintCallable, Category = "Recommendation")
    void ClearContent();

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

private:
    UPROPERTY()
    UTexture2D* OriginalContentImage;  // 保存游戏最开始的初始图片
};