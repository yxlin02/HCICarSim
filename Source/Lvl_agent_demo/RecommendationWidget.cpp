// RecommendationWidget.cpp

#include "RecommendationWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

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
        }
        else
        {
            TextBlock_RecommendationContent->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // -------- Image --------
    if (Image_ReactionContent)
    {
        if (InImage)
        {
            Image_ReactionContent->SetBrushFromTexture(InImage, true);
            Image_ReactionContent->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            Image_ReactionContent->SetVisibility(ESlateVisibility::Collapsed);
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
