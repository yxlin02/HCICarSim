// Copyright WorldBLD LLC. All rights reserved. 

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "WorldBLDKitEditorUtils.generated.h"

UENUM(BlueprintType)
enum class EWorldBLDLicenseState : uint8
{
	NotFound = 0,
	Invalid,
	NoMatch,
	OutOfDate,
	Licensed,
};

UCLASS()
class WORLDBLDEDITOR_API UWorldBLDKitEditorUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category="WorldBLDEditor|Utils")
	static FString GetPlatformUserUniqueId();

	UFUNCTION(BlueprintCallable, Category="WorldBLDEditor|Utils")
	static void CopyTextToClipboard(FString Text);

	// Whether or not the current user is licensed to use the WorldBLD Editor tools.
	// NOTE: THIS IS DEPRECATED
	UFUNCTION(BlueprintCallable, Category="WorldBLDEditor|Utils")
	static EWorldBLDLicenseState CheckLicenseStatus();
	
	UFUNCTION(BlueprintCallable, Category = "WorldBLDEditor|Utils")
	static const UObject* GetStyleObjectByName(const UObject* InObj, const FName& Name);

	UFUNCTION(BlueprintCallable, Category = "WorldBLDEditor|Utils")
	static void GetAllWidgetsWithInterface(UWidget* Parent, TSubclassOf<UInterface> Interface, TArray<UWidget*>& FoundWidgets);

	static FString Internal_GetPlatformUserUniqueId();	


};
