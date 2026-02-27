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

UENUM(BlueprintType)
enum class ERecLaneMode : uint8
{
    Random,
    Defined
};

UENUM(BlueprintType)
enum class ELaneCarDensity : uint8
{
    Low  UMETA(DisplayName="Low"),
    Mid  UMETA(DisplayName="Mid"),
    High UMETA(DisplayName="High"),
};

UENUM(BlueprintType)
enum class ELaneCarVelocity : uint8
{
    Slow UMETA(DisplayName="Slow"),
    Mid  UMETA(DisplayName="Mid"),
    Fast UMETA(DisplayName="Fast"),
};

USTRUCT(BlueprintType)
struct FRecTrafficRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Category = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Subcategory = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ERecLaneMode Mode = ERecLaneMode::Random;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LaneIndex = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ELaneCarDensity CarDensity = ELaneCarDensity::Mid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ELaneCarVelocity CarVelocity = ELaneCarVelocity::Mid;
};

USTRUCT(BlueprintType)
struct FRecommendationPatternRow : public FTableRowBase
{
    GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName PatternID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 OrderIndex;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Category;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Subcategory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDelay = false;
};

UENUM(BlueprintType)
enum class ERecommendMode : uint8
{
    Mode0 = 0,
    Mode1 = 1,
    Mode2 = 2
};

USTRUCT(BlueprintType)
struct FExperimentBlockDesignRow : public FTableRowBase
{
    GENERATED_BODY()

public:
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Sub = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Block = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Mode = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SceneID = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName PatternID;
};
