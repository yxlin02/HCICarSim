#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundBase.h"
#include "RecommendationTypes.generated.h"

USTRUCT(BlueprintType)
struct FRecommendationTypes : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Category;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SubCategory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Item;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Recommendation_Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UTexture2D> Recommendation_Image;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> Recommendation_Sound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool Recommendation_Haptic = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UTexture2D> Content_Image;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> Content_Sound;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UTexture2D> Reaction_Accept_Image;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> Reaction_Accept_Sound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool Reaction_Accept_Haptic = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UTexture2D> Reaction_Decline_Image;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> Reaction_Decline_Sound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool Reaction_Decline_Haptic = true;
};
