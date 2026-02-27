// RecommendationWidget.cpp

#include "RecommendationWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"

// ===== 新增：在 Widget 构造时保存初始图片 =====
void URecommendationWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 保存 Image_ContentContent 的初始图片（游戏最开始的状态）
    if (Image_ContentContent)
    {
        const FSlateBrush& CurrentBrush = Image_ContentContent->GetBrush();
        if (CurrentBrush.GetResourceObject())
        {
            UObject* CurrentResource = CurrentBrush.GetResourceObject();
            OriginalContentImage = Cast<UTexture2D>(CurrentResource);

            if (OriginalContentImage)
            {
                UE_LOG(LogTemp, Display, TEXT("[RecommendationWidget] Initial image saved at construction: %s"),
                    *OriginalContentImage->GetName());
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[RecommendationWidget] Image_ContentContent has no initial texture set in Blueprint"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[RecommendationWidget] Image_ContentContent brush has no resource object"));
        }
    }
}
// ===== 结束新增 =====

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
            // ===== 修改：直接显示新图片，不需要再保存原始图片（已在 NativeConstruct 中保存） =====
            FSlateBrush Brush;
            Brush.SetResourceObject(InImage);
            Image_ContentContent->SetBrush(Brush);
            Image_ContentContent->SetVisibility(ESlateVisibility::Visible);

            UE_LOG(LogTemp, Display, TEXT("[RecommendationWidget] Content image updated: %s"),
                InImage ? *InImage->GetName() : TEXT("None"));
            // ===== 结束修改 =====
        }
        else
        {
            Image_ContentContent->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

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

void URecommendationWidget::ClearContent()
{
    if (Image_ContentContent)
    {
        // ===== 修改：恢复初始保存的原始图片 =====
        if (OriginalContentImage)
        {
            // 恢复游戏最开始的初始图片
            Image_ContentContent->SetBrushFromTexture(OriginalContentImage, true);
            Image_ContentContent->SetVisibility(ESlateVisibility::Visible);

            UE_LOG(LogTemp, Display, TEXT("[RecommendationWidget] Original image restored: %s"),
                *OriginalContentImage->GetName());
        }
        else
        {
            // 如果没有保存初始图片，则隐藏（备用方案）
            Image_ContentContent->SetVisibility(ESlateVisibility::Collapsed);
            UE_LOG(LogTemp, Warning, TEXT("[RecommendationWidget] No original image saved, hiding content instead"));
        }
        // ===== 结束修改 =====
    }
}