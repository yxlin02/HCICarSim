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
            // ===== 另一种写法：直接使用引用 =====
            const FSlateBrush& CurrentBrush = Image_ContentContent->GetBrush();
            if (CurrentBrush.GetResourceObject())
            {
                // 从当前 Brush 中提取纹理资源
                UObject* CurrentResource = CurrentBrush.GetResourceObject();
                OriginalContentImage = Cast<UTexture2D>(CurrentResource);
                
                if (OriginalContentImage)
                {
                    UE_LOG(LogTemp, Display, TEXT("[RecommendationWidget] Original image saved: %s"), 
                           *OriginalContentImage->GetName());
                }
            }
            // ===== 结束修正 =====

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

void URecommendationWidget::ClearContent()
{
    if (Image_ContentContent)
    {
        // ===== 修改：恢复原始图片而不是隐藏 =====
        if (OriginalContentImage)
        {
            // 恢复之前保存的原始图片
            Image_ContentContent->SetBrushFromTexture(OriginalContentImage, true);
            Image_ContentContent->SetVisibility(ESlateVisibility::Visible);
            
            UE_LOG(LogTemp, Display, TEXT("[RecommendationWidget] Original image restored: %s"), 
                   *OriginalContentImage->GetName());
            
            // 清除缓存（可选，根据需求决定是否保留）
            // OriginalContentImage = nullptr;
        }
        else
        {
            // 如果没有原始图片，则隐藏
            Image_ContentContent->SetVisibility(ESlateVisibility::Collapsed);
            UE_LOG(LogTemp, Warning, TEXT("[RecommendationWidget] No original image to restore, hiding instead"));
        }
        // ===== 结束修改 =====
    }
}