#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "WorldBLDLevelTemplateBundle.generated.h"

class UTexture2D;
class UWorld;

UCLASS(BlueprintType)
class WORLDBLDRUNTIME_API UWorldBLDLevelTemplateBundle : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|LevelTemplate")
	FString TemplateName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|LevelTemplate")
	FText TemplateDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|LevelTemplate")
	TSoftObjectPtr<UTexture2D> TemplatePreviewImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|LevelTemplate")
	TSoftObjectPtr<UWorld> TemplateLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|LevelTemplate")
	FString DefaultLightingTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD|LevelTemplate")
	TMap<FString, TSoftObjectPtr<UWorld>> CompatibleLightingPresets;
};
