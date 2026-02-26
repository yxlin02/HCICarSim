// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
//#include "WorldBLDViewportWidgetInterface.h"
#include "ViewportButtonBase.h"
#include "WorldBLDViewportStyleTypes.generated.h"

UCLASS()
class UWorldBLDViewportWidgetStyle : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") 
	UViewportImageButtonStyle* MainMenuButton;

};

