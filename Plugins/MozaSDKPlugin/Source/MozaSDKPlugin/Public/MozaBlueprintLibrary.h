

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MozaBlueprintLibrary.generated.h"

/**
 * 
 */
UCLASS()
class MOZASDKPLUGIN_API UMozaBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "Moza")

	static float GetMozaSteeringAngle();

	UFUNCTION(BlueprintCallable, Category = "Moza|Pedals")

	static float GetAcceleratorValue();

	UFUNCTION(BlueprintCallable, Category = "Moza|Pedals")

	static float GetBrakeValue();
};
	

