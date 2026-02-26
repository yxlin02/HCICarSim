// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
//#include "Blueprint/WidgetBlueprintLibrary.h"
#include "WorldBLDViewportWidgetInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UWorldBLDViewportWidgetInterface : public UInterface { GENERATED_BODY() };

class WORLDBLDEDITOR_API IWorldBLDViewportWidgetInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WorldBLD")
	void GetStyleName(FName& Name);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="WorldBLD")
	void SetWidgetStyle(const UObject* Style);
	
protected:
	virtual void GetStyleName_Implementation(FName& Name) { Name = NAME_None; };
	virtual void SetWidgetStyle_Implementation(const UObject* Style) {};
};
