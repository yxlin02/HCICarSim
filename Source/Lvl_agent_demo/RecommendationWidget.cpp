// RecommendationWidget.cpp

#include "RecommendationWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"

void URecommendationWidget::ShowReaction(
    const FText& InText,
    UTexture2D* InImage
)
{
    // -------- Text --------
    if (TextBlock_RecommendationContent)
    {
        if (!InText.IsEmpty())
        {
            TextBlock_RecommendationContent->SetText(InText);
            TextBlock_RecommendationContent->SetVisibility(ESlateVisibility::Visible);

            if (Border_TextBackground)
            {
                Border_TextBackground->SetVisibility(ESlateVisibility::Visible);
            }
        }
        else
        {
            TextBlock_RecommendationContent->SetVisibility(ESlateVisibility::Collapsed);

            if (Border_TextBackground)
            {
                Border_TextBackground->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
    }

    // -------- Image --------
    if (Image_ReactionContent)
    {
        if (InImage)
        {
            Image_ReactionContent->SetBrushFromTexture(InImage, true);
            Image_ReactionContent->SetVisibility(ESlateVisibility::Visible);

            if (Border_ReactionBackground)
            {
                Border_ReactionBackground->SetVisibility(ESlateVisibility::Visible);
            }
        }
        else
        {
            Image_ReactionContent->SetVisibility(ESlateVisibility::Collapsed);

            if (Border_ReactionBackground)
            {
                Border_ReactionBackground->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
    }
}

void URecommendationWidget::ShowContent(UTexture2D* InImage)
{
    if (Image_ContentContent)
    {
        if (InImage)
        {
            FSlateBrush Brush;
            Brush.SetResourceObject(InImage);
            Image_ContentContent->SetBrush(Brush);
            Image_ContentContent->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            Image_ContentContent->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

// ===== 新增：清除所有显示内容 =====
void URecommendationWidget::ClearAllContent()
{
    // 隐藏文本
    if (TextBlock_RecommendationContent)
    {
        TextBlock_RecommendationContent->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (Border_TextBackground)
    {
        Border_TextBackground->SetVisibility(ESlateVisibility::Collapsed);
    }

    // 隐藏反应图片
    if (Image_ReactionContent)
    {
        Image_ReactionContent->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (Border_ReactionBackground)
    {
        Border_ReactionBackground->SetVisibility(ESlateVisibility::Collapsed);
    }

    // 隐藏内容图片
    if (Image_ContentContent)
    {
        Image_ContentContent->SetVisibility(ESlateVisibility::Collapsed);
    }

    UE_LOG(LogTemp, Display, TEXT("[RecommendationWidget] All content cleared"));
}
// ===== 结束新增 =====

// ===== 新增：只清除反应和推荐内容，保留主背景 =====
void URecommendationWidget::ClearReactionAndRecommendation()
{
    // 隐藏推荐文本
    if (TextBlock_RecommendationContent)
    {
        TextBlock_RecommendationContent->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (Border_TextBackground)
    {
        Border_TextBackground->SetVisibility(ESlateVisibility::Collapsed);
    }

    // 隐藏反应图片
    if (Image_ReactionContent)
    {
        Image_ReactionContent->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (Border_ReactionBackground)
    {
        Border_ReactionBackground->SetVisibility(ESlateVisibility::Collapsed);
    }

    // 注意：不清除 Image_ContentContent（主背景图片）

    UE_LOG(LogTemp, Display, TEXT("[RecommendationWidget] Reaction and recommendation cleared, background preserved"));
}
// ===== 结束新增 =====